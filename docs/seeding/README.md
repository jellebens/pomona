# Seeding

Per-crop sowing and germination docs, plus the shared method. The 30-pod
target layout and nutrient targets live in
[../planting-plan.md](../planting-plan.md).

> **Start here: [poc.md](poc.md).** The full 30-pod sowing is deliberately on
> hold behind a **5-sponge proof of concept** (3 strawberry + 2 lettuce)
> until the seedling technique and the sensor/MQTT/Grafana stack are proven.

## Per-crop docs

| Crop | Doc | Sowing status |
|---|---|---|
| Strawberry (alpine 'Rote Baron Solemacher') | [strawberry.md](strawberry.md) | **in the PoC — 3 sponges now**, seed on hand |
| Lettuce — seed on hand | _to write_ | **in the PoC — 2 sponges now**; keep the tray cool, germinates poorly above ~24 °C |
| Peppers (paprika, Dulce Italiano, chili) | _to write_ | main sowing, batch 1 |
| Parsley | _to write_ | main sowing, batch 1 |
| Basil | _to write_ | main sowing, batch 2 |
| Dill | _to write_ | main sowing, batch 2 |

Full sowing schedule with dates and spare counts: Trello #219, comment
"Seedling plan (2026-07-29)" + checklist "Seedling schedule".

## Shared method — starter sponges

Starter medium is **grow sponges** for every crop (decided 2026-07-29).
Sponges are inert and pH-neutral, so unlike rockwool they need no acid
pre-conditioning — and their surface stays evenly damp, which is what
surface-sown light germinators need. Rockwool was considered and dropped:
it is alkaline (pH ~7.5–8, needs a pH 5.5 pre-soak), its top crusts dry, and
a wet lit rockwool surface algaes up.

**Sponge shape:** cross-cut into four wedges, gaping when dry and closing as
it swells when wet. Soak before sowing, then sow on the **wedge top faces**,
not down the slit — fine seed dropped into the cut is lost in the dark. The
same four wedges grip the seedling stem at transplant. See
[strawberry.md](strawberry.md) for the placement and pipette techniques.

1. **Prep:** dunk in plain demin water, squeeze under water a few times to
   expel air, let absorb fully, drain. No pH correction of the soak water.
2. **Damp, not saturated.** **2–3 mm of plain demin water** in the tray only,
   no nutrients yet; waterlogged sponges rot seed and invite damping-off.
3. **Sow** into the pre-made hole for normal seed — **except light
   germinators, which go on the surface uncovered** (see
   [strawberry.md](strawberry.md)).
4. **Dome for humidity, but do not use a dark cupboard.** Light germinators
   (strawberry, and lettuce prefers it) must have light on the seed from day
   one; only buried-seed crops are indifferent to it. Bright *indirect* light
   is the safe default for a shared tray — direct midday sun through glass
   cooks a domed tray.
5. Keep exposed sponge tops shaded once under light; permanently wet
   surfaces grow algae.
6. At first true leaves, tray-feed **quarter-strength A+B, pH ~5.8**. Demin
   water has no buffering, so the nutrient solution holds the pH.
7. **Transplant gate:** roots through the sponge + true leaves (2 for most
   crops, 3–4 for strawberry). Move the **whole sponge** into the net pod;
   it must not sit in standing water.

## Sowing order rationale

**0. The PoC comes first** — 3 strawberry + 2 lettuce, 5 sponges total, see
[poc.md](poc.md). Everything below is the main sowing, gated behind it.

Then sow slowest-first so everything reaches the tower together:

1. **Strawberry** — 14–30 d germination, 6–8 weeks to transplant size.
   Slowest by a wide margin now that it is seed-grown rather than plugs.
2. **Batch 1** — peppers, chili, parsley: 7–28 d. The doc's recommendation is
   to run this alongside the PoC rather than behind it: they are the
   longest-lead crops and their buried-seed technique carries little risk.
3. **Batch 2** — basil, dill, lettuce: 2–14 d.
   Lettuce is the fastest and turns over continuously afterwards — re-sow a
   couple of sponges every 2–3 weeks.

Batch dates are no longer fixed to the estimated early-August ones; they key
off the **actual PoC sow date**, recorded on Trello #219.

## Reservoir state gate

Nothing is transplanted while the reservoir holds plain demineralised water
(EC ~0, unbuffered). Dose A+B to **EC 0.8–1.0** for young transplants,
correct pH to **~6.0** with the PH-201H pen, then plant. Ramp to the shared
**EC 1.4–1.6** over 2–3 weeks as the fruiters go in.
