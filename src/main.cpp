// Greenhouse application scaffold
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <time.h>
#include "config.h"
#include "relays.h"
#include "webserver.h"
#include "pins.h"
#include "scheduler.h"
#include "sensor.h"
#include "thermostat.h"
#include "automation.h"
// MQTT
#include <WiFi.h>
#include <PubSubClient.h>

// NeoPixel config
// LED module
#include "led.h"
#include "serial_utils.h"

// MQTT globals
static WiFiClient _wifiClient;
static PubSubClient _mqttClient(_wifiClient);
static const char* MQTT_SERVER = "broker.hivemq.com";
static const uint16_t MQTT_PORT = 1883;

static String mqttTopicPrefix() {
  String mac = WiFi.macAddress();
  mac.replace(":", "");
  return String("greenhouse/") + mac + String("/status");
}

static void mqttEnsureConnected() {
  if (_mqttClient.connected()) return;
  if (WiFi.status() != WL_CONNECTED) return;
  String clientId = "gh-" + WiFi.macAddress();
  clientId.replace(":", "");
  if (_mqttClient.connect(clientId.c_str())) {
    logPrintln(String("MQTT connected"));
  } else {
    logPrint(String("MQTT connect failed, rc=")); logPrintln(String(_mqttClient.state()));
  }
}

void setup() {
  Serial.begin(9600);
  // start a secondary UART (Serial1) on configurable pins
  Serial1.begin(9600, SERIAL_8N1, SERIAL1_RX_PIN, SERIAL1_TX_PIN);
  unsigned long start = millis();
  // Allow more time for USB CDC enumeration on some hosts
  while (!Serial && (millis() - start) < 5000UL) delay(10);

  // Temporary debug: confirm Serial is available and app started
  Serial.println("=== Greenhouse app starting ===");
  Serial.flush();

  // Initialize SPIFFS logging
  initLogging();
  // If there are stored logs from previous runs, print them once on boot
  {
    String stored = readLogs();
    if (stored.length() > 0) {
      Serial.println("=== STORED LOGS BEGIN ===");
      Serial.println(stored);
      Serial.println("=== STORED LOGS END ===");
    }
  }

  logPrintln(String("=== Greenhouse app starting ==="));
  logPrint(String("Build: ")); logPrintln(String(__TIMESTAMP__));

  // Start NeoPixel
    // Initialize LED
    initLed();

  // Initialize relays and webserver
  relaysBegin();
  // start sensor (DHT22)
  sensorBegin();
  // start thermostat
  thermostatBegin();
  // automation (daily lights and irrigation)
  automationBegin();
  webBegin();
  // start scheduler (loads schedules from SPIFFS)
  schedulerBegin();

  // MQTT init
  _mqttClient.setServer(MQTT_SERVER, MQTT_PORT);

  logPrintln(String("Initialization complete"));
}

void loop() {
  static unsigned long lastColor = 0;
  static uint8_t colorIndex = 0; // 0=red,1=green,2=blue
  unsigned long now = millis();

  // Handle web requests frequently
  webHandle();

  // Scheduler loop (triggers schedules once per minute when time available)
  schedulerLoop();

  // thermostat loop (controls relay 1 if enabled)
  thermostatLoop();

  // relays background tasks (e.g. enforce lights min duration)
  relaysTick();
  // automation tick
  automationTick();

  // Cycle RGB every COLOR_INTERVAL
    // (LED now used as WiFi status indicator; color cycling removed)

  // Small yield to allow background tasks
  delay(10);

  // Print current time (local if available via NTP) every 2 seconds
  static unsigned long _timeLast = 0;
  if (millis() - _timeLast >= 2000) {
    _timeLast = millis();
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      char buf[64];
      strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
      String out = String("Hora: ") + buf;
      logPrintln(out);
      // publish to MQTT
      mqttEnsureConnected();
      if (_mqttClient.connected()) {
        String topic = mqttTopicPrefix();
        _mqttClient.publish(topic.c_str(), out.c_str());
      }
    } else {
      // fallback: uptime
      unsigned long s = millis() / 1000;
      unsigned long hh = s / 3600;
      unsigned long mm = (s % 3600) / 60;
      unsigned long ss = s % 60;
      char ubuf[32];
      snprintf(ubuf, sizeof(ubuf), "Uptime: %02lu:%02lu:%02lu", hh, mm, ss);
      String out = String(ubuf);
      logPrintln(out);
      // publish uptime to MQTT when NTP unavailable
      mqttEnsureConnected();
      if (_mqttClient.connected()) {
        String topic = mqttTopicPrefix();
        _mqttClient.publish(topic.c_str(), out.c_str());
      }
    }
  }

  // Broadcast a simple heartbeat every second via SSE
  static unsigned long _sseLast = 0;
  static int _sseCounter = 0;
  if (millis() - _sseLast >= 1000) {
    _sseLast = millis();
    _sseCounter++;
    webBroadcast(String("Valor: ") + _sseCounter);
  }

  // MQTT background maintenance (reconnect / loop)
  mqttEnsureConnected();
  if (_mqttClient.connected()) _mqttClient.loop();
}