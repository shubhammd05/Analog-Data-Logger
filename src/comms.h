#include <FS.h>
#include <SD.h> 
#include <LittleFS.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <time.h>


// --- AP Configuration & Constants --- 
const byte DNS_PORT = 53;
const char* AP_SSID = "ESP32-Data-Fetcher";
const char* AP_PASSWORD = "password";
// const int SD_CS_PIN = 5;
const size_t JSON_BUFFER_SIZE = 2043; 
const int TOTAL_LOG_FILE_COUNT = 8;

// AP timeout settings
const int AP_TIMEOUT_MS = 300000; // 5 minutes = 300,000 milliseconds
unsigned long apStartTime = 0;
bool isApTimeoutEnabled = true;

String C1File;
String C2File;
String C3File;
String C4File;





DNSServer dnsServer;
AsyncWebServer server(80); 
bool isApActive = false;

// --- Global File State Variables ---


// --- Configuration Data Structure ---
// struct ChannelConfig {
//     int id;
//     String name;
//     bool enabled;
//     int samplingRate;
//     int resolution;
//     float attenuation;
// };
ADC channelConfigs[] = {C1, C2, C3, C4};
File currentLogFiles[4]; 
// // Initial Configuration
// ChannelConfig channelConfigs[4] = {
//     {1, "Channel 1", true, 1000, 12, 6},
//     {2, "Channel 2", true, 500, 10, 11},
//     {3, "Channel 3", true, 10, 9, 0},
//     {4, "Channel 4", false, 100, 11, 2.5}
// };

// --- Forward Declarations ---
void startAccessPoint();
void stopAccessPoint();

void swapLoggingGroups();
String getLogFilename(int channelId, bool forLogging);
void clearLogFile(int channelId);
void logData(); 

void checkApTimeout();
void onStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info);

bool processJsonConfig(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
void handleFileRequest(String path, String contentType, AsyncWebServerRequest *request);
void handleConfig(AsyncWebServerRequest *request); 
void handleLogRequest(AsyncWebServerRequest *request);
void handleNotFound(AsyncWebServerRequest *request);
void handleFileList(AsyncWebServerRequest *request);
void saveConfigToLittleFS();
void loadConfigFromLittleFS();
void setupWebRoutes();

void setupWebRoutes() {
    // === API ROUTES FIRST (Specific paths) ===
    server.on("/api/files", HTTP_GET, [](AsyncWebServerRequest *request){
        StaticJsonDocument<JSON_BUFFER_SIZE> doc;
        JsonArray channels = doc.createNestedArray("channels");

        // Return only 4 channels that are in download/transmission state
        for(int i = 0; i < 4; i++){
            int channelId = i + 1;
            String downloadFilename = getLogFilename(channelId, false); // false = download group

            // Only include if file exists
            if (SD.exists(downloadFilename)) {
                JsonObject channel = channels.createNestedObject();
                channel["id"] = channelId;
                channel["name"] = channelConfigs[i].name;
                channel["filename"] = downloadFilename.substring(1); // Remove leading slash
            }
        }

        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json);
    });

    // FIXED: /api/stream - Direct file access
    server.on("/api/stream", HTTP_GET, [](AsyncWebServerRequest *request){
        if(!request->hasParam("file")){
            request->send(400, "text/plain", "Missing file parameter");
            return;
        }
        
        String filename = request->getParam("file")->value();
        
        // Remove leading / if present
        if (filename.startsWith("/")) {
            filename = filename.substring(1);
        }
        
        // Add .csv extension if missing
        if (!filename.endsWith(".csv")) {
            filename += ".csv";
        }
        
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
        
        AsyncWebServerResponse *response = request->beginChunkedResponse(
            "text/csv",
            [file = std::move(file)](uint8_t *buffer, size_t maxLen, size_t index) mutable -> size_t {
                if(!file || !file.available()){
                    if (file) file.close();
                    return 0;
                }
                
                size_t chunkSize = min(maxLen, (size_t)1024);
                size_t bytesRead = file.read(buffer, chunkSize);
                vTaskDelay(1 / portTICK_PERIOD_MS);
                return bytesRead;
            }
        );
        
        response->addHeader("Cache-Control", "no-cache");
        response->addHeader("Access-Control-Allow-Origin", "*");
        request->send(response);
    });

    // === STATIC FILES (HTML, JS, CSS) ===
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){ 
        handleFileRequest("/data_page.html", "text/html", request); 
    });
    server.on("/data_page.html", HTTP_GET, [](AsyncWebServerRequest *request){ 
        handleFileRequest("/data_page.html", "text/html", request); 
    });
    server.on("/parameters.html", HTTP_GET, [](AsyncWebServerRequest *request){ 
        handleFileRequest("/parameters.html", "text/html", request); 
    });
    server.on("/tailwind.min.css", HTTP_GET, [](AsyncWebServerRequest *request){ 
        handleFileRequest("/tailwind.min.css", "text/css", request); 
    });
    server.on("/chart.min.js", HTTP_GET, [](AsyncWebServerRequest *request){ 
        handleFileRequest("/chart.min.js", "application/javascript", request); 
    });

    // === CONFIG ===
    server.on("/config.json", HTTP_GET, handleConfig); 
    
    // === SAVE PARAMETERS ===
    server.on("/save-parameters", HTTP_POST, 
        [](AsyncWebServerRequest *request){ 
            request->send(400, "text/plain", "Bad Request: Expected JSON body."); 
        }, 
        NULL, 
        processJsonConfig
    );
    
    // === CLEAR LOGS ===
    server.on("/clear", HTTP_GET, [](AsyncWebServerRequest *request){ 
        if (request->hasParam("ch")) {
            int id = request->getParam("ch")->value().toInt();
            clearLogFile(id);
            request->send(200, "text/plain", "Clear command sent for channel " + String(id));
        } else {
            request->send(400, "text/plain", "Missing 'ch' parameter.");
        }
    });

    // === LIST FILES ===
    server.on("/list", HTTP_GET, handleFileList);

    // === NOT FOUND LAST ===
    server.onNotFound(handleNotFound);
}

