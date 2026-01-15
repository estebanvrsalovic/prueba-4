#ifndef RELAYS_H
#define RELAYS_H

#include <Arduino.h>

void relaysBegin();
void setRelay(uint8_t channel, bool on);
bool getRelay(uint8_t channel);
// Convenience for lights mapped to channel 2
void setLights(bool on);
bool getLights();
void relaysTick();
void setLightsMinDurationSec(unsigned long secs);
unsigned long getLightsMinDurationSec();
// Schedule an automatic lights-off after given seconds from now
void scheduleLightsOffAfterSec(unsigned long secs);
// Return millis when lights were turned on (0 if off)
unsigned long getLightsOnSinceMillis();

#endif // RELAYS_H
