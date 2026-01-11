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
 * - Real-time SD card logging with millisecond precision
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

#include "ADC_config.h"
#include "Functions.h"
#include "comms.h"
#include "Config.h"

// =============================================================================
// GLOBAL OBJECTS & VARIABLES
// =============================================================================

RTC_DS3231 rtc;                           // Real-time clock instance
DateTime now;                              // Current time
SemaphoreHandle_t mutex;                   // Mutex for SD card access

// Task handles
TaskHandle_t Channel1TaskHandle = NULL;
TaskHandle_t Channel2TaskHandle = NULL;
TaskHandle_t Channel3TaskHandle = NULL;
TaskHandle_t Channel4TaskHandle = NULL;
TaskHandle_t MonitorTaskHandle = NULL;

// Interrupt flags
volatile bool uploadButtonPressed = false;
volatile bool isSDFull = false;

// Timing
unsigned long bootMillis = 0;              // Boot timestamp for millisecond tracking

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
void Channel1Task(void *parameter) {
    while(!sdCardIsReady) {
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }

    TickType_t xLastWakeTime = xTaskGetTickCount();

    while(1) {
        // Pause if SD is full
        if (isSDFull) {
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }

        // Check if logging is enabled
        if(!digitalRead(SWITCH) && sdCardIsReady && channelConfigs[0].enabled) {
            if(xSemaphoreTake(mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                C1Data = C1.Read();
                now = rtc.now();
                uint16_t millisecond = (millis() - bootMillis) % 1000;
                
                writeCSV(now.year(), now.month(), now.day(), now.hour(), 
                        now.minute(), now.second(), millisecond, C1Data, 
                        currentLogFiles[0], C1File);
                
                xSemaphoreGive(mutex);
            }
        }

        vTaskDelay(1);  // Feed watchdog
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(channelConfigs[0].samplingRate));
    }
}

/**
 * @brief Channel 2 data acquisition task (see Channel1Task for details)
 */
void Channel2Task(void *parameter) {
    while(!sdCardIsReady) {
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }

    TickType_t xLastWakeTime = xTaskGetTickCount();

    while(1) {
        if (isSDFull) {
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }

        if(!digitalRead(SWITCH) && sdCardIsReady && channelConfigs[1].enabled) {
            if(xSemaphoreTake(mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                C2Data = C2.Read();
                now = rtc.now();
                uint16_t millisecond = (millis() - bootMillis) % 1000;
                
                writeCSV(now.year(), now.month(), now.day(), now.hour(),
                        now.minute(), now.second(), millisecond, C2Data,
                        currentLogFiles[1], C2File);
                
                xSemaphoreGive(mutex);
            }
        }

        vTaskDelay(1);
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(channelConfigs[1].samplingRate));
    }
}

/**
 * @brief Channel 3 data acquisition task (see Channel1Task for details)
 */
void Channel3Task(void *parameter) {
    while(!sdCardIsReady) {
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }

    TickType_t xLastWakeTime = xTaskGetTickCount();

    while(1) {
        if (isSDFull) {
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }

        if(!digitalRead(SWITCH) && sdCardIsReady && channelConfigs[2].enabled) {
            if(xSemaphoreTake(mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                C3Data = C3.Read();
                now = rtc.now();
                uint16_t millisecond = (millis() - bootMillis) % 1000;
                
                writeCSV(now.year(), now.month(), now.day(), now.hour(),
                        now.minute(), now.second(), millisecond, C3Data,
                        currentLogFiles[2], C3File);
                
                xSemaphoreGive(mutex);
            }
        }

        vTaskDelay(1);
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(channelConfigs[2].samplingRate));
    }
}

/**
 * @brief Channel 4 data acquisition task (see Channel1Task for details)
 */
void Channel4Task(void *parameter) {
    while(!sdCardIsReady) {
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }

    TickType_t xLastWakeTime = xTaskGetTickCount();

    while(1) {
        if (isSDFull) {
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }

        if(!digitalRead(SWITCH) && sdCardIsReady && channelConfigs[3].enabled) {
            if(xSemaphoreTake(mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                C4Data = C4.Read();
                now = rtc.now();
                uint16_t millisecond = (millis() - bootMillis) % 1000;
                
                writeCSV(now.year(), now.month(), now.day(), now.hour(),
                        now.minute(), now.second(), millisecond, C4Data,
                        currentLogFiles[3], C4File);
                
                xSemaphoreGive(mutex);
            }
        }

        vTaskDelay(1);
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(channelConfigs[3].samplingRate));
    }
}

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
    bootMillis = millis();
    setCpuFrequencyMhz(CPU_FREQ_LOGGING);

    // Create mutex for SD card access
    mutex = xSemaphoreCreateMutex();
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
    xTaskCreatePinnedToCore(Channel1Task, "Channel1Task", 8192, NULL, 1, &Channel1TaskHandle, 1);
    xTaskCreatePinnedToCore(Channel2Task, "Channel2Task", 8192, NULL, 1, &Channel2TaskHandle, 1);
    xTaskCreatePinnedToCore(Channel3Task, "Channel3Task", 8192, NULL, 1, &Channel3TaskHandle, 1);
    xTaskCreatePinnedToCore(Channel4Task, "Channel4Task", 8192, NULL, 1, &Channel4TaskHandle, 1);
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
    vTaskDelay(10 / portTICK_PERIOD_MS);
}
