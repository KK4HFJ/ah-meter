/*
  AhMeter - DIY Volt / Amp / Watt / Amp-hour / Watt-hour meter
  Steve Spence, KK4HFJ  -  2012 original, 2026 rewrite

  Hardware: Arduino Uno/Nano (ATmega328P), Allegro ACS71x Hall current
  sensor on A0, resistor divider on A4, HD44780 20x4 LCD in 4-bit mode.
  All hardware settings live in config.h.

  What changed vs. the 2012 sketch (see docs/CHANGES-FROM-2012.md):
   - Ah and Wh are integrated every sample with the measured dt
     (the old code multiplied an average by total time, and used present
     voltage x total Ah for Wh).
   - 64-bit integer accumulators: no overflow, no float drift.
   - Supply voltage is measured with the internal bandgap, so the voltage
     reading no longer assumes Vcc = exactly 5.000 V.
   - Bidirectional sensors supported: separate charge-in / discharge-out
     totals and a state-of-charge gauge.
   - Zero and voltage calibration from the serial port, stored in EEPROM.
   - Optional low voltage disconnect output with hysteresis.
   - Totals saved to EEPROM periodically to survive brownouts.
   - Non-blocking loop, millis() rollover safe, CSV output for logging.

  Serial commands (115200, newline terminated):
    ?          help
    z          zero the current sensor (run with NO current flowing)
    v 12.84    calibrate voltage to a reading from a trusted meter
    r          reset Ah/Wh totals and run time
    s          save totals to EEPROM now
    c          toggle CSV / human readable output
*/

#include <LiquidCrystal.h>
#include <EEPROM.h>
#include "config.h"
#include "energy.h"

using namespace ahm;

LiquidCrystal lcd(LCD_PINS);

// ---------------------------------------------------------------------------
// Persistent settings
// ---------------------------------------------------------------------------
static const uint16_t SETTINGS_MAGIC = 0xA4E1;
static const uint16_t TOTALS_MAGIC   = 0xA4E2;
static const int SETTINGS_ADDR = 0;
static const int TOTALS_ADDR   = 32;

struct Settings {
  uint16_t magic;
  int32_t  zeroSum;   // oversampled ADC sum at 0 A
  float    vScale;    // voltage calibration factor
  uint8_t  csv;       // 1 = CSV serial output
};

struct SavedTotals {
  uint16_t magic;
  Totals   t;
};

Settings cfg;
Totals totals;
#if LVD_PIN >= 0
Lvd lvd(LVD_OFF_V, LVD_ON_V, LVD_DELAY_MS);
#endif

const float DIVIDER_RATIO = (DIVIDER_R1_OHMS + DIVIDER_R2_OHMS) / DIVIDER_R2_OHMS;

// Latest readings
float volts = 0, amps = 0, watts = 0, vccMv = 5000;

// ---------------------------------------------------------------------------
// ADC helpers
// ---------------------------------------------------------------------------
int32_t readOversampled(uint8_t pin) {
  analogRead(pin);                       // discard first read after mux change
  int32_t sum = 0;
  for (uint8_t i = 0; i < OVERSAMPLE; i++) sum += analogRead(pin);
  return sum;
}

// Measure Vcc against the internal 1.1 V bandgap (ATmega328P / 168 / 32U4).
float readVccMv() {
#if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega168__) || defined(__AVR_ATmega32U4__)
  #if defined(__AVR_ATmega32U4__)
  ADMUX = _BV(REFS0) | _BV(MUX4) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  #else
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  #endif
  delay(2);                              // let the bandgap settle
  uint32_t sum = 0;
  for (uint8_t i = 0; i < 9; i++) {
    ADCSRA |= _BV(ADSC);
    while (bit_is_set(ADCSRA, ADSC)) {}
    if (i) sum += ADC;                   // drop the first conversion
  }
  if (sum == 0) return 5000.0f;
  return BANDGAP_MV * 1024.0f * 8.0f / (float)sum;
#else
  return 5000.0f;                        // other MCUs: assume nominal
#endif
}

// ---------------------------------------------------------------------------
// EEPROM
// ---------------------------------------------------------------------------
void loadSettings() {
  EEPROM.get(SETTINGS_ADDR, cfg);
  if (cfg.magic != SETTINGS_MAGIC || !(cfg.vScale > 0.5f && cfg.vScale < 2.0f)) {
    cfg.magic   = SETTINGS_MAGIC;
    cfg.zeroSum = (int32_t)((SENSOR_ZERO_MV / 5000.0f) * 1024.0f * OVERSAMPLE + 0.5f);
    cfg.vScale  = 1.0f;
    cfg.csv     = 0;
  }
}
void saveSettings() { EEPROM.put(SETTINGS_ADDR, cfg); }   // put() only writes changed bytes

