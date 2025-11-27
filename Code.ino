/* 
   - TEA5767 direct I2C (no library)
   - LCD 16x2 (4-bit mode): RS=7, E=8, D4=9, D5=10, D6=11, D7=12
   - Buttons: UP=2, DOWN=3, AUTO=4  (use INPUT_PULLUP, buttons to GND)
   - Buzzer: pin 6
   - Frequency step: 0.1 MHz (change STEP if needed)
   - Auto-scan improved: averages multiple RSSI samples per step and requires sustained signal
*/

#include <Wire.h>
#include <LiquidCrystal.h>

// LCD: RS, E, D4, D5, D6, D7
LiquidCrystal lcd(7, 8, 9, 10, 11, 12);

// Pins
const uint8_t BTN_UP   = 2;
const uint8_t BTN_DOWN = 3;
const uint8_t BTN_AUTO = 4;
const uint8_t BUZZER   = 6;

// TEA5767 I2C address
const uint8_t TEA_ADDR = 0x60;

// Frequency control
float freqMHz = 101.10;
const float STEP = 0.1;
const float FMIN = 87.5;
const float FMAX = 108.0;

// Debounce and repeat
const unsigned long DEBOUNCE_MS = 40;
unsigned long lastPressTimeUp = 0;
unsigned long lastPressTimeDown = 0;
unsigned long lastPressTimeAuto = 0;
unsigned long lastRepeat = 0;
const unsigned long REPEAT_DELAY = 400;
const unsigned long REPEAT_INTERVAL = 120;

// Auto-scan tuning (change these if needed)
const float SCAN_STEP = 0.1;
const int SCAN_DWELL_MS = 250;       // dwell per frequency step
const int RSSI_THRESHOLD = 3;        // 0..7 typical. Increase to be stricter.
const int RSSI_SAMPLE_COUNT = 3;     // number of status reads to average per step
const int RSSI_STABLE_COUNT = 2;     // how many consecutive steps must meet threshold

// ---------- TEA5767 low-level ----------
void teaSetFrequency(float mhz) {
  unsigned long freqHz = (unsigned long)(mhz * 1000000.0 + 0.5);
  unsigned long pll = (4 * (freqHz + 225000UL)) / 32768UL; // integer math

  byte out[5];
  out[0] = (byte)((pll >> 8) & 0x3F);
  out[1] = (byte)(pll & 0xFF);
  out[2] = 0xB0; // safe default control byte for normal operation
  out[3] = 0x10; // common default
  out[4] = 0x00;

  Wire.beginTransmission(TEA_ADDR);
  Wire.write(out, 5);
  Wire.endTransmission();
}

bool teaReadStatus(byte status[5]) {
  Wire.requestFrom(TEA_ADDR, (uint8_t)5);
  uint8_t i = 0;
  unsigned long t0 = millis();
  while (Wire.available() < 5) {
    if (millis() - t0 > 50) break;
  }
  for (i = 0; i < 5 && Wire.available(); ++i) {
    status[i] = Wire.read();
  }
  return (i == 5);
}

// Parse stereo and RSSI (0..7 typical)
void teaParseStatus(byte status[5], bool &stereo, int &rssi) {
  // ST (stereo) commonly at status[2] bit7
  stereo = (status[2] & 0x80) != 0;
  // RSSI commonly at status[3] bits 4..6 (value 0..7)
  rssi = (status[3] & 0x70) >> 4;
}

// ---------- utility ----------
void beep(uint16_t ms = 50) {
  tone(BUZZER, 2500, ms);
  delay(ms);
}

// Convert rssi (0..7) to approximate dB for user display
int rssiToDb(int rssi) {
  if (rssi < 0) return -999;
  // Linear approx used previously: dB ≈ RSSI * 6 + 20
  return rssi * 6 + 20;
}

// Print frequency and dB + ST/MO on LCD line 2
void updateLCDSignal() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("FM ");
  lcd.print(freqMHz, 2);
  lcd.print(" MHz");

  byte s[5];
  bool ok = teaReadStatus(s);
  bool stereo = false;
  int rssi = -1;
  if (ok) teaParseStatus(s, stereo, rssi);

  lcd.setCursor(0, 1);
  if (ok && rssi >= 0) {
    int dB = rssiToDb(rssi);
    // Keep formatting within 16 columns: we'll print like "Sig: 48 dB ST"
    lcd.print("Sig:");
    lcd.print(dB);
    lcd.print(" dB");
    // Put ST or MO at column 13 (0-based) if fits
    if (stereo) {
      lcd.setCursor(13, 1);
      lcd.print("ST");
    } else {
      lcd.setCursor(13, 1);
      lcd.print("MO");
    }
  } else {
    lcd.print("Signal: -- dB");
  }
}

// ---------- improved autoscan ----------
bool sampleRssiOnce(int &outRssi, bool &outStereo) {
  byte s[5];
  if (!teaReadStatus(s)) return false;
  teaParseStatus(s, outStereo, outRssi);
  return true;
}