void startAccessPoint() {
    DEBUG_PRINTLN("Starting Access Point...");
    
    setCpuFrequencyMhz(CPU_FREQ_WIFI);

    indicateAPMode();

    WiFi.softAP(AP_SSID, AP_PASSWORD);
    IPAddress apIP = WiFi.softAPIP();

    // Record start time
    apStartTime = millis();
    isApTimeoutEnabled = true;

    dnsServer.start(DNS_PORT, "*", apIP);

    
    server.begin();
    isApActive = true;
    DEBUG_PRINTLN("AP and Captive Portal Started.");
    DEBUG_PRINTF("AP IP address: %s\n", apIP.toString().c_str());
}

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

// -----------------------------------------------------------------
// --- File Management Functions ---
// -----------------------------------------------------------------
bool sdCardIsReady = false; 
bool isLogGroupAActive = true; 


String getLogFilename(int channelId, bool forLogging) {
    int index = channelId - 1;
    int base_index = isLogGroupAActive ? 1 : 5;

    if (!forLogging) {
        base_index = isLogGroupAActive ? 5 : 1;
    }

    int file_number = base_index + index;

    String filename = "/LOG_";
    if (file_number < 10) filename += "0";
    filename += String(file_number) + ".csv";  

    DEBUG_PRINTF("getLogFilename(%d, %s) = %s (group %s)\n", 
                 channelId, forLogging ? "logging" : "download", 
                 filename.c_str(), 
                 isLogGroupAActive ? "A" : "B");

    return filename;
}

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
    
    if(isLogGroupAActive){
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
    
    // Write headers if files are empty
    String files[] = {C1File, C2File, C3File, C4File};
    for(int i = 0; i < 4; i++){
        writeCSVHeader(files[i]);
    }
    
    DEBUG_PRINTF("Switched to Group %s\n", isLogGroupAActive ? "A (01-04)" : "B (05-08)");
}

// void logData() {
//     if (!sdCardIsReady) return;

//     struct tm timeinfo;
//     if (!getLocalTime(&timeinfo)) {
//         DEBUG_PRINTLN("Failed to obtain time");
//         return; 
//     }

//     for (int i = 0; i < 4; i++) {
//         int channelId = i + 1;
//         if (channelConfigs[i].enabled && currentLogFiles[i]) {

//             String logLine = "";

//             logLine += String(timeinfo.tm_year + 1900) + ",";
//             logLine += String(timeinfo.tm_mon + 1) + ",";
//             logLine += String(timeinfo.tm_mday) + ",";
//             logLine += String(timeinfo.tm_wday) + ",";
//             logLine += String(timeinfo.tm_hour) + ",";
//             logLine += String(timeinfo.tm_min) + ",";
//             logLine += String(timeinfo.tm_sec) + ",";
            
//             logLine += String(channelConfigs[i].samplingRate) + ",";
//             logLine += String(channelConfigs[i].resolution) + ",";
//             logLine += String(channelConfigs[i].attenuation) + ",";
            
//             logLine += String(analogRead(34 + i)); 
            
