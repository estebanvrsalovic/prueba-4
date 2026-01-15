#ifndef AUTOMATION_H
#define AUTOMATION_H

#include <Arduino.h>

void automationBegin();
void automationTick();
String automationJson();
bool setDailyLightMinHours(float hours);
bool setIrrigationConfig(uint8_t countPerDay, uint16_t durationSec, uint8_t startHour);
// set explicit irrigation times as CSV of HH:MM (e.g. "06:00,12:00,18:00")
bool setIrrigationTimesCSV(const String &timesCsv, uint16_t durationSec);

// return JSON history of daily light hours
String automationHistoryJson();

#endif // AUTOMATION_H
