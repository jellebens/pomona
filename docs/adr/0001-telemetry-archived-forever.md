# ADR-0001: Every Pomona topic is archived in InfluxDB forever — sensors, the control plane and Demeter's stream, verbatim

- **Status:** Accepted (implemented in gitops `landingzones/pomona` chart 0.8.0 + `platform/influxdb-config` 0.2.0, card #290)
- **Date:** 2026-09-11
- **Deciders:** Jelle (owner), with Claude
- **Tags:** telemetry, mqtt, influxdb, retention, dosing, model-training

## Context

Pomona's telemetry path has been "MQTT → InfluxDB → Grafana" since the design
doc, and card #222 made it real for the numeric sensor topics
(`pomona/water/+`, `pomona/air/+`, RSSI, uptime) plus three status strings.
But the `pomona` bucket was hand-created with a **365-day retention**, and the
rest of the topic tree — everything that makes the tower a *controlled* system
— never reached the database:

- `pomona/dose/test` and `pomona/dose/result` (every pump command and its
  completion; the raw record of what went into the reservoir);
- `pomona/demeter/decision` and `pomona/demeter/ledger` (why Demeter dosed,
  and what it has learned — retained, overwritten every cycle);
- `pomona/pump/request` / `reason` / `override` / `power`, `pomona/light/request`,
  `pomona/control/mode` (the control plane as seen on the broker);
- `pomona/unit/ota_result`, `pomona/unit/i2c_scan` (diagnostics);
- `pomona/water/ph_raw_v` was ingested but undocumented in `mqtt.md`.

Demeter's learned parameters existed only as Prometheus gauges with a 15-day
retention. On 2026-09-11, the first full day of live autodosing, the owner
decided that this history is critical for training better controllers — for
Pomona and every other project — and that it is kept forever (gitops
ADR-0002, card #290).

## Decision

1. **Every `pomona/#` topic is archived in the `pomona` InfluxDB bucket, and
   the bucket's retention is infinite.** Retention is declared in gitops
   (`platform/influxdb-config` `buckets.list`) and reconciled hourly; the
   README's `influx bucket create --retention 8760h` step is gone.
2. **Verbatim first.** The gitops Telegraf bridge stores every non-numeric
   topic as a string field, unchanged (`pomona_events`, tags `zone`/`metric`
   from the topic). JSON documents land as JSON. This is the lossless training
   record and the contract: the firmware and Demeter may add topics or keys
   freely — they are archived without a config change.
3. **Parsed second.** Demeter's decision and ledger are additionally parsed
   into typed measurements (`demeter_decision`, `demeter_model`) for Flux
   queries; the parse is a convenience that can be rebuilt from the verbatim
   record. Renaming a key those parsers read (`ts`, `action`, `condition`,
   `steps[0].ml`, `model.*`, `reagents.*.pumped_ml`) is a contract change made
   together with the gitops Telegraf config — see `docs/mqtt.md` "Archive".
4. **Demeter's Prometheus series are archived through the cluster-wide
   `remote_write` archive** (`prometheus` bucket), not by a Pomona-specific
   scrape. Nothing on the unit or in Demeter changes.
5. **The firmware keeps publishing plain numeric text, one metric per topic,
   non-retained** (the #229 schema). The archive adapts to the unit, never
   the other way round; the unit must stay simple enough to work with the
   broker down.

## Consequences

- **A complete record from 2026-09-11**: chemistry every 30 s (since
  2026-08-31), every dose and its result, every Demeter decision with its
  reason, and the learned model every minute — all in one bucket, forever,
  backed up nightly with hourly increments to the NAS.
- **Retained topics replay on reconnect** (one duplicate string point per
  retained topic per Telegraf reconnect); the parsed decision uses the
  document's own `ts` and is immune. Analyses on the string measurements
  should de-duplicate.
- **`docs/mqtt.md` gains an "Archive" section** listing which measurement each
  topic lands in, and the previously undocumented topics (`ph_raw_v`,
  `i2c_scan`, `pump/override`, `control/mode`) are now in the contract.
- **Cost:** a few MB per day more in the `pomona` bucket; the 10 Gi InfluxDB
  PVC is a watch item on the InfluxDB health dashboard.
- **The `#222` checklist item "point the Influx/Telegraf ingestion at the
  metric topics" is now "at all topics"** — done.

## Alternatives considered

- **Make the GIGA publish JSON-per-zone so one consumer archives everything
  typed** — rejected: the unit's plain-text schema is deliberately dumb and
  Influx-friendly; typing belongs in the archive's parser.
- **Keep a finite retention (a season, a year)** — rejected by the owner:
  every past season is a training set for the next.
- **Archive only Demeter's parsed fields** — rejected: the raw documents are
  the ground truth for whatever the next ledger version adds.
