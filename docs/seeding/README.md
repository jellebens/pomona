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
| Strawberry (alpine 'Rote Baron Solemacher') | [strawberry.md](strawberry.md) | first to sow — seed on hand |
| Peppers (paprika, Dulce Italiano, chili) | _to write_ | batch 1 |
| Parsley | _to write_ | batch 1 |
| Tomato (tros) — seed on hand | _to write_ | batch 2 |
| Basil | _to write_ | batch 2 |
| Dill | _to write_ | batch 2 |
| Lettuce — seed on hand | _to write_ | batch 2 — keep tray cool, germinates poorly above ~24 °C |

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
2. **Damp, not saturated.** A few mm of water in the tray only; waterlogged
   sponges rot seed and invite damping-off.
3. **Sow** into the pre-made hole for normal seed — **except light
   germinators, which go on the surface uncovered** (see
   [strawberry.md](strawberry.md)). Dome until sprouted, then give light.
4. Keep exposed sponge tops shaded once under light; permanently wet
   surfaces grow algae.
5. At first true leaves, tray-feed **quarter-strength A+B, pH ~5.8**. Demin
   water has no buffering, so the nutrient solution holds the pH.
6. **Transplant gate:** roots through the sponge + true leaves (2 for most
   crops, 3–4 for strawberry). Move the **whole sponge** into the net pod;
   it must not sit in standing water.

## Sowing order rationale

Sow slowest-first so everything reaches the tower together:

1. **Strawberry** — 14–30 d germination, 6–8 weeks to transplant size.
   Slowest by a wide margin now that it is seed-grown rather than plugs.
2. **Batch 1** (~02 Aug) — peppers, chili, parsley: 7–28 d.
3. **Batch 2** (~08–10 Aug) — tomato (tros), basil, dill, lettuce: 2–14 d.
   Lettuce is the fastest and turns over continuously afterwards — re-sow a
   couple of sponges every 2–3 weeks.

## Reservoir state gate

Nothing is transplanted while the reservoir holds plain demineralised water
(EC ~0, unbuffered). Dose A+B to **EC 0.8–1.0** for young transplants,
correct pH to **~6.0** with the PH-201H pen, then plant. Ramp to the shared
**EC 1.6–1.8** over 2–3 weeks as the fruiters go in.
