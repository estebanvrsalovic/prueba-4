#include "automation.h"
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <vector>
#include <time.h>
#include "relays.h"

static const char* AUTO_FILE = "/automation.json";

static unsigned long dailyLightMinSec = 12UL * 3600UL;
static unsigned long dailyLightAccumSec = 0;
static int lastDayOfYear = -1;

// history of past days: vector of pair(year, yday) -> accum seconds
static std::vector<unsigned long> lightHistoryAccum;
static std::vector<int> lightHistoryYear;
static std::vector<int> lightHistoryYday;

// irrigation
static uint8_t irrigationCount = 3;
static uint16_t irrigationDurationSec = 60; // default 60s
static uint8_t irrigationStartHour = 6; // default first at 06:00
static std::vector<int> irrigationTimes; // minutes since midnight
static std::vector<unsigned long> irrigationPendingOff; // millis timestamps for pending offs
static std::vector<int> triggeredDay; // track last day triggered per event
static bool irrigationExplicitTimes = false;

static unsigned long lastTick = 0;

static void computeIrrigationTimes() {
  irrigationTimes.clear();
  if (irrigationExplicitTimes) {
    // times already in irrigationTimes
  } else {
    if (irrigationCount == 0) return;
    int interval = 24 * 60 / irrigationCount; // minutes
    int base = irrigationStartHour * 60; // minutes since midnight
    for (uint8_t i = 0; i < irrigationCount; ++i) {
      int t = (base + i * interval) % (24*60);
      irrigationTimes.push_back(t);
    }
  }
  triggeredDay.assign(irrigationTimes.size(), -1);
  irrigationPendingOff.assign(irrigationTimes.size(), 0);
}

static void loadAutomation() {
  if (!SPIFFS.exists(AUTO_FILE)) return;
  File f = SPIFFS.open(AUTO_FILE, "r");
  if (!f) return;
  DynamicJsonDocument doc(512);
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) return;
  dailyLightMinSec = doc["dailyLightMinSec"] | dailyLightMinSec;
  dailyLightAccumSec = doc["dailyLightAccumSec"] | 0;
  irrigationCount = doc["irrigationCount"] | irrigationCount;
  irrigationDurationSec = doc["irrigationDurationSec"] | irrigationDurationSec;
  irrigationStartHour = doc["irrigationStartHour"] | irrigationStartHour;
  // explicit irrigation times array
  if (doc.containsKey("irrigationTimes")) {
    irrigationTimes.clear();
    for (JsonVariant v : doc["irrigationTimes"].as<JsonArray>()) {
      irrigationTimes.push_back((int)v.as<int>());
    }
    irrigationExplicitTimes = irrigationTimes.size() > 0;
    irrigationCount = irrigationExplicitTimes ? (uint8_t)irrigationTimes.size() : irrigationCount;
  }
  // history
  if (doc.containsKey("history")) {
    lightHistoryAccum.clear(); lightHistoryYear.clear(); lightHistoryYday.clear();
    for (JsonVariant h : doc["history"].as<JsonArray>()) {
      int y = h["year"] | 0;
      int d = h["yday"] | 0;
      unsigned long a = h["accumSec"] | 0UL;
      lightHistoryYear.push_back(y);
      lightHistoryYday.push_back(d);
      lightHistoryAccum.push_back(a);
    }
  }
}

static void saveAutomation() {
  DynamicJsonDocument doc(512);
  doc["dailyLightMinSec"] = dailyLightMinSec;
  doc["dailyLightAccumSec"] = dailyLightAccumSec;
  doc["irrigationCount"] = irrigationCount;
  doc["irrigationDurationSec"] = irrigationDurationSec;
  doc["irrigationStartHour"] = irrigationStartHour;
  if (irrigationTimes.size() > 0) {
    JsonArray arr = doc.createNestedArray("irrigationTimes");
    for (int t : irrigationTimes) arr.add(t);
  }
  // save history
  if (lightHistoryAccum.size() > 0) {
    JsonArray harr = doc.createNestedArray("history");
    for (size_t i = 0; i < lightHistoryAccum.size(); ++i) {
      JsonObject it = harr.createNestedObject();
      it["year"] = lightHistoryYear[i];
      it["yday"] = lightHistoryYday[i];
      it["accumSec"] = lightHistoryAccum[i];
    }
  }
  File f = SPIFFS.open(AUTO_FILE, "w");
  if (!f) return;
  serializeJson(doc, f);
  f.close();
}

void automationBegin() {
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS mount failed (automation)");
  }
  loadAutomation();
  computeIrrigationTimes();
  lastTick = millis();
  // determine current day
  struct tm tm;
  if (getLocalTime(&tm)) lastDayOfYear = tm.tm_yday;
}

String automationJson() {
  DynamicJsonDocument doc(512);
  doc["dailyLightMinHours"] = (float)dailyLightMinSec / 3600.0f;
  doc["dailyLightAccumHours"] = (float)dailyLightAccumSec / 3600.0f;
  doc["irrigationCount"] = irrigationCount;
  doc["irrigationDurationSec"] = irrigationDurationSec;
  doc["irrigationStartHour"] = irrigationStartHour;
  JsonArray arr = doc.createNestedArray("irrigationTimes");
  for (int t : irrigationTimes) arr.add(t);
  // include history
  JsonArray harr = doc.createNestedArray("history");
  for (size_t i = 0; i < lightHistoryAccum.size(); ++i) {
    JsonObject it = harr.createNestedObject();
    it["year"] = lightHistoryYear[i];
    it["yday"] = lightHistoryYday[i];
    it["accumHours"] = (float)lightHistoryAccum[i] / 3600.0f;
  }
  String out; serializeJson(doc, out);
  return out;
}

