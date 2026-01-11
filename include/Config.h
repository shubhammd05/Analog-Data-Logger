/**
 * @file Config.h
 * @brief Global Hardware Configuration and Debug Settings
 * @version 1.0
 * @date 2026-01-11
 */

#ifndef CONFIG_H
#define CONFIG_H

// =============================================================================
// HARDWARE PIN DEFINITIONS
// =============================================================================

#define SWITCH          15      // Logging enable/disable switch
#define UPLOAD_BUTTON   27      // Button to start WiFi AP
#define LEDR            RX2     // Red LED (GPIO17)
#define LEDG            TX2     // Green LED (GPIO16)
#define LEDB            4       // Blue LED
#define SD_CS           5       // SD card chip select

// =============================================================================
// SYSTEM CONFIGURATION
// =============================================================================

#define DEBUG               0           // Enable (1) or disable (0) debug output
#define CPU_FREQ_LOGGING    80          // CPU frequency during logging (MHz)
#define CPU_FREQ_WIFI       240         // CPU frequency during WiFi (MHz)

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
