/**
 * @file comms.h
 * @brief WiFi Communication, Web Server, and File Management
 * @version 1.0
 * @date 2026-01-11
 * 
 * @description
 * Handles all network communication and file operations:
 * - WiFi Access Point with captive portal
 * - Asynchronous web server (data download, configuration)
 * - Dual-buffer file rotation system
 * - JSON configuration management
 * - DNS server for captive portal redirection
 */

#ifndef COMMS_H
#define COMMS_H

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "ADC_config.h"
#include "Config.h"

// =============================================================================
// EXTERNAL OBJECTS
// =============================================================================
// Allow access to the RTC defined in main.cpp
extern RTC_DS3231 rtc;

// =============================================================================
// NETWORK CONFIGURATION
// =============================================================================

const char* AP_SSID = "ESP32-Data-Fetcher";
const char* AP_PASSWORD = "password";
const byte DNS_PORT = 53;

// =============================================================================
// CONSTANTS
// =============================================================================

const size_t JSON_BUFFER_SIZE = 2048;
const int TOTAL_LOG_FILE_COUNT = 8;
const int AP_TIMEOUT_MS = 300000;  // 5 minutes

// =============================================================================
// GLOBAL OBJECTS
// =============================================================================

DNSServer dnsServer;
AsyncWebServer server(80);

// =============================================================================
// GLOBAL STATE VARIABLES
// =============================================================================

bool isApActive = false;
bool sdCardIsReady = false;
bool isLogGroupAActive = true;
unsigned long apStartTime = 0;
bool isApTimeoutEnabled = true;
extern SemaphoreHandle_t mutex;

// File paths for active logging group
String C1File, C2File, C3File, C4File;

// Channel configuration array
extern ADC channelConfigs[4];
ADC channelConfigs[] = {C1, C2, C3, C4};

// File handles for current logging
File currentLogFiles[4];

// =============================================================================
// FUNCTION DECLARATIONS
// =============================================================================

// Core functions
void startAccessPoint();
void stopAccessPoint();
void setupWebRoutes();

// File management
void swapLoggingGroups();
void clearLogFile(int channelId);
String getLogFilename(int channelId, bool forLogging);

// Configuration management
void saveConfigToLittleFS();
void loadConfigFromLittleFS();
bool processJsonConfig(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);

// Web handlers
void handleFileRequest(String path, String contentType, AsyncWebServerRequest *request);
void handleFileList(AsyncWebServerRequest *request);
void handleNotFound(AsyncWebServerRequest *request);

// Event handlers
void checkApTimeout();
void onStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info);

// =============================================================================
// FILE MANAGEMENT FUNCTIONS
// =============================================================================

/**
 * @brief Get log filename for specified channel and group
 * @param channelId Channel number (1-4)
 * @param forLogging true = active logging group, false = download group
 * @return Full path to CSV file (e.g., "/LOG_01.csv")
 * 
 * File naming scheme:
 * - Group A: /LOG_01.csv through /LOG_04.csv
 * - Group B: /LOG_05.csv through /LOG_08.csv
 */
String getLogFilename(int channelId, bool forLogging) {
    int index = channelId - 1;
    int base_index = isLogGroupAActive ? 1 : 5;
    
    if (!forLogging) {
        base_index = isLogGroupAActive ? 5 : 1;  // Opposite group
    }
    
    int file_number = base_index + index;
    String filename = "/LOG_";
    if (file_number < 10) filename += "0";
    filename += String(file_number) + ".csv";
    
    DEBUG_PRINTF("getLogFilename(%d, %s) = %s (group %s)\n",
                 channelId, forLogging ? "logging" : "download",
                 filename.c_str(), isLogGroupAActive ? "A" : "B");
    
    return filename;
}

/**
 * @brief Swap between logging groups (A ↔ B)
 * 
 * Dual-buffer system:
 * - Group A (LOG_01-04): Active logging
 * - Group B (LOG_05-08): Available for download
 * 
 * When user presses upload button:
 * 1. Close current logging files
 * 2. Toggle active group
 * 3. Open new logging files
 * 4. Previous group becomes available for download
 */
