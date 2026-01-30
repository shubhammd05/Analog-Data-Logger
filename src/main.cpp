/**
 * @file main.cpp
 * @brief ESP32 4-Channel Data Logger - Main Application
 * @version 1.0
 * @date 2026-01-11
 * @author Your Name
 * 
 * @description
 * Multi-channel data acquisition system with:
 * - 4 independent ADC channels (configurable sampling rates)
 * - Real-time SD card logging with seconds precision
 * - WiFi-based configuration and data download
 * - Automatic file rotation (dual-buffer system)
 * - System health monitoring (SD space, LED indicators)
 */

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <LittleFS.h>
#include <RTClib.h>
#include <esp_pm.h> // Power management

#include "ADC_config.h"
#include "Functions.h"
#include "comms.h"
#include "Config.h"

// =============================================================================
// DATA STRUCTURES
// =============================================================================

struct LogSample {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint16_t value;     // ADC value
};


// =============================================================================
// GLOBAL OBJECTS & VARIABLES
// =============================================================================

RTC_DS3231 rtc;                           // Real-time clock instance
DateTime now;                              // Current time
SemaphoreHandle_t mutex;                   // Mutex for SD card access

// Task handles
// TaskHandle_t Channel1TaskHandle = NULL;
// TaskHandle_t Channel2TaskHandle = NULL;
// TaskHandle_t Channel3TaskHandle = NULL;
// TaskHandle_t Channel4TaskHandle = NULL;
TaskHandle_t ChannelTasks[4] = {NULL, NULL, NULL, NULL};
TaskHandle_t WriterTaskHandle = NULL;
TaskHandle_t MonitorTaskHandle = NULL;

// Queues for buffering
QueueHandle_t logQueues[4];


// Interrupt flags
volatile bool uploadButtonPressed = false;
volatile bool isSDFull = false;

// ADC data buffers
uint16_t C1Data, C2Data, C3Data, C4Data;

// =============================================================================
// INTERRUPT SERVICE ROUTINES
// =============================================================================

/**
 * @brief ISR for upload button press
 * Sets flag to start WiFi AP in main loop
 */
void IRAM_ATTR uploadButtonISR() {
    uploadButtonPressed = true;
}

// =============================================================================
// RTOS TASKS
// =============================================================================

/**
 * @brief System monitor task - checks SD card status and updates LEDs
 * @param parameter Unused
 * 
 * Priority: Low (Core 0)
 * Stack: 4KB
 * Runs every 200ms
 * 
 * Responsibilities:
 * - Check SD card space every 5 seconds
 * - Update RGB LED based on system state
 * - Set isSDFull flag to pause logging when critical
 */
void SystemMonitorTask(void* parameter) {
    const unsigned long CHECK_INTERVAL = 5000;
    unsigned long lastCheck = 0;
    int sdStatus = 0;
    bool blinkState = false;

    while(1) {
        unsigned long currentMillis = millis();

        // Heavy SD check every 5 seconds
        if (currentMillis - lastCheck >= CHECK_INTERVAL) {
            lastCheck = currentMillis;
            
            if (sdCardIsReady) {
                if(xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                    sdStatus = checkSdCardStatus();
                    xSemaphoreGive(mutex);
                }
            } else {
                sdStatus = 2;  // Treat as full/error
            }
            
            isSDFull = (sdStatus == 2);
        }

        // LED priority logic (runs every cycle for responsiveness)
        if (sdStatus == 2) {
            // Priority 1: SD Full - Flashing magenta
            blinkState = !blinkState;
            setLedColor(blinkState, 0, blinkState);
        }
        else if (isApActive) {
            // Priority 2: AP Mode - Solid blue
            setLedColor(0, 0, 1);
        }
        else if (sdStatus == 1) {
            // Priority 3: SD Half Full - Solid yellow
            setLedColor(1, 1, 0);
        }
        else {
            // Priority 4: Normal - Green when logging, off when paused
            if(!digitalRead(SWITCH)) {
                setLedColor(0, 1, 0);
            } else {
                setLedColor(0, 0, 0);
            }
        }

        vTaskDelay(200 / portTICK_PERIOD_MS);
    }
}

