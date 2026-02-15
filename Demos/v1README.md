Smart Rod v1 — TCS34725 distance + BNO085 UART-RVC power bar + LCD1602

This build uses:

ESP32 DevKit V1

TCS34725 color sensor (I2C) for spool mark detection + revolution counting

BNO085 (UART-RVC) for a “power” meter based on dynamic acceleration

LCD1602 (4-bit parallel) for distance + bar display

Wiring summary
Power (common ground)

ESP32 3V3 → TCS34725 VIN

ESP32 3V3 → BNO085 VIN

ESP32 GND → TCS34725 GND

ESP32 GND → BNO085 GND

ESP32 GND → LCD1602 GND (VSS)

TCS34725 (I2C)

ESP32 GPIO21 (SDA) → TCS34725 SDA

ESP32 GPIO22 (SCL) → TCS34725 SCL

In this v1 code, the TCS is the only I2C device on GPIO21/22.

BNO085 (UART-RVC mode)

ESP32 GPIO19 (UART2 RX) → BNO085 SDA (UART output in RVC mode)

BNO085 P0 → ESP32 3V3 (strap before power-up; power-cycle after changing)

Don’t connect the BNO to the I2C bus in this setup. Only VIN/GND + SDA→GPIO19 + P0→3V3.

LCD1602 (4-bit parallel)

Control + data

ESP32 GPIO14 → LCD RS

ESP32 GPIO27 → LCD E

ESP32 GPIO26 → LCD D4

ESP32 GPIO25 → LCD D5

ESP32 GPIO33 → LCD D6

ESP32 GPIO32 → LCD D7

Fixed pins

LCD RW → GND

LCD VSS → GND

LCD VDD → 5V (typical) or 3V3 (can be dim)

Contrast (VO)

Recommended: 10k pot

one end → 5V

other end → GND

wiper → VO

Backlight

LCD A → 3.3v

LCD K → GND