void swapLoggingGroups() {
    if (!sdCardIsReady) return;
    
    DEBUG_PRINTLN("--- LOGGING FILE SWAP ---");
    
    // Close current files
    for (int i = 0; i < 4; i++) {
        if (currentLogFiles[i]) {
            currentLogFiles[i].close();
            DEBUG_PRINTF("Closed log file for Channel %d.\n", i + 1);
        }
    }
    
    // Toggle group
    isLogGroupAActive = !isLogGroupAActive;
    
    if(isLogGroupAActive) {
        C1File = "/LOG_01.csv";
        C2File = "/LOG_02.csv";
        C3File = "/LOG_03.csv";
        C4File = "/LOG_04.csv";
    } else {
        C1File = "/LOG_05.csv";
        C2File = "/LOG_06.csv";
        C3File = "/LOG_07.csv";
        C4File = "/LOG_08.csv";
    }
    
    // Create files with headers if needed
    String files[] = {C1File, C2File, C3File, C4File};
    for(int i = 0; i < 4; i++) {
        writeCSVHeader(files[i]);
    }
    
    DEBUG_PRINTF("Switched to Group %s\n", isLogGroupAActive ? "A (01-04)" : "B (05-08)");
}

/**
 * @brief Clear specified log file in download group
 * @param channelId Channel number (1-4)
 * 
 * Called automatically when:
 * - AP times out (5 minutes)
 * - Last client disconnects
 */
void clearLogFile(int channelId) {
    if (!sdCardIsReady || channelId < 1 || channelId > 4) {
        DEBUG_PRINTLN("ERROR: Cannot clear log file.");
        return;
    }
    
    DEBUG_PRINTLN("--- CLEARING LOG FILE ---");
    String filenameToClear = getLogFilename(channelId, false);
    DEBUG_PRINTF("Clearing: %s\n", filenameToClear.c_str());
    
    if (SD.exists(filenameToClear)) {
        if (SD.remove(filenameToClear)) {
            DEBUG_PRINTF("Successfully cleared: %s\n", filenameToClear.c_str());
        } else {
            DEBUG_PRINTF("ERROR: Failed to clear: %s\n", filenameToClear.c_str());
        }
    } else {
        DEBUG_PRINTF("INFO: File does not exist: %s\n", filenameToClear.c_str());
    }
}

// =============================================================================
// CONFIGURATION MANAGEMENT
// =============================================================================

/**
 * @brief Save channel configuration to LittleFS
 * 
 * Saves to /config.json with structure:
 * {
 *   "channels": [
 *     {
 *       "id": 1,
 *       "name": "Battery_Voltage",
 *       "enabled": true,
 *       "samplingRate": 1000,
 *       "resolution": 12,
 *       "attenuation": 3,
 *       "unit": "V",
 *       "calibration_scale": 0.00293,
 *       "calibration_offset": 0.0
 *     },
 *     ...
 *   ],
 *   "session_info": {
 *     "device_id": "ESP32-Logger-001",
 *     "firmware_version": "2.0"
 *   }
 * }
 */
void saveConfigToLittleFS() {
    if (!LittleFS.begin(true)) {
        DEBUG_PRINTLN("ERROR: LittleFS not mounted!");
        return;
    }
    
    DEBUG_PRINTLN("Saving config to LittleFS...");
    
    StaticJsonDocument<JSON_BUFFER_SIZE> doc;
    JsonArray channels = doc.createNestedArray("channels");
    
    for (int i = 0; i < 4; i++) {
        JsonObject channel = channels.createNestedObject();
        channel["id"] = channelConfigs[i].id;
        channel["name"] = channelConfigs[i].name;
        channel["enabled"] = channelConfigs[i].enabled;
        channel["samplingRate"] = channelConfigs[i].samplingRate;
        channel["resolution"] = channelConfigs[i].resolution;
        channel["attenuation"] = channelConfigs[i].attenuation;
        channel["unit"] = channelConfigs[i].unit;
        channel["calibration_offset"] = channelConfigs[i].calibration_offset;
        channel["calibration_scale"] = channelConfigs[i].calibration_scale;
    }
    
    // Add session metadata
    JsonObject session = doc.createNestedObject("session_info");
    session["device_id"] = "ESP32-Logger-001";
    session["firmware_version"] = "1.0";
    
    File file = LittleFS.open("/config.json", FILE_WRITE);
    if (!file) {
        DEBUG_PRINTLN("ERROR: Failed to create config file!");
        return;
    }
    
    serializeJsonPretty(doc, file);
    file.close();
    
    DEBUG_PRINTLN("Configuration saved successfully.");
}

