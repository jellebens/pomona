# Sensor unit wiring & bring-up — Arduino GIGA R1 WiFi

Status: wiring plan finalized 2026-08-07 (Trello #220). Covers the settled
sensor loadout v1 from [design.md](design.md); the matching bench-verification
sketch lives in [`firmware/bringup/`](../firmware/bringup/bringup.ino).

Scope: wiring + serial bring-up + calibration only. MQTT publishing and the
LVGL screen are the firmware v1 card; cluster-side ingestion is #222.

## Ground rules

- The GIGA is **3.3 V logic and its ADC pins tolerate max 3.3 V**. Every
  sensor here runs off the **3V3 pin** — nothing connects to 5 V.
- External sensors use the main **`Wire`** bus (the dedicated SDA/SCL header
  pins, same bus as D20/D21). The Display Shield's own peripherals (GT911
  touch, BMI270 IMU) sit on `Wire1`, so they never clash with our sensors.
- The Display Shield mounts on the underside of the GIGA; the pin headers
  stay accessible.

## Pin map

| Sensor | Interface | GIGA connection | Address | Power |
|---|---|---|---|---|
| BME280 (air T/RH/P, STEMMA QT) | I²C | SDA/SCL header pins | **0x76 — strap required, see below** | 3V3 |
| BH1750 (light, STEMMA QT) | I²C | QT daisy-chain off the BME280 | 0x23 (ADDR low, default) | 3V3 |
| Grove water level 10 cm | I²C | SDA/SCL header pins | 0x77 (low 8 pads) + 0x78 (high 12 pads) | 3V3 |
| Grove TDS (EC) | analog | **A0** | — | **3V3, not 5 V** |
| pH — SEN0169-V2 via DFR0504 isolator | analog | **A1** | — | 3V3 |
| DS18B20 (water temp) | 1-Wire | **D2**, 4.7 kΩ pull-up D2→3V3 | — | 3V3 |

I²C address map after strapping: `0x23` BH1750 · `0x76` BME280 · `0x77` +
`0x78` level strip. No conflicts.

### ⚠ BME280 must be strapped to 0x76 BEFORE first power-on

The Adafruit BME280 defaults to **0x77**, which the level strip already
occupies (its low-8-pad bank). Two devices on 0x77 means garbage reads on
*both*. Bridge the **SDO solder jumper** on the back of the BME280 breakout
to move it to 0x76. If the bring-up scan shows no 0x76, or BME/level values
look insane, this strap is the first suspect.

## Cable color conventions

- **STEMMA QT** (BME280 → BH1750 chain, QT→male-dupont to the header):
  black = GND, red = 3V3, **blue = SDA, yellow = SCL**.
- **Grove** (both Grove sensors, Grove→male-jumper cables):
  black = GND, red = VCC (**wire to 3V3**), yellow = pin 1 (SCL on I²C /
  signal on analog), white = pin 2 (SDA on I²C / NC on analog).
  - Grove water level: yellow → SCL, white → SDA.
  - Grove TDS: yellow → A0, white unused.

## The pH chain

```
probe (in tank) ─BNC→ pH Pro transmitter board ─3-pin→ DFR0504 isolator ─3-pin→ GIGA
                                                                            (A1 + 3V3 + GND)
```

The DFR0504 galvanically isolates supply *and* signal. It is **not
optional**: the pH and TDS probes share the same 10 L of water, and without
isolation the TDS probe's excitation couples into the pH reading (ground
loop). Gravity analog cables: black = GND, red = VCC, blue = signal.

## Physical placement

- **In the tank:** pH probe, TDS probe, DS18B20 tip — spaced apart, pH and
  TDS at opposite sides if possible. All electronics boards stay dry above
  the waterline.
- **Level strip:** the PCB is not waterproof — mount it in a waterproof
  sleeve inside the tank, or against the outside of a thin non-metal wall.
  It covers the bottom 10 cm; a full reservoir (~16 cm) tops out the range,
  which is fine — the strip exists to catch *low* water (the pump-runs-dry
  failure mode).
- **BME280 + BH1750:** near the tower but out of splash range; BH1750
  facing up, roughly at canopy height, so it sees what the plants see.
- pH probe is a consumable (6–12 months) — store with the KCl cap on
  whenever the system is drained.

## First power-on checklist

1. Wire everything **except** the pH transmitter's probe (leave the BNC
   capped); flash `firmware/bringup`, open serial at 115200 baud.
2. Boot I²C scan must report exactly **0x23, 0x76, 0x77, 0x78**.
   - Missing 0x76 → BME280 strap (see above) or QT chain power.
   - Missing 0x77/0x78 → Grove level cable (yellow=SCL/white=SDA swapped is
     the classic mistake).
3. Sanity-check each reading:
   - breathe on the BME280 → humidity jumps;
   - cover the BH1750 → lux drops to ~0;
   - warm the DS18B20 tip in your hand → water temp rises;
   - TDS probe in the 1413 µS/cm fluid → EC ≈ 1.41 mS/cm after calibration;
   - connect the pH probe, into pH 6.86 buffer → stable voltage.
4. Only then move probes to the reservoir.

## Calibration

Constants live at the top of `bringup.ino` (later: the real firmware's
config) — record the measured values in this doc when done.

- **EC, single-point:** probe in the 1413 µS/cm fluid, note water temp, set
  `EC_CAL_K` so the compensated reading shows 1.413 mS/cm. Redo only if
  readings become suspicious.
- **pH, two-point:** probe in 6.86 buffer → record `PH_V_NEUTRAL`; rinse,
  probe in 4.01 buffer → record `PH_V_ACID`. Slope/offset are derived in the
  sketch. Recalibrate roughly monthly; cross-check any odd reading with the
  PH-201H pen before believing it.
- Temperature compensation for EC (and pH) uses the DS18B20 automatically —
  calibrate with the probe and the DS18B20 in the same liquid.

### Recorded calibration values

| Date | EC_CAL_K | PH_V_NEUTRAL (V @ 6.86) | PH_V_ACID (V @ 4.01) | Notes |
|---|---|---|---|---|
| _(pending first calibration)_ | | | | |

## Out of scope here

- **Pump watts:** Fibaro Wall Plug Type E → Home Assistant zwave_js; nothing
  wires to the GIGA.
- **MQTT + screen:** firmware v1 card (topics/schema decided in #222).