//             currentLogFiles[i].println(logLine);
//             currentLogFiles[i].flush(); 
//         }
//     }
// }

void checkApTimeout() {
    if (!isApTimeoutEnabled) return;
    
    unsigned long currentTime = millis();
    
    if (currentTime < apStartTime) {
        apStartTime = currentTime;
        return;
    }
    
    if (currentTime - apStartTime >= AP_TIMEOUT_MS) {
        DEBUG_PRINTLN("AP timeout reached (5 minutes). Closing AP...");
        stopAccessPoint();
        
        // Clear all 4 download files using existing function
        for (int ch=1; ch<=4; ch++)
        clearLogFile(ch);
    }
}


void onStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info) {
    DEBUG_PRINTF("Station disconnected. MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
        info.wifi_ap_stadisconnected.mac[0],
        info.wifi_ap_stadisconnected.mac[1],
        info.wifi_ap_stadisconnected.mac[2],
        info.wifi_ap_stadisconnected.mac[3],
        info.wifi_ap_stadisconnected.mac[4],
        info.wifi_ap_stadisconnected.mac[5]);
    
    // Check if any clients are connected
    if (WiFi.softAPgetStationNum() == 0) {
        DEBUG_PRINTLN("No clients connected. Closing AP...");
        stopAccessPoint();
        
        // Clear all 4 download files using existing function
        for (int ch = 1; ch<=4; ch++)
        clearLogFile(ch);
    }
}


void clearLogFile(int channelId) {
    if (!sdCardIsReady) {
        DEBUG_PRINTLN("ERROR: Cannot clear log file, SD card not ready.");
        return;
    }

    if (channelId < 1 || channelId > 4) {
        DEBUG_PRINTF("ERROR: Invalid channel ID %d passed to clearLogFile.\n", channelId);
        return;
    }

    DEBUG_PRINTLN("--- CLEARING LOG FILE ---");

    // Get the filename for the group that is NOT being logged to (the download group)
    String filenameToClear = getLogFilename(channelId, false);

    DEBUG_PRINTF("Attempting to clear log file: %s\n", filenameToClear.c_str());

    if (SD.exists(filenameToClear)) {
        if (SD.remove(filenameToClear)) {
            DEBUG_PRINTF("Successfully cleared log file: %s\n", filenameToClear.c_str());
        } else {
            DEBUG_PRINTF("ERROR: Failed to clear log file: %s\n", filenameToClear.c_str());
        }
    } else {
        DEBUG_PRINTF("INFO: Log file %s does not exist, no action needed.\n", filenameToClear.c_str());
    }

    DEBUG_PRINTLN("--- Log file cleared successfully. ---");
}



// -----------------------------------------------------------------
// --- JSON Handler ---
// -----------------------------------------------------------------

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
                channelConfigs[i].id,
                channelConfigs[i].name.c_str(),
                channelConfigs[i].samplingRate,
                channelConfigs[i].resolution,
                atten,
                channelConfigs[i].unit.c_str(),
                channelConfigs[i].calibration_scale,
                channelConfigs[i].calibration_offset,
                channelConfigs[i].enabled ? "Enabled" : "Disabled"
            );
            i++;
        }
        DEBUG_PRINTLN("------------------------------");
        
        saveConfigToLittleFS();
        request->send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Configuration saved\"}");
        return true;
    }
    return false;
}

// -----------------------------------------------------------------
// --- Web Handlers ---
// -----------------------------------------------------------------

void handleConfig(AsyncWebServerRequest *request) {
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

    String jsonResponse;
    serializeJson(doc, jsonResponse);
    request->send(200, "application/json", jsonResponse);
}

