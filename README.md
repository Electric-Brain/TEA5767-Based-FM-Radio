# TEA5767 Arduino FM Radio

A simple FM radio project using Arduino UNO and TEA5767 FM receiver module.  
Features: manual tuning (Up/Down), improved auto-scan, estimated signal strength in dB, stereo/mono indicator (ST/MO), 16×2 LCD (4-bit), buzzer feedback.

## Quick Start
1. Connect hardware
2. Open `src/TEA5767_Radio.ino` in Arduino IDE.
3. Ensure TEA5767 power (3.3V vs 5V) matches your module.
4. Upload to Arduino UNO.
5. Use UP/DOWN to tune; press AUTO to scan.

## Notes & Tuning
- TEA5767 provides a 3-bit RSSI (0..7). This sketch converts RSSI to an approximate dB value (`dB ≈ RSSI*6 + 20`) for user display. This is an estimate, not a calibrated RF dBμV measurement.
- Adjust `RSSI_THRESHOLD`, `SCAN_DWELL_MS`, `RSSI_SAMPLE_COUNT`, `RSSI_STABLE_COUNT` in the sketch for different environments.
- If you get no I2C response, double-check SDA/SCL wiring and module VCC.

## License
This project is published under the MIT License — see `LICENSE`.
