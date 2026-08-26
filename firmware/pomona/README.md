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

| File | Role |
|---|---|
| [`pomona.ino`](pomona.ino) | Orchestration: watchdog, sensor sweep every 5 s, publish every 30 s, LVGL service |
| [`config.h`](config.h) | Pins, ADC, I²C addresses, intervals, blank timeout, MQTT client id + topics |
| [`sensors.h/.cpp`](sensors.h) | All v1 sensors → `Readings` struct with per-sensor validity flags |
| [`network.h/.cpp`](network.h) | WiFi + MQTT connect, exponential-backoff reconnect, publishing, OTA trigger |
| [`display.h/.cpp`](display.h) | LVGL current-readings screen, version bottom-right, idle blanking |
| [`ota.h/.cpp`](ota.h) | Arduino_Portenta_OTA download-and-apply (basic #243 slice) |
| `secrets.h` | WiFi/MQTT credentials — **gitignored**, see below |

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

⚠ **One-time USB prereqs before OTA works on hardware** (owner-run, still
pending on #243): update the bootloader (IDE: `STM32H747_System →
STM32H747_manageBootloader`) and partition the QSPI flash
(`STM32H747_System → QSPIFormat` — erases the QSPI). Until then the
firmware logs `ota: NOT capable` at boot and refuses OTA requests —
everything else works normally. See
[docs/ota-and-secrets.md](../../docs/ota-and-secrets.md).

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
`secrets.h` and fill in WiFi + MQTT credentials. `secrets.h` is gitignored
repo-wide — never commit it. This is Layer 1 of
[docs/ota-and-secrets.md](../../docs/ota-and-secrets.md); #244 moves the
values into the ATECC608A secure element.

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
