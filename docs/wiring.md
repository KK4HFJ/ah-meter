# Wiring

## Current sensor (ACS71x carrier)

| Carrier pin | Arduino |
|---|---|
| VCC | 5V |
| GND | GND |
| OUT | A0 |

The power lugs go **in series with the battery positive lead**.

- **ACS715 (unidirectional):** the arrow points toward the load. It only reads
  discharge, so Ah-in stays zero.
- **ACS714 / ACS712 (bidirectional):** put the sensor between the battery and
  the bus that both the charge controller and the loads tie into. Discharge
  reads positive and charge reads negative. If it reads backwards, set
  `SENSOR_INVERT 1`.

Keep the high-current wiring away from the sensor's signal side. Twist the OUT
and GND wires back to the Arduino. On a long run, add a 1 nF to 10 nF cap from
OUT to GND at the Arduino end.

## Voltage divider

```
Battery + ──[ R1 ]──┬──[ 1k ]── A4
                    │            │
                  [ R2 ]       [100 nF]
                    │            │
Battery − ──────────┴────────────┴── GND
```

| System | R1 | R2 | Ratio | Max input |
|---|---|---|---|---|
| 12 V | 47 kΩ | 10 kΩ | 5.7 | 28.5 V |
| 24 V | 100 kΩ | 10 kΩ | 11.0 | 55 V |

Set `DIVIDER_R1_OHMS` and `DIVIDER_R2_OHMS` in `config.h` to match.

**Why not the original 30k/10k?** That divider maps 20 V to exactly 5 V. A
"12 V" panel at open circuit, or a lead-acid battery on equalize, can exceed
20 V and push A4 past the rail. 47k/10k leaves headroom for that.

Protection: the 1 kΩ series resistor limits current into the pin's internal
clamp diode if the input is ever over-driven. The 100 nF cap filters noise and
gives the ADC's sample-and-hold a low-impedance source. Use 1% metal-film
resistors. Calibration takes out the remaining tolerance.

**Measure at the battery terminals**, not at the far end of a load cable.
Under heavy current the cable drop will otherwise read as low battery voltage.

## LCD (HD44780 20x4, 4-bit mode)

Same pinout as the 2012 build.

| LCD | Arduino |
|---|---|
| RS (4) | D7 |
| E (6) | D8 |
| D4 (11) | D9 |
| D5 (12) | D10 |
| D6 (13) | D11 |
| D7 (14) | D12 |
| RW (5) | GND |
| VSS (1) / VDD (2) | GND / 5V |
| V0 (3) | 10k pot wiper (contrast) |
| A / K (15/16) | 5V via ~220 Ω / GND (backlight) |

## Low voltage disconnect (optional)

D6 goes HIGH while the load should stay connected (`LVD_ACTIVE_HIGH 1`). It
can't drive a load directly. Use one of these:

- **Low-side N-channel logic-level MOSFET** (e.g. IRLZ44N, IRLB8721): D6 →
  100 Ω → gate, with a 100 kΩ gate-to-source pulldown so the load is off if the
  Arduino is unpowered. Size the MOSFET for your load current and heatsink it.
- **Relay module** with its own driver transistor. Add a flyback diode on bare
  relay coils.

Set `LVD_PIN -1` to disable.

## Powering the Arduino from the battery

Use a buck converter (12/24 V → 5 V) into the 5V pin, or a 7–12 V input to
VIN/barrel. The bandgap Vcc measurement corrects the voltage reading for
whatever the regulator actually delivers.