void loadTotals() {
  SavedTotals s;
  EEPROM.get(TOTALS_ADDR, s);
  if (s.magic == TOTALS_MAGIC) totals = s.t; else totals.reset();
}
void saveTotals() {
  SavedTotals s;
  s.magic = TOTALS_MAGIC;
  s.t = totals;
  EEPROM.put(TOTALS_ADDR, s);
}

// ---------------------------------------------------------------------------
// Measurement
// ---------------------------------------------------------------------------
void sample(uint32_t dtMs) {
  int32_t iSum = readOversampled(CURRENT_PIN);
  int32_t vSum = readOversampled(VOLT_PIN);

  amps = countsToAmps(iSum, cfg.zeroSum, OVERSAMPLE, SENSOR_MV_PER_AMP);
  if (SENSOR_INVERT) amps = -amps;
  if (fabs(amps) < CURRENT_DEADBAND_A) amps = 0;

  volts = countsToVolts(vSum, OVERSAMPLE, vccMv, DIVIDER_RATIO, cfg.vScale);
  watts = volts * amps;

  totals.add(volts, amps, dtMs);

#if LVD_PIN >= 0
  bool on = lvd.update(volts, dtMs);
  digitalWrite(LVD_PIN, (on == (bool)LVD_ACTIVE_HIGH) ? HIGH : LOW);
#endif
}

// ---------------------------------------------------------------------------
// Output
// ---------------------------------------------------------------------------
float soc() {
  return stateOfChargePct(totals, BATTERY_CAPACITY_AH, BATTERY_START_SOC, CHARGE_EFFICIENCY);
}

bool lvdOn() {
#if LVD_PIN >= 0
  return lvd.loadOn;
#else
  return true;
#endif
}

// Write a full LCD row, padded with spaces so shorter numbers don't leave
// stale digits behind (a bug in the 2012 sketch).
void lcdRow(uint8_t row, const char* text) {
  char buf[LCD_COLS + 1];
  uint8_t i = 0;
  for (; i < LCD_COLS && text[i]; i++) buf[i] = text[i];
  for (; i < LCD_COLS; i++) buf[i] = ' ';
  buf[LCD_COLS] = 0;
  lcd.setCursor(0, row);
  lcd.print(buf);
}

void updateLcd() {
  char line[32], a[10], b[10], c[10];

  dtostrf(volts, 5, 2, a);
  dtostrf(amps, 6, 2, b);
  snprintf(line, sizeof line, "%sV %sA%5dW", a, b, (int)lround(watts));
  lcdRow(0, line);

  dtostrf(totals.ahOut(), 6, 2, a);
  dtostrf(totals.ahIn(), 6, 2, b);
  snprintf(line, sizeof line, "Ah o%s i%s", a, b);
  lcdRow(1, line);

  dtostrf(totals.whOut(), 6, 1, a);
  dtostrf(totals.whIn(), 6, 1, b);
  snprintf(line, sizeof line, "Wh o%s i%s", a, b);
  lcdRow(2, line);

  uint32_t h = totals.seconds / 3600UL;
  uint8_t m = (totals.seconds / 60UL) % 60;
  float s = soc();
  if (s >= 0) snprintf(c, sizeof c, "%3d%%", (int)lround(s)); else strcpy(c, "  --");
  snprintf(line, sizeof line, "SOC%s %4lu:%02u %s", c, (unsigned long)h, m, lvdOn() ? " ON" : "OFF");
  lcdRow(3, line);
}

void printSerial() {
  if (cfg.csv) {
    // ms,volts,amps,watts,ah_out,ah_in,wh_out,wh_in,soc,load_on,vcc_mv
    Serial.print(millis());            Serial.print(',');
    Serial.print(volts, 3);            Serial.print(',');
    Serial.print(amps, 3);             Serial.print(',');
    Serial.print(watts, 2);            Serial.print(',');
    Serial.print(totals.ahOut(), 4);   Serial.print(',');
    Serial.print(totals.ahIn(), 4);    Serial.print(',');
    Serial.print(totals.whOut(), 3);   Serial.print(',');
    Serial.print(totals.whIn(), 3);    Serial.print(',');
    Serial.print(soc(), 1);            Serial.print(',');
    Serial.print(lvdOn() ? 1 : 0);     Serial.print(',');
    Serial.println(vccMv, 0);
  } else {
    Serial.print(F("V="));      Serial.print(volts, 2);
    Serial.print(F("  A="));    Serial.print(amps, 2);
    Serial.print(F("  W="));    Serial.print(watts, 1);
    Serial.print(F("  Ah out/in/net="));
    Serial.print(totals.ahOut(), 3); Serial.print('/');
    Serial.print(totals.ahIn(), 3);  Serial.print('/');
    Serial.print(totals.ahNet(), 3);
    Serial.print(F("  Wh out/in="));
    Serial.print(totals.whOut(), 2); Serial.print('/');
    Serial.print(totals.whIn(), 2);
    Serial.print(F("  h="));    Serial.print(totals.seconds / 3600.0f, 3);
    Serial.print(F("  SOC="));  Serial.print(soc(), 1);
    Serial.print(F("%  load=")); Serial.print(lvdOn() ? F("ON") : F("OFF"));
    Serial.print(F("  Vcc="));  Serial.println(vccMv, 0);
  }
}

