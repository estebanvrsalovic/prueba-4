// Greenhouse application scaffold (diagnostic probe variant)
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <time.h>
#include "config.h"
#include "relays.h"
#include "webserver.h"
#include "pins.h"
#include "scheduler.h"
#include "sensor.h"

void setup() {
  Serial.begin(9600);
  // brief pause to allow USB CDC enumeration on some hosts
  delay(200);

  // Boot marker
  Serial.println("=== PROBE START ===");

  // Aggressive boot burst: print many lines quickly to force host visibility
  for (int i = 0; i < 200; ++i) {
    char buf[64];
    snprintf(buf, sizeof(buf), "BOOT_BURST %03d: alive", i);
    Serial.println(buf);
  }
  Serial.flush();
}

void loop() {
  static unsigned long last = 0;
  unsigned long now = millis();
  // Faster heartbeat to confirm ongoing activity
  if (now - last >= 500) {
    last = now;
    Serial.println("PROBE: alive");
    Serial.flush();
  }
  delay(10);
}