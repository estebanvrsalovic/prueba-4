#include "sensor.h"
#include "pins.h"
#include "logging.h"
#include <DHTesp.h>

static DHTesp dht_in;
static DHTesp dht_out;
static float lastInTemp = NAN;
static float lastInHum = NAN;
static float lastOutTemp = NAN;
static float lastOutHum = NAN;

void sensorBegin() {
  dht_in.setup(DHT_IN_PIN, DHTesp::DHT22);
  dht_out.setup(DHT_OUT_PIN, DHTesp::DHT22);
  delay(50);
  float t = dht_in.getTemperature();
  float h = dht_in.getHumidity();
  if (!isnan(t)) lastInTemp = t;
  if (!isnan(h)) lastInHum = h;
  t = dht_out.getTemperature();
  h = dht_out.getHumidity();
  if (!isnan(t)) lastOutTemp = t;
  if (!isnan(h)) lastOutHum = h;

  // Log sensor presence/absence for diagnostics
  if (isnan(lastInTemp) || isnan(lastInHum)) {
    Serial.println("[WARN] Indoor DHT sensor not responding or disconnected");
    appendLog(String("Indoor DHT missing or read failed"));
  }
  if (isnan(lastOutTemp) || isnan(lastOutHum)) {
    Serial.println("[WARN] Outdoor DHT sensor not responding or disconnected");
    appendLog(String("Outdoor DHT missing or read failed"));
  }
}

// Log sensor presence on startup if readings are missing
  

float readTemperatureC(bool outside) {
  if (outside) {
    float t = dht_out.getTemperature();
    if (!isnan(t)) lastOutTemp = t;
    return lastOutTemp;
  } else {
    float t = dht_in.getTemperature();
    if (!isnan(t)) lastInTemp = t;
    return lastInTemp;
  }
}

float readHumidity(bool outside) {
  if (outside) {
    float h = dht_out.getHumidity();
    if (!isnan(h)) lastOutHum = h;
    return lastOutHum;
  } else {
    float h = dht_in.getHumidity();
    if (!isnan(h)) lastInHum = h;
    return lastInHum;
  }
}

String sensorJson() {
  float tin = readTemperatureC(false);
  float hin = readHumidity(false);
  float tout = readTemperatureC(true);
  float hout = readHumidity(true);
  String s = "{";
  s += "\"in\":{";
  if (isnan(tin)) s += "\"temp\":null,"; else s += "\"temp\":" + String(tin,2) + ",";
  if (isnan(hin)) s += "\"hum\":null"; else s += "\"hum\":" + String(hin,2);
  s += "},";
  s += "\"out\":{";
  if (isnan(tout)) s += "\"temp\":null,"; else s += "\"temp\":" + String(tout,2) + ",";
  if (isnan(hout)) s += "\"hum\":null"; else s += "\"hum\":" + String(hout,2);
  s += "}";
  s += "}";
  return s;
}
