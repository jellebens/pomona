# EC / nutrients — Grove TDS

**Status: to wire; calibration blocked on the 1413 µS/cm fluid** (still to
buy, see #220 purchases). With the demin-water baseline, EC ≈ nutrient
concentration directly. Targets: EC 1.4–1.6 mS/cm shared (≈1.2 while
strawberry-only) — see [planting-plan.md](../planting-plan.md).

## Wiring

Analog: Grove yellow (signal) → **A0**, red → **3V3 (not 5 V — the GIGA's
ADC tolerates max 3.3 V)**, black → GND, white unused. Probe in the tank
(away from the [pH probe](ph.md) — the DFR0504 isolator on the pH side
prevents their ground loop), board dry above the waterline.

## Conversion & calibration

Firmware applies the standard cubic TDS curve with temperature
compensation from the [DS18B20](water-temp.md) (0.02/°C to 25 °C), scaled
by `EC_CAL_K` in `bringup.ino`.

**Single-point calibration:** probe in the 1413 µS/cm fluid, note water
temp, set `EC_CAL_K` so the compensated reading shows 1.413 mS/cm. Redo
only if readings become suspicious.

| Date | EC_CAL_K | Fluid temp | Notes |
|---|---|---|---|
| _(pending first calibration)_ | | | |
