```markdown
# [Analog-Data-Logger](https://github.com/siddharth11010/Analog-Data-Logger)

[![GitHub](https://img.shields.io/badge/GitHub-Analog--Data--Logger-blue?logo=github)](https://github.com/siddharth11010/Analog-Data-Logger)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
[![PlatformIO](https://img.shields.io/badge/PlatformIO-ESP32-orange.svg)](https://platformio.org)
[![Version](https://img.shields.io/badge/Version-2.0-blue.svg)](https://github.com/siddharth11010/Analog-Data-Logger/releases)

> **ESP32 4-Channel Data Logger** - 

---

# ESP32 4-Channel Data Logger
## Comprehensive Documentation Guide

**Version:** 2.0  
**Date:** January 11, 2026  
**Status:** Production Ready  
**Firmware Version:** 1.0.0

---

## 📋 TABLE OF CONTENTS

1. [Executive Summary](#1-executive-summary)
2. [Hardware Overview](#2-hardware-overview)
3. [Circuit Design & Pin Mapping](#3-circuit-design--pin-mapping)
4. [Software Architecture](#4-software-architecture)
5. [Installation & Setup Guide](#5-installation--setup-guide)
6. [Configuration System](#6-configuration-system)
7. [Calibration Guide](#7-calibration-guide)
8. [Data Format & Storage](#8-data-format--storage)
9. [WiFi Interface & Web Dashboard](#9-wifi-interface--web-dashboard)
10. [API Reference](#10-api-reference)
11. [LED Status Indicators](#11-led-status-indicators)
12. [Developer Guide](#12-developer-guide)
13. [Troubleshooting](#13-troubleshooting)
14. [Technical Specifications](#14-technical-specifications)

---

## 1. EXECUTIVE SUMMARY

### 1.1 Project Overview

The **ESP32 4-Channel Data Logger** is a professional-grade data acquisition system designed for embedded applications requiring simultaneous multi-channel analog signal capture with millisecond-precision timestamping and non-volatile storage.

### 1.2 Key Features

- ✅ **4 Independent ADC Channels** with configurable sampling rates (1ms to 1 hour)
- ✅ **Real-Time Timestamping** via DS3231 RTC with battery backup
- ✅ **Dual-Buffer File System** for continuous logging with background data download
- ✅ **WiFi Web Interface** for wireless configuration and data retrieval
- ✅ **Automatic Calibration** to convert raw ADC values to engineering units
- ✅ **RGB LED Status Indicators** for visual system feedback
- ✅ **FreeRTOS Multitasking** for parallel channel acquisition
- ✅ **Robust SD Card Management** with mutex protection and space monitoring

### 1.3 Applications

✓ Battery Management Systems (BMS) monitoring  
✓ Environmental sensor data logging (temperature, humidity, pressure)  
✓ Industrial IoT and predictive maintenance  
✓ Prototype validation and testing  
✓ Research data collection  
✓ Renewable energy monitoring

---

## 2. HARDWARE OVERVIEW

### 2.1 Microcontroller Specifications

| Parameter | Value |
|-----------|-------|
| Model | ESP32 (NodeMCU-32S or DevKit V1) |
| CPU Architecture | Dual-core Xtensa LX6 |
| Clock Speed | 240 MHz (WiFi), 80 MHz (Logging) |
| RAM | 520 KB SRAM |
| Flash Storage | 4 MB |
| ADC Channels | 2x 12-bit SAR (18 pins total) |
| Resolution | 9-12 bits (configurable per channel) |

### 2.2 Peripheral Modules

| Component | Model | Interface | Function |
|-----------|-------|-----------|----------|
| Real-Time Clock | DS3231 | I2C | Time synchronization, battery backup |
| Storage | MicroSD Module | SPI | Data logging (up to 32GB) |
| Status Indicator | RGB LED (Common Cathode) | GPIO | System status visualization |
| Input Controls | Tactile Buttons | GPIO | Upload trigger, logging enable |

### 2.3 Power Requirements

| Parameter | Value |
|-----------|-------|
| Input Voltage | 5V (USB or VIN pin) |
| Idle Current | ~80 mA |
| Logging Current | ~120 mA |
| WiFi Active Current | ~180-240 mA |
| Recommended Supply | 5V/500mA minimum |

### 2.4 Operating Conditions

| Parameter | Range |
|-----------|-------|
| Temperature | -10°C to +60°C |
| Humidity | 10% to 90% (non-condensing) |
| Storage Capacity | Up to 32 GB SD card |
| RTC Accuracy | ±2 ppm (±1 minute/year) |
| Max File Size | 4 GB (FAT32 limit) |

---

## 3. CIRCUIT DESIGN & PIN MAPPING

### 3.1 Complete Pin Assignment Table

| Function | ESP32 Pin | Component | Notes |
|----------|-----------|-----------|-------|
| **ADC Channel 1** | GPIO34 | Sensor 1 Input | ADC1_CH6, input only |
| **ADC Channel 2** | GPIO35 | Sensor 2 Input | ADC1_CH7, input only |
| **ADC Channel 3** | GPIO32 | Sensor 3 Input | ADC1_CH4, touch capable |
| **ADC Channel 4** | GPIO33 | Sensor 4 Input | ADC1_CH5, touch capable |
| **Upload Button** | GPIO27 | Button (to GND) | Starts WiFi AP mode |
| **Logging Switch** | GPIO15 | Toggle Switch | Enable/disable logging (active LOW) |
| **Red LED** | GPIO16/17 | RGB LED Red | 330Ω resistor |
| **Green LED** | GPIO16/17 | RGB LED Green | 330Ω resistor |
| **Blue LED** | GPIO4 | RGB LED Blue | 330Ω resistor |
| **SD CS** | GPIO5 | SD Card | SPI slave select |
| **SD MOSI** | GPIO23 | SD Card | SPI data output |
| **SD MISO** | GPIO19 | SD Card | SPI data input |
| **SD SCK** | GPIO18 | SD Card | SPI clock |
| **RTC SDA** | GPIO21 | DS3231 | I2C data (4.7kΩ pullup) |
| **RTC SCL** | GPIO22 | DS3231 | I2C clock (4.7kΩ pullup) |
| **Power** | VIN | 5V Supply | USB or external |
| **Ground** | GND | Common GND | Multiple points |

> **Note:** ADC1 channels (GPIO32-39) are used exclusively because ADC2 conflicts with WiFi operation.

### 3.2 System Block Diagram

```
                            ESP32 NodeMCU-32S
                      ┌─────────────────────────────┐
                      │                             │
      Sensor 1 ───────┤ GPIO34 (ADC1_CH6)           │
      Sensor 2 ───────┤ GPIO35 (ADC1_CH7)           │
      Sensor 3 ───────┤ GPIO32 (ADC1_CH4)           │
      Sensor 4 ───────┤ GPIO33 (ADC1_CH5)           │
                      │                             │
                  ┌───┤ GPIO27   Upload Button      ├───┐
      Button ─────┤   │                             │   ├─── 3.3V (Pullup)
                  └───┤ GND                         │   │
                      │                             │   │
                  ┌───┤ GPIO15   Logging Switch     ├───┘
      Switch ─────┤   │                             │
                  └───┤ GND                         │
                      │                             │
    ┌───[330Ω]────────┤ GPIO16/17   Red LED         │
    │   ┌───[330Ω]────┤ GPIO16/17   Green LED       │
    │   │   ┌───[330Ω]┤ GPIO4       Blue LED        │
    └───┴───┴─────────┤ GND                         │
                      │                             │
                      │      SD CARD MODULE         │
                      ├─────────────────────────────┤
                      │ GPIO5  → CS (SS)            │
                      │ GPIO23 → MOSI (DI)          │
                      │ GPIO19 → MISO (DO)          │
                      │ GPIO18 → SCK (CLK)          │
                      │ 3.3V   → VCC                │
                      │ GND    → GND                │
                      │                             │
                      │     DS3231 RTC MODULE       │
                      ├─────────────────────────────┤
                      │ GPIO21 → SDA (4.7kΩ pullup) │
                      │ GPIO22 → SCL (4.7kΩ pullup) │
                      │ 3.3V   → VCC                │
                      │ GND    → GND (+ CR2032)     │
                      │                             │
      5V Supply ──────┤ VIN                         │
      GND ────────────┤ GND                         │
                      └─────────────────────────────┘
