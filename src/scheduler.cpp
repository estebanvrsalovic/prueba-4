#include "scheduler.h"
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <vector>
#include <time.h>
#include "relays.h"

static const char* SCHEDULE_FILE = "/schedules.json";
static std::vector<ScheduleEntry> schedules;
static int lastCheckedMinute = -1;

void loadSchedules() {
  schedules.clear();
  if (!SPIFFS.exists(SCHEDULE_FILE)) return;
  File f = SPIFFS.open(SCHEDULE_FILE, "r");
  if (!f) return;
  DynamicJsonDocument doc(2048);
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) return;
  if (!doc.is<JsonArray>()) return;
  for (JsonObject obj : doc.as<JsonArray>()) {
    ScheduleEntry e;
    e.ch = obj["ch"] | 1;
    e.hour = obj["hour"] | 0;
    e.minute = obj["minute"] | 0;
    e.on = obj["on"] | false;
    e.enabled = obj["enabled"] | true;
    e.days = obj["days"] | 0x7F;
    schedules.push_back(e);
  }
}

void saveSchedules() {
  DynamicJsonDocument doc(4096);
  JsonArray arr = doc.to<JsonArray>();
  for (auto &e : schedules) {
    JsonObject obj = arr.createNestedObject();
    obj["ch"] = e.ch;
    obj["hour"] = e.hour;
    obj["minute"] = e.minute;
    obj["on"] = e.on;
    obj["enabled"] = e.enabled;
    obj["days"] = e.days;
  }
  File f = SPIFFS.open(SCHEDULE_FILE, "w");
  if (!f) return;
  serializeJson(doc, f);
  f.close();
}

void schedulerBegin() {
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS mount failed");
  }
  loadSchedules();

  // Try to configure time if WiFi connected
  if (WiFi.status() == WL_CONNECTED) {
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    Serial.println("NTP configured");
  }
}

static bool timeNow(uint8_t &hour, uint8_t &minute) {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return false;
  hour = timeinfo.tm_hour;
  minute = timeinfo.tm_min;
  return true;
}

void schedulerLoop() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return; // need RTC/NTP
  uint8_t hour = timeinfo.tm_hour;
  uint8_t minute = timeinfo.tm_min;
  int wday = timeinfo.tm_wday; // 0=Sunday..6=Saturday
  if (minute == lastCheckedMinute) return; // only check once per minute
  lastCheckedMinute = minute;

  for (size_t i = 0; i < schedules.size(); ++i) {
    auto &e = schedules[i];
    if (!e.enabled) continue;
    // check day-of-week mask (bit0 = Sunday)
    if ((e.days & (1 << wday)) == 0) continue;
    if (e.hour == hour && e.minute == minute) {
      Serial.print("Schedule trigger ch"); Serial.print(e.ch);
      Serial.print(" -> "); Serial.println(e.on ? "ON" : "OFF");
      setRelay(e.ch, e.on);
    }
  }
}

String scheduleListJson() {
  DynamicJsonDocument doc(4096);
  JsonArray arr = doc.to<JsonArray>();
  for (auto &e : schedules) {
    JsonObject obj = arr.createNestedObject();
    obj["ch"] = e.ch;
    obj["hour"] = e.hour;
    obj["minute"] = e.minute;
    obj["on"] = e.on;
    obj["enabled"] = e.enabled;
    obj["days"] = e.days;
  }
  String out;
  serializeJson(doc, out);
  return out;
}

bool addSchedule(uint8_t ch, uint8_t hour, uint8_t minute, bool on, uint8_t daysMask) {
  if (ch < 1 || ch > 6) return false;
  ScheduleEntry e;
  e.ch = ch; e.hour = hour; e.minute = minute; e.on = on; e.enabled = true; e.days = daysMask;
  schedules.push_back(e);
  saveSchedules();
  return true;
}

bool removeSchedule(size_t index) {
  if (index >= schedules.size()) return false;
  schedules.erase(schedules.begin() + index);
  saveSchedules();
  return true;
}

bool setScheduleEnabled(size_t index, bool enabled) {
  if (index >= schedules.size()) return false;
  schedules[index].enabled = enabled;
  saveSchedules();
  return true;
}

bool editSchedule(size_t index, uint8_t ch, uint8_t hour, uint8_t minute, bool on, uint8_t daysMask) {
  if (index >= schedules.size()) return false;
  if (ch < 1 || ch > 6) return false;
  schedules[index].ch = ch;
  schedules[index].hour = hour;
  schedules[index].minute = minute;
  schedules[index].on = on;
  schedules[index].days = daysMask;
  saveSchedules();
  return true;
}
