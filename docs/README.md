# Pomona docs

Hydroponics project — a Hocomay 30-pod vertical aeroponic tower with an
Arduino GIGA unit reporting to the cluster (MQTT → InfluxDB → Grafana).
Monitoring-first; automated control is deferred.

## Contents

| Doc | What's in it |
|---|---|
| [design.md](design.md) | System choice, sensor loadout v1, monitoring architecture, open items |
| [planting-plan.md](planting-plan.md) | The 30-pod tier layout, nutrient/pH targets, commissioning sequence, shopping list |
| [seeding/](seeding/README.md) | Sowing and germination — shared sponge method, sowing order, per-crop docs |
| [seeding/lettuce.md](seeding/lettuce.md) | Iceberg lettuce — germinated, seedling care, and why it reaches the tower first |
| [seeding/strawberry.md](seeding/strawberry.md) | Alpine strawberry 'Rote Baron Solemacher' from seed — light-germinator sowing, transplant, light deadline |
| [seeding/poc.md](seeding/poc.md) | The 5-sponge proof of concept — superseded for strawberry, exit criteria still live |

## Current state (2026-08-03)

- Tower assembled; reservoir holds **7.5 L plain demineralised water** — no
  nutrients yet, so EC ~0 and pH is unbuffered and meaningless to measure.
- **Strawberry sown 2026-08-03** — 12 sponges, ~5 seeds each, superseding the
  3-sponge PoC allocation. Germination expected 17 Aug – 2 Sept, transplant window
  ≈14–28 Sept. See [seeding/strawberry.md](seeding/strawberry.md).
- **✅ Lettuce germinated 2026-08-03** — the first crop up, and it transplants
  **≈17–24 Aug**, about a month ahead of the strawberries. See
  [seeding/lettuce.md](seeding/lettuce.md).
- **⚠ Most time-critical item in the project:** pH-Down + EC calibration fluid
  in hand by **~14 Aug** — pulled forward from ~7 Sept because the lettuce now
  reaches the tower first, and an undosed reservoir means no transplant. The
  grow light is still needed by **mid-Sept** for the strawberries, though the
  sprouted lettuce wants strong light immediately. Both are #220 purchases
  still outstanding.
- Sensors not yet wired; no landing zone or Argo app yet.

Work is tracked on Trello (label **pomona**): #218 hardware inventory,
#219 design/crops, #220 sensor plan/BOM, #221 gitops landing zone,
#222 telemetry + first dashboard, #223 on-screen graphs, #224 dosing pumps.
