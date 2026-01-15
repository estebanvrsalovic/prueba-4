#include "thermostat.h"
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include "sensor.h"
#include "relays.h"

static const char* THERM_FILE = "/thermostat.json";
static const char* THERM_LOG = "/therm_log.csv";
static float setpoint = 23.0f;
static float hysteresis = 0.5f;
static bool enabled = false;
static bool lastState = false;
// safety and advanced
static unsigned long maxRuntimeSec = 0; // 0 = disabled
static float overtempCutoff = 200.0f; // very high default
static float externalLimit = 200.0f; // if exterior >= this, don't enable heater
static bool loggingEnabled = false;
static unsigned long heaterOnSince = 0; // millis when heater turned on
static int lastLogMinute = -1;

static void loadThermostat() {
  if (!SPIFFS.exists(THERM_FILE)) return;
  File f = SPIFFS.open(THERM_FILE, "r");
  if (!f) return;
  DynamicJsonDocument doc(256);
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) return;
  setpoint = doc["setpoint"] | setpoint;
  hysteresis = doc["hysteresis"] | hysteresis;
  enabled = doc["enabled"] | enabled;
  maxRuntimeSec = doc["maxRuntimeSec"] | maxRuntimeSec;
  overtempCutoff = doc["overtempCutoff"] | overtempCutoff;
  externalLimit = doc["externalLimit"] | externalLimit;
  loggingEnabled = doc["loggingEnabled"] | loggingEnabled;
}

static void saveThermostat() {
  DynamicJsonDocument doc(256);
  doc["setpoint"] = setpoint;
  doc["hysteresis"] = hysteresis;
  doc["enabled"] = enabled;
  doc["maxRuntimeSec"] = maxRuntimeSec;
  doc["overtempCutoff"] = overtempCutoff;
  doc["externalLimit"] = externalLimit;
  doc["loggingEnabled"] = loggingEnabled;
  File f = SPIFFS.open(THERM_FILE, "w");
  if (!f) return;
  serializeJson(doc, f);
  f.close();
}

void thermostatBegin() {
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS mount failed (thermostat)");
  }
  loadThermostat();
}

String thermostatJson() {
  DynamicJsonDocument doc(256);
  doc["setpoint"] = setpoint;
  doc["hysteresis"] = hysteresis;
  doc["enabled"] = enabled;
  float temp = readTemperatureC(false);
  doc["temp"] = temp;
  String out;
  serializeJson(doc, out);
  return out;
}

String thermostatStatusJson() {
  DynamicJsonDocument doc(512);
  doc["setpoint"] = setpoint;
  doc["hysteresis"] = hysteresis;
  doc["enabled"] = enabled;
  doc["maxRuntimeSec"] = maxRuntimeSec;
  doc["overtempCutoff"] = overtempCutoff;
  doc["externalLimit"] = externalLimit;
  doc["loggingEnabled"] = loggingEnabled;
  float temp = readTemperatureC(false);
  float tout = readTemperatureC(true);
  doc["temp"] = temp;
  doc["temp_out"] = tout;
  // heater runtime
  if (heaterOnSince && lastState) doc["heaterRunSec"] = (millis() - heaterOnSince) / 1000;
  else doc["heaterRunSec"] = 0;
  String out;
  serializeJson(doc, out);
  return out;
}

bool setThermostat(float sp, float h, bool en) {
  if (h < 0) return false;
  setpoint = sp;
  hysteresis = h;
  enabled = en;
  saveThermostat();
  return true;
}

bool setThermostatAdvanced(unsigned long maxRuntime, float overtemp, float extLimit, bool logEnabled) {
  maxRuntimeSec = maxRuntime;
  overtempCutoff = overtemp;
  externalLimit = extLimit;
  loggingEnabled = logEnabled;
  saveThermostat();
  return true;
}

void thermostatLoop() {
  if (!enabled) return;
  float temp = readTemperatureC(false); // interior sensor
  if (isnan(temp)) return;
  // Safety: overtemp cutoff
  if (temp >= overtempCutoff) {
    if (lastState) { setRelay(1, false); lastState = false; }
    // disable thermostat to avoid restarting until user re-enables
    enabled = false;
    saveThermostat();
    Serial.println("Thermostat disabled: overtemp cutoff reached");
    return;
  }

  // Check exterior limit: if exterior >= externalLimit, do not turn on heater
  float tout = readTemperatureC(true);
  bool extBlock = false;
  if (!isnan(tout) && tout >= externalLimit) extBlock = true;

  // turn ON if temp <= setpoint - hysteresis and not blocked
  if (temp <= (setpoint - hysteresis) && !extBlock) {
    if (!lastState) {
      setRelay(1, true);
      lastState = true;
      heaterOnSince = millis();
    }
  } else if (temp >= (setpoint + hysteresis) || extBlock) {
    if (lastState) {
      setRelay(1, false);
      lastState = false;
      heaterOnSince = 0;
    }
  }

  // Max runtime enforcement
  if (lastState && heaterOnSince && maxRuntimeSec > 0) {
    unsigned long runSec = (millis() - heaterOnSince) / 1000;
    if (runSec >= maxRuntimeSec) {
      setRelay(1, false);
      lastState = false;
      heaterOnSince = 0;
      enabled = false; // disable until user re-enables
      saveThermostat();
      Serial.println("Thermostat disabled: max runtime exceeded");
    }
  }

  // Logging: append CSV once per minute
  if (loggingEnabled) {
    struct tm ti;
    if (getLocalTime(&ti)) {
      int minute = ti.tm_min;
      if (minute != lastLogMinute) {
        lastLogMinute = minute;
        // append log
        File f = SPIFFS.open(THERM_LOG, FILE_APPEND);
        if (f) {
          time_t nowt = time(nullptr);
          char buf[64];
          snprintf(buf, sizeof(buf), "%lu,%0.2f,%0.2f,%0.2f,%0.2f,%d\n", (unsigned long)nowt, temp, readHumidity(false), tout, readHumidity(true), lastState?1:0);
          f.print(buf);
          f.close();
        }
      }
    }
  }
}
