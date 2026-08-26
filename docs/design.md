# Pomona design — system choice, sensors, monitoring architecture

Status: owner-decided 2026-07-27/28 (Trello #219, #220). Monitoring-first;
no automated control in v1.

## System choice

- **Grow system:** Hocomay FHX0046-01 vertical aeroponic tower — 30 pods /
  6 tiers, top-down drip, 10 L reservoir, 25×25×86 cm, submersible pump on a
  dumb power pack, mechanical water-level indicator.
- **Unit at the tower:** a self-contained **Arduino GIGA R1 WiFi + GIGA
  Display Shield** (3.97" 800×480 touch, LVGL). WiFi is onboard, so the unit
  publishes MQTT **directly** to the existing cluster broker — no ESP32
  bridge. The ESP32 stash is spares / future satellite nodes.
- **Screen v1:** current readings only. On-screen history graphs are
  follow-up #223 (feasible on this display).
- **Control:** explicitly deferred. Pump stays on its power pack; peristaltic
  dosing (auto pH + nutrients) is phase 2, card #224.

## Sensor loadout v1 (settled 2026-07-28, detail in #220)

| Measurement | Sensor | Notes |
|---|---|---|
| EC / nutrients | Grove TDS | analog, 3.3 V |
| Water temperature | DS18B20 | on hand |
| Water level (low-water alarm) | CQRobot CQRSENYW003 contact photoelectric probe (on hand, added 2026-08-25) | 4 points over ~3 cm at pump-intake height; frequency output on D3 |
| Water level (coarse range, optional) | Grove 10 cm I²C strip | mounted OUTSIDE, through-wall; ⚠ occupies I²C 0x77+0x78 → strap the BME280 to 0x76 |
| Air temp/humidity/pressure | BME280 STEMMA QT | address 0x76 (see above) |
| Light | BH1750 | I²C |
| Pump power | Fibaro Wall Plug Type E | via Home Assistant zwave_js (to buy) |
| pH (continuous) | DFRobot Gravity pH Pro SEN0169-V2 | industrial probe, stays in the tank; needs the Gravity **signal isolator** to avoid a ground loop with the TDS probe |
| pH (calibration cross-check) | PH-201H pen | handheld |

Sensors sourced from Antratek. A+B nutrients on hand. Still to buy:
pH-Down/Up, seeds (see [planting-plan.md](planting-plan.md)), EC calibration
fluid 1413 µS/cm, grow light (TBD).

## Monitoring architecture

```
sensors ─→ GIGA R1 WiFi ─→ MQTT (cluster broker) ─→ InfluxDB ─→ Grafana
              └─→ LVGL touchscreen (current readings)
```

- Unit-side wiring rules + pin map: [wiring.md](wiring.md); per-sensor
  docs (wiring, calibration, assumptions): [sensors/](sensors/README.md);
  bring-up firmware in [`firmware/`](../firmware/README.md).
- Cluster side follows the **zeus pattern**: MQTT → InfluxDB → Grafana.
- MQTT topic schema, Influx ingestion and the first Grafana dashboard are
  card #222.
- gitops landing zone `landingzones/pomona` is card #221.
- Pump watts arrive via HA zwave_js (Fibaro plug), not via the GIGA.

## Open items

- #218 hardware inventory (what's already lying around)
- Grow light selection
- MQTT topic schema (#222)
