# Pomona monitoring firmware v1 (`pomona`)

The application sketch for the unit at the tower (Trello #229): reads every
v1 sensor, publishes to the cluster MQTT broker, shows current readings on
the GIGA Display Shield (with idle screen blanking, #248) and accepts
basic OTA updates over WiFi (#243 slice). Bench verification stays in
[`../bringup`](../bringup/bringup.ino); on-screen history graphs are #223.

Target: **Arduino GIGA R1 WiFi + GIGA Display Shield**
(`arduino:mbed_giga:giga`). ⚠ WiFi needs the **external antenna** on the
Micro UFL connector — without it there is no WiFi
([docs/wiring.md](../../docs/wiring.md)).

## Architecture / file layout

One module per folder under `src/` (Arduino compiles the sketch root +
`src/**`; entry point + config + secrets stay at the root):

| File | Role |
|---|---|
| [`pomona.ino`](pomona.ino) | Orchestration: watchdog, sensor sweep every 5 s, publish every 30 s, LVGL service |
| [`config.h`](config.h) | Pins, ADC, I²C addresses, intervals, blank timeout, WiFi SSID + MQTT host/port/user, topics |
| [`src/sensors/`](src/sensors/sensors.h) | All v1 sensors → `Readings` struct with per-sensor validity flags |
| [`src/network/`](src/network/network.h) | WiFi + MQTT connect, exponential-backoff reconnect, publishing, OTA trigger |
| [`src/display/`](src/display/display.h) | LVGL current-readings screen, version bottom-right, idle blanking |
| [`src/ota/`](src/ota/ota.h) | Arduino_Portenta_OTA download-and-apply (basic #243 slice) |
| `secrets.h` | ONLY the two passwords (`WIFI_PASS`, `MQTT_PASS`) — **gitignored**, see below |

Shared libraries (via `--libraries ../libraries` / sketchbook = `firmware/`):
[`PomonaVersion`](../libraries/PomonaVersion/src/PomonaVersion.h) (semver,
bumped by the deploy scripts — printed in the boot banner together with
`__DATE__`/`__TIME__` build info, and shown bottom-right on screen),
[`PomonaCalibration`](../libraries/PomonaCalibration/src/PomonaCalibration.h)
(EC/pH calibration constants — record calibration results there),
[`GroveWaterLevel`](../libraries/GroveWaterLevel/src/GroveWaterLevel.h),
[`PhotoLevelProbe`](../libraries/PhotoLevelProbe/src/PhotoLevelProbe.h).

**Absent sensors are handled gracefully:** every sensor is optional at
runtime — a missing sensor's metrics are skipped on MQTT, shown as `--` on
screen, and its init is re-probed every `SENSOR_REINIT_MS` (60 s — rate
limited so an absent BH1750 doesn't spam NACK errors; hot-plug recovers
without a reboot). The unit never hangs on missing hardware; a hardware
watchdog (30 s) reboots it if anything does wedge.

**Connectivity at a glance:** WiFi + MQTT status icons sit top-right on
the screen — red = down, green = connected — updated live every loop
(the version label stays bottom-right). Serial prints the WiFi firmware
version + status at boot; a connect cycle tries `WiFi.begin` up to
`WIFI_BEGIN_ATTEMPTS` (3) times with per-attempt status codes, then runs
one scan to report whether the SSID is even visible. If the CYW4343W
WiFi firmware partition was never provisioned (version reads `v0.0.0`)
it prints the fix: run the one-time `STM32H747_System →
WiFiFirmwareUpdater` sketch over USB.

## MQTT

Broker: the in-cluster EMQX at **`mqtt.lab.local:1883`**. Topics follow
`pomona/<zone>/<metric>` — full schema, payloads and retain/LWT rules in
**[docs/mqtt.md](../../docs/mqtt.md)** (proposed here, finalized with #222).
Publish cadence 30 s; `pomona/unit/status` is retained online/offline with
a broker LWT.

## Control — pump + light (#260)

Build status: compiles clean for  (arduino-cli 1.5.1,
core 4.6.0), , no warnings from  or .
728,292 B flash (37%), 137,856 B RAM (26%). Not yet flashed — deploys are
owner-gated.


`src/control` decides what the pump and grow light should be doing; Home
Assistant only relays those decisions onto the Fibaro plugs. Design rationale:
`docs/control-architecture.md`. Topic contract: `docs/mqtt.md` "Control topics".

**Nothing here waits on the network.** `controlService()` runs off the local
sweep and `millis()`; if WiFi or the broker are down the tower still waters
correctly, it just cannot report it. Publishing is reporting, not deciding.

**Safe state is set first.** `controlInit()` is the first call in `setup()` —
before serial, watchdog, display, sensors and WiFi — because a rebooting unit
(OTA, watchdog, brown-out) must be in a known state before anything that can
hang gets a chance to run.

| Published (retained, QoS 1) | Payload |
|---|---|
| `pomona/pump/request` | `on` / `off` |
| `pomona/light/request` | `on` / `off` |
| `pomona/pump/reason` | `boot_safe` / `schedule` / `settling` / `level_low` / `override` |

| Subscribed | Payload |
|---|---|
| `pomona/pump/override` | `auto` / `on` / `off` |
| `pomona/control/mode` | `establishment` / `established` |

Requests are republished on **every connect**, not only on change: HA may have
been running its own schedule while the unit was away, so the retained value
could predate the outage.

### Pump duty cycle

15 min on, then off for 15 min (establishment) / 45 min (established, day) /
105 min (established, night). Runs on `millis()` only — no clock needed.

### Photoperiod, and the one clock dependency

The light needs wall-clock time, and NTP at connect is the only source. With no
clock the light is held **off** and the pump falls back to the day cycle:
lights stuck off costs growth, lights stuck on at night costs the plants their
dark period, so off is the safe failure. `TZ_OFFSET_MINUTES` in `config.h` is a
fixed offset — an hour of DST error is irrelevant to a 14 h photoperiod.

### Level interlock

The probe is a top-up gauge, blind below 8.2 L, and it reads low *during* a
pump cycle because the tower holds water in transit. So a raw 0 triggers a
**settle check**: stop the pump, wait 5 min for drain-back, re-read. Recovered
means it was in transit; still low sets the confirmed flag, and only a
confirmed low persisting 24 h inhibits the pump. No signal at all (`-1`) is
*unknown, not empty* — it fails open. An override can never run the pump while
the interlock is inhibiting: firmware keeps the veto.

## OTA updates (basic — #243 slice)

The classic ArduinoOTA "network port" upload does **not** support the
GIGA, so the basic OTA path is
[Arduino_Portenta_OTA](https://github.com/arduino-libraries/Arduino_Portenta_OTA)
(works on the GIGA): publish an http(s) URL of a `.ota` image —
**non-retained** — to `pomona/unit/ota_url` and the unit downloads it into
the QSPI OTA partition, stages it and reboots; the bootloader applies it.
Progress/errors go to serial and retained `pomona/unit/ota_result`;
success shows up as the new version on `pomona/unit/fw_version` (and on
screen). Building the `.ota` image and the version-endpoint pull
automation (periodic check against `POMONA_FW_VERSION`, cluster-served
images) stay on card #243.

⚠ **One-time USB prereq before OTA works on hardware**: partition the QSPI
flash (`STM32H747_System → QSPIFormat`; done on the bench unit 2026-08-27 —
when re-running it answer **n** to reformatting partition 1 or you erase the
WiFi firmware; the same sketch restores it, answer Y/Y). Until then the
firmware logs `ota: NOT capable` at boot (bad/old bootloader) or
`begin failed (-3)` (unformatted partition) and refuses OTA requests —
everything else works normally. See
[docs/ota-and-secrets.md](../../docs/ota-and-secrets.md).

**Download size verification (bench lesson, 2026-08-27):** the mbed core's
`download()` returns the HTTP *Content-Length*, not the bytes actually
written — its `fwrite`s are unchecked, so a sick FAT (e.g. after a reset
mid-write) yields a silently truncated file and `decompress failed (-5)`.
The OTA module therefore stats `/fs/UPDATE.BIN.LZSS` after each download,
compares it to the reported size (`ota: reported N bytes, on flash M`), and
deletes + retries up to **`OTA_DOWNLOAD_ATTEMPTS`** (config.h) times. If
every attempt is short (a deterministic `on flash 4096` means a corrupted
OTA-partition FAT), reformat partition 2 with QSPIFormat and try again.

## Screen blanking (#248)

After **`DISPLAY_BLANK_TIMEOUT_MS`** (config.h, default 60 s) without
touch input the shield's backlight switches off. Any touch wakes it; the
waking touch lands on a full-screen catcher object, so it can never press
a UI element underneath. While blanked, sensors, MQTT and OTA keep
running — only the display sleeps. Uses LVGL's inactivity clock
(`lv_display_get_inactive_time`) + `GigaDisplayBacklight`
(Arduino_GigaDisplay).

## Secrets

Copy [`../secrets.h.example`](../secrets.h.example) into this folder as
`secrets.h` and fill in the **two passwords** (`WIFI_PASS`, `MQTT_PASS`)
— everything non-secret (SSID, MQTT host/port/user) lives in
[`config.h`](config.h) since #251. `secrets.h` is gitignored repo-wide —
never commit it. This is Layer 1 of
[docs/ota-and-secrets.md](../../docs/ota-and-secrets.md); #244 moves the
passwords into the ATECC608A secure element.

## Build & deploy

Compile (from `firmware/`):

```sh
arduino-cli compile --fqbn arduino:mbed_giga:giga --libraries libraries pomona
```

Extra libraries beyond the bring-up set (one-time):

```sh
arduino-cli lib install ArduinoMqttClient lvgl Arduino_GigaDisplayTouch \
  Arduino_GigaDisplay Arduino_Portenta_OTA
```

(`Arduino_H7_Video` and its `lv_conf.h` ship with the `arduino:mbed_giga`
core.) Deploy through the version-bumping scripts — **equivalent by
design**, see [../README.md](../README.md):

- Windows: `.\deploy.ps1 -Sketch pomona` (board on COM6)
- Linux/WSL: `./deploy.sh -s pomona` (default `/dev/ttyACM0`; WSL needs
  the USB device attached first via `usbipd`)