```

### 3.3 Voltage Divider Circuit for High Voltage Inputs

For inputs exceeding 3.3V (e.g., 12V battery monitoring), use a voltage divider:

```
12V Input ─────[27kΩ]─────┬──────[10kΩ]───── GND
                          │
                        GPIO34 (3.3V max)

Calculation:
V_ADC = V_IN × (R2 / (R1 + R2))
V_ADC = 12V × (10k / 37k) = 3.24V ✓ Safe for ESP32

Calibration Factor:
Scale = 12V / 4095 counts = 0.002929 V/count
Offset = 0.0
```

**Common Voltage Divider Ratios:**

| Input Voltage | R1 (kΩ) | R2 (kΩ) | ADC Max Voltage | Scale Factor |
|---------------|---------|---------|-----------------|--------------|
| 5V | 10 | 10 | 2.5V | 0.001221 V/count |
| 12V | 27 | 10 | 3.24V | 0.002929 V/count |
| 24V | 56 | 10 | 3.18V | 0.005861 V/count |

---

## 4. SOFTWARE ARCHITECTURE

### 4.1 Dual-Core Task Distribution

```
┌─────────────────────────────────────────────────┐
│               Core 1: Real-Time                 │
│  ┌───────────────────────────────────────────┐  │
│  │   Logging Tasks (Ch1-4)                   │  │
│  │   -  Acquire Mutex                        │  │
│  │   -  Read ADC Sensors                     │  │
│  │   -  Write to SD Card                     │  │
│  │   -  Release Mutex                        │  │
│  └───────────────────────────────────────────┘  │
└─────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────┐
│               Core 0: Background                │
│  ┌───────────────────────────────────────────┐  │
│  │   System Monitor Task                     │  │
│  │   -  Check SD Space (every 5s)            │  │
│  │   -  Update LED Status (every 200ms)      │  │
│  └───────────────────────────────────────────┘  │
│  ┌───────────────────────────────────────────┐  │
│  │   WiFi & Web Server                       │  │
│  │   -  Handle HTTP Requests                 │  │
│  │   -  Stream Files from SD                 │  │
│  │   -  DNS Captive Portal                   │  │
│  └───────────────────────────────────────────┘  │
└─────────────────────────────────────────────────┘

                ┌──────────────┐
                │ Mutex Lock   │
                │ (SD Access)  │
                └──────────────┘
