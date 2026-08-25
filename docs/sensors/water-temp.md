# Water temperature — DS18B20

**Status: ✅ wired + ice-bath verified (2026-08-25, #242) — no offset
needed.** Besides its own reading, this sensor temperature-compensates
[EC](ec-tds.md) and cross-checks [pH](ph.md) — calibrate those with the
DS18B20 in the same liquid.

Waterproof probe version (steel tip + cable) goes in the tank; a bare
TO-92 chip version is NOT submersible.

## Wiring (as built)

The probe connects through an old pH interface module used purely as a
breakout — its **T2** pin exposes the DS18B20's OneWire data line, and the
board carries the pull-up (no external 4.7 kΩ needed):

| Module pin | GIGA |
|---|---|
| Vcc | **3V3 — never 5 V** (the board's data pull-up ties to Vcc; at 5 V it would exceed the GIGA's 3.3 V pin limit) |
| GND (power) + GND (analog) | GND |
| T2 (DS18B20 data) | **D2** |
| P0 (pH stage), T1 (LM35) | unconnected — pH comes from the [SEN0169-V2](ph.md), air temp from the [BME280](air-bme280.md) |

(Direct wiring without the module also works: VCC → 3V3, GND → GND,
data → D2 with a 4.7 kΩ pull-up to 3V3.)

## Calibration record

| Date | Reference | Raw reading | Verdict |
|---|---|---|---|
| 2026-08-25 | Stirred ice bath, PH-201H pen = 0.0 °C | stable minimum **0.3–0.4 °C** | within the ±0.5 °C factory spec — **no offset applied** |

Bath-technique note for future re-checks: readings of 5–6 °C in a weak
bath (few ice cubes, tip touching the glass, tip not buried in the ice)
are bath artifacts, not sensor error — pack the glass with ice, stir, keep
the tip in the slush, and give the steel tip a few minutes of settling.

## Verification

`bringup` reports it at boot ("DS18B20 NOT FOUND on D2" = check the module
power / T2 wire). Warm the tip in your hand → the water-temp reading rises.

## Placement

Tip in the tank, spaced away from the pH and TDS probes.
