#ifndef THERMOSTAT_H
#define THERMOSTAT_H

#include <Arduino.h>

void thermostatBegin();
void thermostatLoop();
String thermostatJson();
bool setThermostat(float setpoint, float hysteresis, bool enabled);
// Advanced safety and logging
bool setThermostatAdvanced(unsigned long maxRuntimeSec, float overtempCutoff, float externalLimit, bool loggingEnabled);
String thermostatStatusJson();

#endif // THERMOSTAT_H