```

### 4.2 FreeRTOS Task Configuration

| Task | Core | Priority | Stack | Interval | Function |
|------|------|----------|-------|----------|----------|
| Channel1Task | 1 | 2 | 8 KB | Configurable | Read GPIO34 ADC |
| Channel2Task | 1 | 2 | 8 KB | Configurable | Read GPIO35 ADC |
| Channel3Task | 1 | 2 | 8 KB | Configurable | Read GPIO32 ADC |
| Channel4Task | 1 | 2 | 8 KB | Configurable | Read GPIO33 ADC |
| SystemMonitor | 0 | 1 | 4 KB | 200 ms | SD status, LED control |
| WiFi/HTTP | 0 | 1 | 16 KB | Event-driven | AP, DNS, HTTP server |

### 4.3 Dual-Buffer File System

The system uses a clever file rotation scheme to enable continuous logging while downloading data:

```
Initial State:
  Group A (LOG_01.csv to LOG_04.csv) ← ACTIVE LOGGING
  Group B (LOG_05.csv to LOG_08.csv) ← AVAILABLE FOR DOWNLOAD

User presses Upload Button:
  1. Close files in Group A
  2. Switch to Group B for logging
  3. Group A becomes available for download
  4. Downloaded files are cleared when WiFi disconnects

Next cycle:
  Group B (LOG_05.csv to LOG_08.csv) ← ACTIVE LOGGING
  Group A (LOG_01.csv to LOG_04.csv) ← AVAILABLE FOR DOWNLOAD
```

This design eliminates file locks and allows uninterrupted data acquisition.

---

## 5. INSTALLATION & SETUP GUIDE

### 5.1 Prerequisites

**Development Environment:**
- VS Code with PlatformIO extension
- Python 3.7+ (for PlatformIO)
- Git (optional)

**Hardware:**
- ESP32 DevKit V1 or NodeMCU-32S
- USB cable (Type-B or Micro-USB)
- Working USB port

**Libraries (auto-installed by PlatformIO):**
- Arduino-ESP32 core
- RTClib (Adafruit)
- ArduinoJson 6.x
- AsyncTCP
- ESPAsyncWebServer

### 5.2 Step-by-Step Installation

#### Step 1: Create PlatformIO Project

```bash
# Create new project
platformio init --board esp32dev --framework arduino

# Or open existing project
cd esp32-data-logger
```

#### Step 2: Configure platformio.ini

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
upload_speed = 921600
monitor_speed = 115200

lib_deps =
    RTClib
    ArduinoJson@^6.19.0
    me-no-dev/AsyncTCP
    me-no-dev/ESP Async WebServer

build_flags =
    -DCORE_DEBUG_LEVEL=0
    -DDEBUG=1
```

#### Step 3: Project Structure

```
project/
├── src/
│   └── main.cpp
├── include/
│   ├── Config.h
│   ├── ADC_config.h
│   ├── Functions.h
│   └── comms.h
└── data/
    ├── config.json
    ├── data_page.html
    ├── parameters.html
    └── chart.min.js
```

#### Step 4: Upload Filesystem

```bash
# Upload LittleFS files
platformio run --target uploadfs

# Verify upload
platformio device monitor
```

#### Step 5: Compile and Upload Firmware

```bash
# Build project
platformio run

# Upload to device
platformio run --target upload

# Monitor serial output
platformio device monitor --baud 115200
```

#### Step 6: Verify Boot Sequence

Expected serial output:

```
[INFO] Starting ESP32 Data Logger v2.0
[INFO] LittleFS Initialized. (328 KB used)
[INFO] RTC initialized. Time: 2026-01-11 15:45:30
[INFO] SD Card initialized. (7.8 GB free)
[INFO] All channels configured and ready.
[INFO] Logging enabled. Green LED on.
```

### 5.3 Initial Configuration

1. Press **Upload Button** (GPIO27) for 3 seconds
2. Connect to `ESP32-Data-Fetcher` WiFi (password: `password`)
3. Open browser → `http://192.168.4.1`
4. Go to **Parameters** tab
5. Configure channels
6. Click **Save Config**
7. Wait for disconnection (LED turns green)

---

## 6. CONFIGURATION SYSTEM

### 6.1 Configuration File Structure (config.json)