void handleLogRequest(AsyncWebServerRequest *request) {
    if (!sdCardIsReady) {
        request->send(503, "text/plain", "SD Card not initialized.");
        return;
    }
    
    int channelId = 0;

    if (request->hasParam("ch")) {
        String paramValue = request->getParam("ch")->value();
        channelId = paramValue.toInt();
        DEBUG_PRINTF("INFO: Parsing channel ID from string '%s', result: %d\n", paramValue.c_str(), channelId);
    } else {
        DEBUG_PRINTLN("ERROR: Request missing 'ch' parameter.");
        request->send(400, "text/plain", "Missing channel ID ('ch') parameter in request.");
        return;
    }

    if (channelId < 1 || channelId > 4) {
        String responseBody = "Invalid channel ID requested. Must be 1-4. Received: " + String(channelId);
        DEBUG_PRINTLN(responseBody);
        request->send(400, "text/plain", responseBody);
        return;
    }

    String readyDownloadFilename = getLogFilename(channelId, false); // false == download group 
    
    DEBUG_PRINTF("Serving channel %d from: %s\n", channelId, readyDownloadFilename.c_str());

    if (!SD.exists(readyDownloadFilename)) {
        DEBUG_PRINTF("File not found for download: %s\n", readyDownloadFilename.c_str());
        request->send(404, "text/plain", "Log file is not ready for download or does not exist.");
        return;
    }

    File file = SD.open(readyDownloadFilename, FILE_READ);
    if (!file) {
        request->send(500, "text/plain", "Failed to open log file");
        return;
    }

    // Chunked response for large files
    AsyncWebServerResponse *response = request->beginChunkedResponse("text/csv", [&file](uint8_t *buffer, size_t maxLen, size_t index) mutable -> size_t {
        if (!file.available()) {
            if (file) {
                file.close();
            }
            return 0;  // End of stream
        }

        // Read in smaller chunks to prevent watchdog timeout
        size_t chunkSize = min(maxLen, (size_t)1024);
        size_t bytesRead = file.read(buffer, chunkSize);
        
        // Feed watchdog
        vTaskDelay(1 / portTICK_PERIOD_MS);
        
        return bytesRead;
    });
    response->addHeader("Cache-Control", "no-cache");
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Content-Disposition", "attachment; filename=\"" + readyDownloadFilename.substring(1) + "\"");

    request->send(response);
    DEBUG_PRINTF("Successfully served log for Channel %d from file: %s\n", channelId, readyDownloadFilename.c_str());
}

void handleFileRequest(String path, String contentType, AsyncWebServerRequest *request) {
    DEBUG_PRINTLN("Serving file from LittleFS: " + path);
    if (LittleFS.exists(path)) {
        request->send(LittleFS, path, contentType, false, NULL); 
    } 
    else if (LittleFS.exists(path)) {
        request->send(LittleFS, path, contentType);
    } else { 
        request->send(404, "text/plain", "404: File Not Found in LittleFS");
    }
}

void handleNotFound(AsyncWebServerRequest *request) {
    String host = request->host();
    String uri = request->url();

    // Suppress logs for all portal probes/hostnames
    if (host.indexOf("google") >= 0 || 
        host.indexOf("apple") >= 0 || 
        host.indexOf("microsoft") >= 0 || 
        host.indexOf("gstatic") >= 0 ||
        host.indexOf("generate_204") >= 0 ||
        host.indexOf("msftconnecttest") >= 0 ||
        host.indexOf("hotspot-detect") >= 0 ||
        host.indexOf("ncsi.txt") >= 0 ||
        host.indexOf("fedoraproject") >= 0
    ) {
        request->redirect(String("http://") + WiFi.softAPIP().toString() + String("/data_page.html")); // No Content
        return;
    }

    // Only log if ACTUAL main page or web resource is requested (not a redirect/probe)
    // Example: user requests root "/", data_page.html, parameters.html, tailwind.min.css, chart.min.js
    if (
        uri == "/" ||
        uri.indexOf("data_page.html") == 0 ||
        uri.indexOf("parameters.html") == 0 ||
        uri.indexOf("tailwind.min.css") == 0 ||
        uri.indexOf("chart.min.js") == 0
    ) {
        DEBUG_PRINTF("Serving captive portal page: %s\n", uri.c_str());
        // Optionally: you can serve the file here if not already handled by route
        request->send(LittleFS, uri == "/" ? "/data_page.html" : uri, "text/html");
        return;
    }

    // For all other requests: do NOT log anything; just silently redirect or send 204/404
    if (!host.startsWith(WiFi.softAPIP().toString())) {
        // No log here!
        request->redirect(String("http://") + WiFi.softAPIP().toString());
    } else {
        request->send(405, "text/plain", "404: Not Found");
    }
}



void handleFileList(AsyncWebServerRequest *request) {
    String response = "<h1>Files on LittleFS</h1><ul>";
    File root = LittleFS.open("/");
    File file = root.openNextFile();
    while(file){
        response += "<li>" + String(file.name()) + " - " + String(file.size()) + " bytes</li>";
        file = root.openNextFile();
    }
    response += "</ul>";
    request->send(200, "text/html", response);
}

// void saveConfigToSD() {
//     if (!sdCardIsReady) {
//         DEBUG_PRINTLN("ERROR: Cannot save config to SD card, SD card not ready.");
//         return;
//     }

//     DEBUG_PRINTLN("Saving config to SD card...");
//     // Create JSON document
//     StaticJsonDocument<JSON_BUFFER_SIZE> doc;
//     JsonArray channels = doc.createNestedArray("channels");

