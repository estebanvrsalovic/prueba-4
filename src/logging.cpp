#include "logging.h"
#include <SPIFFS.h>

bool initLogging() {
  if (SPIFFS.begin(true)) {
    return true;
  }
  return false;
}

void appendLog(const String &s) {
  if (!SPIFFS.begin()) {
    // try to init once
    SPIFFS.begin(true);
  }
  File f = SPIFFS.open("/logs.txt", FILE_APPEND);
  if (!f) return;
  String line = String(millis()) + ": " + s + "\n";
  f.print(line);
  f.close();
}

String readLogs() {
  if (!SPIFFS.begin()) return String();
  File f = SPIFFS.open("/logs.txt", FILE_READ);
  if (!f) return String();
  String out;
  while (f.available()) {
    out += (char)f.read();
  }
  f.close();
  return out;
}
