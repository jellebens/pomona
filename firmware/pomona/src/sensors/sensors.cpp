// Pomona firmware v1 — sensor module implementation (Trello #229).
//
// Reading logic proven in firmware/bringup (card #220/#242); calibration
// constants come from the shared PomonaCalibration library. Every sensor
// is optional at runtime: a failed init is retried on the next sweep, so
// plugging a sensor in later recovers without a reboot.

#include "sensors.h"
#include "../../config.h"

#include <Wire.h>
#include <Adafruit_BME280.h>
#include <BH1750.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <GroveWaterLevel.h>   // libraries/GroveWaterLevel
#include <PhotoLevelProbe.h>   // libraries/PhotoLevelProbe
#include <PomonaCalibration.h> // EC_CAL_K, PH_V_NEUTRAL, PH_V_ACID

static Adafruit_BME280 bme;
static BH1750 lux(ADDR_BH1750);
static OneWire oneWire(PIN_ONEWIRE);
static DallasTemperature ds18b20(&oneWire);
static GroveWaterLevel level(Wire, GroveWaterLevel::DEFAULT_WET_THRESHOLD);
static PhotoLevelProbe probe(PIN_PROBE);

static bool bmeUp = false;
static bool luxUp = false;
static bool busStuck = false; // SDA/SCL held low by bad wiring — skip I2C

// ---- helpers ---------------------------------------------------------

#include <mbed.h>
static void kickWd() { mbed::Watchdog::get_instance().kick(); }

#ifndef PIN_WIRE_SDA
#define PIN_WIRE_SDA 20
#endif
#ifndef PIN_WIRE_SCL
#define PIN_WIRE_SCL 21
#endif

// A miswired run (data line on GND, short, pinched cable) holds SDA or SCL
// low; every I2C transaction then waits out its timeout and the 127-address
// scan outlives the 30 s watchdog — the bench boot-loop of 2026-08-28.
// Check the raw pins BEFORE first I2C use and skip the bus entirely if
// wedged; fixing the wiring needs a power cycle to re-enable I2C.
static bool checkBusStuck() {
  pinMode(PIN_WIRE_SDA, INPUT_PULLUP);
  pinMode(PIN_WIRE_SCL, INPUT_PULLUP);
  delay(2);
  bool stuck = digitalRead(PIN_WIRE_SDA) == LOW ||
               digitalRead(PIN_WIRE_SCL) == LOW;
  if (stuck)
    Serial.println("I2C: SDA/SCL held LOW — check wiring; skipping I2C "
                   "until the next power cycle");
  return stuck;
}

static void scanI2C() {
  Serial.println("I2C scan (Wire): expecting 0x23 BH1750, 0x76/0x77 BME280");
  int found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    kickWd(); // a sick bus can make every address wait out a timeout
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.print("  found 0x");
      Serial.println(addr, HEX);
      found++;
    }
  }
  if (found == 0) Serial.println("  NOTHING found — check 3V3/GND and SDA/SCL");
}

int sensorsI2CScan(char *out, size_t outLen) {
  if (busStuck) {
    snprintf(out, outLen, "SDA/SCL stuck LOW - check wiring");
    return 0;
  }
  int found = 0;
  size_t used = 0;
  if (outLen > 0) out[0] = '\0';
  for (uint8_t addr = 1; addr < 127; addr++) {
    kickWd();
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      int n = snprintf(out + used, outLen > used ? outLen - used : 0,
                       "%s0x%02X", found ? "," : "", addr);
      if (n > 0) used += (size_t)n;
      found++;
    }
  }
  return found;
}

static float readVoltageAvg(int pin, int samples = 32) {
  uint32_t sum = 0;
  for (int i = 0; i < samples; i++) {
    sum += analogRead(pin);
    delay(2);
  }
  return (sum / (float)samples) * VREF / ADC_MAX;
}

// Grove TDS: cubic ppm curve (TDS-500 scale), temp-compensated to 25 C.
// Returns EC in mS/cm.
static float readEC(float waterTempC, float &rawVolts) {
  rawVolts = readVoltageAvg(PIN_TDS);
  float comp = 1.0f + 0.02f * (waterTempC - 25.0f);
  float v = rawVolts / comp;
  float tdsPpm = (133.42f * v * v * v - 255.86f * v * v + 857.39f * v) * 0.5f;
  float ecMs = tdsPpm * 2.0f / 1000.0f; // undo 0.5 TDS factor -> uS/cm -> mS/cm
  return ecMs * EC_CAL_K;
}

