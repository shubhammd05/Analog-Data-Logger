/**
 * @file Config.h
 * @brief Global Hardware Configuration and Debug Settings
 * @version 1.2
 * @date 2026-01-26
 */

#ifndef CONFIG_H
#define CONFIG_H

// =============================================================================
// HARDWARE PIN DEFINITIONS
// =============================================================================

#define SWITCH          15      // Logging enable/disable switch
#define UPLOAD_BUTTON   27      // Button to start WiFi AP
#define LEDR            16      // Red LED (GPIO16 / RX2)
#define LEDG            17      // Green LED (GPIO17 / TX2)
#define LEDB            4       // Blue LED
#define SD_CS           5       // SD card chip select

// =============================================================================
// SYSTEM CONFIGURATION
// =============================================================================

#define DEBUG               1           // Enable (1) or disable (0) debug output
#define CPU_FREQ_LOGGING    80          // CPU frequency during logging (MHz)
#define CPU_FREQ_WIFI       240         // CPU frequency during WiFi (MHz)

// Buffering & Power
#define RESERVE_MEMORY      512
#define QUEUE_SIZE          50          // Number of samples per channel queue
#define BURST_WRITE_COUNT   10          // Samples to accumulate before writing to SD
// Note: BURST_WRITE_COUNT should always <= QUEUE_SIZE, for best results, 50 %
// Also you can even set it 1 (not lower than that), but we  will lose the point of including buffer
// Incase of power loss, we will loose the samples in the buffer that are not yet written to SD
#define LIGHT_SLEEP_EN      1           // Enable automatic light sleep

// =============================================================================
// DEBUG MACROS
// =============================================================================

#if DEBUG
    #define DEBUG_PRINTLN(x)        Serial.println(x)
    #define DEBUG_PRINTF(...)       Serial.printf(__VA_ARGS__)
#else
    #define DEBUG_PRINTLN(x)        
    #define DEBUG_PRINTF(...)       do {} while (0)
#endif

#endif // CONFIG_H