```json
{
  "channels": [
    {
      "id": 1,
      "name": "Battery_Voltage",
      "enabled": true,
      "samplingRate": 1000,
      "resolution": 12,
      "attenuation": 3,
      "unit": "V",
      "calibration_scale": 0.002929,
      "calibration_offset": 0.0
    },
    {
      "id": 2,
      "name": "Current_Draw",
      "enabled": true,
      "samplingRate": 500,
      "resolution": 12,
      "attenuation": 3,
      "unit": "A",
      "calibration_scale": 0.00435,
      "calibration_offset": -3100
    },
    {
      "id": 3,
      "name": "Temperature",
      "enabled": false,
      "samplingRate": 5000,
      "resolution": 12,
      "attenuation": 0,
      "unit": "°C",
      "calibration_scale": 0.02686,
      "calibration_offset": 0.0
    },
    {
      "id": 4,
      "name": "Pressure",
      "enabled": true,
      "samplingRate": 2000,
      "resolution": 12,
      "attenuation": 3,
      "unit": "mbar",
      "calibration_scale": 1.0,
      "calibration_offset": 0.0
    }
  ],
  "session_info": {
    "device_id": "ESP32-Logger-001",
    "firmware_version": "1.0"
  }
}
```

### 6.2 Parameter Reference

| Parameter | Type | Range | Description |
|-----------|------|-------|-------------|
| `id` | int | 1-4 | Channel identifier |
| `name` | string | 1-50 chars | Display name (no spaces) |
| `enabled` | bool | true/false | Enable data acquisition |
| `samplingRate` | int | 1-3600000 ms | Time between samples |
| `resolution` | int | 9, 10, 11, 12 bits | ADC bit depth |
| `attenuation` | int | 0-3 | Voltage range setting |
| `unit` | string | Any text | Measurement unit |
| `calibration_scale` | float | Any value | Multiplier for raw value |
| `calibration_offset` | float | Any value | Add this after scaling |

### 6.3 ADC Attenuation Settings

| Value | Constant | Voltage Range | Use Case |
|-------|----------|---------------|----------|
| 0 | ADC_0db | 0-1.1V | Precision low voltage sensors |
| 1 | ADC_2_5db | 0-1.5V | Battery monitoring |
| 2 | ADC_6db | 0-2.2V | Temperature sensors |
| 3 | ADC_11db | 0-3.3V | General purpose (recommended) |

### 6.4 Debug Mode Configuration

**Firmware Debug (Config.h):**
```cpp
#define DEBUG 1  // Set to 0 for production
```
- **Effect:** Enables/Disables serial output
- **Usage:** Set to 1 during development, 0 for deployment

**Web Interface Debug (data_page.html):**
```javascript
const DEBUG = false;  // Set to true to show log box
```
- **Effect:** Shows/Hides system log in browser
- **Usage:** Set to true for troubleshooting API calls

---

## 7. CALIBRATION GUIDE

### 7.1 Calibration Formula

```
Calibrated_Value = (Raw_ADC × Scale) + Offset
```

**Where:**
- **Raw_ADC:** 0-4095 (12-bit) or 0-511 (9-bit)
- **Scale:** Conversion factor (volts/amps/°C per count)
- **Offset:** Zero-point or bias correction

### 7.2 Battery Voltage Monitoring (0-12V)

**Circuit:**
```
12V ───[27kΩ]───+───[10kΩ]─── GND
                │
              GPIO34
```

**Calculation:**
```
Voltage divider ratio = R2 / (R1 + R2) = 10k / (27k + 10k) = 0.27027

At 12V input: 
V_ADC = 12V × 0.27027 = 3.24V

ADC count at 3.24V:
Count = (3.24V / 3.3V) × 4095 = 4020 counts

Scale = 12V / 4020 counts = 0.002985 V/count
Offset = 0.0
```

**Configuration:**
```json
{
  "calibration_scale": 0.002985,
  "calibration_offset": 0.0,
  "unit": "V",
  "attenuation": 3
}
```

### 7.3 Current Sensor (ACS712 ±5A)

**Datasheet Specifications:**
- Sensitivity: 185 mV/A
- Zero current output: 2.5V (Vcc/2)
- Bidirectional: ±5A range

**Calculation:**
```
At ±5A range:
-5A → 2.5V - (5 × 0.185V) = 1.575V → ~1950 counts
 0A → 2.5V               = 2.5V  → ~3100 counts (zero offset)
+5A → 2.5V + (5 × 0.185V) = 3.425V → ~4250 counts

Scale = 5A / ((4250 - 1950) / 2) = 0.00435 A/count
Offset = -3100 (subtract zero point)
```

**Configuration:**
```json
{
  "calibration_scale": 0.00435,
  "calibration_offset": -3100,
  "unit": "A",
  "attenuation": 3
}
```

### 7.4 Temperature Sensor (LM35)

**Datasheet Specifications:**
- Output: 10 mV/°C
- 0°C → 0V
- 100°C → 1V

**Calculation:**
```
Using ADC_0db attenuation (0-1.1V range):
100°C = 1.0V → (1.0V / 1.1V) × 4095 = 3723 counts

Scale = 100°C / 3723 counts = 0.02686 °C/count
Offset = 0.0
```

**Configuration:**
```json
{
  "calibration_scale": 0.02686,
  "calibration_offset": 0.0,
  "unit": "°C",
  "attenuation": 0
}
```

