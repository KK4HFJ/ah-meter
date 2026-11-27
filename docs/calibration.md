# Calibration

Both calibrations are stored in EEPROM and survive power cycles and re-flashing
(unless you erase EEPROM). Serial monitor: 115200 baud, "Newline" line ending.

## 1. Zero the current sensor: `z`

Hall sensors have a zero offset that varies a few mV part to part and drifts
with temperature. The 2012 sketch hardcoded 500 mV.

1. Disconnect all loads and charge sources from the sensor. Unplug, don't just
   switch off.
2. Let the board warm up for a couple of minutes.
3. Send `z`. The meter averages 512 readings and stores the result.

Redo it if you notice a constant small current with nothing connected.
Readings below `CURRENT_DEADBAND_A` (50 mA default) are treated as zero, so a
little residual noise won't accumulate into phantom Ah overnight.

## 2. Calibrate voltage: `v <volts>`

This single factor corrects divider resistor tolerance and the ATmega's
bandgap tolerance (±10%).

1. Measure the battery at the same point the divider connects, using a
   trusted DMM.
2. Send `v` followed by the reading, e.g. `v 12.84`.
3. The meter prints the new scale factor. Values between 0.9 and 1.1 are
   normal. Anything outside 0.5–2.0 is rejected, which usually means the
   divider values in `config.h` don't match the board.

## 3. Check current against a clamp meter (optional)

With a known steady load (a 50 W bulb, a resistor bank, etc.), compare the
reading to a DC clamp meter. If it's consistently off by a percentage, adjust
`SENSOR_MV_PER_AMP` in `config.h`. Example: reads 2% high → multiply the value
by 1.02.

## 4. Start the gauge: `r`

Fully charge the battery, then send `r`. SOC then counts down from
`BATTERY_START_SOC` (100%) as Ah go out and back up as Ah come in, scaled by
`CHARGE_EFFICIENCY`. Coulomb counting drifts slowly over weeks, so reset at
each full charge.
