#include "serial_cmds.h"
#include "serial_utils.h"
#include "relays.h"
#include "thermostat.h"

static String makeStatusJson() {
  String s = "{";
  for (int i = 1; i <= 6; ++i) {
    s += "\"ch";
    s += String(i);
    s += "\":";
    s += (getRelay(i) ? "1" : "0");
    if (i < 6) s += ",";
  }
  s += ",\"lights\":";
  s += (getLights() ? "1" : "0");
  s += "}";
  return s;
}

static void handleCommand(const String &cmd) {
  String c = cmd;
  c.trim();
  if (c.length() == 0) return;
  if (c.equalsIgnoreCase("help")) {
    logPrintln("Commands: help, status, relay <ch> on|off|toggle, lights on|off|toggle, setpoint <val>");
    return;
  }
  if (c.equalsIgnoreCase("status")) {
    logPrintln(makeStatusJson());
    return;
  }
  if (c.startsWith("relay ")) {
    // relay <ch> on|off|toggle
    int sp = c.indexOf(' ');
    int sp2 = c.indexOf(' ', sp + 1);
    if (sp2 < 0) { logPrintln("Invalid relay command"); return; }
    String chs = c.substring(sp + 1, sp2);
    int ch = chs.toInt();
    String act = c.substring(sp2 + 1);
    act.toLowerCase();
    if (ch < 1 || ch > 6) { logPrintln("Invalid channel"); return; }
    if (act == "on") setRelay(ch, true);
    else if (act == "off") setRelay(ch, false);
    else if (act == "toggle") setRelay(ch, !getRelay(ch));
    else { logPrintln("Unknown action"); return; }
    logPrintln(String("OK relay ") + ch);
    return;
  }
  if (c.startsWith("lights ")) {
    String act = c.substring(7);
    act.toLowerCase();
    if (act == "on") setLights(true);
    else if (act == "off") setLights(false);
    else if (act == "toggle") setLights(!getLights());
    else { logPrintln("Unknown action"); return; }
    logPrintln(String("OK lights"));
    return;
  }
  if (c.startsWith("setpoint ")) {
    String val = c.substring(9);
    float sp = val.toFloat();
    // keep current hysteresis and enabled flag (use thermostatJson or status otherwise)
    // We'll call setThermostat with same hysteresis and enabled if possible
    // For simplicity enable with hysteresis 0.5 and enabled true
    if (setThermostat(sp, 0.5f, true)) {
      logPrintln(String("OK setpoint ") + sp);
    } else {
      logPrintln("Failed to set thermostat");
    }
    return;
  }
  logPrintln("Unknown command. Send 'help' for list.");
}

void serialCmdsBegin() {
  // nothing to init for now
}

void serialCmdsLoop() {
  // Process USB Serial
  if (Serial && Serial.available()) {
    String line = Serial.readStringUntil('\n');
    handleCommand(line);
  }
  // Process Serial1 (UART)
  if (Serial1 && Serial1.available()) {
    String line = Serial1.readStringUntil('\n');
    handleCommand(line);
  }
}
