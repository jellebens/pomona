# Water level — CQRobot CQRSENYW003 photoelectric probe

**Status: ✅ wired, dip-tested and calibrated (2026-08-25, #242).**
**Mount is permanent (2026-08-31):** remounting at pump-intake height was
ruled out — the tank geometry does not allow it — so this stays a **top-up
gauge**, blind below 8.2 L. A raw 0 while the pump runs is not trustworthy;
see [the settle check](#reading-the-probe-while-the-pump-runs--the-settle-check).

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

⚠ **Do not trust a 0 read while the pump is running** — see the settle check
below. Dashboards and alerts should either debounce it or key off the settled
verdict, not the raw ladder.

## Reading the probe while the pump runs — the settle check

**A raw 0 during a pump cycle does not even mean "below 8.2 L".**

The tower holds a real volume of water **in transit** whenever the pump is on —
in the riser, the drip line and across the six tiers. The reservoir therefore
sits visibly lower mid-cycle than the total water in the system justifies, and
turbulence at the optical tips adds flicker on top of that. A probe mounted at
the 8.2 L waterline will read 0 during a cycle at fills that are perfectly
healthy once everything drains back.

That is why the raw ladder is unusable as a pump interlock, and it is a
property of the *tower*, not a fault of the probe — any future consumer
(dashboard tiles, #222 alerts, dosing logic) has to account for it.

### The compensating control

Implemented in Home Assistant — `packages/pomona_telemetry.yaml` and the
`pomona-schedule.md` runbook in the `home-assitant` repo. On a raw 0 sustained
for 2 minutes:

1. **stop the pump**, so the tower drains back and the surface stills;
2. **wait 5 minutes** — longer than the drain-back, costing at most one skipped
   15-minute irrigation cycle;
3. **re-read and decide:**

| Settled reading | Verdict |
|---|---|
| ≥ 1 point | Water was in transit. False alarm — resume, no alert |
| Still 0 | Genuinely below the 8.2 L line. Notify to top up, set the confirmed-low flag |
| Unknown / unavailable | Unknown is not empty. Resume, change nothing |

Rate-limited to one check per hour so it cannot thrash the pump. The 24 h pump
inhibit is keyed off the **settled verdict**, never the raw ladder.

### What it does and does not buy

- **Does:** removes the false positives that made the low signal unusable, and
  stops the pump promptly once a low reading is confirmed.
- **Does not:** detect an empty tank. Nothing mounted at the 8.2 L line can see
  below it, and no amount of software changes that. The reservoir still has to
  be topped up by a human, and the mechanical indicator on the tower remains
  the quickest way to check it.

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

- **Status:** ⚠ **still accepted, but its escape hatch is CLOSED (2026-08-31).**
  The probe **cannot** be remounted at pump-intake height — the tank geometry
  does not allow it. So "top-up mount without dry-run protection" is no longer
  a choice among options; it is the permanent shape of this instrument. The
  compensating control is the [settle check](#reading-the-probe-while-the-pump-runs--the-settle-check)
  in Home Assistant, which cannot see an empty tank either but does make the
  one available low signal trustworthy.
- **Newly relevant (2026-08-31):** the tower is now planted and the pump runs
  on an unattended HA schedule, so a dry-run can happen at 03:00 with nobody
  watching. That raised the stakes on this assumption without changing the
  hardware options.
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
- ~~**Escape hatch:** remount at pump-intake height~~ — **ruled out
  2026-08-31, the tank geometry does not allow it.** If dry-run protection is
  ever genuinely wanted it needs *added hardware*, not a different mounting
  height: a second probe low on another digital pin (the four points span only
  ~3 cm, so one probe cannot be both a top-up gauge and an intake cutoff), or
  the [Fibaro plug's power reading](pump-power.md) used actively — a pump
  running dry draws measurably less than one moving water, which is a genuine
  second signal that needs no new sensor in the tank.

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