### 7.5 Two-Point Calibration Method

For custom or unknown sensors:

**Step 1:** Measure at known point 1
- Apply known input (e.g., 0V reference)
- Record ADC reading: ADC₁

**Step 2:** Measure at known point 2
- Apply different known input (e.g., 3.3V)
- Record ADC reading: ADC₂

**Step 3:** Calculate scale
```
Scale = (Input₂ - Input₁) / (ADC₂ - ADC₁)
```

**Step 4:** Calculate offset
```
Offset = Input₁ - (ADC₁ × Scale)
```

**Example:**
```
Point 1: 0V input → ADC reads 50 counts (due to noise/bias)
Point 2: 3.3V input → ADC reads 4090 counts

Scale = (3.3 - 0) / (4090 - 50) = 0.000817 V/count
Offset = 0 - (50 × 0.000817) = -0.0409 V
```

---

## 8. DATA FORMAT & STORAGE

### 8.1 CSV File Structure

**Header Row:**
```csv
Year,Month,Date,Hour,Minute,Second,Millisecond,Value
```

**Example Data:**
```csv
2026,1,11,15,30,45,123,2048
2026,1,11,15,30,46,125,2051
2026,1,11,15,30,47,127,2049
```

### 8.2 Column Descriptions

| Column | Type | Range | Unit | Example |
|--------|------|-------|------|---------|
| Year | uint16 | 2000-2099 | Year | 2026 |
| Month | uint8 | 1-12 | Month | 1 (January) |
| Date | uint8 | 1-31 | Day | 11 |
| Hour | uint8 | 0-23 | Hour (24h) | 15 (3 PM) |
| Minute | uint8 | 0-59 | Minute | 30 |
| Second | uint8 | 0-59 | Second | 45 |
| Millisecond | uint16 | 0-999 | ms | 123 |
| Value | uint16 | 0-4095 | Raw ADC | 2048 |

> **Note:** Year range limited by DS3231 RTC hardware

### 8.3 File System Layout

**SD Card:**
```
/
├── LOG_01.csv    ┐
├── LOG_02.csv    ├─ Group A (Active Logging or Download)
├── LOG_03.csv    │
├── LOG_04.csv    ┘
├── LOG_05.csv    ┐
├── LOG_06.csv    ├─ Group B (Active Logging or Download)
├── LOG_07.csv    │
└── LOG_08.csv    ┘
```

**LittleFS (Internal Flash):**
```
/
├── config.json          (Configuration - persistent)
├── data_page.html       (Main web interface - 14 KB)
├── parameters.html      (Settings page - 17 KB)
└── chart.min.js         (Chart library - 280 KB)

Total: ~328 KB / 1 MB available
```

### 8.4 File Size Estimation

```
Formula: Size = (Samples/Second) × (Seconds) × 60 bytes/sample

Examples:
- 1 Hz × 3600 seconds (1 hour)   = 216 KB
- 10 Hz × 3600 seconds (1 hour)  = 2.16 MB
- 100 Hz × 3600 seconds (1 hour) = 21.6 MB
- 1 kHz × 60 seconds (1 minute)  = 3.6 MB

SD Card Capacity:
- 1 GB card  → ~17 million samples
- 4 GB card  → ~68 million samples
- 8 GB card  → ~140 million samples
- 32 GB card → ~550 million samples
```

### 8.5 Data Processing Example (Python)

```python
import pandas as pd

# Load CSV file
df = pd.read_csv('LOG_01.csv')

# Create timestamp column
df['Timestamp'] = pd.to_datetime(
    df[['Year','Month','Date','Hour','Minute','Second']]
)

# Apply calibration (example: battery voltage)
scale = 0.002929
offset = 0.0
df['Voltage'] = (df['Value'] * scale) + offset

# Display results
print(df[['Timestamp', 'Value', 'Voltage']])

# Plot data
import matplotlib.pyplot as plt
plt.plot(df['Timestamp'], df['Voltage'])
plt.xlabel('Time')
plt.ylabel('Voltage (V)')
plt.title('Battery Voltage over Time')
plt.show()
```

---

## 9. WIFI INTERFACE & WEB DASHBOARD

### 9.1 Accessing the System

**Trigger WiFi Mode:**
1. Press **Upload Button** (GPIO27) for 3 seconds
2. **Blue LED** turns on
3. WiFi AP broadcasts: `ESP32-Data-Fetcher`

**Connect Device:**

**Mobile Phone (iOS/Android):**
1. Settings → WiFi
2. Select `ESP32-Data-Fetcher`
3. Enter password: `password`
4. Captive portal opens automatically

**Laptop/Desktop:**
1. WiFi Networks → `ESP32-Data-Fetcher`
2. Password: `password`
3. Open browser → `http://192.168.4.1`

### 9.2 Web Interface Pages

#### Data Page (/)

**Purpose:** Download and visualize logged data

**Features:**
- List of available log files
- Real-time CSV preview
- Interactive Chart.js visualization
- Calibrated value display with units
- Download raw CSV files

**URL:** `http://192.168.4.1/data_page.html`

