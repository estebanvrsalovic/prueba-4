#ifndef SENSOR_H
#define SENSOR_H

#include <Arduino.h>

void sensorBegin();
// readTemperatureC(false) -> interior, true -> exterior
float readTemperatureC(bool outside = false);
float readHumidity(bool outside = false);
String sensorJson();

#endif // SENSOR_H