// --- SD WRITER TASK (The Consumer) ---
// Monitors queues and writes data to SD card in bursts
void WriterTask(void* parameter) {
    LogSample sample;
    String dataBuffer;
    dataBuffer.reserve(RESERVE_MEMORY); // Reserve memory to prevent fragmentation

    while(1) {
        bool wroteData = false;

        // Logging allowed if: Switch is ON, SD is ready, and SD not full
        bool loggingEnabled = !digitalRead(SWITCH) && sdCardIsReady && !isSDFull;

        // Iterate through all 4 channels
        for (int i = 0; i < 4; i++) {
            // Check if queue has enough data for a burst OR is getting full
            if (uxQueueMessagesWaiting(logQueues[i]) >= BURST_WRITE_COUNT) {
                
                dataBuffer = ""; // Clear buffer
                
                // Pop N samples and format string
                for (int j = 0; j < BURST_WRITE_COUNT; j++) {
                    if (xQueueReceive(logQueues[i], &sample, 0) == pdTRUE) {
                        if (loggingEnabled) {
                            // Format: Y,M,D,H,M,S,Value\n
                            dataBuffer += String(sample.year) + "," + String(sample.month) + "," + 
                                          String(sample.day) + "," + String(sample.hour) + "," + 
                                          String(sample.minute) + "," + String(sample.second) + "," + 
                                          String(sample.value) + "\n";
                        }
                    }
                }

                // Write block to SD
                if (loggingEnabled && dataBuffer.length() > 0) {
                    if(xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                        String fname = getLogFilename(i + 1, true); // Get filename for channel i+1
                        writeCSVBuffer(dataBuffer, fname);
                        xSemaphoreGive(mutex);
                        wroteData = true;
                    }
                }
            }
        }

        // Power Optimization: 
        // If we wrote data, loop quickly to clear queues. 
        // If idle, sleep longer to allow CPU Light Sleep.
        if (wroteData) {
            vTaskDelay(10 / portTICK_PERIOD_MS);
        } else {
            vTaskDelay(100 / portTICK_PERIOD_MS); // Idle wait
        }
    }
}

// --- GENERIC CHANNEL TASK (The Producer) ---
// Reads ADC and pushes to queue. Does NOT access SD card directly.
void ChannelProducerTask(void *parameter) {
    int chIndex = (int)parameter; // 0 to 3
    
    while(!sdCardIsReady) vTaskDelay(100);

    TickType_t xLastWakeTime = xTaskGetTickCount();
    LogSample sample;

    while(1) {
        // Only sample if switch is ON and channel enabled
        if(!digitalRead(SWITCH) && channelConfigs[chIndex].enabled) {
            
            // 1. Read Sensor
            sample.value = channelConfigs[chIndex].Read();
            
            // 2. Get Time
            DateTime now = rtc.now();
            sample.year = now.year();
            sample.month = now.month();
            sample.day = now.day();
            sample.hour = now.hour();
            sample.minute = now.minute();
            sample.second = now.second();

            // 3. Push to Queue (Non-blocking if possible)
            // If queue is full, we drop the sample (better than blocking and crashing timing)
            if (xQueueSend(logQueues[chIndex], &sample, 0) != pdTRUE) {
                // Optional: Count dropped samples
                // DEBUG_PRINTF("Ch%d Queue Full!\n", chIndex + 1);
            }
        }

        // Precise Timing & Light Sleep Opportunity
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(channelConfigs[chIndex].samplingRate));
    }
}



/**
 * @brief Channel 1 data acquisition task
 * @param parameter Unused
 * 
 * Priority: Normal (Core 1)
 * Stack: 8KB
 * 
 * Reads GPIO34 ADC at configured sampling rate
 * Writes to /LOG_01.csv or /LOG_05.csv depending on active group
 */
// void Channel1Task(void *parameter) {
//     while(!sdCardIsReady) {
//         vTaskDelay(100 / portTICK_PERIOD_MS);
//     }

//     TickType_t xLastWakeTime = xTaskGetTickCount();

//     while(1) {
//         // Pause if SD is full
//         if (isSDFull) {
//             vTaskDelay(1000 / portTICK_PERIOD_MS);
//             continue;
//         }

//         // Check if logging is enabled
//         if(!digitalRead(SWITCH) && sdCardIsReady && channelConfigs[0].enabled) {
//             if(xSemaphoreTake(mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
//                 C1Data = C1.Read();
//                 now = rtc.now();
                
//                 writeCSVBuffer(now.year(), now.month(), now.day(), now.hour(), 
//                         now.minute(), now.second(), C1Data, 
//                         currentLogFiles[0], C1File);
                
//                 xSemaphoreGive(mutex);
//             }
//         }

//         vTaskDelay(1);  // Feed watchdog
//         vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(channelConfigs[0].samplingRate));
//     }
// }

// /**
//  * @brief Channel 2 data acquisition task (see Channel1Task for details)
//  */
// void Channel2Task(void *parameter) {
//     while(!sdCardIsReady) {
//         vTaskDelay(100 / portTICK_PERIOD_MS);
//     }

