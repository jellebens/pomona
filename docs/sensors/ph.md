# pH (continuous) — DFRobot Gravity pH Pro SEN0169-V2 + DFR0504 isolator

**Status: ✅ wired + two-point calibrated on the assembled unit
(2026-08-28, #254).** Industrial probe rated for 24/7 immersion. The
PH-201H handheld pen is the calibration cross-check. Target: pH ~6.0
(window 5.8–6.2) — see [planting-plan.md](../planting-plan.md).

## Wiring

```
probe (in tank) ─BNC→ pH Pro transmitter board ─3-pin→ DFR0504 isolator ─3-pin→ GIGA
                                                                            (A1 + 3V3 + GND)
```

Analog out → **A1**. Gravity analog cables: black = GND, red = VCC (3V3),
blue = signal.

The DFR0504 galvanically isolates supply *and* signal. It is **not
optional**: the pH and [TDS](ec-tds.md) probes share the same 10 L of
water, and without isolation the TDS probe's excitation couples into the
pH reading (ground loop).

## Calibration (two-point)

The buffer anchor values are the `PH_BUF_*` constants in
`PomonaCalibration` — set them to the pH PRINTED on the packets actually
used (the owner's set is **7.00 / 4.00**, not the 6.86/4.01 family).

Probe in the neutral buffer → record `PH_V_NEUTRAL`; rinse (demin, dab —
never rub the glass bulb); probe in the acid buffer → record `PH_V_ACID`
(constants in `PomonaCalibration`; slope/offset derived in the sketch).
~20 ml per buffer in a narrow vessel suffices; let each reading settle
fully — the 4.00 point crept 2.08→2.09 V over the last minute. From
v0.1.27 the raw voltage publishes as `pomona/water/ph_raw_v`, so
recalibration is doable remotely. Recalibrate roughly monthly;
cross-check any odd reading with the PH-201H pen before believing it.
Calibrate with the [DS18B20](water-temp.md) in the same liquid.

| Date | PH_V_NEUTRAL | PH_V_ACID | Notes |
|---|---|---|---|
| 2026-08-28 | **1.39 V** @ 7.00, ~28.0 °C | **2.09 V** @ 4.00, ~25.7 °C | first calibration, assembled unit; slope −4.29 pH/V |

## Care

The probe is a consumable (6–12 months). Store with the KCl cap on
whenever the system is drained. Leave the BNC capped until the analog
chain is verified (first-power-on checklist in [wiring.md](../wiring.md)).
