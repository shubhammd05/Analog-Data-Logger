/**
 * @file Functions.h
 * @brief Utility Functions for LED Control, SD Card, and CSV Operations
 * @version 1.0
 * @date 2026-01-11
 */

#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#include <SD.h>
#include <FS.h>
#include "Config.h"

// =============================================================================
// LED INDICATOR FUNCTIONS
// =============================================================================

/**
 * @brief Set RGB LED color
 * @param r Red state (true = ON)
 * @param g Green state (true = ON)
 * @param b Blue state (true = ON)
 */
void setLedColor(bool r, bool g, bool b) {
    digitalWrite(LEDR, r ? HIGH : LOW);
    digitalWrite(LEDG, g ? HIGH : LOW);
    digitalWrite(LEDB, b ? HIGH : LOW);
}

// System state indicators
void indicateInitialization()   { setLedColor(1, 0, 0); }  // Red
void indicateNormal()            { setLedColor(0, 1, 0); }  // Green
void indicateAPMode()            { setLedColor(0, 0, 1); }  // Blue
void indicateSDHalfFull()        { setLedColor(1, 1, 0); }  // Yellow
void indicateSDFull()            { setLedColor(1, 0, 1); }  // Magenta
void indicateError()             { setLedColor(1, 0, 0); }  // Red

// =============================================================================
// SD CARD MANAGEMENT
// =============================================================================

/**
 * @brief Check SD card status and return appropriate indicator
 * @return 0 = OK, 1 = Half full (>50%), 2 = Critical/Full (<10MB free)
 */
int checkSdCardStatus() {
    if (!SD.totalBytes()) return 2;  // SD not ready

    uint64_t total = SD.totalBytes();
    uint64_t used = SD.usedBytes();
    uint64_t free = total - used;

    const uint64_t CRITICAL_THRESHOLD = 10 * 1024 * 1024;  // 10MB

    if (free < CRITICAL_THRESHOLD) {
        indicateSDFull();
        return 2;  // FULL
    } 
    else if (used > (total / 2)) {
        indicateSDHalfFull();
        return 1;  // HALF FULL
    }
    
    return 0;  // OK
}

/**
 * @brief Print SD card storage information to serial
 * @return true if sufficient space available, false otherwise
 */
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

    return (free_bytes >= LOW_SPACE_THRESHOLD_BYTES);
}

// =============================================================================
// CSV FILE OPERATIONS
// =============================================================================

/**
 * @brief Write CSV data row to SD card
 * @param Y Year
 * @param M Month
 * @param D Date
 * @param H Hour (24-hour format)
 * @param Mi Minute
 * @param S Second
 * @param DataValue Raw ADC value
 * @param myFile File reference (not used, kept for compatibility)
 * @param FileName Full path to CSV file
 */
void writeCSV(uint16_t Y, uint8_t M, uint8_t D, uint8_t H, uint8_t Mi, 
              uint8_t S, uint16_t DataValue, File &myFile, String FileName) {
    File file = SD.open(FileName.c_str(), FILE_APPEND);
    
    if (file) {
        // Format: Year,Month,Date,Hour,Minute,Second,Value
        file.printf("%d,%d,%d,%d,%d,%d,%d\n", Y, M, D, H, Mi, S, DataValue);
        file.close();
    } else {
        DEBUG_PRINTLN("Error opening file for writing.");
    }
}

/**
 * @brief Write CSV header to file if empty
 * @param FileName Full path to CSV file
 */
void writeCSVHeader(String FileName) {
    File myFile = SD.open(FileName.c_str(), FILE_WRITE);
    
    if (myFile) {
        if (myFile.size() == 0) {
            myFile.println("Year,Month,Date,Hour,Minute,Second,Value");
        }
        myFile.close();
    }
}

#endif // FUNCTIONS_H
