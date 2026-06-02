# 🌡️ Heat Stress Monitoring System

> **ATmega8-based real-time environmental safety monitor for outdoor workers**  


---

## 📋 Table of Contents

- [Overview](#-overview)
- [Features](#-features)
- [Hardware](#-hardware)
- [Repository Structure](#-repository-structure)
- [Getting Started](#-getting-started)
- [Threshold Configuration](#-threshold-configuration)
- [UART Output Format](#-uart-output-format)
- [Demo](#-demo)



---

## 🔍 Overview

Outdoor workers in Oman and the GCC face life-threatening heat exposure during summer months. This system continuously monitors **temperature**, **relative humidity**, and **air quality (CO₂ index)** and provides immediate visual alerts when danger thresholds are crossed.

```
┌─────────────────┐    ┌──────────────┐    ┌──────────────────┐    ┌──────────────────┐
│  Analog         │    │  Signal      │    │  ATmega8 MCU     │    │  Outputs         │
│  Front-End      │───▶│  Conditioning│───▶│  ADC → Calibrate │───▶│  LCD / LED / UART│
│  LM35 HIH MQ135 │    │  Op-Amp / VD │    │  → Threshold     │    │                  │
└─────────────────┘    └──────────────┘    └──────────────────┘    └──────────────────┘
```

---

## ✨ Features

| Feature | Details |
|---|---|
| **Temperature sensing** | LM35 — 10 mV/°C, range 0–150 °C |
| **Humidity sensing** | HIH-5030 — ratiometric 0–5 V = 0–100% RH |
| **Air quality sensing** | MQ-135 — CO₂ / mixed gas, 0–1000 ppm scaled |
| **Three-level alert** | 🟢 SAFE → 🟡 WARNING → 🔴 DANGER |
| **16×2 LCD display** | Real-time T / H / Gas + status, 4-bit mode |
| **UART logging** | 9600 bps formatted data stream to PC |
| **Low cost** | < 15 OMR total BOM |
| **Bonus criteria met** | Multiple sensor types + actuators + LCD |

---

## 🔧 Hardware

### Bill of Materials

| Ref | Component | Value / Part | Role |
|---|---|---|---|
| U1 | ATmega8-16PU | 16 MHz DIP | Main MCU |
| U3 | LM358N | DIP-8 | Op-amp buffer (×2 channels) |
| U2 | LM35DZ | TO-92 | Temperature sensor |
| — | HIH-5030 | SIP-4 | Humidity sensor |
| — | MQ-135 | Module | Air quality sensor |
| LCD1 | LM016L | 16×2 HD44780 | Display |
| D1 | Red LED | 5 mm | DANGER indicator |
| D2 | Yellow LED | 5 mm | WARNING indicator |
| D3 | Green LED | 5 mm | SAFE indicator |
| X1 | Crystal | 16 MHz | Clock source |
| C1, C2 | Capacitor | 22 pF | Crystal load caps |
| R1–R3 | Resistor | 1 kΩ | LED current limiting |
| RV1 | Potentiometer | 10 kΩ | LCD contrast |
| RV2 | Voltage divider | 10k+1kΩ | MQ-135 attenuation |

### Pin Assignment

```
ATmega8 Pin     Direction   Function
──────────────────────────────────────────────
PC0 / ADC0      INPUT       LM35 temperature
PC1 / ADC1      INPUT       HIH-5030 humidity
PC2 / ADC2      INPUT       MQ-135 air quality
PB0             OUTPUT      LCD RS
PB1             OUTPUT      LCD EN
PB2–PB5         OUTPUT      LCD D4–D7
PD0             OUTPUT      Green LED (SAFE)
PD1 / TXD       OUTPUT      UART TX → PC
PD5             OUTPUT      Yellow LED (WARNING)
PD6             OUTPUT      Red LED (DANGER)
```

---

## 📁 Repository Structure

```
heat-stress-monitoring-system/
│
├── src/
│   └── main.c                  # ATmega8 firmware (AVR-GCC)
│
├── simulation/
│   └── heat_stress_proteus.pdsprj   # Proteus 8 simulation file
│
├── hardware/
│   ├── schematic.png           # Circuit schematic screenshot
│   └── breadboard_photo.jpg    # Implemented system photo
│
├── docs/
│   └── Heat_Stress_Report_ECCE4227_FINAL.docx  # Full project report
│
├── scripts/
│   └── uart_monitor.py         # Python PC-side serial logger
│
├── .gitignore
├── LICENSE
└── README.md
```

---

## 🚀 Getting Started

### Prerequisites

```bash
# AVR toolchain (Ubuntu/Debian)
sudo apt install gcc-avr avr-libc avrdude binutils-avr

# Python serial monitor (optional)
pip install pyserial
```

### Build & Flash

```bash
# 1. Clone the repo
git clone https://github.com/MohAsaad18/heat-stress-monitoring-system.git
cd heat-stress-monitoring-system

# 2. Compile
avr-gcc -mmcu=atmega8 -DF_CPU=16000000UL -O1 -o main.elf src/main.c
avr-objcopy -O ihex main.elf main.hex

# 3. Flash via USBasp or Arduino-as-ISP
avrdude -c usbasp -p m8 -U flash:w:main.hex:i

# 4. Monitor UART output (9600 bps)
python3 scripts/uart_monitor.py --port /dev/ttyUSB0
```

### Simulation (Proteus)

1. Open `simulation/heat_stress_proteus.pdsprj` in **Proteus 8**
2. Load `main.hex` into the ATmega8 component
3. Press **Play** — adjust RV1/RV2 to test all three alert states

---

## ⚙️ Threshold Configuration

All thresholds are defined as `#define` macros at the top of [`src/main.c`](src/main.c):

```c
#define TEMP_WARNING     30.0   // °C
#define TEMP_DANGER      35.0   // °C
#define HUMIDITY_WARNING 60.0   // %
#define HUMIDITY_DANGER  75.0   // %
#define GAS_WARNING      400.0  // ppm
#define GAS_DANGER       600.0  // ppm
```

Change these values and recompile — no wiring changes needed.

| State | LED | LCD | Condition |
|---|---|---|---|
| 🟢 **SAFE** | Green | `SAFE` | All parameters below warning |
| 🟡 **WARNING** | Yellow | `WARN` | Any parameter ≥ warning threshold |
| 🔴 **DANGER** | Red | `DANGR` | Any parameter ≥ danger threshold |

---

## 📡 UART Output Format

At 9600 bps (8-N-1), the system transmits every 2 seconds:

```
----- Readings -----
Temp: 13.18 C
Humi: 41.01 %
Gas:  259.76 ppm
Status: SAFE
```

---

## 🎬 Demo
https://youtu.be/dbsSpo_JKxE

---



> Sultan Qaboos University — Department of Electrical and Computer Engineering  
> ECCE4227: Embedded Systems, Spring 2026

---

## 📄 License

This project is licensed under the [MIT License](LICENSE).
