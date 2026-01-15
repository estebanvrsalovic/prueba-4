#include <Adafruit_NeoPixel.h>
#include "led.h"
#include "pins.h"

#define NUMPIXELS 1
static Adafruit_NeoPixel strip(NUMPIXELS, LED_PIN, NEO_GRB + NEO_KHZ800);

void initLed() {
  strip.begin();
  strip.setBrightness(64);
  strip.setPixelColor(0, strip.Color(0,0,0));
  strip.show();
}

void setPixelColorRGB(uint8_t r, uint8_t g, uint8_t b) {
  strip.setPixelColor(0, strip.Color(r, g, b));
  strip.show();
}
