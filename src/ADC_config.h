#ifndef ADC_CONFIG_H
#define ADC_CONFIG_H

#include <Arduino.h>

class ADC{
public:
    int pin;
    int id;
    String name;
    bool enabled;
    int resolution;
    int samplingRate;
    String unit;
    float calibration_offset;
    float calibration_scale;
    int attenuation;
    
    ADC(int p, int w, String ChannelName, int ChannelID, bool status){
        pin = p;
        name = ChannelName;
        id = ChannelID;
        enabled = status;
        unit = "raw";
        calibration_offset = 0.0;
        calibration_scale = 1.0;
        attenuation = ADC_11db;  // Default to 11dB (0-3.3V)
        
        pinMode(pin, INPUT);
        
        if(w >= 9 && w <= 12){
            resolution = w;
        } else {
            resolution = 12;
        }
        
        // Set attenuation for this pin
        analogSetPinAttenuation(pin, (adc_attenuation_t)attenuation);
    }
    
    void SetResolution(int r){
        if(r >= 9 && r <= 12){
            resolution = r;
        } else {
            resolution = 12;
        }
    }
    
    void SetAttenuation(int atten){
        attenuation = atten;
        analogSetPinAttenuation(pin, (adc_attenuation_t)attenuation);
    }
    
    uint16_t Read(){
        analogSetWidth(resolution);
        return analogRead(pin);
    }
    
    // Apply calibration to raw value
    float GetCalibratedValue(){
        uint16_t raw = Read();
        return (raw * calibration_scale) + calibration_offset;
    }
};

// Initialize 4 channels with GPIO pins
ADC C1(34, 12, "Channel_1", 1, true);
ADC C2(35, 12, "Channel_2", 2, true);
ADC C3(32, 12, "Channel_3", 3, false);
ADC C4(33, 12, "Channel_4", 4, true);

#endif