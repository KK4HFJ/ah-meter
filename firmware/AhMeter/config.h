// config.h - edit this file to match your hardware. Everything a builder
// normally needs to change is here; AhMeter.ino should not need edits.
#pragma once

// ---------------- Current sensor ----------------
// Pick ONE. Sensitivity is mV per amp at Vcc = 5.0 V (from the datasheet).
//   ACS715-30A  unidirectional, 0..30 A   133 mV/A  (original 2012 build)
//   ACS714-30A  bidirectional, +/-30 A     66 mV/A
//   ACS712-05B  bidirectional, +/-5 A     185 mV/A
//   ACS712-20A  bidirectional, +/-20 A    100 mV/A
//   ACS712-30A  bidirectional, +/-30 A     66 mV/A
#define SENSOR_MV_PER_AMP   133.0f
// Nominal zero-current output in mV (ACS715 = 500, ACS714/712 = 2500).
// Only used until you run the "z" (zero) command, which measures it.
#define SENSOR_ZERO_MV      500
// Set to 1 if current reads backwards (sensor arrow pointing the other way).
#define SENSOR_INVERT       0
// Readings smaller than this are shown and integrated as 0 (noise floor).
#define CURRENT_DEADBAND_A  0.05f

// ---------------- Voltage divider ----------------
// Vbat -- R1 --+-- R2 -- GND, junction to VOLT_PIN, 100 nF from pin to GND.
// Max measurable = 5 V * (R1+R2)/R2. Leave headroom above panel Voc!
//   47k / 10k  -> 28.5 V max  (12 V systems, default)
//   100k / 10k -> 55 V max    (24 V systems / higher Voc panels)
#define DIVIDER_R1_OHMS     47000.0f
#define DIVIDER_R2_OHMS     10000.0f

// Internal bandgap reference, nominal 1100 mV (+/-10% part to part).
// The "v" calibration command corrects for this, so leave it at 1100.
#define BANDGAP_MV          1100.0f

// ---------------- Pins ----------------
#define CURRENT_PIN   A0
#define VOLT_PIN      A4
// LCD in 4-bit mode: RS, E, D4, D5, D6, D7 (same as the 2012 build)
#define LCD_PINS      7, 8, 9, 10, 11, 12
#define LCD_COLS      20
#define LCD_ROWS      4
// Low voltage disconnect output (drives a MOSFET gate / relay driver).
// Set to -1 to disable LVD.
#define LVD_PIN       6
#define LVD_ACTIVE_HIGH 1     // 1: pin HIGH = load connected

// ---------------- Low voltage disconnect ----------------
#define LVD_OFF_V       11.5f   // disconnect below this...
#define LVD_ON_V        12.6f   // ...reconnect above this
#define LVD_DELAY_MS    10000UL // must persist this long before switching

// ---------------- Battery gauge ----------------
// Set capacity to 0 to hide SOC.
#define BATTERY_CAPACITY_AH  100.0f
#define BATTERY_START_SOC    100.0f  // assumed SOC at power-up / reset
#define CHARGE_EFFICIENCY    0.99f   // LiFePO4 ~0.99, flooded lead-acid ~0.85

// ---------------- Timing ----------------
#define OVERSAMPLE          16      // ADC reads averaged per sample (max 64)
#define SAMPLE_INTERVAL_MS  100UL   // integration step
#define DISPLAY_INTERVAL_MS 500UL
#define SERIAL_INTERVAL_MS  1000UL
#define SERIAL_BAUD         115200
// Save totals to EEPROM every N minutes so a brownout doesn't lose them.
// EEPROM is rated ~100k writes: 60 min -> 11+ years. 0 = never auto-save.
#define PERSIST_TOTALS_MIN  60
