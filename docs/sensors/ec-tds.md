# EC / nutrients — Grove TDS

**Status: ✅ wired + calibrated on the assembled unit (2026-08-28, #254).**
With the demin-water baseline, EC ≈ nutrient concentration directly.
Targets: EC 1.4–1.6 mS/cm shared (≈1.2 while strawberry-only) — see
[planting-plan.md](../planting-plan.md).

## Wiring (as built)

Analog: signal → **A2** (perm wiring 2026-09-10; bench was A0 — a flat
phantom ~0.15 mS/cm after the move was the firmware still reading the
floating A0, fixed in fw v1.3.6), Vcc → **3V3 (not 5 V — the GIGA's ADC
tolerates max 3.3 V)**, GND → GND. The as-built cable is a 3-wire black/red/green
set: **green = signal, red = 3V3, black = GND** — when in doubt, follow
the TDS board's own silkscreen (GND/VCC/A), not wire colors. Probe in the
tank (away from the [pH probe](ph.md) — the DFR0504 isolator on the pH
side prevents their ground loop), board dry above the waterline.

⚠ First hookup read a flat ~0.01 mS/cm in calibration fluid — a
no-signal symptom (wrong signal wire/pin), not a calibration problem.
A plausible uncalibrated reading in 1413 µS/cm fluid is ~1–2 mS/cm.

## Between pump runs (2.4.4)

The probe reads the circulation pump: while it runs the reading climbs (1.43 → 1.81 and
1.63 → 1.96 mS/cm on the tower, the step within seconds of each switch), with the pump off it is
steady. pH is isolated (DFR0504) and is not affected; this board is not. Until it gets its own
isolator, the published `ec_ms_cm` is taken only with the pump off for 2 min and held through the
next run (`src/sensors/ec_hold.h`; dropped after 45 min); `ec_live_ms_cm` is every sweep's raw
reading. With an isolator fitted the two should agree pump on and off — then the hold can go.
Fitting one is a wiring change: recalibrate (below).

## Conversion & calibration

Firmware applies the standard cubic TDS curve with temperature
compensation from the [DS18B20](water-temp.md) (0.02/°C to 25 °C), scaled
by `EC_CAL_K` in `PomonaCalibration` (shared by bringup + pomona).

**Single-point calibration:** TDS probe AND DS18B20 tip together in a cup
of the 1413 µS/cm fluid (**20 ml in a narrow vessel proved enough**,
2026-08-28 — electrodes fully covered is the only requirement; never dip
in the bottle — contaminates the standard). Let the probe settle until the
reading is flat (~8 min; a gentle swirl dislodges electrode bubbles),
then set `EC_CAL_K = 1.413 / reading`. Redo only if readings become
suspicious.

| Date | EC_CAL_K | Fluid temp | Notes |
|---|---|---|---|
| 2026-08-28 | **0.9745** | 25.6 °C | first calibration, assembled unit; stable 1.45 mS/cm raw (K=1.0) after ~8 min settling, drift 1.57→1.45 while settling |
| 2026-09-10 | **0.7692** | 26.2 °C | post-Cat5e perm wiring (signal → A2, fw 1.3.6/1.3.7); stable 1.79 mS/cm with old K from first reading (no settle drift this time); 21% shift = the new cable run — recalibrate after any wiring change |