//     TickType_t xLastWakeTime = xTaskGetTickCount();

//     while(1) {
//         if (isSDFull) {
//             vTaskDelay(1000 / portTICK_PERIOD_MS);
//             continue;
//         }

//         if(!digitalRead(SWITCH) && sdCardIsReady && channelConfigs[1].enabled) {
//             if(xSemaphoreTake(mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
//                 C2Data = C2.Read();
//                 now = rtc.now();

//                 writeCSVBuffer(now.year(), now.month(), now.day(), now.hour(),
//                         now.minute(), now.second(), C2Data,
//                         currentLogFiles[1], C2File);
                
//                 xSemaphoreGive(mutex);
//             }
//         }

//         vTaskDelay(1);
//         vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(channelConfigs[1].samplingRate));
//     }
// }

// /**
//  * @brief Channel 3 data acquisition task (see Channel1Task for details)
//  */
// void Channel3Task(void *parameter) {
//     while(!sdCardIsReady) {
//         vTaskDelay(100 / portTICK_PERIOD_MS);
//     }

//     TickType_t xLastWakeTime = xTaskGetTickCount();

//     while(1) {
//         if (isSDFull) {
//             vTaskDelay(1000 / portTICK_PERIOD_MS);
//             continue;
//         }

//         if(!digitalRead(SWITCH) && sdCardIsReady && channelConfigs[2].enabled) {
//             if(xSemaphoreTake(mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
//                 C3Data = C3.Read();
//                 now = rtc.now();

//                 writeCSVBuffer(now.year(), now.month(), now.day(), now.hour(),
//                         now.minute(), now.second(), C3Data,
//                         currentLogFiles[2], C3File);
                
//                 xSemaphoreGive(mutex);
//             }
//         }

//         vTaskDelay(1);
//         vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(channelConfigs[2].samplingRate));
//     }
// }

// /**
//  * @brief Channel 4 data acquisition task (see Channel1Task for details)
//  */
// void Channel4Task(void *parameter) {
//     while(!sdCardIsReady) {
//         vTaskDelay(100 / portTICK_PERIOD_MS);
//     }

//     TickType_t xLastWakeTime = xTaskGetTickCount();

//     while(1) {
//         if (isSDFull) {
//             vTaskDelay(1000 / portTICK_PERIOD_MS);
//             continue;
//         }

//         if(!digitalRead(SWITCH) && sdCardIsReady && channelConfigs[3].enabled) {
//             if(xSemaphoreTake(mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
//                 C4Data = C4.Read();
//                 now = rtc.now();

//                 writeCSVBuffer(now.year(), now.month(), now.day(), now.hour(),
//                         now.minute(), now.second(), C4Data,
//                         currentLogFiles[3], C4File);
                
//                 xSemaphoreGive(mutex);
//             }
//         }

//         vTaskDelay(1);
//         vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(channelConfigs[3].samplingRate));
//     }
// }

// =============================================================================
// SETUP & INITIALIZATION
// =============================================================================

/**
 * @brief System initialization
 * 
 * Initialization sequence:
 * 1. Serial communication (9600 baud)
 * 2. GPIO configuration (buttons, LEDs, switches)
 * 3. Watchdog disable during setup
 * 4. LittleFS filesystem mount
 * 5. I2C bus initialization
 * 6. RTC initialization with retry logic
 * 7. SD card initialization
 * 8. Configuration loading from LittleFS
 * 9. Web server route setup
 * 10. File group swap (activate Group A or B)
 * 11. Mutex creation
 * 12. WiFi event handler registration
 * 13. RTOS task creation
 * 14. Watchdog re-enable
 */