void printCsvHeader() {
  Serial.println(F("ms,volts,amps,watts,ah_out,ah_in,wh_out,wh_in,soc,load_on,vcc_mv"));
}

// ---------------------------------------------------------------------------
// Serial commands
// ---------------------------------------------------------------------------
void help() {
  Serial.println(F("Commands: ?=help  z=zero current (no load!)  v <volts>=calibrate"));
  Serial.println(F("          r=reset totals  s=save totals  c=toggle CSV"));
}

void handleCommand(char* cmd) {
  while (*cmd == ' ') cmd++;
  switch (cmd[0]) {
    case '?': help(); break;

    case 'z': case 'Z': {
      int32_t sum = 0;
      for (uint8_t i = 0; i < 32; i++) { sum += readOversampled(CURRENT_PIN); delay(5); }
      cfg.zeroSum = (sum + 16) / 32;
      saveSettings();
      Serial.print(F("Zero set: ")); Serial.print(cfg.zeroSum / (float)OVERSAMPLE, 2);
      Serial.println(F(" counts"));
      break;
    }

    case 'v': case 'V': {
      float actual = atof(cmd + 1);
      if (actual <= 0) { Serial.println(F("Usage: v 12.84")); break; }
      int32_t vSum = readOversampled(VOLT_PIN);
      float raw = countsToVolts(vSum, OVERSAMPLE, vccMv, DIVIDER_RATIO, 1.0f);
      if (raw < 0.5f) { Serial.println(F("Voltage too low to calibrate")); break; }
      float scale = actual / raw;
      if (scale < 0.5f || scale > 2.0f) { Serial.println(F("Out of range - check divider")); break; }
      cfg.vScale = scale;
      saveSettings();
      Serial.print(F("Voltage scale = ")); Serial.println(cfg.vScale, 5);
      break;
    }

    case 'r': case 'R':
      totals.reset();
      saveTotals();
      Serial.println(F("Totals reset"));
      break;

    case 's': case 'S':
      saveTotals();
      Serial.println(F("Totals saved"));
      break;

    case 'c': case 'C':
      cfg.csv = !cfg.csv;
      saveSettings();
      if (cfg.csv) printCsvHeader();
      break;

    default:
      if (cmd[0]) help();
  }
}

void pollSerial() {
  static char buf[24];
  static uint8_t len = 0;
  while (Serial.available()) {
    char ch = Serial.read();
    if (ch == '\n' || ch == '\r') {
      if (len) { buf[len] = 0; handleCommand(buf); len = 0; }
    } else if (len < sizeof(buf) - 1) {
      buf[len++] = ch;
    }
  }
}

// ---------------------------------------------------------------------------
// Setup / loop
// ---------------------------------------------------------------------------
uint32_t lastSample, lastDisplay, lastSerial, lastVcc, lastPersist;

void setup() {
  Serial.begin(SERIAL_BAUD);
  lcd.begin(LCD_COLS, LCD_ROWS);
  lcdRow(0, "AhMeter  KK4HFJ");
  lcdRow(1, "starting...");

#if LVD_PIN >= 0
  pinMode(LVD_PIN, OUTPUT);
  digitalWrite(LVD_PIN, LVD_ACTIVE_HIGH ? HIGH : LOW);   // load on at boot
#endif

  loadSettings();
  loadTotals();
  vccMv = readVccMv();

  help();
  if (cfg.csv) printCsvHeader();

  lastSample = lastDisplay = lastSerial = lastVcc = lastPersist = millis();
}

void loop() {
  const uint32_t now = millis();   // unsigned subtraction below is rollover safe

  pollSerial();

  if (now - lastVcc >= 1000UL) {
    lastVcc = now;
    vccMv = readVccMv();
  }

  if (now - lastSample >= SAMPLE_INTERVAL_MS) {
    const uint32_t dt = now - lastSample;   // real elapsed time, not assumed
    lastSample = now;
    sample(dt);
  }

  if (now - lastDisplay >= DISPLAY_INTERVAL_MS) {
    lastDisplay = now;
    updateLcd();
  }

  if (now - lastSerial >= SERIAL_INTERVAL_MS) {
    lastSerial = now;
    printSerial();
  }

#if PERSIST_TOTALS_MIN > 0
  if (now - lastPersist >= PERSIST_TOTALS_MIN * 60000UL) {
    lastPersist = now;
    saveTotals();
  }
#endif
}
