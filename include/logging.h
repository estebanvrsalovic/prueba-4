// Logging to SPIFFS
#ifndef LOGGING_H
#define LOGGING_H

#include <Arduino.h>

bool initLogging();
void appendLog(const String &s);
String readLogs();

#endif // LOGGING_H