//     for (int i = 0; i < 4; i++) {
//         JsonObject channel = channels.createNestedObject();
//         channel["id"] = channelConfigs[i].id;
//         channel["name"] = channelConfigs[i].name;
//         channel["enabled"] = channelConfigs[i].enabled;
//         channel["samplingRate"] = channelConfigs[i].samplingRate;
//         channel["resolution"] = channelConfigs[i].resolution;
//         channel["attenuation"] = channelConfigs[i].attenuation;
//     }

//     // Write to SD card
//     File file = SD.open("/config.json", FILE_WRITE);
//     if (!file) {
//         DEBUG_PRINTLN("ERROR: Failed to create config file on SD card!");
//         return;
//     }
    
//     serializeJson(doc, file);
//     file.close();
    
//     DEBUG_PRINTLN("Configuration saved successfully to SD card.");

// }

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
    
    // Add session info
    JsonObject session = doc.createNestedObject("session_info");
    session["device_id"] = "ESP32-Logger-001";
    session["firmware_version"] = "1.0.0";
    
    File file = LittleFS.open("/config.json", FILE_WRITE);
    if (!file) {
        DEBUG_PRINTLN("ERROR: Failed to create config file!");
        return;
    }
    
    serializeJsonPretty(doc, file);
    file.close();
    
    DEBUG_PRINTLN("Configuration saved successfully to LittleFS.");
}


// Load configuration from SD card
// void loadConfigFromSD() {
//     if (!sdCardIsReady) {
//         DEBUG_PRINTLN("SD card not ready. Using default configuration.");
//         return;
//     }
    
//     if (!SD.exists("/config.json")) {
//         DEBUG_PRINTLN("No saved config found on SD card. Using defaults.");
//         return;
//     }
    
//     DEBUG_PRINTLN("Loading configuration from SD card...");
    
//     File file = SD.open("/config.json", FILE_READ);
//     if (!file) {
//         DEBUG_PRINTLN("ERROR: Failed to open config file from SD card!");
//         return;
//     }
    
//     StaticJsonDocument<JSON_BUFFER_SIZE> doc;
//     DeserializationError error = deserializeJson(doc, file);
//     file.close();
    
//     if (error) {
//         DEBUG_PRINTF("ERROR: Failed to parse config JSON! Error: %s\n", error.c_str());
//         return;
//     }
    
//     JsonArray channels = doc["channels"];
//     if (channels.isNull()) {
//         DEBUG_PRINTLN("ERROR: Invalid config format - missing 'channels' array!");
//         return;
//     }
    
//     int i = 0;
//     for (JsonObject channel : channels) {
//         if (i >= 4) break;  // Safety check
        
//         channelConfigs[i].id = channel["id"] | channelConfigs[i].id;
//         channelConfigs[i].name = channel["name"] | channelConfigs[i].name;
//         channelConfigs[i].enabled = channel["enabled"] | channelConfigs[i].enabled;
//         channelConfigs[i].samplingRate = channel["samplingRate"] | channelConfigs[i].samplingRate;
//         channelConfigs[i].resolution = channel["resolution"] | channelConfigs[i].resolution;
//         channelConfigs[i].attenuation = channel["attenuation"] | channelConfigs[i].attenuation;
        
//         DEBUG_PRINTF("Loaded Ch%d: %s, %dHz, %d-bit, %.1fdB, %s\n", 
//             channelConfigs[i].id, 
//             channelConfigs[i].name.c_str(),
//             channelConfigs[i].samplingRate,
//             channelConfigs[i].resolution,
//             channelConfigs[i].attenuation,
//             channelConfigs[i].enabled ? "Enabled" : "Disabled");
//         i++;
//     }
    
//     DEBUG_PRINTLN("Configuration loaded successfully from SD card.");
// }


void loadConfigFromLittleFS() {
    if (!LittleFS.begin(true)) {
        DEBUG_PRINTLN("LittleFS not ready. Using defaults.");
        return;
    }
    
    if (!LittleFS.exists("/config.json")) {
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
        
        // Load and apply attenuation
        int atten = channel["attenuation"] | ADC_11db;
        channelConfigs[i].attenuation = atten;
        channelConfigs[i].SetAttenuation(atten);
        
        DEBUG_PRINTF("Loaded Ch%d: %s, %dms, %s, scale=%.5f\n",
            channelConfigs[i].id,
            channelConfigs[i].name.c_str(),
            channelConfigs[i].samplingRate,
            channelConfigs[i].unit.c_str(),
            channelConfigs[i].calibration_scale
        );
        i++;
    }
    
    DEBUG_PRINTLN("Configuration loaded successfully.");
}
