# Hardware Notes

## Analog Front-End Design

### LM35 (Temperature) — ADC CH0
- **Transfer function:** 10 mV / °C, linear
- **Output range:** 0 V (0 °C) → 1.5 V (150 °C)
- **Conditioning:** Unity-gain op-amp buffer (LM358N) to prevent ADC input loading
- **ADC formula:** `temp_C = (ADC / 1024.0) × 5.0 × 100`

### HIH-5030 (Humidity) — ADC CH1
- **Transfer function:** Ratiometric 0–5 V = 0–100% RH
- **Conditioning:** Unity-gain op-amp buffer (second stage of LM358N dual package)
- **ADC formula:** `humidity = (ADC / 1024.0) × 100`

### MQ-135 (Air Quality) — ADC CH2
- **Output:** Higher voltage → higher gas concentration
- **Conditioning:** 10 kΩ / 1 kΩ voltage divider (11:1) to keep output ≤ 5 V
- **ADC formula:** `gas_ppm = (ADC / 1024.0) × 1000`
- **Note:** The MQ-135 requires a 24–48 hour burn-in period before reliable readings

## Op-Amp Notes (LM358N)
- Single-supply device: Vcc = 5 V, GND = 0 V
- Input common-mode range: 0 V to (Vcc − 1.5 V) = 0 to 3.5 V
- Both sensor outputs fall within this range for Oman ambient conditions
- Both channels configured as voltage followers (R_f short, R_in open)

## LCD Contrast
- Connect a 10 kΩ potentiometer between 5 V and GND
- Wiper → LCD pin 3 (V0)
- Adjust until characters are clearly visible on both lines

## Crystal
- 16 MHz fundamental-mode crystal
- 22 pF load capacitors on both XTAL1 and XTAL2
- Fuses: `lfuse = 0xDF` (full swing crystal oscillator, no CLKDIV8)

## Fuse Settings (avrdude)
```bash
# Set fuses for 16 MHz external crystal, BOD at 4.0 V
avrdude -c usbasp -p m8 -U lfuse:w:0xdf:m -U hfuse:w:0xC9:m
```

## Programming Interface
The SQU ATmega8 development board (Rev 2.2) exposes ISP via the 6-pin
header (MOSI, MISO, SCK, RST, VCC, GND). Compatible programmers:
- USBasp
- Arduino as ISP (sketch → Examples → ArduinoISP)
- AVRISP mkII