/**
 * @brief Load channel configuration from LittleFS
 * 
 * Loads /config.json and applies settings to all channels
 * Falls back to defaults if file missing or corrupt
 */
void loadConfigFromLittleFS() {
    if (!LittleFS.begin(true) || !LittleFS.exists("/config.json")) {
        DEBUG_PRINTLN("No saved config found. Using defaults.");
        return;
    }
    
    DEBUG_PRINTLN("Loading configuration from LittleFS...");
    
    File file = LittleFS.open("/config.json", FILE_READ);
    if (!file) {
        DEBUG_PRINTLN("ERROR: Failed to open config file!");
        return;
    }
    
    StaticJsonDocument<JSON_BUFFER_SIZE> doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    
    if (error) {
        DEBUG_PRINTF("ERROR: Failed to parse config! %s\n", error.c_str());
        return;
    }
    
    JsonArray channels = doc["channels"];
    if (channels.isNull()) {
        DEBUG_PRINTLN("ERROR: Invalid config format!");
        return;
    }
    
    int i = 0;
    for (JsonObject channel : channels) {
        if (i >= 4) break;
        
        channelConfigs[i].id = channel["id"] | channelConfigs[i].id;
        channelConfigs[i].name = channel["name"] | channelConfigs[i].name;
        channelConfigs[i].enabled = channel["enabled"] | channelConfigs[i].enabled;
        channelConfigs[i].samplingRate = channel["samplingRate"] | channelConfigs[i].samplingRate;
        channelConfigs[i].resolution = channel["resolution"] | channelConfigs[i].resolution;
        channelConfigs[i].unit = channel["unit"] | channelConfigs[i].unit;
        channelConfigs[i].calibration_offset = channel["calibration_offset"] | channelConfigs[i].calibration_offset;
        channelConfigs[i].calibration_scale = channel["calibration_scale"] | channelConfigs[i].calibration_scale;
        
        // Apply attenuation
        int atten = channel["attenuation"] | ADC_11db;
        channelConfigs[i].attenuation = atten;
        channelConfigs[i].SetAttenuation(atten);
        
        DEBUG_PRINTF("Loaded Ch%d: %s, %dms, %s, scale=%.5f\n",
            channelConfigs[i].id, channelConfigs[i].name.c_str(),
            channelConfigs[i].samplingRate, channelConfigs[i].unit.c_str(),
            channelConfigs[i].calibration_scale);
        i++;
    }
    
    DEBUG_PRINTLN("Configuration loaded successfully.");
}

/**
 * @brief Process incoming JSON configuration from web POST
 * @param request HTTP request object
 * @param data JSON payload
 * @param len Data length
 * @param index Current chunk index
 * @param total Total data length
 * @return true if successful, false otherwise
 */
bool processJsonConfig(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    if (index + len == total) {
        String jsonString = (const char*)data;
        StaticJsonDocument<JSON_BUFFER_SIZE> doc;
        DeserializationError error = deserializeJson(doc, jsonString);
        
        if (error) {
            DEBUG_PRINTLN("JSON Parsing failed! Error: " + String(error.c_str()));
            request->send(500, "text/plain", "JSON parsing failed.");
            return false;
        }
        
        JsonArray channels = doc["channels"];
        if (channels.isNull() || channels.size() == 0) {
            DEBUG_PRINTLN("JSON missing or invalid 'channels' array.");
            request->send(400, "text/plain", "JSON missing 'channels' array.");
            return false;
        }
        
        DEBUG_PRINTLN("--- Received Configuration ---");
        int i = 0;
        for (JsonObject channel : channels) {
            channelConfigs[i].id = channel["id"] | 0;
            channelConfigs[i].name = channel["name"] | "NA";
            channelConfigs[i].enabled = channel["enabled"] | false;
            channelConfigs[i].samplingRate = channel["samplingRate"] | 1000;
            channelConfigs[i].resolution = channel["resolution"] | 12;
            channelConfigs[i].unit = channel["unit"] | "raw";
            channelConfigs[i].calibration_offset = channel["calibration_offset"] | 0.0;
            channelConfigs[i].calibration_scale = channel["calibration_scale"] | 1.0;
            
            // Apply attenuation
            int atten = channel["attenuation"] | ADC_11db;
            channelConfigs[i].attenuation = atten;
            channelConfigs[i].SetAttenuation(atten);
            
            DEBUG_PRINTF("Ch%d: %s, %dms, %dbit, Atten=%d, %s, scale=%.5f, offset=%.2f, %s\n",
                channelConfigs[i].id, channelConfigs[i].name.c_str(),
                channelConfigs[i].samplingRate, channelConfigs[i].resolution,
                atten, channelConfigs[i].unit.c_str(),
                channelConfigs[i].calibration_scale, channelConfigs[i].calibration_offset,
                channelConfigs[i].enabled ? "Enabled" : "Disabled");
            i++;
        }
        DEBUG_PRINTLN("------------------------------");
        
        saveConfigToLittleFS();
        request->send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Configuration saved\"}");
        return true;
    }
    return false;
}

