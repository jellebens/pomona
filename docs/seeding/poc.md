# Seeding PoC — 5 sponges

> **Status: SUPERSEDED for strawberry (2026-08-03).** The owner sowed the full
> **12 sponges × ~5 seeds** the same day, not the 3 this PoC allotted, so the
> "prove the technique on 15 seeds" hedge no longer applies — see
> [strawberry.md](strawberry.md) for the as-built sowing and its dated
> timeline.
>
> **What still stands:** the hardware/software exit criteria below, which now
> carry a hard date. The seedlings will be transplant-ready ≈14–28 September, so
> the sensor/telemetry work and the pH-Down purchase have to land before then
> rather than whenever the PoC happened to finish. The 2 lettuce sponges are
> also still worth sowing as the fast early-warning signal on tray conditions.

Original rationale, kept for the record. **Sow a 5-sponge proof of concept
first**, and hold the full 30-pod sowing until the seedling technique *and*
the hardware/software stack are nailed down.

Rationale: the tower is not instrumented yet (#218/#220 pending), nothing is
dosed, there is no grow light, and the hardest crop in the plan is a
surface-sown light germinator whose technique has never been tried here.
Committing the whole seed packet and all 30 pods to an unproven system risks
losing a full generation. Five sponges cost ~25 seeds and prove the loop
end to end.

## What to sow

| Sponges | Crop | Why it's in the PoC |
|---|---|---|
| 3 | Strawberry (alpine 'Rote Baron Solemacher') | The **hard case** — surface-sown light germinator, 14–30 d. If the technique is wrong, we learn it on 15 seeds instead of 60. Also the slowest crop, so starting it now costs nothing. |
| 2 | Lettuce | The **fast validator** — up in 2–7 d, so the sponge/dome/tray routine is confirmed within a week instead of three, and it gives a genuine edible harvest ~5–6 weeks in, proving the whole chain. Basil is an equally good substitute. |

~5 seeds per sponge (see [strawberry.md](strawberry.md) for placement on the
quadrant faces and the pipette trick). **All remaining seed goes to the
fridge**, damp in a sealed bag — cold stratification for 2–4 weeks costs
nothing and improves the later full sowing.

**Tray temperature: 20–22 °C.** This is the overlap window: strawberry wants
>20 °C, lettuce germinates poorly above ~24 °C. Do not put this tray
anywhere that bakes.

**Tray water: 2–3 mm of plain demin water, no nutrients.** Feeding starts at
the first true leaves, not before — see [strawberry.md](strawberry.md).

**⚠ Not in the dark.** Both crops in this PoC want light on the seed
(strawberry is a Lichtkeimer, lettuce germinates better lit), so the usual
warm-dark-cupboard habit is exactly wrong here. Domed, warm, and in bright
indirect light.

## Exit criteria — what "nailed down" means

Sow the full plan only when **all** of these hold.

### Seedling technique

- [ ] ≥2 of the 3 strawberry sponges germinate within 30 days → surface-sowing
      technique is validated
- [ ] Lettuce up within 7 days, true leaves by ~2 weeks
- [ ] No damping-off, mould, or algae takeover in the tray
- [ ] Seedlings are **not leggy** — this is the direct read on whether
      available light is sufficient, i.e. how urgently the grow light is needed

### Hardware / software (cards #218, #220, #222)

- [ ] #218 hardware inventory closed
- [ ] #220 sensors wired and publishing: EC, pH, water temp, level, air, lux
- [ ] #222 MQTT → InfluxDB → Grafana: readings land, persist, and render
- [ ] GIGA screen shows current readings without wedging over a multi-day run
- [ ] pH/EC hand-dosing loop exercised at least once: dose A+B to EC 0.8–1.0,
      correct pH to ~6.0, and confirm the **SEN0169-V2 probe agrees with the
      PH-201H pen** (that cross-check is the whole reason the pen exists)
- [ ] Reservoir top-up rate observed at low plant load, as the baseline for
      what a full tower will do

### Transplanting

Five plants on 7.5 L is a very forgiving load — ideal for shaking out drip
coverage on every tier, crown-above-medium placement, and sensor baselines
with real roots in the water before 30 plants depend on it.

## Cost of the PoC — read this before serialising everything

Waiting for the PoC to pass pushes the full sowing to roughly **early
September**, and the peppers are what pay for that: paprika, Dulce Italiano
and chili germinate in 7–21 days and then have a long juvenile phase, so
sown in September under lights they realistically fruit in late winter or
spring rather than autumn.

Two ways to handle it:

- **Recommended — don't hold the peppers hostage.** Sow peppers and parsley
  on the original ~02 Aug schedule alongside the PoC. Their technique is
  low-risk (buried seed, well understood) and they are the long-lead crops;
  the PoC's real purpose is proving the *system* and the *strawberry*
  technique, neither of which the peppers depend on.
- **Strict serial.** Everything waits for the PoC. Simpler and cheaper in
  seed, but accept a spring fruit harvest for the peppers (there is no tomato
  in the plan — it was dropped 2026-08-03).

Either way the **grow light deadline of mid-September stands** — the PoC
seedlings hit their growth phase exactly as Belgian daylight collapses.

## Timeline

| When | Event |
|---|---|
| Sow day (record it) | 5 sponges: 3 strawberry + 2 lettuce; rest of seed to the fridge |
| +2–7 d | Lettuce up → sponge/dome/tray routine confirmed |
| +14–30 d | Strawberry up → surface-sowing technique confirmed |
| ~+3 weeks | Lettuce transplants into the tower; first real EC/pH loop with roots in the water |
| ~+5–6 weeks | First lettuce harvest — full chain proven, plant to dashboard |
| ~+6–8 weeks | Strawberry transplants (3–4 true leaves) |
| Decision point | Exit criteria reviewed → full 30-pod sowing |

Record the actual sow date on Trello #219 so the batch dates key off reality
rather than an estimate. *(Done: strawberry went in 2026-08-03.)*
