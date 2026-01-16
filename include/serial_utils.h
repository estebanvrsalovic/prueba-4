// Simple logging helper: write to USB Serial if available, UART1, and SSE/telnet
#ifndef SERIAL_UTILS_H
#define SERIAL_UTILS_H
#include <Arduino.h>
#include "webserver.h"
#include "logging.h"

inline void logPrintln(const String &s) {
  // Emit logs to Serial (USB CDC) if available and to Serial1 (USB-TTL adapter)
  if (Serial) Serial.println(s);
  if (Serial1) Serial1.println(s);
  // Also persist logs to SPIFFS
  appendLog(s);
}

inline void logPrint(const String &s) {
  if (Serial) Serial.print(s);
  if (Serial1) Serial1.print(s);
  appendLog(s);
}

#endif // SERIAL_UTILS_H
