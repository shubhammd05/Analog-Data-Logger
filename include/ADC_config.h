/**
 * @file ADC_config.h
 * @brief ADC Channel Configuration and Management
 * @version 1.0
 * @date 2026-01-11
 * 
 * Manages 4 independent ADC channels with configurable parameters:
 * - Resolution (9-12 bits)
 * - Attenuation (voltage range)
 * - Calibration (scale & offset)
 * - Sampling rate
 */

#ifndef ADC_CONFIG_H
#define ADC_CONFIG_H

#include <Arduino.h>

// =============================================================================
// ADC CHANNEL CLASS
// =============================================================================

/**
 * @class ADC
 * @brief Represents a single ADC input channel with calibration
 */
class ADC {
public:
    // Configuration parameters
    int pin;                      // GPIO pin number
    int id;                       // Channel ID (1-4)
    String name;                  // User-defined channel name
    bool enabled;                 // Enable/disable logging
    int resolution;               // ADC resolution (9-12 bits)
    int samplingRate;             // Sampling interval in milliseconds
    String unit;                  // Measurement unit (V, A, °C, etc.)
    float calibration_offset;     // Calibration offset (added to result)
    float calibration_scale;      // Calibration scale (multiplied with raw)
    int attenuation;              // ADC attenuation setting

    /**
     * @brief Constructor for ADC channel
     * @param p GPIO pin number
     * @param w ADC resolution (9-12 bits)
     * @param ChannelName Display name for channel
     * @param ChannelID Unique channel identifier (1-4)
     * @param status Initial enable/disable state
     */
    ADC(int p, int w, String ChannelName, int ChannelID, bool status) {
        pin = p;
        name = ChannelName;
        id = ChannelID;
        enabled = status;
        unit = "raw";
        calibration_offset = 0.0;
        calibration_scale = 1.0;
        attenuation = ADC_11db;  // Default: 0-3.3V range

        pinMode(pin, INPUT);
        SetResolution(w);
        analogSetPinAttenuation(pin, (adc_attenuation_t)attenuation);
    }

    /**
     * @brief Set ADC resolution
     * @param r Resolution in bits (9-12)
     */
    void SetResolution(int r) {
        resolution = (r >= 9 && r <= 12) ? r : 12;
    }

    /**
     * @brief Set ADC attenuation (voltage range)
     * @param atten Attenuation value (ADC_0db, ADC_2_5db, ADC_6db, ADC_11db)
     */
    void SetAttenuation(int atten) {
        attenuation = atten;
        analogSetPinAttenuation(pin, (adc_attenuation_t)attenuation);
    }

    /**
     * @brief Read raw ADC value
     * @return Raw ADC reading (0-4095 for 12-bit)
     */
    uint16_t Read() {
        analogSetWidth(resolution);
        return analogRead(pin);
    }

    /**
     * @brief Get calibrated measurement value
     * @return Calibrated value = (raw × scale) + offset
     */
    float GetCalibratedValue() {
        uint16_t raw = Read();
        return (raw * calibration_scale) + calibration_offset;
    }
};

// =============================================================================
// CHANNEL INSTANCES
// =============================================================================

// Initialize 4 ADC channels with default GPIO mappings
ADC C1(34, 12, "Channel_1", 1, true);   // GPIO34, 12-bit, enabled
ADC C2(35, 12, "Channel_2", 2, true);   // GPIO35, 12-bit, enabled
ADC C3(32, 12, "Channel_3", 3, false);  // GPIO32, 12-bit, disabled
ADC C4(33, 12, "Channel_4", 4, true);   // GPIO33, 12-bit, enabled

#endif // ADC_CONFIG_H
