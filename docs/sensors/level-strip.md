# Water level (coarse range, optional) — Grove 10 cm capacitive strip

**Status: mount test pending — optional.** The
[CQRSENYW003 probe](level-probe.md) is the alarm instrument; this strip only
adds coarse full-range display. If its through-wall mount test fails, drop
it — nothing else needs it.

20 capacitive pads over 10 cm (0.5 cm/pad ≈ 0.3 L/pad in the ~25×25 cm
reservoir). Two I²C targets: **0x77** (low 8 pads, bottom) + **0x78**
(high 12 pads). The PCB is **not waterproof** — it stays OUTSIDE the tank
(through-wall mount). Covers the bottom 10 cm of a ~16 cm-full reservoir.

Driver: [`firmware/libraries/GroveWaterLevel`](../../firmware/libraries/GroveWaterLevel/src/GroveWaterLevel.h) ·
test sketch: [`firmware/leveltest/`](../../firmware/leveltest/leveltest.ino).

## Wiring

Grove cable: black → GND, red → **3V3**, yellow → SCL, white → SDA (main
`Wire` bus, dedicated SDA/SCL header pins). A yellow/white swap is the
classic mistake — `leveltest` diagnoses which bank is missing.

⚠ This strip occupies **0x77**, which is the Adafruit BME280's default
address — the [BME280 must be strapped to 0x76](air-bme280.md) before both
share the bus.

## Mount test + threshold

The strip is capacitive, so "calibration" means: pick the mount, verify the
wet/dry raw values are cleanly separated, set the threshold, and map pads to
actual water depth. Use `leveltest`, which dumps all 20 raw pad values
(bottom pad first) once per second.

1. **Dry baseline:** strip dry and mounted → note the highest raw value
   (`dry_max`). Seeed's reference: dry < 100, direct-wet ≈ 250.
2. **Mount test — through-wall:** tape the strip vertically on the *outside*
   of the reservoir, pads toward the wall, bottom pad level with the tank
   floor, pressed flat (an air gap kills capacitive coupling). Covered
   pads' raw values must jump; note the lowest covered-pad value
   (`wet_min`).
   - `wet_min` clearly above `dry_max` (≥ 50 apart) → through-wall works.
     Set the threshold midway.
   - Barely separated (wall too thick) → the fallback would be an in-tank
     waterproof sleeve, but with the probe handling alarms, prefer to
     **drop the strip** instead.
3. **Depth map:** verify pad count against the mechanical indicator at 2–3
   known fills and record below.
4. Copy the chosen threshold into `LEVEL_WET_THRESHOLD` in `bringup.ino`
   (and later the real firmware).

## Calibration

| Date | Mount (through-wall / dropped) | dry_max | wet_min | Threshold | Pads↔depth check |
|---|---|---|---|---|---|
| _(pending)_ | | | | | |