// =============================================================================
// WEB SERVER ROUTE SETUP
// =============================================================================

/**
 * @brief Configure all web server routes
 * 
 * Route structure:
 * - /api/* : REST API endpoints (file list, streaming)
 * - /*.html : Static HTML pages
 * - /*.js, /*.css : Static assets
 * - /config.json : Configuration endpoint
 * - /save-parameters : POST endpoint for configuration
 */
void setupWebRoutes() {
    // =========================================================================
    // API ROUTES
    // =========================================================================
    
    /**
     * GET /api/files
     * Returns JSON list of available download files
     * Response: {"channels": [{"id": 1, "name": "Ch1", "filename": "LOG_05.csv"}, ...]}
     */
    server.on("/api/files", HTTP_GET, [](AsyncWebServerRequest *request){
        StaticJsonDocument<JSON_BUFFER_SIZE> doc;
        JsonArray channels = doc.createNestedArray("channels");

        for(int i = 0; i < 4; i++){
            int channelId = i + 1;
            String downloadFilename = getLogFilename(channelId, false);

            if (SD.exists(downloadFilename)) {
                JsonObject channel = channels.createNestedObject();
                channel["id"] = channelId;
                channel["name"] = channelConfigs[i].name;
                channel["filename"] = downloadFilename.substring(1);
            }
        }

        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json);
    });

    /**
     * GET /api/stream?file=LOG_05.csv
     * Streams CSV file to client with chunking
     * Content-Type: text/csv
     */
    server.on("/api/stream", HTTP_GET, [](AsyncWebServerRequest *request){
        if(!request->hasParam("file")){
            request->send(400, "text/plain", "Missing file parameter");
            return;
        }
        
        String filename = request->getParam("file")->value();
        
        // Normalize filename
        if (filename.startsWith("/")) filename = filename.substring(1);
        if (!filename.endsWith(".csv")) filename += ".csv";
        
        String fullPath = "/" + filename;
        DEBUG_PRINTF("API/Stream: %s -> %s\n", filename.c_str(), fullPath.c_str());
        
        if(!SD.exists(fullPath)){
            request->send(404, "text/plain", "File not found: " + fullPath);
            return;
        }
        
        File file = SD.open(fullPath, FILE_READ);
        if(!file){
            request->send(500, "text/plain", "Failed to open file");
            return;
        }
        
        size_t fileSize = file.size();
        DEBUG_PRINTF("Streaming: %s (%d bytes)\n", fullPath.c_str(), fileSize);
        
        // Chunked response with move semantics (C++17)
        AsyncWebServerResponse *response = request->beginChunkedResponse(
            "text/csv",
            [file](uint8_t *buffer, size_t maxLen, size_t index) mutable -> size_t {
                if(!file || !file.available()){
                    if (file) file.close();
                    return 0;
                }
                
                size_t chunkSize = min(maxLen, (size_t)1024);
                size_t bytesRead = file.read(buffer, chunkSize);
                vTaskDelay(1 / portTICK_PERIOD_MS);  // Feed watchdog
                return bytesRead;
            }
        );
        
        response->addHeader("Cache-Control", "no-cache");
        response->addHeader("Access-Control-Allow-Origin", "*");
        request->send(response);
    });


    // API to Set Time
    server.on("/api/set-time", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL, 
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
        StaticJsonDocument<256> doc;
        deserializeJson(doc, data);
        
        if(doc.containsKey("timestamp")) {
            unsigned long ts = doc["timestamp"];
            // Adjust for timezone if needed, normally timestamp is UTC or local sent by browser
            // Assuming browser sends its local time as epoch
            rtc.adjust(DateTime(ts));
            DEBUG_PRINTF("Time updated to: %lu\n", ts);
            request->send(200, "application/json", "{\"status\":\"ok\"}");
        } else {
            request->send(400, "text/plain", "Invalid JSON");
        }
    });

    // =========================================================================
    // STATIC FILE ROUTES
    // =========================================================================
    
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){ 
        handleFileRequest("/data_page.html", "text/html", request); 
    });
    server.on("/data_page.html", HTTP_GET, [](AsyncWebServerRequest *request){ 
        handleFileRequest("/data_page.html", "text/html", request); 
    });
    server.on("/parameters.html", HTTP_GET, [](AsyncWebServerRequest *request){ 
        handleFileRequest("/parameters.html", "text/html", request); 
    });
    server.on("/chart.min.js", HTTP_GET, [](AsyncWebServerRequest *request){ 
        handleFileRequest("/chart.min.js", "application/javascript", request); 
    });

    // =========================================================================
    // CONFIGURATION ROUTES
    // =========================================================================
    
    server.on("/config.json", HTTP_GET, [](AsyncWebServerRequest *request){
        handleFileRequest("/config.json", "application/json", request);
    });
    
    server.on("/save-parameters", HTTP_POST, 
        [](AsyncWebServerRequest *request){ 
            request->send(400, "text/plain", "Bad Request: Expected JSON body."); 
        }, 
        NULL, 
        processJsonConfig
    );
    
    // =========================================================================
    // UTILITY ROUTES
    // =========================================================================
    
    server.on("/clear", HTTP_GET, [](AsyncWebServerRequest *request){ 
        if (request->hasParam("ch")) {
            int id = request->getParam("ch")->value().toInt();
            clearLogFile(id);
            request->send(200, "text/plain", "Clear command sent for channel " + String(id));
        } else {
            request->send(400, "text/plain", "Missing 'ch' parameter.");
        }
    });

    server.on("/list", HTTP_GET, handleFileList);
    server.onNotFound(handleNotFound);
}

