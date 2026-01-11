#ifndef CONFIG_H
#define CONFIG_H

#define SWITCH 15
#define LEDG TX2
#define LEDR RX2
#define LEDB 4
#define UPLOAD_BUTTON 27

#define DEBUG 1

#define CPU_FREQ_LOGGING 80
#define CPU_FREQ_WIFI 240

// --- Debug Macro ---
#if DEBUG 
#define DEBUG_PRINTLN(x)  Serial.println(x)
#define DEBUG_PRINTF(...) Serial.printf(__VA_ARGS__)
#else
#define DEBUG_PRINTLN(x)
#define DEBUG_PRINTF(...) do {} while (0)
#endif

#endif