bool setDailyLightMinHours(float hours) {
  if (hours < 0) return false;
  dailyLightMinSec = (unsigned long)(hours * 3600.0f);
  saveAutomation();
  return true;
}

bool setIrrigationConfig(uint8_t countPerDay, uint16_t durationSec, uint8_t startHour) {
  if (countPerDay > 24 || durationSec == 0) return false;
  irrigationCount = countPerDay;
  irrigationDurationSec = durationSec;
  irrigationStartHour = startHour % 24;
  irrigationExplicitTimes = false;
  irrigationTimes.clear();
  computeIrrigationTimes();
  saveAutomation();
  return true;
}

bool setIrrigationTimesCSV(const String &timesCsv, uint16_t durationSec) {
  // parse CSV of HH:MM or H:MM
  irrigationTimes.clear();
  irrigationExplicitTimes = false;
  String s = timesCsv;
  int p = 0;
  while (p < (int)s.length()) {
    int comma = s.indexOf(',', p);
    if (comma == -1) comma = s.length();
    String token = s.substring(p, comma);
    token.trim();
    if (token.length() > 0) {
      int colon = token.indexOf(':');
      if (colon > 0) {
        int h = token.substring(0, colon).toInt();
        int m = token.substring(colon+1).toInt();
        if (h >=0 && h < 24 && m >=0 && m < 60) {
          irrigationTimes.push_back(h*60 + m);
        }
      }
    }
    p = comma + 1;
  }
  if (irrigationTimes.size() == 0) return false;
  irrigationDurationSec = durationSec;
  irrigationCount = (uint8_t)irrigationTimes.size();
  irrigationExplicitTimes = true;
  triggeredDay.assign(irrigationTimes.size(), -1);
  irrigationPendingOff.assign(irrigationTimes.size(), 0);
  saveAutomation();
  return true;
}

String automationHistoryJson() {
  DynamicJsonDocument doc(512);
  JsonArray harr = doc.createNestedArray("history");
  for (size_t i = 0; i < lightHistoryAccum.size(); ++i) {
    JsonObject it = harr.createNestedObject();
    it["year"] = lightHistoryYear[i];
    it["yday"] = lightHistoryYday[i];
    it["accumHours"] = (float)lightHistoryAccum[i] / 3600.0f;
  }
  String out; serializeJson(doc, out);
  return out;
}

// helper: minutes since midnight
static int getMinutesSinceMidnight(struct tm &tm) {
  return tm.tm_hour * 60 + tm.tm_min;
}

void automationTick() {
  unsigned long now = millis();
  unsigned long delta = now - lastTick;
  if (delta == 0) return;
  lastTick = now;

  // Update daily lights accumulation
  bool lightsOn = getLights();
  if (lightsOn) {
    // add elapsed seconds
    dailyLightAccumSec += delta / 1000UL;
  }

  // Check irrigation triggers
  struct tm tm;
  if (!getLocalTime(&tm)) return; // need time for scheduling
  int day = tm.tm_yday;
  if (day != lastDayOfYear) {
    // day rollover: push yesterday's accumulation into history
    if (lastDayOfYear >= 0) {
      time_t nowEpoch = time(nullptr);
      struct tm prevTm;
      // we can store lastDayOfYear with current year approximation
      int yy = 0;
      if (getLocalTime(&prevTm)) yy = prevTm.tm_year + 1900;
      lightHistoryYear.push_back(yy);
      lightHistoryYday.push_back(lastDayOfYear);
      lightHistoryAccum.push_back(dailyLightAccumSec);
      // trim to last 120 entries (retain ~120 days)
      while (lightHistoryAccum.size() > 120) {
        lightHistoryAccum.erase(lightHistoryAccum.begin());
        lightHistoryYear.erase(lightHistoryYear.begin());
        lightHistoryYday.erase(lightHistoryYday.begin());
      }
      saveAutomation();
    }
    // day rollover: check lights requirement
    if (dailyLightAccumSec < dailyLightMinSec) {
      unsigned long remaining = dailyLightMinSec - dailyLightAccumSec;
      Serial.printf("Daily lights short by %lu seconds, enforcing now\n", remaining);
      // ensure lights on and schedule off after remaining seconds
      setLights(true);
      scheduleLightsOffAfterSec(remaining);
      // also set relays min duration to remaining to avoid early off
      setLightsMinDurationSec(remaining);
    }
    // reset accumulator for new day
    dailyLightAccumSec = 0;
    lastDayOfYear = day;
    // reset irrigation triggered markers
    for (size_t i = 0; i < triggeredDay.size(); ++i) triggeredDay[i] = -1;
  }

  // check irrigation times
  int nowMin = getMinutesSinceMidnight(tm);
  for (size_t i = 0; i < irrigationTimes.size(); ++i) {
    int t = irrigationTimes[i];
    if (triggeredDay.size() <= i) continue; // safety
    if (triggeredDay[i] == tm.tm_yday) continue; // already triggered today
    if (t == nowMin) {
      // trigger irrigation: turn on CH3 and schedule off
      Serial.printf("Trigger irrigation %d at %02d:%02d for %d sec\n", (int)i, tm.tm_hour, tm.tm_min, irrigationDurationSec);
      setRelay(3, true);
      irrigationPendingOff[i] = millis() + (unsigned long)irrigationDurationSec * 1000UL;
      triggeredDay[i] = tm.tm_yday;
    }
  }

  // handle irrigation pending offs
  for (size_t i = 0; i < irrigationPendingOff.size(); ++i) {
    unsigned long offAt = irrigationPendingOff[i];
    if (offAt == 0) continue;
    if ((long)(millis() - offAt) >= 0) {
      // turn off irrigation channel
      setRelay(3, false);
      irrigationPendingOff[i] = 0;
    }
  }
}