// =============================================================================
// WEB REQUEST HANDLERS
// =============================================================================

/**
 * @brief Serve file from LittleFS
 * @param path File path (e.g., "/data_page.html")
 * @param contentType MIME type (e.g., "text/html")
 * @param request HTTP request object
 */
void handleFileRequest(String path, String contentType, AsyncWebServerRequest *request) {
    DEBUG_PRINTLN("Serving file from LittleFS: " + path);
    
    if (LittleFS.exists(path)) {
        request->send(LittleFS, path, contentType);
    } else {
        request->send(404, "text/plain", "404: File Not Found in LittleFS");
    }
}

/**
 * @brief Handle 404 errors and captive portal redirects
 * @param request HTTP request object
 * 
 * Behavior:
 * - Detects OS captive portal probes (Google, Apple, Microsoft)
 * - Redirects to data_page.html
 * - Serves HTML files for valid requests
 * - Returns 404 for invalid requests
 */
void handleNotFound(AsyncWebServerRequest *request) {
    String host = request->host();
    String uri = request->url();

    // Suppress logs and redirect captive portal probes
    if (host.indexOf("google") >= 0 || host.indexOf("apple") >= 0 ||
        host.indexOf("microsoft") >= 0 || host.indexOf("gstatic") >= 0 ||
        host.indexOf("generate_204") >= 0 || host.indexOf("msftconnecttest") >= 0 ||
        host.indexOf("hotspot-detect") >= 0 || host.indexOf("ncsi.txt") >= 0 ||
        host.indexOf("fedoraproject") >= 0) {
        request->redirect(String("http://") + WiFi.softAPIP().toString() + String("/data_page.html"));
        return;
    }

    // Serve HTML pages
    if (uri == "/" || uri.indexOf("data_page.html") == 0 || 
        uri.indexOf("parameters.html") == 0 || uri.indexOf("chart.min.js") == 0) {
        DEBUG_PRINTF("Serving captive portal page: %s\n", uri.c_str());
        request->send(LittleFS, uri == "/" ? "/data_page.html" : uri, "text/html");
        return;
    }

    // Redirect unknown hosts
    if (!host.startsWith(WiFi.softAPIP().toString())) {
        request->redirect(String("http://") + WiFi.softAPIP().toString());
    } else {
        request->send(404, "text/plain", "404: Not Found");
    }
}