#### Parameters Page (/parameters.html)

**Purpose:** Configure channels and sampling

**Features:**
- Enable/disable individual channels
- Set sampling rates
- Adjust calibration factors
- Select voltage range (attenuation)
- Set measurement units
- Save configuration to persistent storage

**URL:** `http://192.168.4.1/parameters.html`

### 9.3 WiFi Security & Timeout

**Current Security:**
- SSID: `ESP32-Data-Fetcher`
- Password: `password`
- ⚠️ **Warning:** Change default password for production use

**Auto-Timeout:**
- AP stays active for 5 minutes maximum
- Automatically closes after timeout
- Also closes when last client disconnects
- Downloaded files cleared on disconnect
- Green LED resumes when AP closes

---

## 10. API REFERENCE

### 10.1 GET /api/files

**Description:** Returns list of available log files

**Response:**
```json
{
  "channels": [
    {
      "id": 1,
      "name": "Battery_Voltage",
      "filename": "LOG_05.csv"
    },
    {
      "id": 2,
      "name": "Current_Draw",
      "filename": "LOG_06.csv"
    }
  ]
}
```

### 10.2 GET /api/stream?file=LOG_05.csv

**Description:** Stream CSV file with chunked transfer

**Parameters:**
- `file`: Filename (e.g., `LOG_05.csv`)

**Example:** `http://192.168.4.1/api/stream?file=LOG_05.csv`

**Response:** CSV file content (chunked transfer)

### 10.3 GET /config.json

**Description:** Returns current channel configuration

**Response:** JSON object (see Configuration System section)

### 10.4 POST /save-parameters

**Description:** Save new channel configuration

**Request Body:** JSON (same format as config.json)

**Response:**
```json
{
  "status": "ok",
  "message": "Configuration saved"
}
```

### 10.5 GET /clear?ch=1

**Description:** Clear log file for specified channel

**Parameters:**
- `ch`: Channel number (1-4)

**Example:** `http://192.168.4.1/clear?ch=1`

**Response:** `Clear command sent for channel 1`

### 10.6 Example API Requests (curl)

```bash
# Get available files
curl http://192.168.4.1/api/files

# Stream log file
curl http://192.168.4.1/api/stream?file=LOG_05.csv > data.csv

# Get current configuration
curl http://192.168.4.1/config.json

# Save configuration
curl -X POST http://192.168.4.1/save-parameters \
  -H "Content-Type: application/json" \
  -d @new_config.json
```

---

## 11. LED STATUS INDICATORS

### 11.1 LED Color Codes

| Color | Status |
|-------|--------|
| **🟢 Green** | Logging active, SD OK |
| **🔴 Red** | Logging paused (switch OFF) |
| **🔵 Blue** | WiFi AP Mode active |
| **🟡 Yellow** | Low SD space (>50% full) |
| **🟣 Magenta** (blink) | SD Full (<10MB) or error |

### 11.2 LED Priority Logic

The system monitor updates LED status every 200ms with the following priority (highest first):

1. **SD Full/Error** (Magenta Flickering) - Critical
2. **WiFi Active** (Blue) - High
3. **SD Low** (Yellow) - Medium
4. **Normal/Paused** (Green/Red) - Low

**Example Scenarios:**

- **Startup:** Red → System initializing
- **Ready:** Green → Logging active, SD OK
- **Low Space:** Yellow → SD >50% full
- **WiFi Mode:** Blue → AP active, logging continues
- **Critical:** Magenta (flickering) → SD full, logging stopped

---

## 12. DEVELOPER GUIDE

### 12.1 Core Data Structures

```cpp
// Channel Configuration Structure
typedef struct {
    uint8_t id;                      // Channel number (1-4)
    String name;                     // Display name
    bool enabled;                    // Enable/disable channel
    unsigned long samplingRate;      // Sampling interval in ms
    adc_bits_width_t resolution;     // 9, 10, 11, or 12 bits
    adc_attenuation_t attenuation;   // Voltage range
    String unit;                     // Engineering unit
    float calibration_scale;         // Multiplier
    float calibration_offset;        // Offset
} ChannelConfig;
```

### 12.2 Global Variables

```cpp
SemaphoreHandle_t mutex;       // SD card access mutex
bool sdCardIsReady = false;    // SD initialization status
bool isSdFull = false;         // SD space critical flag
bool isApActive = false;       // WiFi AP mode active
ChannelConfig channels;     // Configuration for all 4 channels
```

### 12.3 Key Functions

**SD Card Status Check:**
```cpp
int checkSdCardStatus() {
    if (!SD.totalBytes()) return 2;  // Error
    
    uint64_t total = SD.totalBytes();
    uint64_t used = SD.usedBytes();
    uint64_t free = total - used;
    
    if (free < 10*1024*1024) return 2;  // Critical (<10MB)
    if (used > (total/2)) return 1;      // Low (>50%)
    return 0;                            // OK
}
```

**LED Control:**
```cpp
void setLedColor(bool r, bool g, bool b) {
    digitalWrite(LEDR, r ? HIGH : LOW);
    digitalWrite(LEDG, g ? HIGH : LOW);
    digitalWrite(LEDB, b ? HIGH : LOW);
}
```

