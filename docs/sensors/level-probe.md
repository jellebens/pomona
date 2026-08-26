# Water level — CQRobot CQRSENYW003 photoelectric probe

**Status: ✅ wired, dip-tested and calibrated (2026-08-25, #242).**

Product: [cqrobot.com/product/CQRSENYW003](https://cqrobot.com/product/CQRSENYW003) —
contact multi-point photoelectric probe, 4 optical detection points over
~3 cm of probe height, ±1 mm, designed for immersion (no waterproofing
needed). Reports the highest wet point as a frequency on the green wire:
~20 Hz dry, ~50/100/200/400 Hz for points 1–4.

Driver: [`firmware/libraries/PhotoLevelProbe`](../../firmware/libraries/PhotoLevelProbe/src/PhotoLevelProbe.h) ·
test sketch: [`firmware/levelprobe/`](../../firmware/levelprobe/levelprobe.ino).

## Wiring

black → GND · red → 3V3 · green → **D3**. Open collector; the pin uses the
GIGA's internal pull-up, no external resistor. Draws up to **~80 mA at
3.3 V**. The stock silicone cable is only **21 cm** — extend all three wires
(soldered + heat-shrunk, splice kept OUTSIDE the tank) to reach the GIGA.

## Bench verification

Flash `levelprobe`: dry ≈ 20 Hz / 0 pts, then dip step by step in a glass —
each point should add cleanly (50→100→200→400 Hz). Verified 2026-08-25:
all 4 points step cleanly; dry = 20.2 Hz.

## Mounting

Bolt it vertically to the tank wall through its 3 mm holes (nylon M3
screws, or zip-tie to a bracket over the rim). Two sensible heights:
**pump-intake** (point 1 just above the intake ⇒ 0/4 = stop the pump /
refill NOW) or **top-up gauge** near the full line (0/4 = below the band,
time to refill). Keep the optical tips out of the pump's outflow
turbulence and reachable for an occasional wipe — algae/biofilm on the
tips is the long-term failure mode.

**As built (2026-08-25): top-up gauge.** Point 1 wets at **8.2 L**, so the
4-point band covers roughly 8.2 L → full and **0/4 means "below 8.2 L —
top up"**. ⚠ There is NO dry-run protection below that line — the probe
can't tell 8 L from empty (see assumption A3). The probe replaces the
[Grove strip](level-strip.md) as the alarm instrument.

## Calibration

| Date | Height of point 1 above tank floor | Points→litres (4/3/2/1) | Dry Hz | Notes |
|---|---|---|---|---|
| 2026-08-25 | ≈ the 8.2 L waterline (top-up mount) | 1 → **8.2 L** · 2 → ≈8.9 L (interpolated) · 3 → **9.7 L** · 4 → ≈10.4 L (extrapolated — above nominal 10 L full, may never wet) | 20.2 | Dip test: all 4 points step cleanly (≈50/100/200/400 Hz). Live in tank: 50.5 Hz = 1/4 at just-topped-up level. Spacing ≈ 0.75 L/point. |

**Reading the ladder** (what firmware, dashboard and alerts in #222 should
show — note point 4 sits above nominal full, so 3/4 already means full):

| Points wet | Tank | Action |
|---|---|---|
| 3–4 | full (≥ 9.7 L) | — |
| 2 | ok (≈ 8.9 L) | — |
| 1 | ≈ 8.2 L | top up soon |
| 0 | below 8.2 L | **refill now** (probe is blind below this line) |

## Assumptions

Move an entry to **verified/retired** rather than deleting it, so the
reasoning stays traceable.

### A1 — Points are linearly spaced in volume (~0.75 L/point)

- **Status:** open (2026-08-25)
- **Assumption:** the four points map to tank volume at equal ~0.75 L
  steps, so the two *unmeasured* points can be derived: point 2 ≈ 8.9 L
  (interpolated), point 4 ≈ 10.4 L (extrapolated).
- **Measured basis:** point 1 = 8.2 L and point 3 = 9.7 L (observed);
  evenly spaced tips; reservoir cross-section treated as constant between
  those waterlines.
- **If wrong:** the "2 points = ok" band and the point-4 note shift by up
  to a few tenths of a litre — display labels and #222 alert copy would
  mislabel the level, but the hard alarm (0 points = below the *measured*
  8.2 L) is unaffected.
- **To verify:** when the tank sits exactly at the 1→2 or 3→4 transition,
  note the volume from the mechanical indicator and update the table.

### A2 — Point 4 never triggers in normal use

- **Status:** open (2026-08-25)
- **Assumption:** point 4's derived waterline (≈10.4 L) is above the
  nominal 10 L fill, so **3/4 points is the "full" reading**; a never-seen
  point 4 is expected, not a fault.
- **If wrong** (point 4 wets on a generous top-up): harmless — record the
  volume, it verifies A1 for free.

### A3 — Top-up mount without dry-run protection is acceptable

- **Status:** accepted by owner (2026-08-25)
- **Assumption:** the top-up mount (blind below 8.2 L) is enough level
  protection, because the reservoir is topped up promptly when the
  0-points alert fires. There is deliberately **no sensor between 8.2 L
  and the pump running dry.**
- **Rests on:** the owner acting on refill alerts within roughly a day —
  tower evaporation/uptake is far below ~8 L/day — and, later, the
  [Fibaro plug's power reading](pump-power.md) catching a dry-running pump
  as a second line of defence.
- **If wrong:** an unnoticed leak or long absence could run the pump dry
  with no alert between "8.2 L" and "damage".
- **Escape hatch:** remount at pump-intake height (needs the cable
  extension) — wiring unchanged, only the points→litres table would need
  re-measuring.

### A4 — The mechanical indicator is the volume reference

- **Status:** open (2026-08-25)
- **Assumption:** the 8.2 L / 9.7 L figures are read off the tower's
  mechanical water-level indicator and taken at face value; its own
  accuracy is unverified.
- **If wrong:** all points→litres numbers shift *consistently* by the
  indicator's error — ladder order and alert logic unaffected, absolute
  litres approximate.
- **To verify:** once, fill with a measuring jug (e.g. 2 L steps) and
  compare against the indicator; note the offset here.
