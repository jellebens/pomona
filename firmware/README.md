# Pomona firmware

Target: **Arduino GIGA R1 WiFi** (+ GIGA Display Shield). Wiring for
everything below is in [docs/wiring.md](../docs/wiring.md).

## Sketches

| Sketch | Purpose |
|---|---|
| [`bringup/`](bringup/bringup.ino) | Bench verification: boot I²C scan + read every v1 sensor, print a status block over serial every 2 s. No WiFi/MQTT, no display. |
| [`leveltest/`](leveltest/leveltest.ino) | Grove level strip calibration: dumps all 20 raw pad values 1×/s for mount testing + threshold tuning (procedure in [docs/sensors/level-strip.md](../docs/sensors/level-strip.md)). |
| [`levelprobe/`](levelprobe/levelprobe.ino) | CQRSENYW003 photoelectric probe test: prints output frequency + decoded submerged points 1×/s ([docs/sensors/level-probe.md](../docs/sensors/level-probe.md)). |

## Shared libraries

Sensor drivers shared between sketches live in [`libraries/`](libraries/):

| Library | Purpose |
|---|---|
| [`GroveWaterLevel`](libraries/GroveWaterLevel/src/GroveWaterLevel.h) | Grove 10 cm level strip driver (0x77+0x78): raw pads, wet threshold, wet count / percent / depth. |
| [`PhotoLevelProbe`](libraries/PhotoLevelProbe/src/PhotoLevelProbe.h) | CQRSENYW003 contact photoelectric probe driver: frequency measurement → submerged points 0–4. |

**Arduino IDE:** set File → Preferences → *Sketchbook location* to this
`firmware/` folder — the IDE then picks up `libraries/` automatically.
**arduino-cli:** pass `--libraries libraries` (as below).

Firmware v1 proper (MQTT publish + LVGL current-readings screen) comes after
bring-up, once the #222 topic schema exists.

## Toolchain

Arduino IDE 2.x or `arduino-cli`. One-time setup:

```sh
arduino-cli core install arduino:mbed_giga
arduino-cli lib install "Adafruit BME280 Library" "BH1750" "OneWire" "DallasTemperature"
```

(`Adafruit BME280 Library` pulls in Adafruit Unified Sensor + BusIO as
dependencies. `BH1750` is the Christopher Laws library. The Grove water
level strip needs no library — raw `Wire` reads.)

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
[`deploy.ps1`](deploy.ps1)** (Windows PowerShell — the board's USB lands
there), which auto-bumps the version on every deploy and then
builds + flashes:

```powershell
.\deploy.ps1                     # patch bump, bringup -> COM6
.\deploy.ps1 -Sketch levelprobe  # other sketch
.\deploy.ps1 -Bump minor         # features; -Bump major for breaking
.\deploy.ps1 -Bump none          # reflash without bumping
```

Commit the bumped `PomonaVersion.h` together with the change it ships.
Flashing via the IDE skips the bump — use the script. The OTA flow (#243)
will reuse the same version to decide whether an update is due.

## Secrets

WiFi/MQTT credentials never go in git: copy
[`secrets.h.example`](secrets.h.example) into the sketch folder as
`secrets.h` (gitignored) and fill it in. Target design moves them into
the ATECC608A secure element — see
[docs/ota-and-secrets.md](../docs/ota-and-secrets.md).

## Calibration constants

`EC_CAL_K`, `PH_V_NEUTRAL`, `PH_V_ACID` at the top of `bringup.ino` —
procedures and recorded values live in the per-sensor docs under
[docs/sensors/](../docs/sensors/README.md) ([ec-tds.md](../docs/sensors/ec-tds.md),
[ph.md](../docs/sensors/ph.md)).
