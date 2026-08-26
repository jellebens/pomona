# Water temperature — DS18B20

**Status: ✅ wired + ice-bath verified (2026-08-25, #242) — no offset
needed.** Besides its own reading, this sensor temperature-compensates
[EC](ec-tds.md) and cross-checks [pH](ph.md) — calibrate those with the
DS18B20 in the same liquid.

Waterproof probe version (steel tip + cable) goes in the tank; a bare
TO-92 chip version is NOT submersible.

## Wiring (as built)

The probe connects through an old pH interface module — a
**"Logo-Rnaenaor V2.0"** pH sensor board (the PH0-14 clone family) — used
purely as a breakout: its **T2** pin exposes the DS18B20's OneWire data
line, and the board carries the pull-up (no external 4.7 kΩ needed).
Reference for the module:
[Arduino pH meter using pH sensor — bestengineeringprojects.com](https://bestengineeringprojects.com/arduino-ph-meter-using-ph-sensor/).

### Module pinout

| Pin | Function (per the module's documentation) | Our use |
|---|---|---|
| Vcc | positive power supply | **3V3 — never 5 V here** (the board's DS18B20 data pull-up ties to Vcc; at 5 V it would exceed the GIGA's 3.3 V pin limit) |
| GND | power supply ground | GND |
| GND | analog sensor ground | GND |
| P0 | pH output (analog) | unconnected — pH comes from the [SEN0169-V2](ph.md) |
| T1 | output of the onboard LM35 temp sensor (analog) | unconnected — air temp comes from the [BME280](air-bme280.md) |
| **T2** | **output of the DS18B20 waterproof temp sensor (digital, 1-Wire)** | → **GIGA D2** |

### Other module details (from the article, for the record)

- BNC connector for a pH electrode (easy connect/disconnect); dedicated
  connector for the DS18B20 waterproof probe — that's where our probe
  plugs in.
- Two potentiometers: **offset** (the one nearer the BNC) and **pH
  limit**. They affect only the P0 pH stage, not T2 — irrelevant for the
  temperature use, but needed if P0 is ever pressed into service.
- The article's pH calibration (only relevant if P0 is ever used as a
  backup pH input): board at 5 V, short the BNC → trim the offset pot to
  **2.5 V on P0 = neutral / pH 7 midpoint**, then establish the
  voltage→pH slope with 6.86 and 4.01 buffers. ⚠ Note that runs the
  board at 5 V per the article — incompatible with our 3V3-only rule
  while T2 is wired to the GIGA, and the 2.5 V midpoint scales with the
  supply. This board is the drifty type #220 deliberately passed over,
  so P0 stays a break-glass fallback at most.

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
