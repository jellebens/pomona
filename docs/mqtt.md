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

## Control topics — the GIGA decides, HA relays (Trello #260)

Status: **contract agreed, HA side built and merged
(`home-assitant` PR #16), firmware side not written yet.**

The GIGA publishes what it wants to happen; Home Assistant relays it onto the
Fibaro plugs. This moves the decision to the device that actually holds the
sensors without touching any mains wiring — see
[control-architecture.md](control-architecture.md) for why, and for the fuller
relay-driven end state this is the first half of.

| Topic | Payload | Retained | Published |
|---|---|---|---|
| `pomona/pump/request` | `on` / `off` | **yes**, QoS 1 | whenever the desired pump state changes |
| `pomona/light/request` | `on` / `off` | **yes**, QoS 1 | whenever the desired light state changes |
| `pomona/pump/reason` | free text — `schedule` / `level_low` / `settling` / `override` / `boot_safe` | **yes** | alongside each pump request |
| `pomona/pump/power` | watts drawn by the pump plug, e.g. `4.7` / `0.0` — **published by Home Assistant** (`sensor.pomona_pump_power`, the Fibaro), not the GIGA | **yes**, QoS 1 | on every change, at HA start and every 5 min (`pomona_schedule.yaml` ≥ 1.3.0). Demeter's proof that the pump actually ran before it judges a dose (demeter ADR-0005) |

**Retain these, unlike the metrics.** The metric topics are deliberately
non-retained because a stale sensor reading is worse than none. A *request* is
the opposite: it is the current desired state, and on an HA restart the broker
replaying it immediately is exactly what stops the plugs sitting stale until
the next decision.

### Who is in command

HA obeys these topics only while the firmware is **both enabled and
reachable** — `input_boolean.pomona_firmware_control` on **and**
`pomona/unit/status` = `online`. Otherwise HA falls back to its own schedule
and says so. So a crash, a wedge or an OTA reboot hands control back
automatically rather than freezing the pump in its last requested state.

That fallback is why the firmware must **publish a request on every connect**,
not only on change: HA may have been driving in the meantime, and the retained
value could be from before the outage.

### Firmware obligations

- Publish `boot_safe` and the safe request state **early in `setup()`**, before
  WiFi and before the sensors — the first thing anyone learns about a rebooted
  unit should be that it is in a known state.
- Do not gate the schedule on WiFi. A unit that cannot reach the broker must
  still decide correctly locally; publishing is reporting, not deciding.
- Keep the local interlock authoritative. HA applies its own sustained-low
  inhibit as a second opinion, but it is belt and braces, not the mechanism.

Cadence: sensor sweep every **5 s** (also refreshes the screen), publish
every **30 s**. QoS 0 for metrics; QoS 1 for the retained `unit/status`,
`unit/fw_version` and `unit/sensors`.

Pump power is **not** on MQTT from the unit — it arrives via Home Assistant
zwave_js (Fibaro plug), see [design.md](design.md).

## Dosing topics — Demeter, the k3s autodosing brain (#278, design #224)

Status: **LIVE (active) since 2026-09-10.** The controller lives in its own
private repo, <https://github.com/jellebens/demeter> (extracted from this
repo's `controller/` on 2026-09-11, history preserved); it deploys via gitops
`landingzones/pomona`. The interim Tethys agent regime is retired — never
both.

Demeter connects as the dedicated broker user **`pomona-demeter`**
(subscribe `pomona/#`; publish only `pomona/dose/test`,
`pomona/pump/override` and `pomona/demeter/#`). Around every dose it
forces the circulation pump via `pomona/pump/override` (demeter ADR-0003)
and, since 0.5.1, counts mixing time from `pomona/pump/power` above
(demeter ADR-0005).

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