// Return averaged RSSI (0..7) from N samples; returns -1 on read failure
int averageRssiSamples(int samples, int delayBetweenMs = 40) {
  int sum = 0;
  int count = 0;
  for (int i = 0; i < samples; ++i) {
    int rssi;
    bool st;
    if (!sampleRssiOnce(rssi, st)) {
      // failed read
      return -1;
    }
    sum += rssi;
    count++;
    delay(delayBetweenMs);
  }
  if (count == 0) return -1;
  return (sum + count/2) / count; // rounded average
}

// Improved autoscan upward with wrap-around and sustained detection
void autoScanUpImproved() {
  beep(120);
  int stableHits = 0;
  float startFreq = freqMHz;
  float f = freqMHz + SCAN_STEP;
  if (f > FMAX) f = FMIN; // wrap immediately if at top

  // We'll loop at most (band width / step) steps to avoid infinite loops
  int maxSteps = (int)((FMAX - FMIN) / SCAN_STEP) + 2;
  for (int step = 0; step < maxSteps; ++step) {
    freqMHz = f;
    teaSetFrequency(freqMHz);
    updateLCDSignal();

    // dwell and sample multiple times for stable detection
    int avgRssi = averageRssiSamples(RSSI_SAMPLE_COUNT, SCAN_DWELL_MS / RSSI_SAMPLE_COUNT);
    if (avgRssi >= 0) {
      if (avgRssi >= RSSI_THRESHOLD) {
        stableHits++;
      } else {
        stableHits = 0;
      }
      // require consecutive stable steps to lock (reduces false positives)
      if (stableHits >= RSSI_STABLE_COUNT) {
        beep(160); // locked
        return;
      }
    } else {
      // failed read: continue scanning but don't lock
      stableHits = 0;
    }

    // allow user to interrupt scan by pressing any button
    if (digitalRead(BTN_UP) == LOW || digitalRead(BTN_DOWN) == LOW || digitalRead(BTN_AUTO) == LOW) {
      // wait for release to avoid accidental immediate re-trigger
      while (digitalRead(BTN_UP) == LOW || digitalRead(BTN_DOWN) == LOW || digitalRead(BTN_AUTO) == LOW) delay(10);
      break;
    }

    // advance frequency
    f += SCAN_STEP;
    if (f > FMAX) f = FMIN; // wrap
  }

  // if not locked, keep last freq (or return to start if you prefer)
  beep(100); // scan finished
  updateLCDSignal();
}

// ---------- UI logic ----------
bool isPressedStable(uint8_t pin, unsigned long &lastTime) {
  if (digitalRead(pin) == LOW) {
    unsigned long now = millis();
    if (now - lastTime > DEBOUNCE_MS) {
      lastTime = now;
      return true;
    }
  }
  return false;
}

void changeFreq(float delta) {
  freqMHz += delta;
  if (freqMHz < FMIN) freqMHz = FMIN;
  if (freqMHz > FMAX) freqMHz = FMAX;
  teaSetFrequency(freqMHz);
  updateLCDSignal();
}

// ---------- Arduino setup & loop ----------
void setup() {
  Wire.begin();
  lcd.begin(16, 2);

  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_AUTO, INPUT_PULLUP);
  pinMode(BUZZER, OUTPUT);

  // set initial frequency
  teaSetFrequency(freqMHz);
  delay(50);
  updateLCDSignal();
  beep(80);
}

void loop() {
  // UP
  if (isPressedStable(BTN_UP, lastPressTimeUp)) {
    changeFreq(STEP);
    beep(40);
    lastRepeat = millis();
    delay(120);
  }

  // DOWN
  if (isPressedStable(BTN_DOWN, lastPressTimeDown)) {
    changeFreq(-STEP);
    beep(40);
    lastRepeat = millis();
    delay(120);
  }

  // AUTO
  if (isPressedStable(BTN_AUTO, lastPressTimeAuto)) {
    // wait release to avoid immediate stop
    while (digitalRead(BTN_AUTO) == LOW) delay(10);
    autoScanUpImproved();
  }

  // Hold repeat for UP/DOWN
  if (digitalRead(BTN_UP) == LOW) {
    if (millis() - lastRepeat > REPEAT_DELAY) {
      changeFreq(STEP);
      beep(30);
      lastRepeat = millis() - (REPEAT_DELAY - REPEAT_INTERVAL);
      delay(80);
    }
  } else if (digitalRead(BTN_DOWN) == LOW) {
    if (millis() - lastRepeat > REPEAT_DELAY) {
      changeFreq(-STEP);
      beep(30);
      lastRepeat = millis() - (REPEAT_DELAY - REPEAT_INTERVAL);
      delay(80);
    }
  }

  // periodic refresh of signal indicator
  static unsigned long lastRefresh = 0;
  if (millis() - lastRefresh > 1000) {
    updateLCDSignal();
    lastRefresh = millis();
  }
}
