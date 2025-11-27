# Wiring (TEA5767 Based FM Radio)

## TEA5767 Module (I2C)
- VCC -> 3.3V or 5V (check module marking; many require 3.3V)
- GND -> GND
- SDA -> A4 (Arduino Uno)
- SCL -> A5 (Arduino Uno)
- Audio L/R -> Amplifier input (do not connect to Arduino pins)

## 16x2 LCD (HD44780, 4-bit)
- Pin 1 (VSS) -> GND
- Pin 2 (VCC) -> 5V
- Pin 3 (VO) -> Middle of 10k pot (left -> 5V, right -> GND)
- Pin 4 (RS) -> Arduino D7
- Pin 5 (RW) -> GND
- Pin 6 (E) -> Arduino D8
- Pin 11 (D4) -> Arduino D9
- Pin 12 (D5) -> Arduino D10
- Pin 13 (D6) -> Arduino D11
- Pin 14 (D7) -> Arduino D12
- Pin 15 (LED+) -> 5V (with 100-220Ω if needed)
- Pin 16 (LED-) -> GND

## Buttons (use INPUT_PULLUP, press to GND)
- UP -> D2
- DOWN -> D3
- AUTO -> D4

## Buzzer
- + -> D6
- - -> GND

## Notes
- Use a small amplifier between TEA5767 audio outputs and speaker.
- If module is 3.3V-only, do NOT power from 5V.
