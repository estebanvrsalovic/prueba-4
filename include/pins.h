// Pin definitions for prueba 4 project
// Board: ESP32-S3 (esp32s3usbotg)

#ifndef PINS_H
#define PINS_H

// NeoPixel (RGB) pin
// On this board the labeled RGB control is on GPIO38
#define LED_PIN 38

// If you use separate R/G/B PWM pins, define them here for future reference:
// #define R_PIN 9
// #define G_PIN 10
// #define B_PIN 11

#// Relay outputs (board labelled CH1..CH6)
// NOTE: GPIO1/GPIO2 are UART0 TX/RX and can interfere with USB/Serial.
// Move CH1/CH2 off UART0 to avoid serial conflicts during boot/upload.
// CH1 -> select GPIO4 (was GPIO1)
// CH2 -> select GPIO5 (was GPIO2)
// CH3 -> GPIO41
// CH4 -> GPIO42
// CH5 -> GPIO45
// CH6 -> GPIO46
#define RELAY_CH1_PIN 4
#define RELAY_CH2_PIN 5
#define RELAY_CH3_PIN 41
#define RELAY_CH4_PIN 42
#define RELAY_CH5_PIN 45
#define RELAY_CH6_PIN 46

// DHT sensor pins (temperature + humidity)
// Connect DHT22 data pins to these GPIOs (use a 4.7K pull-up on each)
#define DHT_IN_PIN 21
#define DHT_OUT_PIN 20

// Serial1 pins (example) - change if needed
#define SERIAL1_RX_PIN 16
#define SERIAL1_TX_PIN 17

#endif // PINS_H
