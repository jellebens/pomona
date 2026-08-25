// Pomona sensor unit — bring-up sketch (Trello #220)
//
// Bench verification only: scans the I2C bus at boot, then reads every v1
// sensor and prints a status block over serial every 2 s. No WiFi, no MQTT,
// no display. Wiring: docs/wiring.md.
//
// Board: Arduino GIGA R1 WiFi (arduino:mbed_giga:giga)
// Libraries: Adafruit BME280 Library, BH1750 (Claws), OneWire, DallasTemperature

#include <Wire.h>
#include <Adafruit_BME280.h>
#include <BH1750.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <GroveWaterLevel.h> // libraries/GroveWaterLevel (sketchbook = firmware/)
#include <PhotoLevelProbe.h> // libraries/PhotoLevelProbe — CQRSENYW003 low-water ladder

// ---- pins ------------------------------------------------------------
const int PIN_TDS = A0;      // Grove TDS, powered from 3V3
const int PIN_PH = A1;       // SEN0169-V2 via DFR0504 isolator
const int PIN_ONEWIRE = 2;   // DS18B20 data, 4.7k pull-up to 3V3
const int PIN_PROBE = 3;     // CQRSENYW003 green wire (open collector)

// ---- ADC -------------------------------------------------------------
const float VREF = 3.3f;
const int ADC_BITS = 12;
const float ADC_MAX = 4095.0f;

// ---- I2C addresses ---------------------------------------------------
const uint8_t ADDR_BME280 = 0x76; // strapped! 0x77 belongs to the level strip
// Level strip (0x77+0x78) lives in libraries/GroveWaterLevel; set the wet
// threshold below once calibrated per docs/wiring.md.
const uint8_t LEVEL_WET_THRESHOLD = GroveWaterLevel::DEFAULT_WET_THRESHOLD;

// ---- calibration (see docs/sensors/) ---------------------------------
// EC: single-point against 1413 uS/cm fluid. 1.0 = uncalibrated.
const float EC_CAL_K = 1.0f;
// pH: two-point. Record the measured voltages in the buffers; NAN = not yet
// calibrated, sketch prints raw voltage only.
const float PH_V_NEUTRAL = NAN; // volts in pH 6.86 buffer
const float PH_V_ACID = NAN;    // volts in pH 4.01 buffer

Adafruit_BME280 bme;
BH1750 lux(0x23);
OneWire oneWire(PIN_ONEWIRE);
DallasTemperature ds18b20(&oneWire);
GroveWaterLevel level(Wire, LEVEL_WET_THRESHOLD);
PhotoLevelProbe probe(PIN_PROBE);

bool bmeOk = false;
bool luxOk = false;

// ---- helpers ---------------------------------------------------------

void scanI2C() {
  Serial.println("I2C scan (Wire): expecting 0x23 BH1750, 0x76 BME280, 0x77+0x78 level strip");
  int found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.print("  found 0x");
      Serial.println(addr, HEX);
      found++;
    }
  }
  if (found == 0) Serial.println("  NOTHING found — check 3V3/GND and SDA/SCL");
}

float readVoltageAvg(int pin, int samples = 32) {
  uint32_t sum = 0;
  for (int i = 0; i < samples; i++) {
    sum += analogRead(pin);
    delay(2);
  }
  return (sum / (float)samples) * VREF / ADC_MAX;
}

// Grove TDS: cubic ppm curve (TDS-500 scale), temp-compensated to 25 C.
// Returns EC in mS/cm.
float readEC(float waterTempC, float &rawVolts) {
  rawVolts = readVoltageAvg(PIN_TDS);
  float comp = 1.0f + 0.02f * (waterTempC - 25.0f);
  float v = rawVolts / comp;
  float tdsPpm = (133.42f * v * v * v - 255.86f * v * v + 857.39f * v) * 0.5f;
  float ecMs = tdsPpm * 2.0f / 1000.0f; // undo the 0.5 TDS factor -> uS/cm -> mS/cm
  return ecMs * EC_CAL_K;
}

// pH: two-point linear between the recorded buffer voltages.
float readPH(float &rawVolts) {
  rawVolts = readVoltageAvg(PIN_PH);
  if (isnan(PH_V_NEUTRAL) || isnan(PH_V_ACID)) return NAN;
  float slope = (4.01f - 6.86f) / (PH_V_ACID - PH_V_NEUTRAL);
  return 6.86f + (rawVolts - PH_V_NEUTRAL) * slope;
}

// ---- setup / loop ----------------------------------------------------

void setup() {
  Serial.begin(115200);
  unsigned long t0 = millis();
  while (!Serial && millis() - t0 < 5000) {}
  Serial.println();
  Serial.println("=== Pomona bring-up (docs/wiring.md) ===");

  analogReadResolution(ADC_BITS);
  Wire.begin();
  scanI2C();

  bmeOk = bme.begin(ADDR_BME280, &Wire);
  if (!bmeOk) Serial.println("BME280 NOT FOUND at 0x76 — SDO strap missing? (0x77 = level strip!)");

  luxOk = lux.begin(BH1750::CONTINUOUS_HIGH_RES_MODE, 0x23, &Wire);
  if (!luxOk) Serial.println("BH1750 NOT FOUND at 0x23");

  ds18b20.begin();
  if (ds18b20.getDeviceCount() == 0)
    Serial.println("DS18B20 NOT FOUND on D2 — check 4.7k pull-up");

  probe.begin();
}

void loop() {
  // water temperature first: EC compensation needs it
  ds18b20.requestTemperatures();
  float waterC = ds18b20.getTempCByIndex(0); // DEVICE_DISCONNECTED_C = -127 on failure
  bool waterTempOk = waterC > -100.0f;

  float tdsV, phV;
  float ec = readEC(waterTempOk ? waterC : 25.0f, tdsV);
  float ph = readPH(phV);
  level.read();
  int levelPct = level.percent();
  uint32_t padBitmap = level.bitmap();

  Serial.println("---");
  Serial.print("water: temp=");
  waterTempOk ? Serial.print(waterC, 1) : Serial.print("ERR");
  Serial.print(" C  EC=");
  Serial.print(ec, 2);
  Serial.print(" mS/cm (raw ");
  Serial.print(tdsV, 3);
  Serial.print(" V)  pH=");
  isnan(ph) ? Serial.print("UNCAL") : Serial.print(ph, 2);
  Serial.print(" (raw ");
  Serial.print(phV, 3);
  Serial.print(" V)  level=");
  if (levelPct < 0) {
    Serial.print("ERR");
  } else {
    Serial.print(levelPct);
    Serial.print("% pads=0b");
    Serial.print(padBitmap, BIN);
  }
  int probePts = probe.points();
  Serial.print("  probe=");
  if (probePts < 0) Serial.println("no signal");
  else {
    Serial.print(probePts);
    Serial.println("/4 pts");
  }

  Serial.print("air:   ");
  if (bmeOk) {
    Serial.print("temp=");
    Serial.print(bme.readTemperature(), 1);
    Serial.print(" C  RH=");
    Serial.print(bme.readHumidity(), 1);
    Serial.print(" %  P=");
    Serial.print(bme.readPressure() / 100.0f, 1);
    Serial.print(" hPa");
  } else {
    Serial.print("BME280 ERR");
  }
  Serial.print("  lux=");
  luxOk ? Serial.println(lux.readLightLevel(), 0) : Serial.println("ERR");

  delay(2000);
}
