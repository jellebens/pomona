# Pomona 🍓

Hydroponics project — growing fruits, vegetables and herbs on a small indoor
tower, with cluster-grade monitoring. Named after the Roman goddess of fruit
and orchards. Started 2026-07-27.

**Monitoring-first:** v1 only measures and displays. Automated control
(pump/dosing/lights) is explicitly deferred — the pump stays on its dumb power
pack. Auto pH/nutrient dosing is phase 2 (card #224).

## Hardware

| Part | Detail |
|---|---|
| Tower | Hocomay FHX0046-01 (Amazon B0FFFWBSS7) — 30 pods / 6 tiers, aeroponic top-down drip, 10 L reservoir, 25×25×86 cm |
| Pump | Submersible, on its own power pack (no controller) |
| Controller | Arduino GIGA R1 WiFi + GIGA Display Shield (3.97" 800×480 touch, LVGL) |
| Pump power metering | Fibaro Wall Plug Type E via Home Assistant zwave_js |

The GIGA unit is self-contained at the tower: onboard WiFi speaks MQTT
**directly** to the existing cluster broker — no ESP32 bridge (the ESP32 stash
is spares / future satellite nodes).

## Architecture

```
sensors ─→ GIGA R1 WiFi ─→ MQTT (cluster broker) ─→ InfluxDB ─→ Grafana
              │
              └─→ on-device touchscreen (LVGL): current readings (v1),
                  history graphs are follow-up #223
```

Cluster side follows the zeus pattern; the gitops landing zone is
`landingzones/pomona` (card #221, pending).

## Documentation

Start at the docs index: **[docs/README.md](docs/README.md)**.

- [docs/design.md](docs/design.md) — system choice, sensor loadout v1, monitoring architecture
- [docs/planting-plan.md](docs/planting-plan.md) — the 30-pod planting layout, EC/pH targets, commissioning steps
- [docs/seeding/](docs/seeding/README.md) — sowing & germination, per crop

## Status / roadmap (Trello cards)

- [x] #219 crops + planting layout decided (2026-07-29, see planting plan)
- [ ] #218 hardware inventory
- [ ] #220 sensor plan / BOM (loadout settled, purchases pending)
- [ ] #221 gitops landing zone skeleton
- [ ] #222 MQTT topics + Influx ingestion + first Grafana dashboard
- [ ] #223 history graphs on the unit screen (follow-up)
- [ ] #224 phase 2 — peristaltic dosing pumps (auto pH + nutrients)

## Repo layout

- `docs/` — design + operations documentation
- `firmware/` — Arduino GIGA firmware (future)

## Branching

GitFlow, same as jupiter/gitops: trunk is `master`, work branches off
`develop` and merges back via PR; releases are user-commanded
`develop` → `master` PRs.