// pH: two-point linear between the recorded buffer voltages. NAN until the
// calibration voltages are recorded in PomonaCalibration.h.
static float readPH(float &rawVolts) {
  rawVolts = readVoltageAvg(PIN_PH);
  if (isnan(PH_V_NEUTRAL) || isnan(PH_V_ACID)) return NAN;
  float slope = (4.01f - 6.86f) / (PH_V_ACID - PH_V_NEUTRAL);
  return 6.86f + (rawVolts - PH_V_NEUTRAL) * slope;
}

// Absent-sensor re-probes are rate-limited to SENSOR_REINIT_MS: retrying
// begin() every 5 s sweep spammed the log (the BH1750 driver prints a
// NACK error per failed begin). Hot-plug is still picked up, just within
// a minute instead of 5 s.
static bool reprobeDue(uint32_t &lastAttemptMs) {
  uint32_t now = millis();
  if (lastAttemptMs != 0 && now - lastAttemptMs < SENSOR_REINIT_MS)
    return false;
  lastAttemptMs = now;
  return true;
}

static bool tryBme() {
  if (busStuck) return false;
  static uint32_t lastAttemptMs = 0;
  if (!bmeUp && reprobeDue(lastAttemptMs))
    // strapped boards sit at 0x76; an unstrapped BME280 defaults to 0x77
    // (free on this unit — the Grove level strip is not used)
    bmeUp = bme.begin(ADDR_BME280, &Wire) || bme.begin(ADDR_BME280_ALT, &Wire);
  return bmeUp;
}

static bool tryLux() {
  if (busStuck) return false;
  static uint32_t lastAttemptMs = 0;
  if (!luxUp && reprobeDue(lastAttemptMs))
    luxUp = lux.begin(BH1750::CONTINUOUS_HIGH_RES_MODE, ADDR_BH1750, &Wire);
  return luxUp;
}

// ---- public API ------------------------------------------------------

void sensorsInit() {
  analogReadResolution(ADC_BITS);

  busStuck = checkBusStuck();
  if (!busStuck) {
    Wire.begin();
    scanI2C();
    if (!tryBme())
      Serial.println("BME280 NOT FOUND at 0x76 or 0x77 — check wiring");
    if (!tryLux())
      Serial.println("BH1750 NOT FOUND at 0x23");
  }

  kickWd();
  ds18b20.begin();
  if (ds18b20.getDeviceCount() == 0)
    Serial.println("DS18B20 NOT FOUND on D2 — check 4.7k pull-up");

  probe.begin();
}

void sensorsRead(Readings &r) {
  // water temperature first: EC compensation needs it
  if (ds18b20.getDeviceCount() == 0) ds18b20.begin(); // re-search: hot-plug
  ds18b20.requestTemperatures();
  float waterC = ds18b20.getTempCByIndex(0); // DEVICE_DISCONNECTED_C = -127
  r.waterTempOk = waterC > -100.0f;
  r.waterTempC = r.waterTempOk ? waterC : NAN;

  r.ecMsCm = readEC(r.waterTempOk ? waterC : 25.0f, r.ecRawV);
  r.ph = readPH(r.phRawV);
  r.phOk = !isnan(r.ph);

  r.levelOk = busStuck ? false : level.read(); // Grove strip is I2C too
  r.levelPct = r.levelOk ? level.percent() : -1;

  r.probePoints = probe.points(); // blocks ~250 ms max when no signal

  r.bmeOk = tryBme();
  if (r.bmeOk) {
    r.airTempC = bme.readTemperature();
    r.humidityPct = bme.readHumidity();
    r.pressureHpa = bme.readPressure() / 100.0f;
    // A flaky cable can drop the sensor after a good init: failed reads
    // come back NaN or wildly implausible. Drop the flag so the 60 s
    // re-probe owns recovery and pomona/unit/sensors stays honest.
    if (isnan(r.airTempC) || isnan(r.humidityPct) ||
        r.airTempC < -40.0f || r.airTempC > 85.0f ||
        r.pressureHpa < 300.0f || r.pressureHpa > 1200.0f) {
      bmeUp = false;
      r.bmeOk = false;
      r.airTempC = r.humidityPct = r.pressureHpa = NAN;
    }
  } else {
    r.airTempC = r.humidityPct = r.pressureHpa = NAN;
  }

  r.luxOk = tryLux();
  if (r.luxOk) {
    float lx = lux.readLightLevel();
    if (lx < 0) { // driver reports errors as negative
      r.luxOk = false;
      r.lux = NAN;
      luxUp = false; // same flaky-cable honesty as the BME280
    } else {
      r.lux = lx;
    }
  } else {
    r.lux = NAN;
  }
}
