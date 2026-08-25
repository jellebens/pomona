# Assumptions

Working assumptions the build relies on but hasn't (fully) verified —
what each one rests on, what breaks if it's wrong, and how to close it.
One entry per assumption; move an entry to **Status: verified/retired**
rather than deleting it, so the reasoning stays traceable.

## A1 — Level probe points are linearly spaced in volume (~0.75 L/point)

- **Status:** open (2026-08-25)
- **Assumption:** the four detection points of the CQRSENYW003 map to tank
  volume at equal ~0.75 L steps, so the two *unmeasured* points can be
  derived: point 2 ≈ 8.9 L (interpolated), point 4 ≈ 10.4 L (extrapolated).
- **Measured basis:** point 1 = 8.2 L and point 3 = 9.7 L (observed
  2026-08-25); the probe's tips are physically evenly spaced, and the
  Hocomay reservoir's cross-section is treated as constant between those
  waterlines.
- **If wrong:** the "2 points = ok" band and the point-4 note in
  [wiring.md](wiring.md) shift by up to a few tenths of a litre — display
  labels and the #222 alert copy would mislabel the level, but the hard
  alarm (0 points = below the *measured* 8.2 L) is unaffected.
- **To verify:** when the tank happens to sit exactly at the 1→2 or 3→4
  transition, note the volume from the mechanical indicator and update the
  calibration table in [wiring.md](wiring.md).

## A2 — Point 4 never triggers in normal use

- **Status:** open (2026-08-25)
- **Assumption:** point 4's derived waterline (≈10.4 L) is above the
  nominal 10 L fill, so **3/4 points is the "full" reading**; a
  never-seen point 4 is expected, not a fault.
- **Measured basis:** follows from A1's extrapolation; not directly
  observed.
- **If wrong** (i.e. point 4 does wet on a generous top-up): harmless —
  it just means the tank was overfilled past nominal or A1's spacing is
  off; record the volume, it verifies A1 for free.
- **To verify:** same observation as A1's 3→4 transition.

## A3 — Top-up mount without dry-run protection is acceptable

- **Status:** accepted by owner (2026-08-25)
- **Assumption:** the probe mounted as a top-up gauge (blind below 8.2 L)
  is enough level protection, because the reservoir is topped up promptly
  when the 0-points refill alert fires. There is deliberately **no sensor
  between 8.2 L and the pump running dry.**
- **Rests on:** the owner acting on refill alerts within roughly a day —
  tower evaporation/uptake is far below ~8 L/day — and, later, the Fibaro
  plug's power reading (via Home Assistant) catching a dry-running pump
  as a second line of defence.
- **If wrong:** an unnoticed leak or long absence could run the pump dry
  with no alert between "8.2 L" and "damage".
- **Escape hatch:** remount the probe at pump-intake height (needs the
  3-wire cable extension, splice outside the tank) — procedure and pin
  map unchanged, only the points→litres table would need re-measuring.

## A4 — The mechanical indicator is the volume reference

- **Status:** open (2026-08-25)
- **Assumption:** the 8.2 L / 9.7 L calibration figures are read off the
  tower's mechanical water-level indicator and taken at face value; its
  own accuracy is unverified.
- **If wrong:** all points→litres numbers shift by the indicator's error
  *consistently* — relative behaviour (ladder order, alert logic) is
  unaffected, absolute litres are approximate.
- **To verify:** once, fill with a measuring jug (e.g. 2 L steps) and
  compare against the indicator; note the offset here.
