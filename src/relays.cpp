#include "relays.h"
#include "config.h"
#include "pins.h"
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <time.h>

static const int relayPins[6] = {
  RELAY_CH1_PIN,
  RELAY_CH2_PIN,
  RELAY_CH3_PIN,
  RELAY_CH4_PIN,
  RELAY_CH5_PIN,
  RELAY_CH6_PIN
};

// Lights minimum-on enforcement (default 12 hours)
static unsigned long lightsMinSec = 12UL * 3600UL;
static unsigned long lightsOnSince = 0; // millis when lights were turned on
static unsigned long pendingLightsOffAt = 0; // millis when allowed to turn off
static const char* RELAYS_STATE_FILE = "/relays_state.json";
// store epoch seconds when lights were turned on across reboot
static time_t lightsOnSinceEpoch = 0;

void relaysBegin() {
  for (int i = 0; i < 6; ++i) {
    pinMode(relayPins[i], OUTPUT);
    // Set to off
    if (RELAY_ACTIVE_LOW) digitalWrite(relayPins[i], HIGH);
    else digitalWrite(relayPins[i], LOW);
  }
  // Mount SPIFFS and restore persisted lights-on time if present
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS mount failed (relays)");
  } else {
    if (SPIFFS.exists(RELAYS_STATE_FILE)) {
      File f = SPIFFS.open(RELAYS_STATE_FILE, "r");
      if (f) {
        DynamicJsonDocument doc(256);
        if (!deserializeJson(doc, f)) {
          lightsOnSinceEpoch = doc["lightsOnSinceEpoch"] | 0;
        }
        f.close();
      }
    }
    // If lights are physically on, reconstruct lightsOnSince using epoch
    if (lightsOnSinceEpoch > 0 && getLights()) {
      time_t now = time(nullptr);
      if (now > lightsOnSinceEpoch) {
        unsigned long elapsed = (unsigned long)(now - lightsOnSinceEpoch);
        // set lightsOnSince so millis-based logic sees the elapsed time
        lightsOnSince = millis() - elapsed * 1000UL;
      } else {
        // cannot compute, just set lightsOnSince = millis()
        lightsOnSince = millis();
      }
    }
  }
}

void setRelay(uint8_t channel, bool on) {
  if (channel < 1 || channel > 6) return;
  int idx = channel - 1;
  // Enforce lights minimum-on when channel == 2
  if (channel == 2) {
    bool currentlyOn = getRelay(2);
    if (on) {
      // turn on immediately if not already
      if (!currentlyOn) {
        bool level = (RELAY_ACTIVE_LOW ? LOW : HIGH);
        digitalWrite(relayPins[idx], level);
        lightsOnSince = millis();
        // persist epoch time if RTC/NTP available
        time_t now = time(nullptr);
        if (now > 100000) {
          lightsOnSinceEpoch = now;
          // save file
          DynamicJsonDocument doc(256);
          doc["lightsOnSinceEpoch"] = (unsigned long)lightsOnSinceEpoch;
          File f = SPIFFS.open(RELAYS_STATE_FILE, "w");
          if (f) { serializeJson(doc, f); f.close(); }
        } else {
          lightsOnSinceEpoch = 0;
        }
        pendingLightsOffAt = 0;
      }
      return;
    } else {
      // request to turn off: if min duration not reached, schedule pending
      if (currentlyOn) {
        unsigned long now = millis();
        unsigned long elapsedSec = (now - lightsOnSince) / 1000UL;
        if (lightsOnSince == 0 || elapsedSec >= lightsMinSec) {
          bool level = (RELAY_ACTIVE_LOW ? HIGH : LOW);
          digitalWrite(relayPins[idx], level);
          lightsOnSince = 0;
          pendingLightsOffAt = 0;
          // clear persisted epoch
          lightsOnSinceEpoch = 0;
          if (SPIFFS.exists(RELAYS_STATE_FILE)) SPIFFS.remove(RELAYS_STATE_FILE);
        } else {
          // schedule off for later
          pendingLightsOffAt = lightsOnSince + lightsMinSec * 1000UL;
          Serial.printf("Lights off deferred, will allow at %lu (in %lu s)\n", pendingLightsOffAt, (lightsMinSec - elapsedSec));
        }
      }
      return;
    }
  }

  bool level = on ? (RELAY_ACTIVE_LOW ? LOW : HIGH) : (RELAY_ACTIVE_LOW ? HIGH : LOW);
  digitalWrite(relayPins[idx], level);
}

bool getRelay(uint8_t channel) {
  if (channel < 1 || channel > 6) return false;
  int idx = channel - 1;
  int val = digitalRead(relayPins[idx]);
  if (RELAY_ACTIVE_LOW) return val == LOW;
  return val == HIGH;
}

// Lights convenience mapped to channel 2
void setLights(bool on) {
  setRelay(2, on);
}

bool getLights() {
  return getRelay(2);
}

void relaysTick() {
  if (pendingLightsOffAt == 0) return;
  unsigned long now = millis();
  // handle wrap-around safely
  if ((long)(now - pendingLightsOffAt) >= 0) {
    // time reached
    int idx = 2 - 1;
    bool level = (RELAY_ACTIVE_LOW ? HIGH : LOW);
    digitalWrite(relayPins[idx], level);
    Serial.println("Lights auto-turned off after minimum duration");
    lightsOnSince = 0;
    pendingLightsOffAt = 0;
  }
}

void setLightsMinDurationSec(unsigned long secs) {
  lightsMinSec = secs;
}

unsigned long getLightsMinDurationSec() {
  return lightsMinSec;
}

void scheduleLightsOffAfterSec(unsigned long secs) {
  if (secs == 0) {
    pendingLightsOffAt = 0;
    return;
  }
  unsigned long now = millis();
  unsigned long at = now + secs * 1000UL;
  pendingLightsOffAt = at;
}

unsigned long getLightsOnSinceMillis() {
  return lightsOnSince;
}
