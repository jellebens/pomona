# A02YYUW ultrasonic — tank level (Trello #270 / #283)

DFRobot SEN0311 (A02YYUW): a sealed ultrasonic distance sensor. Mounted
face-down above the reservoir, it measures the distance to the water surface;
the firmware turns that into litres. It is the **continuous** level gauge:
the CQRSENYW003 probe ([level-probe.md](level-probe.md)) stays the top-up
ladder and the pump interlock — nothing acts on the ultrasonic yet.

| | |
|---|---|
| Range | 30–4500 mm (blind below 30 mm: mount the face ≥ 3 cm above the FULL line) |
| Supply | 3.3–5 V — **use 3V3** (its TX swings to supply; the GIGA is not 5 V tolerant) |
| Output | UART 9600 8N1, frames `0xFF H L SUM`, distance = `H*256+L` mm, `SUM = (0xFF+H+L) & 0xFF` |
| Beam | ~ 40–60° cone: keep the tower column, the pump and the probe out of it |
| Firmware | `firmware/libraries/A02YYUW`, read in `src/sensors` since 2.4.0 |

## Wiring

| Sensor wire | GIGA |
|---|---|
| red (VCC) | **3V3** |
| black (GND) | GND |
| white (TX) | **D19 / RX1** (`Serial2`) |
| yellow (RX, mode) | **not connected** — floating = "processed value" output, the sensor streams on its own |

D0/D1 (`Serial1`) is not an option: the DS18B20 lives on D1. Don't confuse
the yellow wire with the DFR0523 pump pigtails ([../dosing/dfr0523.md](../dosing/dfr0523.md)).

## Bench check

After flashing, the tank tile shows the raw distance dimmed (`212mm`) and the
node publishes `ceres/pomona-0001/tele/water/level_distance_mm` every 30 s.
Hold a flat object at a measured distance: the value should agree to ±1 cm.
`--` on the tile = no valid frame for 3 s → check TX on D19 and 3V3/GND.

## Calibration (on the tower)

Two readings, **pump OFF and the water settled** (≥ 5 min; the tower holds
water in transit while it runs):

1. **FULL** — reservoir at its nominal `reservoir_l` (10 L). Read the
   `level_distance_mm` (tile or MQTT, take the value that repeats) →
   `TANK_DIST_FULL_MM`.
2. **EMPTY** — the reservoir floor (or the lowest level the pump still draws
   from, if that is the useful zero). Either drain it once, or measure the
   depth of the full water column with a ruler and add it to the FULL reading
   → `TANK_DIST_EMPTY_MM`.

Record both in `firmware/libraries/PomonaCalibration/src/PomonaCalibration.h`
with the date, rebuild and roll out. From then on the node publishes
`level_pct` and `volume_l`, and the tile shows litres, green ≥ 60 %, amber
≥ 30 %, red below.

Assumption: litres are linear between EMPTY and FULL — straight reservoir
walls. If the reservoir tapers, add a mid-point and revisit.

### Recorded values

| Date | FULL mm | EMPTY mm | Notes |
|---|---|---|---|
| — | — | — | not yet calibrated |
