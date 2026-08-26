# MQTT — broker & topic schema

Status: **proposed** with firmware v1 (Trello #229); to be **finalized with
#222** (Influx ingestion + first Grafana dashboard). Until #222 lands there
is no cluster-side consumer — the unit publishes, nothing ingests yet.

## Broker

The in-cluster **EMQX** cluster: **`mqtt.lab.local:1883`** — the same broker
zeus/jupiter use (the `mqtt.lab.local` A record is served by the
authoritative `lab.local` zone in the gitops repo). Per-app credentials
follow the existing pattern (`zeus-mqtt` → user **`pomona`**); creating the
account on the broker is #221/#222 territory. On the unit the credentials
live in the gitignored `secrets.h`
([ota-and-secrets.md](ota-and-secrets.md), Layer 1).

## Topic schema — `pomona/<zone>/<metric>`

Zones: `water` (reservoir), `air` (ambient at the tower), `unit` (the GIGA
itself). Payloads are **plain numeric text** (Influx-friendly), one metric
per topic. A metric whose sensor did not answer is simply **not published**
— consumers read availability from `pomona/unit/sensors` instead of
parsing sentinels.

| Topic | Payload | Retained | Published |
|---|---|---|---|
| `pomona/water/temp_c` | °C, 1 decimal | no | DS18B20 answered |
| `pomona/water/ec_ms_cm` | mS/cm, 2 decimals | no | always (analog — no absence detection; ~0 when unplugged) |
| `pomona/water/ph` | pH, 2 decimals | no | only once pH is calibrated (PomonaCalibration) |
| `pomona/water/level_pct` | 0–100 | no | Grove strip answered (optional hardware) |
| `pomona/water/level_points` | 0–4 (CQRSENYW003 ladder) | no | probe signal present |
| `pomona/air/temp_c` | °C, 1 decimal | no | BME280 answered |
| `pomona/air/humidity_pct` | %RH, 1 decimal | no | BME280 answered |
| `pomona/air/pressure_hpa` | hPa, 1 decimal | no | BME280 answered |
| `pomona/air/lux` | lux, integer | no | BH1750 answered |
| `pomona/unit/status` | `online` / `offline` | **yes** | on connect; `offline` is the broker **LWT** |
| `pomona/unit/fw_version` | semver (PomonaVersion) | **yes** | on connect |
| `pomona/unit/sensors` | JSON availability map, e.g. `{"water_temp":true,"ph_calibrated":false,…}` | **yes** | every publish cycle |
| `pomona/unit/rssi_dbm` | WiFi RSSI, dBm | no | every publish cycle |
| `pomona/unit/uptime_s` | seconds since boot | no | every publish cycle |
| `pomona/unit/ota_url` | http(s) URL of a `.ota` image — **command topic, the unit subscribes**; publish **non-retained** (a retained URL would re-flash on every reconnect) | no | by the operator (basic OTA, #243 slice) |
| `pomona/unit/ota_result` | `applying <url>` / `failed: <reason>` | **yes** | on an OTA attempt; success = new `unit/fw_version` after reboot |

Cadence: sensor sweep every **5 s** (also refreshes the screen), publish
every **30 s**. QoS 0 for metrics; QoS 1 for the retained `unit/status`,
`unit/fw_version` and `unit/sensors`.

Pump power is **not** on MQTT from the unit — it arrives via Home Assistant
zwave_js (Fibaro plug), see [design.md](design.md).

## #222 checklist (when finalizing)

- Create the `pomona` broker account (+ ACL limited to `pomona/#`).
- Point the Influx/Telegraf ingestion at the metric topics above.
- Revisit payload format here if ingestion prefers JSON-per-zone — the
  firmware's topic definitions sit in one place
  ([`firmware/pomona/config.h`](../firmware/pomona/config.h)).
