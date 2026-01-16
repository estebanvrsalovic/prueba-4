#include <Arduino.h>
#include "serial_probe.h"

void serialProbeBegin() {
  Serial.begin(9600);
  // give host a moment
  delay(2000);
  Serial.println("=== SERIAL PROBE START ===");
}

void serialProbeLoop() {
  Serial.println("SERIAL PROBE: alive");
  Serial.flush();
  delay(1000);
}
