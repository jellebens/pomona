# Air temperature / humidity / pressure — BME280 (STEMMA QT)

**Status: to wire — ⚠ strap SDO to 0x76 FIRST.**

## ⚠ Address strap BEFORE first power-on

The Adafruit BME280 defaults to **0x77**, which the
[Grove level strip](level-strip.md) already occupies (its low-8-pad bank).
Two devices on 0x77 means garbage reads on *both*. Bridge the **SDO solder
jumper** on the back of the breakout to move it to **0x76**. If the
bring-up scan shows no 0x76, or BME/level values look insane, this strap
is the first suspect.

## Wiring

I²C on the main `Wire` bus: QT→male-dupont cable to the SDA/SCL header
pins, 3V3 + GND. QT colors: black = GND, red = 3V3, **blue = SDA,
yellow = SCL**. The [BH1750](light-bh1750.md) daisy-chains off this
board's second QT socket.

## Verification

`bringup` reports it at boot. Breathe on it → humidity jumps. No
calibration needed.

## Placement

Near the tower but out of splash range.