**Logging Task:**
```cpp
void LogChannelTask(void* parameter) {
    int chId = (int)parameter;
    ChannelConfig &cfg = channels[chId - 1];
    
    for (;;) {
        if (cfg.enabled && !isSdFull && !digitalRead(SWITCH)) {
            uint16_t raw = analogRead(getAdcPin(chId));
            DateTime now = rtc.now();
            String line = formatCsvLine(now, raw);
            
            if (xSemaphoreTake(mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
                File file = SD.open(getLogFile(chId), FILE_APPEND);
                if (file) {
                    file.println(line);
                    file.close();
                }
                xSemaphoreGive(mutex);
            }
        }
        vTaskDelay(cfg.samplingRate / portTICK_PERIOD_MS);
    }
}
```

### 12.4 Modifying Sampling Rates

To change sampling rates programmatically:

```cpp
// In setup() or via web interface
channels.samplingRate = 500;   // 500ms = 2 samples/second
channels.samplingRate = 1000;  // 1 second[2]
channels.samplingRate = 5000;  // 5 seconds[3]
channels.samplingRate = 100;   // 100ms = 10 samples/second[4]

// Save to config.json
saveConfigToLittleFS();
```

### 12.5 Adding New Sensors

**Step 1:** Determine sensor specifications
- Output voltage range
- Sensitivity/scale factor
- Zero-point offset

**Step 2:** Design voltage divider (if needed)
- Ensure ADC input < 3.3V
- Calculate resistor values

**Step 3:** Calculate calibration factors
```cpp
float scale = (MaxValue - MinValue) / (MaxADC - MinADC);
float offset = MinValue - (MinADC * scale);
```

**Step 4:** Update configuration
```json
{
  "id": 1,
  "name": "NewSensor",
  "enabled": true,
  "samplingRate": 1000,
  "resolution": 12,
  "attenuation": 3,
  "unit": "units",
  "calibration_scale": 0.001,
  "calibration_offset": 0.0
}
```

---

## 13. TROUBLESHOOTING

### 13.1 Boot Issues

#### Problem: Red LED stays on, no serial output

**Cause:** LittleFS not initialized or SD card missing

**Solutions:**
1. Check USB cable and power supply
2. Re-upload filesystem: `platformio run --target uploadfs`
3. Ensure SD card inserted and formatted as FAT32
4. Try different USB port
5. Check serial baud rate (115200)

#### Problem: "Couldn't find RTC"

**Cause:** DS3231 not detected on I2C bus

**Solutions:**
1. Verify I2C connections (SDA→GPIO21, SCL→GPIO22)
2. Check 4.7kΩ pullup resistors on SDA/SCL
3. Test RTC battery (CR2032)
4. Run I2C scanner sketch
5. Try different RTC module

#### Problem: "Card Mount Failed!"

**Cause:** SD card module not responding

**Solutions:**
1. Check SPI wiring (CS, MOSI, MISO, SCK)
2. Verify 3.3V power to SD module
3. Format SD card as FAT32
4. Try different SD card brand
5. Reduce SPI speed: `SD.begin(SD_CS, SPI, 4000000)`

### 13.2 WiFi Issues

#### Problem: WiFi AP not appearing

**Cause:** Button not registering or WiFi disabled

**Solutions:**
1. Hold upload button for 3+ seconds
2. Check serial for "Starting Access Point..."
3. Verify GPIO27 button wiring
4. Test button continuity to GND
5. Restart ESP32

#### Problem: Captive portal doesn't open

**Cause:** OS security or browser not recognizing portal

**Solutions:**
1. Disable mobile data
2. Manually browse to `http://192.168.4.1`
3. Try different device
4. Use incognito/private browsing
5. Forget network and reconnect

#### Problem: Can't save configuration

**Cause:** LittleFS not mounted or corrupted

**Solutions:**
1. Check serial: "LittleFS Initialized"
2. Re-upload filesystem
3. Verify `/data/` folder contents
4. Check free space on flash
5. Format LittleFS if needed

### 13.3 Data Logging Issues

#### Problem: No data in CSV files

**Cause:** Logging disabled or ADC not reading

**Solutions:**
1. Check logging switch (GPIO15) - must be LOW
2. Verify channel enabled in config.json
3. Check ADC input voltage < 3.3V
4. Confirm sampling rate > 0
5. Monitor serial for write errors

#### Problem: CSV files corrupt or too large

**Cause:** Sampling rate too fast or SD corruption

**Solutions:**
1. Reduce sampling rate (increase interval)
2. Reformat SD card (FAT32)
3. Verify adequate power (500mA minimum)
4. Check operating temperature
5. Use high-quality SD card

#### Problem: Timestamp incorrect

**Cause:** RTC not set or battery dead

**Solutions:**
1. Check RTC battery installed (CR2032)
2. Set RTC time via web interface
3. Verify I2C communication
4. Replace battery
5. Check DS3231 crystal oscillator

### 13.4 Performance Issues

#### Problem: System crashes or resets

