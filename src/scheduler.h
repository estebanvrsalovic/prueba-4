#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <Arduino.h>

struct ScheduleEntry {
  uint8_t ch;
  uint8_t hour;
  uint8_t minute;
  bool on;
  bool enabled;
  uint8_t days; // bitmask: bit0=Sunday .. bit6=Saturday
};

void schedulerBegin();
void schedulerLoop();
String scheduleListJson();
bool addSchedule(uint8_t ch, uint8_t hour, uint8_t minute, bool on, uint8_t daysMask = 0x7F);
bool removeSchedule(size_t index);
bool editSchedule(size_t index, uint8_t ch, uint8_t hour, uint8_t minute, bool on, uint8_t daysMask);
bool setScheduleEnabled(size_t index, bool enabled);
bool editSchedule(size_t index, uint8_t ch, uint8_t hour, uint8_t minute, bool on);

#endif // SCHEDULER_H
