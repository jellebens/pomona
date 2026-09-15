# MQTT — broker & topic schema

Status: **v2 since firmware 2.0.0** (ceres card #295, 2026-09-12). The
contract of record is the ceres repo's
**ADR-0008 — MQTT topic contract v2** (`docs/adr/0008-mqtt-topic-contract-v2.md`):
one flat tree per Ceres unit, `ceres/<unit_id>/…`, owned by role. This
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

Unit ids are `<name>-NNNN` (ceres ADR-0009); the tower is `pomona-0001`.
Everything below is under `ceres/pomona-0001/`. Payloads: telemetry is a
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
`config`) is Ceres's view of the unit — published by the brain, Robigus
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
| `actuator/pump/set` | `auto` / `on` / `off` — a *request*; the level interlock always wins | **no**, QoS 1 | Ceres (mixing around a dose, ADR-0003), HA / a human |
| `actuator/light/set` | `auto` / `on` / `off` — a human override of the photoperiod; resets to `auto` at boot | **no**, QoS 1 | HA / a human (new in 2.0.0) |
| `actuator/pump/power_w` | watts drawn by the pump plug, e.g. `4.7` / `0.0` — **published by Home Assistant** (the Fibaro), not the GIGA | **yes**, QoS 1 | HA on every change, at start and every 5 min — Ceres's proof the pump ran before it judges a dose (ADR-0005) |

**Retain the state, unlike the metrics.** A stale metric is worse than none;
a *state* is the current desired state, and on an HA restart the broker
replaying it immediately is exactly what stops the plugs sitting stale. The
firmware republishes on **every connect**, not only on change: HA may have
been driving in the meantime.

**Who is in command:** HA obeys these topics only while the firmware is both
enabled (`input_boolean.pomona_firmware_control`) and reachable (`sys/status`
= `online`); otherwise HA falls back to its own schedule and says so.

### `desired` — what Ceres wants the unit to be

Retained JSON published by Annona (Ceres' config service) (ADR-0011):
`{"unit", "stage": "establishment|established", "targets": {...}, "photoperiod": {"hours"},
"dosing_enabled", "config_version", "ts"}`. The node applies what it supports —
today the **stage** (the wetter establishment cycle vs. established, v1's
`control/mode`) — and ignores the rest.

### `dose/…` — the #224 contract: ml, ids, acks, the node's own rails

| Topic | Payload | Retained | Who |
|---|---|---|---|
| `pomona/dose/test` | `chN fwd <ms> [speed]` — v1 command channel (bench module, 10 s hard cap, one channel at a time) | no | Demeter publishes (active mode), unit subscribes |
| `pomona/dose/result` | event line per run | **yes** | unit publishes; Demeter treats a live event it did not command as a foreign dose → lockout restarts |
| `pomona/demeter/status` | `online` / `offline` (LWT) | **yes** | Demeter |
| `pomona/demeter/mode` | `shadow` / `active` | **yes** | Demeter, on connect |
| `pomona/demeter/decision` | JSON — ts, action, condition, reason, steps, `executed` | **yes** | Demeter, on every non-quiet decision |
| `pomona/demeter/ledger` | JSON rolling 24 h dose ledger (v2 since controller 0.2.0: + pending dose response, settled responses, learned pH sensitivity, no-response streak) | **yes** | Demeter; reloaded at boot so a restart cannot forget the acid cap, lockout or what it learned |

**Why the commands are non-retained and the ledger is retained:** a replayed
dose command would dose twice (same reason as `ota_url`); a replayed ledger
is exactly what a restarted controller needs.

**#224 firmware follow-up:** replace the bench channel with a first-class
`pomona/dose/request` contract — ml-based payloads, per-command idempotency
ids, explicit acks, and local rails (per-channel ml caps + daily budget
mirrored in firmware) so the unit stays safe even against a misbehaving
controller. Only Demeter's `runtime.py` transport changes.

## #222 checklist (when finalizing)

- Create the `pomona` broker account (+ ACL limited to `pomona/#`).
- Point the Influx/Telegraf ingestion at the metric topics above.
- Revisit payload format here if ingestion prefers JSON-per-zone — the
  firmware's topic definitions sit in one place
  ([`firmware/pomona/config.h`](../firmware/pomona/config.h)).

## Archive — where every topic lands in InfluxDB (#290, ADR-0001)

**Every `pomona/#` topic is archived in the `pomona` InfluxDB bucket with
infinite retention** ([ADR-0001](adr/0001-telemetry-archived-forever.md);
platform decision gitops ADR-0002). The gitops `landingzones/pomona` Telegraf
bridge does the ingestion; the bucket and its retention are declared in
`platform/influxdb-config` and reconciled hourly.

| Topics | Measurement | How |
|---|---|---|
| `pomona/water/+`, `pomona/air/+`, `pomona/unit/rssi_dbm`, `pomona/unit/uptime_s` | `pomona` | float `value`, tags `zone`/`metric` |
| `pomona/unit/status`, `pomona/unit/fw_version`, `pomona/unit/sensors` | `pomona_meta` | string `value`, verbatim |
| `pomona/dose/+`, `pomona/demeter/+`, `pomona/pump/+`, `pomona/light/+`, `pomona/control/+`, `pomona/unit/ota_result`, `pomona/unit/i2c_scan` | `pomona_events` | string `value`, **verbatim** (JSON stays JSON) — the lossless record |
| `pomona/demeter/decision` | `demeter_decision` | parsed: tags `action`,`condition`,`stage`,`mode`; fields `executed`,`reason`,`ml`,`reagent`,`version`; point time = the document's `ts` |
| `pomona/demeter/ledger` | `demeter_model` | parsed: `model.*` → `k`,`b`,`n`,`settle_s`,`rebound_ph_h`,`kf_level`,`kf_slope`,`kf_r`,`trough_ph`,`no_response_streak`,`overshoots`; `reagents.*.pumped_ml`; `pending.ml`/`pre_ph`; tag `last_dose_tier` |
| Demeter's `demeter_*` Prometheus series | `prometheus` bucket, measurement `prometheus` | cluster-wide `remote_write` archive (field = metric name) |

Contract consequences:

- A **new topic** under any of the archived prefixes is archived with no
  config change. A new top-level zone needs one line in the gitops Telegraf
  config.
- The parsed measurements read specific keys (`ts`, `action`, `condition`,
  `steps[0].ml`, `steps[0].reagent`, `model.*`, `reagents.*.pumped_ml`);
  **renaming one is a breaking change** made together with the gitops config.
- **Retained topics replay on every Telegraf reconnect** → one duplicate
  string point per retained topic at reconnect time in `pomona_events`.
  `demeter_decision` is immune (document `ts` as point time).
- Topics that were live but undocumented until #290 and are now archived:
  `pomona/water/ph_raw_v` (raw electrode volts, always published),
  `pomona/unit/i2c_scan` (retained JSON), `pomona/pump/override` (Demeter →
  unit, `on`/`auto`, non-retained), `pomona/control/mode`
  (`establishment`/`established`, subscribed).