void setup() {
    Serial.begin(9600);
    delay(1000);

    // Power Management Configuration
    #if LIGHT_SLEEP_EN
        esp_pm_config_esp32_t pm_config;
        pm_config.max_freq_mhz = 240;
        pm_config.min_freq_mhz = 80;
        pm_config.light_sleep_enable = true; // Enable automatic light sleep
        esp_pm_configure(&pm_config);
    #endif


    // GPIO initialization
    pinMode(UPLOAD_BUTTON, INPUT_PULLUP);
    pinMode(LEDR, OUTPUT);
    pinMode(LEDG, OUTPUT);
    pinMode(LEDB, OUTPUT);
    pinMode(SWITCH, INPUT_PULLUP);

    attachInterrupt(digitalPinToInterrupt(UPLOAD_BUTTON), uploadButtonISR, FALLING);

    // Disable watchdog during setup
    disableCore0WDT();
    disableCore1WDT();

    // Initialize LittleFS
    if(!LittleFS.begin(true)) {
        DEBUG_PRINTLN("FATAL: LittleFS Mount Failed.");
    } else {
        DEBUG_PRINTLN("LittleFS Initialized.");
    }
    delay(100);

    // Initialize I2C and RTC
    Wire.begin();
    delay(500);

    int rtcRetries = 3;
    while (!rtc.begin() && rtcRetries > 0) {
        DEBUG_PRINTLN("RTC init failed, retrying...");
        delay(500);
        rtcRetries--;
    }

    if (rtcRetries == 0) {
        DEBUG_PRINTLN("FATAL: Couldn't find RTC");
        indicateError();
        while (1) delay(10);
    }
    DEBUG_PRINTLN("RTC initialized.");
    delay(100);

    // Initialize SD card
    if (!SD.begin(SD_CS, SPI, 4000000)) {
        DEBUG_PRINTLN("Card Mount Failed!");
        indicateError();
        sdCardIsReady = false;
        while (1);
    }
    DEBUG_PRINTLN("SD Card initialized.");
    sdCardIsReady = true;
    delay(200);

    // Check initial SD status and set LED
    int status = checkSdCardStatus();
    if(status == 0) indicateNormal();

    // Load configuration and setup
    loadConfigFromLittleFS();
    setupWebRoutes();
    swapLoggingGroups();
    setCpuFrequencyMhz(CPU_FREQ_LOGGING);

    // Create mutex for SD card access
    mutex = xSemaphoreCreateMutex();

    for(int i=0; i<4; i++) {
        logQueues[i] = xQueueCreate(QUEUE_SIZE, sizeof(LogSample));
        if(logQueues[i] == NULL) {
            DEBUG_PRINTLN("Queue Create Failed!");
            while(1);
        }
    }

    if (mutex == NULL) {
        DEBUG_PRINTLN("FATAL: Mutex creation failed!");
        while(1);
    }

    // Register WiFi event handlers
    WiFi.onEvent(onStationDisconnected, ARDUINO_EVENT_WIFI_AP_STADISCONNECTED);

    DEBUG_PRINTLN("Ready. Press button to start AP.");
    indicateNormal();
    delay(500);

    // Create RTOS tasks
    xTaskCreatePinnedToCore(ChannelProducerTask, "Channel1Task", 4096, (void*)0, 2, &ChannelTasks[0], 1);
    xTaskCreatePinnedToCore(ChannelProducerTask, "Channel2Task", 4096, (void*)1, 2, &ChannelTasks[1], 1);
    xTaskCreatePinnedToCore(ChannelProducerTask, "Channel3Task", 4096, (void*)2, 2, &ChannelTasks[2], 1);
    xTaskCreatePinnedToCore(ChannelProducerTask, "Channel4Task", 4096, (void*)3, 2, &ChannelTasks[3], 1);

    // Writer Task on Core 0 
    xTaskCreatePinnedToCore(WriterTask, "WriterTask", 8192, NULL, 1, &WriterTaskHandle, 0);

    // Monitor Task on Core 0
    xTaskCreatePinnedToCore(SystemMonitorTask, "MonitorTask", 4096, NULL, 0, &MonitorTaskHandle, 0);

    // Re-enable watchdog
    enableCore0WDT();
    enableCore1WDT();
}

// =============================================================================
// MAIN LOOP
// =============================================================================

/**
 * @brief Main program loop
 * 
 * Responsibilities:
 * - Monitor upload button press
 * - Start WiFi AP when requested
 * - Process DNS requests for captive portal
 * - Check AP timeout
 * - Feed watchdog
 */
void loop() {
    // Handle upload button press
    if (uploadButtonPressed) {
        uploadButtonPressed = false;
        if (!isApActive) {
            // Disable Light Sleep during WiFi AP mode for stability
            #if LIGHT_SLEEP_EN
                esp_pm_config_esp32_t pm_config;
                pm_config.max_freq_mhz = 240;
                pm_config.min_freq_mhz = 240; // Lock to high freq
                pm_config.light_sleep_enable = false;
                esp_pm_configure(&pm_config);
            #endif

            DEBUG_PRINTF("Free Heap BEFORE WiFi: %d bytes\n", ESP.getFreeHeap());
            swapLoggingGroups();
            startAccessPoint();
        }
    }

    // Process WiFi AP requests
    if (isApActive) {
        dnsServer.processNextRequest();
        checkApTimeout();
    }

    // Feed watchdog
    yield();


    // Note: Light sleep re-enabling logic can be complex. 
    // For simplicity, we only disable it when entering AP mode.
    // To re-enable, you would need to detect AP stop and call esp_pm_configure again with true.

    vTaskDelay(10 / portTICK_PERIOD_MS);
}