/**
 * @brief List all files in LittleFS (debug endpoint)
 */
void handleFileList(AsyncWebServerRequest *request) {
    String response = "<h1>Files on LittleFS</h1>";
    File root = LittleFS.open("/");
    File file = root.openNextFile();
    
    while(file){
        response += "<p>" + String(file.name()) + " - " + String(file.size()) + " bytes</p>";
        file = root.openNextFile();
    }
    
    request->send(200, "text/html", response);
}

// =============================================================================
// ACCESS POINT MANAGEMENT
// =============================================================================

/**
 * @brief Start WiFi Access Point and web server
 * 
 * Actions:
 * 1. Set CPU frequency to 240 MHz (WiFi mode)
 * 2. Activate blue LED indicator
 * 3. Create WiFi AP with captive portal
 * 4. Start DNS server for captive portal
 * 5. Start web server
 * 6. Record start time for timeout
 */
void startAccessPoint() {
    DEBUG_PRINTLN("Starting Access Point...");
    setCpuFrequencyMhz(CPU_FREQ_WIFI);
    
    WiFi.softAP(AP_SSID, AP_PASSWORD);
    IPAddress apIP = WiFi.softAPIP();
    
    apStartTime = millis();
    isApTimeoutEnabled = true;
    
    indicateAPMode();

    dnsServer.start(DNS_PORT, "*", apIP);
    server.begin();
    isApActive = true;
    
    DEBUG_PRINTLN("AP and Captive Portal Started.");
    DEBUG_PRINTF("AP IP address: %s\n", apIP.toString().c_str());
}

/**
 * @brief Stop WiFi Access Point
 * 
 * Actions:
 * 1. Stop web server
 * 2. Disconnect WiFi AP
 * 3. Stop DNS server
 * 4. Set CPU frequency to 80 MHz (logging mode)
 * 5. Restore green LED indicator
 */
void stopAccessPoint() {
    DEBUG_PRINTLN("Stopping Access Point...");
    server.end();
    WiFi.softAPdisconnect(true);
    dnsServer.stop();
    isApActive = false;
    setCpuFrequencyMhz(CPU_FREQ_LOGGING);
    indicateNormal();
    DEBUG_PRINTLN("AP Stopped.");
}

/**
 * @brief Check if AP has timed out (5 minutes)
 * 
 * Called from main loop every cycle
 * On timeout:
 * - Stops AP
 * - Clears all download files
 */
void checkApTimeout() {
    if (!isApTimeoutEnabled) return;

    unsigned long currentTime = millis();
    if (currentTime < apStartTime) {
        apStartTime = currentTime;  // Handle millis() overflow
        return;
    }

    if (currentTime - apStartTime >= AP_TIMEOUT_MS) {
        DEBUG_PRINTLN("AP timeout reached (5 minutes). Closing AP...");
        stopAccessPoint();
        
        // Clear all download files
        for (int ch = 1; ch <= 4; ch++) {
            clearLogFile(ch);
        }
    }
}

/**
 * @brief WiFi event handler for client disconnection
 * @param event WiFi event type
 * @param info Event information
 * 
 * When last client disconnects:
 * - Stops AP
 * - Clears all download files
 */
void onStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info) {
    DEBUG_PRINTF("Station disconnected. MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
        info.wifi_ap_stadisconnected.mac[0], info.wifi_ap_stadisconnected.mac[1],
        info.wifi_ap_stadisconnected.mac[2], info.wifi_ap_stadisconnected.mac[3],
        info.wifi_ap_stadisconnected.mac[4], info.wifi_ap_stadisconnected.mac[5]);

    if (WiFi.softAPgetStationNum() == 0) {
        DEBUG_PRINTLN("No clients connected. Closing AP...");
        stopAccessPoint();
        
        // Clear all download files
        for (int ch = 1; ch <= 4; ch++) {
            clearLogFile(ch);
        }
    }
}

#endif // COMMS_H
