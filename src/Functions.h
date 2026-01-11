#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#include <SD.h>
#include <FS.h>
#include "Config.h"

void setLedColor(bool r, bool g, bool b) {
    digitalWrite(LEDR, r ? HIGH : LOW);
    digitalWrite(LEDG, g ? HIGH : LOW);
    digitalWrite(LEDB, b ? HIGH : LOW);
}

// --- Indicators ---
void indicateInitialization() { setLedColor(1, 0, 0); } // Red
void indicateNormal()         { setLedColor(0, 1, 0); } // Green
void indicateAPMode()         { setLedColor(0, 0, 1); } // Blue
void indicateSDHalfFull()     { setLedColor(1, 1, 0); } // Yellow
void indicateSDFull()         { setLedColor(1, 0, 1); } // Magenta
void indicateError()          { setLedColor(1, 0, 0); } // Red

// --- Dynamic Space Check ---
// Returns: 0=OK, 1=HalfFull, 2=Full
int checkSdCardStatus() {
    // 1. If SD not ready, return Error
    if (!SD.totalBytes()) return 2;

    uint64_t total = SD.totalBytes();
    uint64_t used = SD.usedBytes();
    uint64_t free = total - used;
    
    // Dynamic Calculation:
    // 1. Critical if free space < 10MB (Safe buffer for closing files)
    const uint64_t CRITICAL_THRESHOLD = 10 * 1024 * 1024; 
    
    if (free < CRITICAL_THRESHOLD) {
        indicateSDFull(); // Magenta
        return 2; // FULL
    } 
    // 2. Warning if Used > 50% of Total
    else if (used > (total / 2)) {
        indicateSDHalfFull(); // Yellow
        return 1; // HALF FULL
    } 
    
    // 3. Normal
    // Only set Green if we are NOT in AP mode (to avoid overwriting Blue)
    // This check is usually done in the main loop or setup
    return 0; // OK
}

void SwitchOn(int red, int green){
    digitalWrite(green, HIGH);
    digitalWrite(red, LOW);
}

void SwitchOff(int red, int green){
    digitalWrite(red, HIGH);
    digitalWrite(green, LOW);
}

// CSV Format: Year,Month,Date,Hour,Minute,Second,Millisecond,Value
void writeCSV(uint16_t Y, uint8_t M, uint8_t D, uint8_t H, uint8_t Mi, uint8_t S, uint16_t Ms, uint16_t DataValue, File &myFile, String FileName) {
    myFile = SD.open(FileName.c_str(), FILE_APPEND);
    
    if (myFile) {
        // myFile.print(Y);
        // myFile.print(",");
        // myFile.print(M);
        // myFile.print(",");
        // myFile.print(D);
        // myFile.print(",");
        // myFile.print(H);
        // myFile.print(",");
        // myFile.print(Mi);
        // myFile.print(",");
        // myFile.print(S);
        // myFile.print(",");
        // myFile.print(Ms);
        // myFile.print(",");
        // myFile.println(DataValue);
        
        myFile.printf("%d,%d,%d,%d,%d,%d,%d,%d\n", Y, M, D, H, Mi, S, Ms, DataValue);
        myFile.close();

    } else {
        DEBUG_PRINTLN("Error opening file for writing.");
    }
}

// Write CSV header
void writeCSVHeader(String FileName) {
    File myFile = SD.open(FileName.c_str(), FILE_WRITE);
    
    if (myFile) {
        if (myFile.size() == 0) {
            myFile.println("Year,Month,Date,Hour,Minute,Second,Millisecond,Value");
        }
        myFile.close();
    }
}

bool checkSdCardSpace() {
    uint64_t total_bytes = SD.totalBytes();
    uint64_t used_bytes = SD.usedBytes();
    uint64_t free_bytes = total_bytes - used_bytes;
    
    DEBUG_PRINTLN("\n--- SD Card Storage Information ---");
    DEBUG_PRINTF("Total Size: %.2f GB\n", (double)total_bytes / (1024 * 1024 * 1024));
    DEBUG_PRINTF("Used Space: %.2f MB\n", (double)used_bytes / (1024 * 1024));
    DEBUG_PRINTF("Free Space: %.2f MB\n", (double)free_bytes / (1024 * 1024));
    
    const uint64_t LOW_SPACE_THRESHOLD_MB = 10;
    const uint64_t LOW_SPACE_THRESHOLD_BYTES = LOW_SPACE_THRESHOLD_MB * 1024 * 1024;
    
    if (free_bytes < LOW_SPACE_THRESHOLD_BYTES) {
        return false;
    }
    return true;
}

#endif