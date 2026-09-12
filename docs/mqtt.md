# MQTT — broker & topic schema

Status: **v2 since firmware 2.0.0** (demeter card #295, 2026-09-12). The
contract of record is the demeter repo's
**ADR-0008 — MQTT topic contract v2** (`docs/adr/0008-mqtt-topic-contract-v2.md`):
one flat tree per Demeter unit, `demeter/<unit_id>/…`, owned by role. This
page is the node's view of it — what the GIGA publishes and reads — plus the
v1 history the archive still carries. Firmware 1.x spoke `pomona/<zone>/<metric>`
(section "v1 — history" below); during the transition the platform broker's
republish bridge mirrors the two trees onto each other (gitops
`platform/mqtt` README "Republish bridge").

## Broker

The in-cluster **EMQX** cluster: **`mqtt.lab.local:1883`** — the same broker
zeus/jupiter use (the `mqtt.lab.local` A record is served by the
authoritative `lab.local` zone in the gitops repo). The unit authenticates as
its own least-privilege user **`unit-pomona-0001`** (publish only its own
telemetry, actuator state, dose acks and self-description; subscribe only to
requests, desired state, OTA and diag — gitops `platform/mqtt/files/acl.conf`).
On the unit the credentials live in the gitignored `secrets.h`
([ota-and-secrets.md](ota-and-secrets.md), Layer 1).

## The unit: `pomona-0001`

Unit ids are `<name>-NNNN` (demeter ADR-0009); the tower is `pomona-0001`.
Everything below is under `demeter/pomona-0001/`. Payloads: telemetry is a
plain number per topic, non-retained; state is retained; commands are never
retained; documents are JSON with a `ts` (unix seconds, `0` when the node has
no clock yet).

### `sys/` — what the node says about itself

| Topic | Payload | Retained | Published |
|---|---|---|---|
| `sys/status` | `online` / `offline` | **yes**, QoS 1 | on connect; `offline` is the broker **LWT** |
| `sys/meta` | JSON `{unit, type: "aeroponic_tower", node: "giga-r1", fw_version, contract: 2, sensors: [...], actuators: ["pump","light"], reservoir_l, dosers: {reagent: {channel, ml_s_full, ml_s_slow, slow_speed, max_ml_per_cmd, max_ml_per_24h}}, ts}` | **yes**, QoS 1 | on connect |
| `sys/health` | JSON availability map `{"water_temp":true,"ph_calibrated":false,…,"ts"}` | **yes**, QoS 1 | every publish cycle |
| `sys/diag/i2c_scan` | JSON `{"found": n, "addrs": "0x23,0x76"}` | yes | every publish cycle and on `sys/diag/i2c_scan/get` |
| `sys/diag/i2c_scan/get` | any payload → scan now | no | **subscribed** |
| `sys/ota/url` | http(s) URL of a `.ota` image — **command, the unit subscribes**; publish **non-retained** (a retained URL would re-flash on every reconnect) | no | **subscribed** (basic OTA, #243) |
| `sys/ota/result` | `applying <url>` / `failed: <reason>` / `skipped same-version <url>` | yes | on an OTA attempt; success = a new `fw_version` in `sys/meta` after reboot |

The rest of `sys/` (`role`, `decision`, `ledger`, `alerts`, `advice`,
`config`) is Demeter's view of the unit — published by the brain, Robigus
and the registry, never by the node.

### `tele/<zone>/<metric>` — telemetry

Zones: `water` (reservoir), `air` (ambient at the tower), `node` (the GIGA
itself). Plain numeric text, one metric per topic, **non-retained**, QoS 0. A
metric whose sensor did not answer is simply **not published** — consumers
read availability from `sys/health`, not from sentinels.

| Topic | Payload | Published |
|---|---|---|
| `tele/water/temp_c` | °C, 1 decimal | DS18B20 answered |
| `tele/water/ec_ms_cm` | mS/cm, 2 decimals | always (analog — no absence detection; ~0 when unplugged) |
| `tele/water/ph` | pH, 2 decimals | only once pH is calibrated (PomonaCalibration) |
| `tele/water/ph_raw_v` | probe volts, 3 decimals | always (calibration/drift aid) |
| `tele/water/level_pct` | 0–100 | Grove strip answered (optional hardware) |
| `tele/water/level_points` | 0–4 (CQRSENYW003 ladder; a TOP-UP gauge, blind below 8.2 L) | probe signal present |
| `tele/air/temp_c`, `tele/air/humidity_pct`, `tele/air/pressure_hpa` | °C / %RH / hPa, 1 decimal | BME280 answered |
| `tele/air/lux` | lux, integer | BH1750 answered |
| `tele/node/rssi_dbm`, `tele/node/uptime_s` | WiFi RSSI / seconds since boot | every publish cycle |

Cadence: sensor sweep every **5 s** (also refreshes the screen), publish
every **30 s**.

### `actuator/<name>/…` — the node decides, HA relays (Trello #260)

The GIGA publishes what it wants to happen; Home Assistant relays it onto the
Fibaro plugs (`home-assitant` `packages/pomona_schedule.yaml` ≥ 2.0.0). The
decision lives on the device that holds the sensors, no mains wiring is
touched — see [control-architecture.md](control-architecture.md).

| Topic | Payload | Retained | Who |
|---|---|---|---|
| `actuator/pump/state` | `on` / `off` — the node's decision | **yes**, QoS 1 | node, on change and on every connect |
| `actuator/pump/reason` | `boot_safe` / `schedule` / `settling` / `level_low` / `override` | **yes** | node |
| `actuator/light/state` | `on` / `off` | **yes**, QoS 1 | node |
| `actuator/pump/set` | `auto` / `on` / `off` — a *request*; the level interlock always wins | **no**, QoS 1 | Demeter (mixing around a dose, ADR-0003), HA / a human |
| `actuator/light/set` | `auto` / `on` / `off` — a human override of the photoperiod; resets to `auto` at boot | **no**, QoS 1 | HA / a human (new in 2.0.0) |
| `actuator/pump/power_w` | watts drawn by the pump plug, e.g. `4.7` / `0.0` — **published by Home Assistant** (the Fibaro), not the GIGA | **yes**, QoS 1 | HA on every change, at start and every 5 min — Demeter's proof the pump ran before it judges a dose (ADR-0005) |

**Retain the state, unlike the metrics.** A stale metric is worse than none;
a *state* is the current desired state, and on an HA restart the broker
replaying it immediately is exactly what stops the plugs sitting stale. The
firmware republishes on **every connect**, not only on change: HA may have
been driving in the meantime.

**Who is in command:** HA obeys these topics only while the firmware is both
enabled (`input_boolean.pomona_firmware_control`) and reachable (`sys/status`
= `online`); otherwise HA falls back to its own schedule and says so.

### `desired` — what Demeter wants the unit to be

Retained JSON published by Demeter's registry (ADR-0011):
`{"unit", "stage": "establishment|established", "targets": {...}, "photoperiod": {"hours"},
"dosing_enabled", "config_version", "ts"}`. The node applies what it supports —
today the **stage** (the wetter establishment cycle vs. established, v1's
`control/mode`) — and ignores the rest.

### `dose/…` — the #224 contract: ml, ids, acks, the node's own rails

| Topic | Payload | Retained | Who |
|---|---|---|---|
| `dose/request` | JSON `{"id": "<unique>", "reagent": "ph_down\|nutrient_a\|nutrient_b", "ml": 1.1, "rate": "full\|slow", "ts"}` | **no**, QoS 1 | Demeter publishes, the unit subscribes |
| `dose/result` | JSON `{"id", "reagent", "status": "done\|refused\|failed", "ml", "ms", "channel", "reason", "ts"}` — `id` absent = a bench dose (foreign to Demeter → its lockout restarts) | **yes**, QoS 1 | node, on every request and on every bench run |

The node converts ml with **its own** calibration
(`firmware/libraries/PomonaCalibration` `DOSER_CAL`, announced in `sys/meta`)
and enforces **its own rails** — `config.h` `DOSE_MAX_ML_PER_CMD`
(pH-Down 2 ml, A/B 10 ml), `DOSE_MAX_ML_PER_24H` (8 / 40 / 40 ml, rolling),
one channel at a time, an absolute 60 s run cap — so the unit stays safe
against a misbehaving controller. Every request is acked: a `refused` or
`failed` dose never reached the tank and Demeter takes it back out of its
ledger; `done` carries the ms actually run. The bench channel
(`chN fwd|rev|stop [ms] [speed]`, 10 s cap) survives over USB Serial only
(`dose chN …`).

## Consumers

| Consumer | Reads | Writes |
|---|---|---|
| Demeter brain (`brain-pomona-0001`) | `demeter/pomona-0001/#`, `demeter/sys/mode` | `actuator/+/set`, `dose/request`, its `sys/role\|decision\|ledger`, `demeter/sys/status/brain-pomona-0001` |
| Robigus, the registry | `demeter/#` | `sys/alerts\|advice`, `sys/config`, `desired` |
| Home Assistant (`homeassistant`) | `demeter/#` | `actuator/+/power_w`, `actuator/+/set` |
| Telegraf archive (`telegraf-demeter`) | `demeter/#` | — (InfluxDB bucket `demeter`, forever) |

## v1 — history (firmware 1.x, `pomona/…`)

Until 2.0.0 the unit spoke its own tree: `pomona/<water|air>/<metric>`,
`pomona/unit/{status,fw_version,sensors,rssi_dbm,uptime_s,i2c_scan,ota_url,ota_result}`,
`pomona/pump/{request,reason,override,power}`, `pomona/light/request`,
`pomona/control/mode`, the bench dose channel `pomona/dose/test` (`chN fwd
<ms> [speed]`, no id, no ack) and `pomona/dose/result` (an event line), and
Demeter's own `pomona/demeter/{status,mode,decision,ledger}`. The InfluxDB
`pomona` bucket keeps that era forever; the `demeter` bucket carries v2. The
mapping v1 → v2 is the republish bridge's table in the gitops
`platform/mqtt` README; the bridge and the v1 users (`pomona`,
`pomona-demeter`, `pomona-ingest`) are retired once 2.0.0 is on the tower.
