# Changelog

All notable changes to the Pomona project are documented here.
Format: [Keep a Changelog](https://keepachangelog.com/); versions are the
firmware semver from `firmware/libraries/PomonaVersion` (single source of
truth, bumped by the deploy scripts). Tags `v<version>` mark each release
merged to `master`.

## [0.1.12] - 2026-08-27

OTA release — over-the-air updates work end-to-end and untethered:
v0.1.11 (wired serial watch) and v0.1.12 (no cable at all, owner-confirmed
on screen) were both delivered via MQTT trigger → HTTP download →
bootloader apply. The unit no longer needs USB for firmware updates.

### Fixed
- **OTA download size verification + retry** (#243, PR #35) — the mbed
  core's `download()` returns the HTTP Content-Length while its file writes
  go unchecked, so a truncated file surfaced later as
  `decompress failed (-5)`. The OTA module now stats the stored file,
  compares it to the reported size and deletes + retries
  (`OTA_DOWNLOAD_ATTEMPTS`, config.h). A deterministic 4096-byte result
  indicates a corrupted OTA-partition FAT — reformat via QSPIFormat
  (runbook in the sketch README, including restoring the WiFi firmware if
  partition 1 is accidentally erased).
- **OTA watchdog starvation — the root cause** (#243, PR #36) —
  Arduino_Portenta_OTA only feeds the watchdog if `setFeedWatchdogFunc`
  callbacks are registered; without them the 1–2 minute decompress starved
  the 30 s IWDG, silently resetting the device mid-apply (and those
  mid-write resets were what corrupted the FAT). Callbacks are now
  registered for both the OTA stages and the WiFi download path.

### Changed
- MQTT status icon on the screen now uses the envelope symbol (PR #34).

### Infrastructure
- `pomona` EMQX user granted least-privilege ACLs (`allow pomona/#`,
  runtime; git DR mirror tracked separately) — without them the broker
  silently dropped all unit publishes and rejected the OTA subscription.
- QSPI flash partitioned on the bench unit (partition 2 = OTA). The
  bootloader shipped on the board applies OTA images fine — no bootloader
  update was needed.
- Known wart: retained `pomona/unit/ota_result` still reads
  `applying <url>` after a successful update — cross-check
  `pomona/unit/fw_version`; success reporting is a future #243 slice.

## [0.1.4] - 2026-08-26

First full monitoring release — the unit runs end-to-end: WiFi → MQTT →
publishing, with the touchscreen UI. Bench-verified all-green.

### Added
- **`firmware/pomona` v1 monitoring app** (#229, PR #28) — from-scratch modular
  sketch (entry point `pomona.ino`): WiFi + MQTT with backoff reconnect and
  LWT, publishing all v1 sensor readings to the cluster EMQX broker
  (`mqtt.lab.local:1883`, topics `pomona/<zone>/<metric>` — see
  `docs/mqtt.md`); LVGL current-readings screen on the GIGA Display Shield
  with the firmware version bottom-right; boot banner with version + build
  date; 30 s hardware watchdog; graceful absent-sensor handling (publishes
  what is readable plus retained availability JSON).
- **Screen blanking** (#248) — backlight off after a configurable idle
  timeout (`DISPLAY_BLANK_TIMEOUT_MS`, default 60 s); touch wakes the screen
  without activating UI elements; everything else keeps running while blanked.
- **WiFi/MQTT status icons** (#229 follow-up) — live red/green indicators on
  the screen for WiFi and MQTT connection state.
- **Basic OTA slice** (#243) — MQTT-triggered `Arduino_Portenta_OTA` fetch
  via `pomona/unit/ota_url`. (The planned IDE network-port upload is not
  possible on the GIGA — no mbed-H7 ArduinoOTA backend. Runtime OTA still
  requires the one-time USB prereqs: bootloader update + QSPIFormat, tracked
  on #243.)
- **`firmware/deploy.sh`** — Linux/WSL twin of `deploy.ps1` (same version
  auto-bump semantics).
- **WiFi failure diagnostics** — firmware version + numeric status per
  attempt, 3 begin-attempts per cycle, and a post-failure scan that reports
  whether the SSID is visible (this pinpointed both bench issues: SSID not
  broadcast on 2.4 GHz, then a WPA3-only/passphrase refusal).
- `PomonaCalibration` shared library (calibration constants used by both the
  app and the bringup sketch).

### Changed
- **Firmware restructured into module folders** (#251, PR #29) —
  `firmware/pomona/src/{display,network,sensors,ota}/`; `pomona.ino` and
  `config.h` stay at the sketch root. Byte-identical compile (pure refactor).
- **Config/secrets split** (#251) — non-secret settings (`WIFI_SSID`,
  `MQTT_HOST`, `MQTT_PORT`, `MQTT_USER`) live in `config.h`; `secrets.h` is
  passwords-only (`WIFI_PASS`, `MQTT_PASS`), `SECRET_` prefixes dropped.
- Absent-sensor re-probes rate-limited to 60 s (was every 5 s log spam).

### Infrastructure
- `pomona` MQTT user created in EMQX (broker denies anonymous).
- Bench: AP now broadcasts the 2.4 GHz SSID `B3ns-2-4` (GIGA radio is
  2.4 GHz-only) with WPA2 enabled.

## [0.1.0] - 2026-08-26

### Added
- Firmware version infrastructure (#243, PR #27) — `PomonaVersion` library as
  the semver single source of truth, printed in every sketch's boot banner;
  `firmware/deploy.ps1` compiles with arduino-cli, flashes over USB (COM6)
  and auto-bumps the version each deploy (patch by default).

## Pre-versioning (2026-07 → 2026-08)

- Bench bring-up of the v1 sensors (#242): wiring, per-sensor docs under
  `docs/sensors/`, level-probe calibration ladder, DS18B20 ice-bath
  verification, GIGA pinout references.
- OTA + secure-secrets design (#243/#244): `docs/ota-and-secrets.md`
  (layered plan toward the ATECC608A secure element), gitignored `secrets.h`
  convention + `secrets.h.example`.
- `firmware/bringup` sketch (#220, PR #12): first sensor readings on the GIGA.
- Growing side: seeding PoC and sowing records (`docs/`), plan v4 (30 pods,
  sponges, EC 1.4–1.6).
