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
#include <esp_task_wdt.h>

#define SD_CS 5

RTC_DS3231 rtc;

volatile bool uploadButtonPressed = false;
volatile bool isSDFull = false;
unsigned long bootMillis = 0;  // Track milliseconds for sub-second timing

void IRAM_ATTR uploadButtonISR() {
    uploadButtonPressed = true;
}

uint16_t C1Data, C2Data, C3Data, C4Data;
DateTime now;
static SemaphoreHandle_t mutex;

TaskHandle_t Channel1TaskHandle = NULL;
TaskHandle_t Channel2TaskHandle = NULL;
TaskHandle_t Channel3TaskHandle = NULL;
TaskHandle_t Channel4TaskHandle = NULL;
TaskHandle_t MonitorTaskHandle = NULL;

void SystemMonitorTask(void* parameter){
    const unsigned long CHECK_INTERVAL = 5000; // Check SD every 5 seconds
    unsigned long lastCheck = 0;
    int sdStatus = 0; // 0=OK, 1=Low, 2=Full
    bool blinkState = false;

    while(1){
        unsigned long currentMillis = millis();

        // 1. HEAVY CHECK (Every 5 seconds)
        if (currentMillis - lastCheck >= CHECK_INTERVAL) {
            lastCheck = currentMillis;
            
            // Only check if SD is supposed to be ready
            if (sdCardIsReady) {
                // LOCK MUTEX before accessing SD (Critical!)
                if(xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) == pdTRUE){
                    sdStatus = checkSdCardStatus();
                    xSemaphoreGive(mutex);
                }
            } else {
                sdStatus = 2; // Treat not ready as "Full/Error"
            }

            // UPDATE GLOBAL SIGNAL
            isSDFull = (sdStatus == 2);
        }

        // 2. LED LOGIC (Runs frequently for responsiveness)
        // Priority 1: SD Full -> Magenta (Flickering)
        if (sdStatus == 2) {
            blinkState = !blinkState;
            setLedColor(blinkState, 0, blinkState); // Red + Blue = Magenta
        }
        // Priority 2: AP Mode Active -> Blue
        else if (isApActive) {
            setLedColor(0, 0, 1);
        }
        // Priority 3: SD Low Space -> Yellow
        else if (sdStatus == 1) {
            setLedColor(1, 1, 0); // Red + Green = Yellow
        }
        // Priority 4: Normal Logging -> Green
        else {
            // Check manual switch (if you use it to pause)
            if(!digitalRead(SWITCH)){
                setLedColor(0, 1, 0); // Green
            } else {
                setLedColor(0, 0, 0); // Off (Paused)
            }
        }

        vTaskDelay(200 / portTICK_PERIOD_MS); // Update LEDs 5 times/sec
    }
}

void Channel1Task(void *parameter) {
    // Wait for SD to be ready
    while(!sdCardIsReady) {
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    
    TickType_t xLastWakeTime = xTaskGetTickCount();

    while(1){
        // Stop if manager say SD is full
        if (isSDFull) {
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }
        if(!digitalRead(SWITCH) && sdCardIsReady && channelConfigs[0].enabled){
            if(xSemaphoreTake(mutex, pdMS_TO_TICKS(50)) == pdTRUE){
                C1Data = C1.Read();
                now = rtc.now();
                uint16_t millisecond = (millis() - bootMillis) % 1000;
                writeCSV(now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second(), millisecond, C1Data, currentLogFiles[0], C1File);
                xSemaphoreGive(mutex);
            }
        }

        // Feed watchdog
        vTaskDelay(1);

        // delay until next sample time
        // vTaskDelay(channelConfigs[0].samplingRate / portTICK_PERIOD_MS);
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(channelConfigs[0].samplingRate));
    }
}

