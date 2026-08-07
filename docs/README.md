# Pomona docs

Hydroponics project — a Hocomay 30-pod vertical aeroponic tower with an
Arduino GIGA unit reporting to the cluster (MQTT → InfluxDB → Grafana).
Monitoring-first; automated control is deferred.

## Contents

| Doc | What's in it |
|---|---|
| [design.md](design.md) | System choice, sensor loadout v1, monitoring architecture, open items |
| [wiring.md](wiring.md) | Sensor unit wiring (GIGA pin map, I²C addresses), first power-on checklist, calibration |
| [planting-plan.md](planting-plan.md) | The 30-pod tier layout, nutrient/pH targets, commissioning sequence, shopping list |
| [seeding/](seeding/README.md) | Sowing and germination — shared sponge method, sowing order, per-crop docs |
| [seeding/lettuce.md](seeding/lettuce.md) | Iceberg lettuce — germinated, seedling care, and why it reaches the tower first |
| [seeding/strawberry.md](seeding/strawberry.md) | Alpine strawberry 'Rote Baron Solemacher' from seed — light-germinator sowing, transplant, light deadline |
| [seeding/poc.md](seeding/poc.md) | The 5-sponge proof of concept — superseded for strawberry, exit criteria still live |

## Current state (2026-08-03)

- Tower assembled; reservoir holds **7.5 L plain demineralised water** — no
  nutrients yet, so EC ~0 and pH is unbuffered and meaningless to measure.
- **✅ Both crops germinated 2026-08-07**, from an early-August sowing —
  lettuce, and strawberry on **day 4** against a 14–30 day book figure (the
  28 °C ambient accelerated it). Strawberry is 12 sponges × ~5 seeds,
  superseding the 3-sponge PoC allocation.
- **Transplant windows: lettuce ≈21–28 Aug, strawberry ≈31 Aug – 14 Sept** —
  both about two weeks earlier than planned.
- **⚠ The critical path is now purchases, not plants.** pH-Down + EC
  calibration fluid in hand by **~14 Aug** (an undosed reservoir means nothing
  transplants), and the **grow light by end of August**, pulled forward from
  mid-September because seedlings that raced up in a hot room stretch just as
  fast in weak light. Both are #220 purchases still outstanding.
- Sensors not yet wired — wiring plan + bring-up sketch ready as of
  2026-08-07 ([wiring.md](wiring.md), [firmware/](../firmware/README.md));
  no landing zone or Argo app yet.

Work is tracked on Trello (label **pomona**): #218 hardware inventory,
#219 design/crops, #220 sensor plan/BOM, #221 gitops landing zone,
#222 telemetry + first dashboard, #223 on-screen graphs, #224 dosing pumps.
