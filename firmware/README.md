# Pomona firmware

Target: **Arduino GIGA R1 WiFi** (+ GIGA Display Shield). Wiring for
everything below is in [docs/wiring.md](../docs/wiring.md).

## Sketches

| Sketch | Purpose |
|---|---|
| [`pomona/`](pomona/README.md) | **The application firmware (v1, #229):** MQTT publishing to the cluster broker, LVGL current-readings screen (idle blanking #248), basic OTA (#243 slice). Own README in the sketch folder. |
| [`bringup/`](bringup/bringup.ino) | Bench verification: boot I²C scan + read every v1 sensor, print a status block over serial every 2 s. No WiFi/MQTT, no display. |
| [`leveltest/`](leveltest/leveltest.ino) | Grove level strip calibration: dumps all 20 raw pad values 1×/s for mount testing + threshold tuning (procedure in [docs/sensors/level-strip.md](../docs/sensors/level-strip.md)). |
| [`levelprobe/`](levelprobe/levelprobe.ino) | CQRSENYW003 photoelectric probe test: prints output frequency + decoded submerged points 1×/s ([docs/sensors/level-probe.md](../docs/sensors/level-probe.md)). |

## Shared libraries

Sensor drivers shared between sketches live in [`libraries/`](libraries/):

| Library | Purpose |
|---|---|
| [`GroveWaterLevel`](libraries/GroveWaterLevel/src/GroveWaterLevel.h) | Grove 10 cm level strip driver (0x77+0x78): raw pads, wet threshold, wet count / percent / depth. |
| [`PhotoLevelProbe`](libraries/PhotoLevelProbe/src/PhotoLevelProbe.h) | CQRSENYW003 contact photoelectric probe driver: frequency measurement → submerged points 0–4. |
| [`PomonaCalibration`](libraries/PomonaCalibration/src/PomonaCalibration.h) | EC/pH calibration constants — single source of truth, record calibration results here (procedures: [docs/sensors/](../docs/sensors/README.md)). |
| [`PomonaVersion`](libraries/PomonaVersion/src/PomonaVersion.h) | Firmware semver — see Versioning below. |

**Arduino IDE:** set File → Preferences → *Sketchbook location* to this
`firmware/` folder — the IDE then picks up `libraries/` automatically.
**arduino-cli:** pass `--libraries libraries` (as below).

Firmware v1 proper lives in [`pomona/`](pomona/README.md); the MQTT topic
schema it publishes is [docs/mqtt.md](../docs/mqtt.md) (finalized with #222).

## Toolchain

Arduino IDE 2.x or `arduino-cli`. One-time setup:

```sh
arduino-cli core install arduino:mbed_giga
arduino-cli lib install "Adafruit BME280 Library" "BH1750" "OneWire" "DallasTemperature"
arduino-cli lib install ArduinoMqttClient lvgl Arduino_GigaDisplayTouch Arduino_GigaDisplay Arduino_Portenta_OTA
```

(`Adafruit BME280 Library` pulls in Adafruit Unified Sensor + BusIO as
dependencies. `BH1750` is the Christopher Laws library. The Grove water
level strip needs no library — raw `Wire` reads. The second lib line is
only needed for the [`pomona`](pomona/README.md) app sketch;
`Arduino_H7_Video` + `lv_conf.h` ship with the core.)

Build & flash:

```sh
arduino-cli compile --fqbn arduino:mbed_giga:giga --libraries libraries bringup
arduino-cli upload -p /dev/ttyACM0 --fqbn arduino:mbed_giga:giga bringup
arduino-cli monitor -p /dev/ttyACM0 -c baudrate=115200
```

(run from this `firmware/` directory; same commands with `leveltest`.)

On Windows the port is `COMx` (check `arduino-cli board list`).

## Versioning + deploying

The firmware version (semver, single source of truth) lives in
[`libraries/PomonaVersion/src/PomonaVersion.h`](libraries/PomonaVersion/src/PomonaVersion.h);
every sketch prints it in its boot banner. **Deploy through
[`deploy.ps1`](deploy.ps1)** (Windows PowerShell — the board's USB usually
lands there) **or [`deploy.sh`](deploy.sh)** (Linux/WSL) — the two scripts
are behaviorally equivalent (same bump semantics, same version file) and
must be kept in sync. Both auto-bump the version on every deploy and then
build + flash:

```powershell
.\deploy.ps1                     # patch bump, bringup -> COM6
.\deploy.ps1 -Sketch levelprobe  # other sketch
.\deploy.ps1 -Bump minor         # features; -Bump major for breaking
.\deploy.ps1 -Bump none          # reflash without bumping
```

```sh
./deploy.sh                      # patch bump, bringup -> /dev/ttyACM0
./deploy.sh -s pomona            # other sketch
./deploy.sh -b minor             # features; -b major / -b none
./deploy.sh -p /dev/ttyACM1      # other port (or POMONA_PORT env)
```

`deploy.sh` needs a Linux `arduino-cli` on PATH (it will print an install
hint; the repo-referenced `arduino-cli.exe` is Windows-only). ⚠ USB
flashing from WSL requires attaching the board to WSL first:
`usbipd list` + `usbipd attach --wsl --busid <id>` on the Windows side.

Commit the bumped `PomonaVersion.h` together with the change it ships.
Flashing via the IDE skips the bump — use the scripts. The OTA flow (#243)
reuses the same version to decide whether an update is due; the basic
MQTT-triggered OTA slice ships with [`pomona`](pomona/README.md).

## Secrets

Passwords never go in git: copy
[`secrets.h.example`](secrets.h.example) into the sketch folder as
`secrets.h` (gitignored) and fill in `WIFI_PASS` + `MQTT_PASS` — that is
all it holds since #251; the non-secret connection settings (SSID, MQTT
host/port/user) are committed in the sketch's `config.h`. Target design
moves the passwords into the ATECC608A secure element — see
[docs/ota-and-secrets.md](../docs/ota-and-secrets.md).

## Calibration constants

`EC_CAL_K`, `PH_V_NEUTRAL`, `PH_V_ACID` live in the shared
[`PomonaCalibration`](libraries/PomonaCalibration/src/PomonaCalibration.h)
library (moved out of `bringup.ino` with #229) so every sketch reads the
same values — record calibration results there. Procedures and recorded
values live in the per-sensor docs under
[docs/sensors/](../docs/sensors/README.md) ([ec-tds.md](../docs/sensors/ec-tds.md),
[ph.md](../docs/sensors/ph.md)).