void Channel2Task(void *parameter) {
    while(!sdCardIsReady) {
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    
    TickType_t xLastWakeTime = xTaskGetTickCount();
    
    while(1){
        // Stop if manager say SD is full
        if (isSDFull) {
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }

        if(!digitalRead(SWITCH) && sdCardIsReady && channelConfigs[1].enabled){
            
            if(xSemaphoreTake(mutex, pdMS_TO_TICKS(50)) == pdTRUE){
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

void Channel3Task(void *parameter) {
    while(!sdCardIsReady) {
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    
    TickType_t xLastWakeTime = xTaskGetTickCount();
    
    while(1){
        // Stop if manager say SD is full
        if (isSDFull) {
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }
        if(!digitalRead(SWITCH) && sdCardIsReady && channelConfigs[2].enabled){
            
            if(xSemaphoreTake(mutex, pdMS_TO_TICKS(50)) == pdTRUE){
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

void Channel4Task(void *parameter) {
    while(!sdCardIsReady) {
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    
    TickType_t xLastWakeTime = xTaskGetTickCount();
    
    while(1){
        // Stop if manager say SD is full
        if (isSDFull) {
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }
        if(!digitalRead(SWITCH) && sdCardIsReady && channelConfigs[3].enabled){
            
            if(xSemaphoreTake(mutex, pdMS_TO_TICKS(50)) == pdTRUE){
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

void setup() {
    Serial.begin(9600);
    delay(1000);
    
    pinMode(UPLOAD_BUTTON, INPUT_PULLUP);
    pinMode(LEDR, OUTPUT);
    pinMode(LEDG, OUTPUT);
    pinMode(LEDB, OUTPUT);
    pinMode(SWITCH, INPUT_PULLUP);
    
    attachInterrupt(digitalPinToInterrupt(UPLOAD_BUTTON), uploadButtonISR, FALLING);

    disableCore0WDT();
    disableCore1WDT();

    // Initialize LittleFS
    if(!LittleFS.begin(true)){
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
    if (!SD.begin(SD_CS, SPI)) {
        DEBUG_PRINTLN("Card Mount Failed!");
        indicateError();
        sdCardIsReady = false;
        while (1);
    }
    
    DEBUG_PRINTLN("SD Card initialized.");
    sdCardIsReady = true;
    delay(200);

    // --- CALCULATE SPACE STATUS IMMEDIATELY ---
    int status = checkSdCardStatus(); 
    if(status == 0) indicateNormal(); // Explicitly set Green if OK
    // ------------------------------------------

    loadConfigFromLittleFS();
    setupWebRoutes();
    swapLoggingGroups();
    
    bootMillis = millis();  // Track boot time for millisecond calculation
    
    setCpuFrequencyMhz(CPU_FREQ_LOGGING);

    mutex = xSemaphoreCreateMutex();
    if (mutex == NULL) {
        DEBUG_PRINTLN("FATAL: Mutex creation failed!");
        while(1);
    }
    
    WiFi.onEvent(onStationDisconnected, ARDUINO_EVENT_WIFI_AP_STADISCONNECTED);
    
    DEBUG_PRINTLN("Ready. Press button to start AP.");
    indicateNormal();
    delay(500);
    
    xTaskCreatePinnedToCore(Channel1Task, "Channel1Task", 8192, NULL, 1, &Channel1TaskHandle, 1);
    xTaskCreatePinnedToCore(Channel2Task, "Channel2Task", 8192, NULL, 1, &Channel2TaskHandle, 1);
    xTaskCreatePinnedToCore(Channel3Task, "Channel3Task", 8192, NULL, 1, &Channel3TaskHandle, 1);
    xTaskCreatePinnedToCore(Channel4Task, "Channel4Task", 8192, NULL, 1, &Channel4TaskHandle, 1);
    // Start System Monitor on Core 1 (same as others) or Core 0 (background)
    // Core 0 is safer for "housekeeping" to not block logging on Core 1
    xTaskCreatePinnedToCore(SystemMonitorTask, "MonitorTask", 4096, NULL, 0, &MonitorTaskHandle, 0);

    // Re-enable watchdog with longer timeout
    enableCore0WDT();
    enableCore1WDT();
}

void loop(){
    if (uploadButtonPressed) {
        uploadButtonPressed = false;
        if (!isApActive) {
            DEBUG_PRINTF("Free Heap BEFORE WiFi: %d bytes\n", ESP.getFreeHeap());
            swapLoggingGroups();
            startAccessPoint();
        }
    }
    
    if (isApActive) {
        dnsServer.processNextRequest();
        checkApTimeout();
    }
    
    // Feed watchdog - CRITICAL!
    yield();

    vTaskDelay(10 / portTICK_PERIOD_MS);
}
