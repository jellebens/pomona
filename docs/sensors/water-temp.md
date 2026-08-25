# Water temperature — DS18B20

**Status: to wire** (sensor on hand). Besides its own reading, this sensor
temperature-compensates [EC](ec-tds.md) and cross-checks [pH](ph.md) —
calibrate those with the DS18B20 in the same liquid.

Waterproof probe version (steel tip + cable) goes in the tank; a bare
TO-92 chip version is NOT submersible.

## Wiring

1-Wire: data → **D2** with a **4.7 kΩ pull-up from D2 to 3V3**; VCC → 3V3,
GND → GND.

## Verification

`bringup` reports it at boot ("DS18B20 NOT FOUND on D2" = check the
pull-up). Warm the tip in your hand → the water-temp reading rises. No
calibration needed (factory ±0.5 °C).

## Placement

Tip in the tank, spaced away from the pH and TDS probes.
