# Changelog

## 2.0.0 (2026-11-27)
Firmware rewrite. See docs/CHANGES-FROM-2012.md for the full list.
- Per-sample I·dt and V·I·dt integration, 64-bit accumulators
- Bandgap Vcc compensation for the voltage channel
- Bidirectional sensor support, Ah/Wh in and out, SOC gauge
- Serial zero/voltage calibration stored in EEPROM
- Low voltage disconnect output with hysteresis and delay
- Periodic EEPROM save of totals, CSV output
- Host unit tests and GitHub Actions CI

## 1.1 (2014-09-09)
Corrections and additions to the Instructable.

## 1.0 (2012-05-09)
Original Instructable: ACS715 + divider + 20x4 LCD.