**Cause:** Insufficient power or memory

**Solutions:**
1. Use 5V/1A power supply
2. Reduce sampling rates
3. Increase task stack sizes
4. Disable unused channels
5. Check for memory leaks

#### Problem: Slow web interface

**Cause:** Large files or weak WiFi

**Solutions:**
1. Clear old log files
2. Reduce file group size
3. Move closer to ESP32
4. Increase CPU frequency (240 MHz)
5. Use chunked transfer

---

## 14. TECHNICAL SPECIFICATIONS

### 14.1 Electrical Specifications

| Parameter | Value |
|-----------|-------|
| Operating Voltage | 3.3V (internal) / 5V (input) |
| ADC Resolution | 12-bit max (0-4095 counts) |
| ADC Voltage Range | 0-3.3V (full range) |
| ADC Reference | Internal 1.1V - 3.3V (attenuation) |
| I2C Clock Speed | 100 kHz (standard) / 400 kHz (fast) |
| SPI Clock Speed | 10 MHz (SD card) |
| GPIO Drive Current | 40 mA max per pin |
| Total Current Draw | 80-240 mA (mode dependent) |

### 14.2 Memory Usage

| Component | Size |
|-----------|------|
| Flash (Program) | ~800 KB |
| Flash (LittleFS) | ~1 MB |
| SRAM (Runtime) | ~200-300 KB |
| SRAM (Tasks) | ~40 KB |
| Free SRAM | ~200 KB |

### 14.3 File Format Standards

**CSV:**
- Line Endings: LF (Unix)
- Delimiter: Comma (,)
- Encoding: UTF-8
- No quotes on numeric values

**JSON:**
- Format: Pretty-printed
- Encoding: UTF-8
- Max Size: 2048 bytes

### 14.4 Recommended Components

| Component | Supplier | Notes |
|-----------|----------|-------|
| ESP32 NodeMCU | AliExpress, Amazon | Verify ESP32 (not ESP8266) |
| DS3231 RTC | Amazon, AliExpress | Includes CR2032 battery |
| MicroSD Module | Amazon | 3.3V or level shifter |
| MicroSD Card | SanDisk, Samsung | Class 10, FAT32 |
| RGB LED | Local electronics | Common cathode, 5mm |
| Resistors (330Ω) | Local electronics | 1/4W carbon film |
| Buttons | Local electronics | Tactile push button |

---

## 📚 APPENDIX

### A. Quick Reference Card

**Pin Connections:**
- ADC: GPIO34, 35, 32, 33
- Control: GPIO27 (button), GPIO15 (switch)
- LED: GPIO16, 17, 4
- SD: GPIO5 (CS), 23 (MOSI), 19 (MISO), 18 (SCK)
- RTC: GPIO21 (SDA), 22 (SCL)

**WiFi Credentials:**
- SSID: ESP32-Data-Fetcher
- Password: password
- IP: http://192.168.4.1

**LED Colors:**
- Green = Normal
- Red = Paused
- Blue = WiFi
- Yellow = Low space
- Magenta = Full/Error

### B. Calibration Cheat Sheet

**Voltage Divider:**
```
Scale = V_max / ADC_at_Vmax
```

**Current Sensor:**
```
Scale = Current_range / (ADC_max - ADC_min)
Offset = -ADC_at_zero
```

**Temperature:**
```
Scale = Temp_range / ADC_range
```

### C. Common Issues & Quick Fixes

| Issue | Quick Fix |
|-------|-----------|
| Red LED | Check SD card inserted |
| No WiFi | Hold button 3+ seconds |
| No data | Check switch position (LOW) |
| Wrong time | Replace RTC battery |
| Corrupt files | Reformat SD card (FAT32) |

---

## 🤝 Contributing

Contributions are welcome! Please follow these steps:

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/AmazingFeature`)
3. Commit your changes (`git commit -m 'Add some AmazingFeature'`)
4. Push to the branch (`git push origin feature/AmazingFeature`)
5. Open a Pull Request

---

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

---

## 🎯 QUICK START CHECKLIST

- [ ] Hardware assembled per circuit diagram
- [ ] PlatformIO installed and configured
- [ ] Filesystem uploaded (`uploadfs`)
- [ ] Firmware uploaded (`upload`)
- [ ] SD card inserted (FAT32)
- [ ] RTC battery installed
- [ ] Initial configuration via WiFi
- [ ] Test logging with switch
- [ ] Verify data files on SD
- [ ] Calibrate channels for sensors

**Ready to log! 🚀**

---

<div align="center">

**Document Version:** 2.0  
**Last Updated:** January 11, 2026  
**Maintained By:** [siddharth11010](https://github.com/siddharth11010)

[![⭐ Star this repo](https://img.shields.io/github/stars/siddharth11010/Analog-Data-Logger?style=social)](https://github.com/siddharth11010/Analog-Data-Logger)
[![🍴 Fork this repo](https://img.shields.io/github/forks/siddharth11010/Analog-Data-Logger?style=social)](https://github.com/siddharth11010/Analog-Data-Logger/fork)

*Built with ❤️ for the embedded systems community*

</div>

---
