# AhMeter: DIY Volt / Amp / Watt / Amp-hour / Watt-hour Meter (Arduino)

A battery monitor and energy meter for off-grid solar, wind, and ham radio power
systems. It shows volts, amps, watts, **Ah in/out**, **Wh in/out**, run time,
battery **state of charge**, and an optional **low voltage disconnect**.
Readings go to a 20x4 LCD and the serial port, with an optional CSV mode for
logging.

This is the 2026 rewrite of my 2012 Instructable,
[DIY Amp / Watt Hour Volt Meter - Arduino](https://www.instructables.com/DIY-Amp-Hour-Meter-Arduino/).
The hardware is the same, so an existing build only needs new firmware and
(optionally) a new divider. The firmware was rewritten to fix the accumulation
math and add calibration. See [what changed and why](docs/CHANGES-FROM-2012.md).

— Steve Spence, KK4HFJ

## Features

- True coulomb counting. Ah and Wh are integrated each 100 ms sample using the
  measured time step. 64-bit integer accumulators run for years without
  overflow or drift.
- The supply voltage is measured against the internal bandgap, so voltage stays
  accurate when USB or regulator Vcc isn't exactly 5.000 V.
- Bidirectional sensors (ACS714/ACS712) are supported, with separate charge and
  discharge totals and a SOC gauge that accounts for charge efficiency.
- Calibration from the serial port: `z` zeros the current sensor, and
  `v 12.84` matches a trusted meter. Both are stored in EEPROM.
- Low voltage disconnect output with hysteresis and a hold-off delay, so a
  motor-start sag won't drop the load.
- Totals are saved to EEPROM every hour (configurable), so a brownout doesn't
  wipe the day's numbers.
- CSV output for a Pi, Node-RED, n8n, or a spreadsheet.
- Unit-tested math. `energy.h` has no Arduino dependencies and is tested on
  the host with `make test`.

## Hardware

| Part | Notes |
|---|---|
| Arduino Uno or Nano (ATmega328P) | 32U4 boards (Leonardo/Micro) also work |
| Allegro ACS71x current sensor carrier | ACS715-30A (original), ACS714-30A or ACS712 for bidirectional |
| Resistors 47 kΩ + 10 kΩ, 1% metal film | 12 V systems, 28.5 V max. Use 100k/10k for 24 V |
| 100 nF ceramic cap | A4 to GND, ADC filter |
| 20x4 HD44780 LCD + 10k pot | contrast |
| Optional: logic-level MOSFET or relay driver | for the LVD output on D6 |

Full wiring, with the protection parts I recommend, is in [docs/wiring.md](docs/wiring.md).

## Quick start

1. Open `firmware/AhMeter/config.h` and set your sensor, divider resistors,
   battery capacity, and LVD thresholds.
2. Open `firmware/AhMeter/AhMeter.ino` in the Arduino IDE (the LiquidCrystal and
   EEPROM libraries ship with it) and upload to an Uno/Nano. Or use the CLI:
   ```
   arduino-cli compile --fqbn arduino:avr:uno firmware/AhMeter
   arduino-cli upload  --fqbn arduino:avr:uno -p /dev/ttyUSB0 firmware/AhMeter
   ```
3. Open the serial monitor at **115200**, newline line ending.
4. With **no current flowing**, send `z` to zero the sensor.
5. Measure the battery with a good DMM and send `v 12.84` (your reading).
6. Send `r` to reset the totals once the battery is full.

Details are in [docs/calibration.md](docs/calibration.md).

## Serial commands

| Cmd | Action |
|---|---|
| `?` | help |
| `z` | zero current sensor (no load connected!) |
| `v <volts>` | calibrate voltage to a reference meter |
| `r` | reset Ah/Wh totals and run time |
| `s` | save totals to EEPROM now |
| `c` | toggle CSV output |

CSV columns: `ms,volts,amps,watts,ah_out,ah_in,wh_out,wh_in,soc,load_on,vcc_mv`

## LCD layout

```
12.84V  -8.21A -105W     volts, amps (negative = charging), watts
Ah o 23.41 i 31.07       amp-hours out / in
Wh o 291.2 i 433.9       watt-hours out / in
SOC 92%    7:41  ON      state of charge, run time h:mm, LVD state
```

## Repository layout

```
firmware/AhMeter/   AhMeter.ino, config.h, energy.h (pure math)
test/               host unit tests for energy.h
docs/               wiring, calibration, changes from the 2012 version
legacy/             the 2012 sketch as published, for reference
```

## Development

```
make test      # build & run host tests (g++)
make compile   # arduino-cli compile for Uno
```

GitHub Actions runs both on every push.

## Ideas / roadmap

- ESP32 version with MQTT/InfluxDB/Grafana and a web dashboard
- INA226 + external shunt option for better low-current accuracy
- SD card logging
- Hybrid relay (MOSFET + relay) driver for the LVD and dump-load control

## License

MIT. See [LICENSE](LICENSE).
