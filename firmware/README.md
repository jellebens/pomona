# Pomona firmware

Target: **Arduino GIGA R1 WiFi** (+ GIGA Display Shield). Wiring for
everything below is in [docs/wiring.md](../docs/wiring.md).

## Sketches

| Sketch | Purpose |
|---|---|
| [`bringup/`](bringup/bringup.ino) | Bench verification: boot I²C scan + read every v1 sensor, print a status block over serial every 2 s. No WiFi/MQTT, no display. |

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
arduino-cli compile --fqbn arduino:mbed_giga:giga bringup
arduino-cli upload -p /dev/ttyACM0 --fqbn arduino:mbed_giga:giga bringup
arduino-cli monitor -p /dev/ttyACM0 -c baudrate=115200
```

On Windows the port is `COMx` (check `arduino-cli board list`).

## Calibration constants

`EC_CAL_K`, `PH_V_NEUTRAL`, `PH_V_ACID` at the top of `bringup.ino` —
procedure and the recorded values live in
[docs/wiring.md](../docs/wiring.md#calibration).
