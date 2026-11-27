# Changes from the 2012 version

The 2012 sketch (preserved in `legacy/`) served a lot of builders, but it has
bugs that matter for long-running battery monitoring. Each one is covered by a
test in `test/test_energy.cpp`.

| # | 2012 behaviour | Effect | Fix |
|---|---|---|---|
| 1 | `ratio` declared twice, `avgBVal` undeclared, `#include` line lost its `<LiquidCrystal.h>` | Sketch as posted doesn't compile | Rewritten |
| 2 | `int sample` counter | Overflows at 32,767 loops (~1 h), Ah goes negative | No sample counter; integrate per step |
| 3 | `Ah = average(amps) × millis()` | Only right if every loop takes the same time; float sum loses precision | `I × dt` each sample with the measured dt, 64-bit integer mA·ms |
| 4 | `Wh = present V × total Ah` | Wrong whenever voltage changes (always). 30 min at 14 V + 30 min at 11 V at 10 A reads 110 Wh instead of 125 Wh | `V × I × dt` each sample |
| 5 | `int outputValue` (mA) | Overflows above ~32.7 A, inside the ACS715-30A range | Float amps, 32-bit mA |
| 6 | `0.00488 V/count` (assumes Vcc = 5.000 V) | USB Vcc of 4.8 V reads 13.75 V for a 13.2 V battery (+4%) | Vcc measured against the internal bandgap every second |
| 7 | 30k/10k divider, 20 V = 5 V | No headroom. Panel Voc or equalize charge over-drives A4 | 47k/10k default, series 1k + 100 nF |
| 8 | Hardcoded 500 mV zero, 133 mV/A | No bidirectional support, offset error accumulates | Sensor table in config, `z` zero command, deadband |
| 9 | LCD numbers not padded | Stale digits when a value gets shorter (e.g. 10.00 → 9.99**0**) | Full-row padded writes |
| 10 | `millis()` used as absolute time | Glitch at 49.7-day rollover | Unsigned interval subtraction |
| 11 | Blocking `delay()` loop, 9600 baud | Uneven sample timing, slow serial | Non-blocking scheduler, 115200 baud |
| 12 | Totals lost at power-down | A brownout wipes the day's numbers | Periodic EEPROM save (`EEPROM.put` writes only changed bytes) |

New in 2026: separate charge/discharge Ah and Wh, a SOC gauge with charge
efficiency, low voltage disconnect (the "future expansion" item from step 7),
CSV output, and serial calibration commands.
