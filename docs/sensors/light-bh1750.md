# Light — BH1750 (STEMMA QT)

**Status: to wire.** Becomes more relevant once the grow light arrives
(verifying schedule + output).

## Wiring

I²C address **0x23** (ADDR low, default). Daisy-chained off the
[BME280](air-bme280.md)'s second STEMMA QT socket — no extra wiring to the
GIGA. QT colors: black = GND, red = 3V3, blue = SDA, yellow = SCL.

## Verification

`bringup` reports it at boot. Cover it → lux drops to ~0. No calibration
needed.

## Placement

Facing up, roughly at canopy height, so it sees what the plants see.
