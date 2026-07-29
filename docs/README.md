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
| [seeding/poc.md](seeding/poc.md) | **Start here** — the 5-sponge proof of concept and the exit criteria gating full sowing |
| [seeding/strawberry.md](seeding/strawberry.md) | Alpine strawberry 'Rote Baron Solemacher' from seed — light-germinator sowing, transplant, light deadline |

## Current state (2026-07-29)

- Tower assembled; reservoir holds **7.5 L plain demineralised water** — no
  nutrients yet, so EC ~0 and pH is unbuffered and meaningless to measure.
- **Nothing is planted.** First sowing is the **5-sponge PoC** (3 strawberry
  + 2 lettuce) — full 30-pod sowing is gated behind it, see
  [seeding/poc.md](seeding/poc.md).
- Sensors not yet wired; no landing zone or Argo app yet.

Work is tracked on Trello (label **pomona**): #218 hardware inventory,
#219 design/crops, #220 sensor plan/BOM, #221 gitops landing zone,
#222 telemetry + first dashboard, #223 on-screen graphs, #224 dosing pumps.
