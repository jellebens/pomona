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

## Calibration (on the tower) — a measured fill table

The tank **tapers** (narrower at the bottom): a litre is ~12 mm of height near
the top and ~25 mm near the floor, so two points and a straight line are wrong.
Distance → litres is a table, `TANK_TABLE` in
`firmware/libraries/PomonaCalibration/src/PomonaCalibration.h`, interpolated
linearly between points and clamped at both ends (`libraries/A02YYUW/src/TankTable.h`).
The node publishes `level_pct` (of `TANK_FULL_L`) and `volume_l`, and the tile
shows litres, green ≥ 60 %, amber ≥ 30 %, red below.

To (re)measure it — after moving the sensor or changing the tank:

1. **Pump OFF** (`vertumnusctl.sh pomona-0001 pump off`) and keep it off. A
   Vertumnus restart puts it back on `auto`, so hold any ceres release meanwhile.
2. Drain the tank to a puddle; note the reading — that is 0 L.
3. Pour in **known volumes** — 0.5 L steps near the bottom, 1 L above — and
   after each wait for two equal `level_distance_mm` readings (≈ 1 min).
4. Put the pairs in `TANK_TABLE`, fullest first; set `TANK_FULL_L` to the
   volume you call full. Rebuild, roll out, put the pump back on `auto`.

Full is **9.5 L**, not 10 L: 10 L would sit ~40 mm from the face, too close to
the 30 mm blind zone. Annona's `reservoir_l` is 9.5 to match (config v14).

### Recorded table — 2026-10-09 21:56–22:14 CEST

| L in tank | 0 | 0.5 | 1.0 | 1.5 | 2.0 | 2.5 | 3.0 | 3.5 | 4.0 | 4.5 | 5.5 | 6.5 | 9.5 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| mm | 248 | 227 | 215 | 201 | 185 | 173 | 156 | 135 | 121 | 113 | 92 | 80 | 46 |

6.5 → 9.5 L was a single 3 L pour, so the top of the curve is one straight
segment. Sensor face → floor ≈ 248–260 mm.

## The double echo

Over a flat surface the pulse can bounce water → sensor face → water and come
back at **twice the distance**: on the tower 320–355 mm for a 160–175 mm
surface, often enough to win a plain median (2.4.0 then read a well-filled tank
as nearly empty). Since 2.4.1 the driver drops every frame within 10 % of twice
another frame in its 15-frame window. Damping the mount helps too: a flat lid
right around the face reflects best — mount the face flush, or ring it with a
little foam or felt.
