#include <WiFi.h>
#include "webserver.h"
#include "config.h"
#include "relays.h"
#include <SPIFFS.h>
#include "sensor.h"
#include "thermostat.h"
#include "automation.h"
#include "led.h"
#include "serial_utils.h"

static WiFiServer server(80);
// Telnet-like server for remote serial log viewing
static WiFiServer telnetServer(23);

// SSE clients (simple fixed-size array)
static WiFiClient sseClients[4];
static WiFiClient telnetClients[2];

static void telnetCleanSlot(int i) {
  if (telnetClients[i] && !telnetClients[i].connected()) {
    telnetClients[i].stop();
    telnetClients[i] = WiFiClient();
  }
}

static void sseCleanSlot(int i) {
  if (sseClients[i] && !sseClients[i].connected()) {
    sseClients[i].stop();
    sseClients[i] = WiFiClient();
  }
}

void webBroadcast(const String &msg) {
  for (int i = 0; i < 4; ++i) {
    if (sseClients[i] && sseClients[i].connected()) {
      sseClients[i].print("data: ");
      sseClients[i].print(msg);
      sseClients[i].print("\n\n");
    } else {
      sseCleanSlot(i);
    }
  }
  // also send to telnet clients (plain text)
  for (int i = 0; i < 2; ++i) {
    if (telnetClients[i] && telnetClients[i].connected()) {
      telnetClients[i].print(msg);
      telnetClients[i].print("\r\n");
    } else {
      telnetCleanSlot(i);
    }
  }
}

String relayStatusJson() {
  String s = "{";
  for (int i = 1; i <= 6; ++i) {
    s += "\"ch" + String(i) + "\":" + (getRelay(i) ? "1" : "0");
    if (i < 6) s += ",";
  }
  s += "}";
  return s;
}

static void sendResponse(WiFiClient &client, const char* contentType, const String &body) {
  client.print("HTTP/1.1 200 OK\r\n");
  client.print("Content-Type: "); client.print(contentType); client.print("\r\n");
  client.print("Content-Length: "); client.print(body.length()); client.print("\r\n");
  client.print("Connection: close\r\n\r\n");
  client.print(body);
}

// Very small URL parser for GET path and query
static void handleRequest(WiFiClient &client, const String &path) {
  if (path == "/" || path == "") {
    // Dynamic UI: relays + schedules (fetched by JS)
    String page = "<html><head><meta name=viewport content='width=device-width, initial-scale=1'/>";
    page += "<style>body{font-family:sans-serif} table{border-collapse:collapse} td,th{border:1px solid #ccc;padding:6px}</style></head><body>";
    page += "<h1>Invernadero - Control</h1>";
    page += "<h2>Relés</h2><ul>";
    for (int i = 1; i <= 6; ++i) {
      page += "<li>Relé CH" + String(i) + ": " + (getRelay(i) ? "ENCENDIDO" : "APAGADO") + " <a href='/relay?ch=" + String(i) + "&state=toggle'>Alternar</a></li>";
    }
    page += "</ul>";

    page += "<h2>Luces</h2>";
    page += "<div>Estado: " + String(getLights() ? "ENCENDIDO" : "APAGADO") + " <button onclick=\"fetch('/relay?ch=2&state=toggle').then(()=>location.reload())\">Alternar luces</button>";
    page += " <button onclick=\"document.getElementById('ch').value=2;\">Usar CH2 para agregar horario</button></div>";

    page += "<h2>Temperatura</h2>";
    page += "<div id='sensor'><em>Cargando...</em></div>";

    page += "<h2>Termostato</h2>";
    page += "<div id='thermostat'><em>Cargando...</em></div>";
    page += "Consigna: <input id='setpoint' size=4> Histeresis: <input id='hysteresis' size=3> Habilitado: <select id='ten'><option value='1'>Sí</option><option value='0'>No</option></select> <button id='setTherm'>Guardar</button><br>";
    page += "Tiempo máx (s): <input id='maxrun' size=5> Corte por sobretemp (°C): <input id='overtemp' size=4> Bloqueo exterior si >= (°C): <input id='extlimit' size=4> Registro: <select id='logen'><option value='1'>Sí</option><option value='0'>No</option></select> <button id='setAdv'>Guardar avanzado</button> <button id='dlLog'>Descargar registro</button>";

    page += "<h2>Automatización</h2>";
    page += "<div id='automation'><em>Cargando...</em></div>";
    page += "<div id='lightHistory'><em>Cargando historial...</em></div>";
    page += "Min diario de luces (h): <input id='dailyHours' size=4> <button id='setDaily'>Guardar</button>";
    page += " Riego - cantidad: <input id='irCount' size=2> Duración(s): <input id='irDur' size=3> Hora inicio: <input id='irStart' size=2> <button id='setIrr'>Guardar riego</button>";
    page += "<br>O tiempos explícitos (CSV HH:MM): <input id='irTimes' size=20> <button id='setIrrTimes'>Definir tiempos</button>";

    page += "<h2>Horarios</h2>";
    page += "<div id='schedules'><em>Cargando...</em></div>";

    page += "<h3>Agregar horario</h3>";
    page += "CH: <select id='ch'><option value='2'>Luces (CH2)</option><option value='3'>Riego (CH3)</option></select> ";
    page += "Hora: <input id='hour' size=2> ";
    page += "Minuto: <input id='minute' size=2> ";
    page += "Acción: <select id='on'><option value='1'>Encender</option><option value='0'>Apagar</option></select>";
    page += "<div>Días: <label><input type=checkbox id=d0>Dom</label> <label><input type=checkbox id=d1>Lun</label> <label><input type=checkbox id=d2>Mar</label> <label><input type=checkbox id=d3>Mié</label> <label><input type=checkbox id=d4>Jue</label> <label><input type=checkbox id=d5>Vie</label> <label><input type=checkbox id=d6>Sáb</label></div>";
    page += " <button id='addBtn'>Agregar</button>";

    page += R"RAW(
<script>
async function loadSchedules(){
  let res = await fetch('/schedules');
  let arr = await res.json();
  let html = '<table><tr><th>#</th><th>CH</th><th>Hora</th><th>Minuto</th><th>Acción</th><th>Activado</th><th>Acciones</th></tr>';
  function daysToText(mask){
    if(mask === undefined) return '';
    const names=['Dom','Lun','Mar','Mié','Jue','Vie','Sáb'];
    let out=[];
    for(let b=0;b<7;b++) if(mask & (1<<b)) out.push(names[b]);
    return out.join(',');
  }
  for(let i=0;i<arr.length;i++){
    let s=arr[i];
    html += `<tr><td>${i}</td><td>${s.ch}</td><td>${s.hour}</td><td>${s.minute}</td><td>${s.on? 'Encendido':'Apagado'}</td><td>${s.enabled? 'Sí':'No'}</td><td>${daysToText(s.days)}</td>`;
    html += `<td><button onclick="del(${i})">Eliminar</button> <button onclick="toggle(${i},${s.enabled?1:0})">${s.enabled? 'Desactivar':'Activar'}</button> <button onclick="edit(${i})">Editar</button></td></tr>`;
  }
  html += '</table>';
  document.getElementById('schedules').innerHTML = html;
}
async function loadSensor(){
  try{
    let r = await fetch('/sensor');
    let o = await r.json();
    document.getElementById('sensor').innerText = `Interior: ${o.in.temp} °C, ${o.in.hum} % \nExterior: ${o.out.temp} °C, ${o.out.hum} %`;
  }catch(e){ document.getElementById('sensor').innerText = 'Error de temperatura'; }
}

async function loadThermostat(){
  try{
    let r = await fetch('/thermostat');
    let o = await r.json();
    document.getElementById('thermostat').innerText = `Temp: ${o.temp} °C | Consigna: ${o.setpoint} °C | Histeresis: ${o.hysteresis} °C | Habilitado: ${o.enabled}`;
    document.getElementById('setpoint').value = o.setpoint;
    document.getElementById('hysteresis').value = o.hysteresis;
    document.getElementById('ten').value = o.enabled? '1':'0';
  }catch(e){ document.getElementById('thermostat').innerText = 'Error del termostato'; }
}

async function loadAutomation(){
  try{
    let r = await fetch('/automation');
    let o = await r.json();
    document.getElementById('automation').innerText = `Lights min: ${o.dailyLightMinHours} h | Accum: ${o.dailyLightAccumHours} h | Irr count: ${o.irrigationCount} | Dur(s): ${o.irrigationDurationSec} | Start: ${o.irrigationStartHour}`;
    document.getElementById('dailyHours').value = o.dailyLightMinHours;
    document.getElementById('irCount').value = o.irrigationCount;
    document.getElementById('irDur').value = o.irrigationDurationSec;
    document.getElementById('irStart').value = o.irrigationStartHour;
    // render history if present
    try{
      if (o.history && o.history.length) {
        let html = '<h3>Historial horas de luz</h3><table><tr><th>Fecha</th><th>Horas</th></tr>';
        for(let i=0;i<o.history.length;i++){
          let h = o.history[i];
          // convert year+yday to a readable date approximated
          let d = new Date(Date.UTC(h.year,0,1));
          d.setUTCDate(d.getUTCDate() + h.yday);
          let ds = d.toISOString().slice(0,10);
          html += `<tr><td>${ds}</td><td>${(h.accumHours).toFixed(2)}</td></tr>`;
        }
        html += '</table>';
        document.getElementById('lightHistory').innerHTML = html;
      } else document.getElementById('lightHistory').innerHTML = '<em>Sin historial</em>';
    }catch(e){ document.getElementById('lightHistory').innerHTML = '<em>Error de historial</em>'; }
  }catch(e){ document.getElementById('automation').innerText = 'Error de automatización'; }
}

document.getElementById('setDaily').addEventListener('click', async ()=>{
  let h = document.getElementById('dailyHours').value;
  await fetch(`/automation?action=setDaily&hours=${encodeURIComponent(h)}`);
  loadAutomation();
});
document.getElementById('setIrr').addEventListener('click', async ()=>{
  let cnt = document.getElementById('irCount').value;
  let dur = document.getElementById('irDur').value;
  let st = document.getElementById('irStart').value;
  await fetch(`/automation?action=setIrrigation&count=${encodeURIComponent(cnt)}&duration=${encodeURIComponent(dur)}&start=${encodeURIComponent(st)}`);
  loadAutomation();
});
document.getElementById('setIrrTimes').addEventListener('click', async ()=>{
  let times = document.getElementById('irTimes').value;
  let dur = document.getElementById('irDur').value || '60';
  await fetch(`/automation?action=setIrrTimes&times=${encodeURIComponent(times)}&duration=${encodeURIComponent(dur)}`);
  loadAutomation();
});

document.getElementById('setTherm').addEventListener('click', async ()=>{
  let sp = document.getElementById('setpoint').value;
  let hy = document.getElementById('hysteresis').value;
  let en = document.getElementById('ten').value;
  await fetch(`/thermostat?action=set&setpoint=${encodeURIComponent(sp)}&hysteresis=${encodeURIComponent(hy)}&enabled=${encodeURIComponent(en)}`);
  loadThermostat();
});
document.getElementById('setAdv').addEventListener('click', async ()=>{
  let maxrun = document.getElementById('maxrun').value || '0';
  let overt = document.getElementById('overtemp').value || '200';
  let extl = document.getElementById('extlimit').value || '200';
  let logen = document.getElementById('logen').value || '0';
  await fetch(`/thermostat?action=setAdvanced&maxruntime=${encodeURIComponent(maxrun)}&overtemp=${encodeURIComponent(overt)}&extlimit=${encodeURIComponent(extl)}&log=${encodeURIComponent(logen)}`);
  loadThermostat();
});
document.getElementById('dlLog').addEventListener('click', ()=>{ window.location='/thermostat?action=download'; });
async function del(i){
  await fetch(`/schedule?action=delete&index=${i}`);
  loadSchedules();
}
async function toggle(i,cur){
  let newv = cur?0:1;
  await fetch(`/schedule?action=enable&index=${i}&enabled=${newv}`);
  loadSchedules();
}
async function edit(i){
  let ch = prompt('CH (2=Luces,3=Riego):');
  if (ch === null) return;
  let hour = prompt('Hora (0-23):'); if (hour === null) return;
  let minute = prompt('Minuto (0-59):'); if (minute === null) return;
  let on = prompt('Acción? (1=encender,0=apagar):'); if (on === null) return;
  let days = prompt('Días (lista separada por comas 0=Dom..6=Sáb o "all")','all'); if (days === null) return;
  let mask = 0;
  if(days.trim().toLowerCase() === 'all') mask = 0x7F;
  else {
    days.split(',').forEach(x=>{ let v=parseInt(x); if(!isNaN(v) && v>=0 && v<7) mask |= (1<<v); });
  }
  await fetch(`/schedule?action=edit&index=${i}&ch=${encodeURIComponent(ch)}&hour=${encodeURIComponent(hour)}&minute=${encodeURIComponent(minute)}&on=${encodeURIComponent(on)}&days=${encodeURIComponent(mask)}`);
  loadSchedules();
}
document.getElementById('addBtn').addEventListener('click', async ()=>{
  let ch=document.getElementById('ch').value;
  let hour=document.getElementById('hour').value;
  let minute=document.getElementById('minute').value;
  let on=document.getElementById('on').value;
  let mask = 0;
  for(let b=0;b<7;b++) if(document.getElementById('d'+b).checked) mask |= (1<<b);
  // basic validation
  if(!ch||!hour||!minute){ alert('Rellena los campos'); return; }
  await fetch(`/schedule?action=add&ch=${encodeURIComponent(ch)}&hour=${encodeURIComponent(hour)}&minute=${encodeURIComponent(minute)}&on=${encodeURIComponent(on)}&days=${encodeURIComponent(mask)}`);
  loadSchedules();
});
loadSchedules();
setInterval(loadSensor, 5000);
loadSensor();
setInterval(loadThermostat, 5000);
loadThermostat();
 loadAutomation();
 setInterval(loadAutomation, 10000);
</script>
)RAW";

    page += "</body></html>";
    sendResponse(client, "text/html", page);
    return;
  }

    // Server-Sent Events endpoint: keep connection open and register client
    if (path.startsWith("/events")) {
      client.print("HTTP/1.1 200 OK\r\n");
      client.print("Content-Type: text/event-stream\r\n");
      client.print("Cache-Control: no-cache\r\n");
      client.print("Connection: keep-alive\r\n\r\n");
      // register client in first free slot
      for (int i = 0; i < 4; ++i) {
        if (!sseClients[i] || !sseClients[i].connected()) {
          sseClients[i] = client;
          return; // keep connection open
        }
      }
      // no slot available
      String msg = "No SSE slots available";
      sendResponse(client, "text/plain", msg);
      return;
    }

  // Direct download of logs file
  if (path.startsWith("/logs") || path.startsWith("/logs.txt")) {
    if (SPIFFS.exists("/logs.txt")) {
      File f = SPIFFS.open("/logs.txt", "r");
      if (f) {
        client.print("HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n");
        client.print("Connection: close\r\n\r\n");
        while (f.available()) client.write(f.read());
        f.close();
        return;
      }
    }
    sendResponse(client, "text/plain", String("No logs"));
    return;
  }

  // /relay?ch=1&state=on  or /status
  if (path.startsWith("/relay")) {
    int q = path.indexOf('?');
    String query = q >= 0 ? path.substring(q + 1) : String();
    int ch = 0; String state;
    // parse simple query
    int p = 0;
    while (p < (int)query.length()) {
      int amp = query.indexOf('&', p);
      if (amp == -1) amp = query.length();
      String pair = query.substring(p, amp);
      int eq = pair.indexOf('=');
      if (eq > 0) {
        String k = pair.substring(0, eq);
        String v = pair.substring(eq + 1);
        if (k == "ch") ch = v.toInt();
        else if (k == "state") state = v;
      }
      p = amp + 1;
    }
    if (state == "on") setRelay(ch, true);
    else if (state == "off") setRelay(ch, false);
    else if (state == "toggle") setRelay(ch, !getRelay(ch));
    sendResponse(client, "application/json", relayStatusJson());
    return;
  }

  if (path.startsWith("/status")) {
    sendResponse(client, "application/json", relayStatusJson());
    return;
  }

  if (path.startsWith("/schedules")) {
    // return JSON list of schedules
    extern String scheduleListJson();
    String j = scheduleListJson();
    sendResponse(client, "application/json", j);
    return;
  }

  if (path.startsWith("/sensor")) {
    extern String sensorJson();
    String j = sensorJson();
    sendResponse(client, "application/json", j);
    return;
  }

  if (path.startsWith("/thermostat")) {
    int q = path.indexOf('?');
    String query = q >= 0 ? path.substring(q + 1) : String();
    int p = 0;
    String action;
    float sp = 0; float hy = 0; int enabledVal = -1;
    while (p < (int)query.length()) {
      int amp = query.indexOf('&', p);
      if (amp == -1) amp = query.length();
      String pair = query.substring(p, amp);
      int eq = pair.indexOf('=');
      if (eq > 0) {
        String k = pair.substring(0, eq);
        String v = pair.substring(eq + 1);
        if (k == "action") action = v;
        else if (k == "setpoint") sp = v.toFloat();
        else if (k == "hysteresis") hy = v.toFloat();
        else if (k == "enabled") enabledVal = v.toInt();
      }
      p = amp + 1;
    }
    if (action == "set") {
      bool en = (enabledVal == 1);
      bool ok = setThermostat(sp, hy, en);
      sendResponse(client, "application/json", String(ok?"{\"ok\":1}":"{\"ok\":0}"));
      return;
    }
    if (action == "setAdvanced") {
      int maxr = 0; float overt = 200.0; float extl = 200.0; bool logen = false;
      // parse again simpler
      int mpos = query.indexOf("maxruntime="); if (mpos>=0) { int amp = query.indexOf('&', mpos); if (amp==-1) amp=query.length(); maxr = query.substring(mpos+11, amp).toInt(); }
      int opos = query.indexOf("overtemp="); if (opos>=0) { int amp = query.indexOf('&', opos); if (amp==-1) amp=query.length(); overt = query.substring(opos+9, amp).toFloat(); }
      int epos = query.indexOf("extlimit="); if (epos>=0) { int amp = query.indexOf('&', epos); if (amp==-1) amp=query.length(); extl = query.substring(epos+9, amp).toFloat(); }
      int lpos = query.indexOf("log="); if (lpos>=0) { int amp = query.indexOf('&', lpos); if (amp==-1) amp=query.length(); logen = query.substring(lpos+4, amp).toInt() == 1; }
      bool ok = setThermostatAdvanced((unsigned long)maxr, overt, extl, logen);
      sendResponse(client, "application/json", String(ok?"{\"ok\":1}":"{\"ok\":0}"));
      return;
    }
    if (action == "download") {
      // stream log file if exists
      if (SPIFFS.exists("/therm_log.csv")) {
        File f = SPIFFS.open("/therm_log.csv", "r");
        if (f) {
          client.print("HTTP/1.1 200 OK\r\nContent-Type: text/csv\r\n");
          client.print("Connection: close\r\n\r\n");
          while (f.available()) client.write(f.read());
          f.close();
          return;
        }
      }
      String nf = "No log";
      sendResponse(client, "text/plain", nf);
      return;
    }

    // Allow downloading the general logs file
    if (action == "download_logs") {
      if (SPIFFS.exists("/logs.txt")) {
        File f = SPIFFS.open("/logs.txt", "r");
        if (f) {
          client.print("HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n");
          client.print("Connection: close\r\n\r\n");
          while (f.available()) client.write(f.read());
          f.close();
          return;
        }
      }
      String nf = "No logs";
      sendResponse(client, "text/plain", nf);
      return;
    }
    // default: return thermostat JSON
    extern String thermostatJson();
    String j = thermostatJson();
    sendResponse(client, "application/json", j);
    return;
  }

  if (path.startsWith("/automation")) {
    int q = path.indexOf('?');
    String query = q >= 0 ? path.substring(q + 1) : String();
    int p = 0; String action;
    while (p < (int)query.length()) {
      int amp = query.indexOf('&', p);
      if (amp == -1) amp = query.length();
      String pair = query.substring(p, amp);
      int eq = pair.indexOf('=');
      if (eq > 0) {
        String k = pair.substring(0, eq);
        String v = pair.substring(eq + 1);
        if (k == "action") action = v;
      }
      p = amp + 1;
    }
    if (action == "setDaily") {
      int hpos = query.indexOf("hours=");
      if (hpos >= 0) {
        int amp = query.indexOf('&', hpos); if (amp == -1) amp = query.length();
        float h = query.substring(hpos+6, amp).toFloat();
        extern bool setDailyLightMinHours(float hours);
        bool ok = setDailyLightMinHours(h);
        sendResponse(client, "application/json", String(ok?"{\"ok\":1}":"{\"ok\":0}"));
        return;
      }
    }
    if (action == "setIrrigation") {
      int cpos = query.indexOf("count="); int dpos = query.indexOf("duration="); int spos = query.indexOf("start=");
      if (cpos>=0 && dpos>=0 && spos>=0) {
        int amp = query.indexOf('&', cpos); if (amp==-1) amp = query.length(); int cnt = query.substring(cpos+6, amp).toInt();
        amp = query.indexOf('&', dpos); if (amp==-1) amp = query.length(); int dur = query.substring(dpos+9, amp).toInt();
        amp = query.indexOf('&', spos); if (amp==-1) amp = query.length(); int st = query.substring(spos+6, amp).toInt();
        extern bool setIrrigationConfig(uint8_t countPerDay, uint16_t durationSec, uint8_t startHour);
        bool ok = setIrrigationConfig((uint8_t)cnt, (uint16_t)dur, (uint8_t)st);
        sendResponse(client, "application/json", String(ok?"{\"ok\":1}":"{\"ok\":0}"));
        return;
      }
      if (action == "setIrrTimes") {
        int tpos = query.indexOf("times="); int dpos = query.indexOf("duration=");
        if (tpos >= 0 && dpos >= 0) {
          int amp = query.indexOf('&', tpos); if (amp == -1) amp = query.length(); String times = query.substring(tpos+6, amp);
          amp = query.indexOf('&', dpos); if (amp == -1) amp = query.length(); int dur = query.substring(dpos+9, amp).toInt();
          extern bool setIrrigationTimesCSV(const String &timesCsv, uint16_t durationSec);
          bool ok = setIrrigationTimesCSV(times, (uint16_t)dur);
          sendResponse(client, "application/json", String(ok?"{\"ok\":1}":"{\"ok\":0}"));
          return;
        }
      }
      if (action == "history") {
        extern String automationHistoryJson();
        String j = automationHistoryJson();
        sendResponse(client, "application/json", j);
        return;
      }
    }
    extern String automationJson();
    String j = automationJson();
    sendResponse(client, "application/json", j);
    return;
  }

  if (path.startsWith("/schedule")) {
    int q = path.indexOf('?');
    String query = q >= 0 ? path.substring(q + 1) : String();
    int ch = 0; String action; int hour = -1; int minute = -1; int index = -1; int daysMask = 0x7F;
    int p = 0;
    while (p < (int)query.length()) {
      int amp = query.indexOf('&', p);
      if (amp == -1) amp = query.length();
      String pair = query.substring(p, amp);
      int eq = pair.indexOf('=');
      if (eq > 0) {
        String k = pair.substring(0, eq);
        String v = pair.substring(eq + 1);
        if (k == "ch") ch = v.toInt();
        else if (k == "action") action = v;
        else if (k == "hour") hour = v.toInt();
        else if (k == "minute") minute = v.toInt();
        else if (k == "index") index = v.toInt();
        else if (k == "days") daysMask = v.toInt();
      }
      p = amp + 1;
    }
    // action=add -> needs ch,hour,minute and optional on flag
    if (action == "add" && ch > 0 && hour >= 0 && minute >= 0) {
      String onVal = "1";
      // parse on= from query
      int onPos = query.indexOf("on=");
      if (onPos >= 0) {
        int amp = query.indexOf('&', onPos);
        if (amp == -1) amp = query.length();
        onVal = query.substring(onPos + 3, amp);
      }
      bool onFlag = (onVal == "1" || onVal == "true" || onVal == "on");
      extern bool addSchedule(uint8_t ch, uint8_t hour, uint8_t minute, bool on, uint8_t daysMask);
      bool ok = addSchedule((uint8_t)ch, (uint8_t)hour, (uint8_t)minute, onFlag, (uint8_t)daysMask);
      sendResponse(client, "application/json", String(ok ? "{\"ok\":1}" : "{\"ok\":0}"));
      return;
    }

    // action=edit -> edit schedule including days
    if (action == "edit" && index >= 0 && ch > 0 && hour >= 0 && minute >= 0) {
      String onVal = "1";
      int onPos = query.indexOf("on=");
      if (onPos >= 0) {
        int amp = query.indexOf('&', onPos);
        if (amp == -1) amp = query.length();
        onVal = query.substring(onPos + 3, amp);
      }
      bool onFlag = (onVal == "1" || onVal == "true" || onVal == "on");
      extern bool editSchedule(size_t index, uint8_t ch, uint8_t hour, uint8_t minute, bool on, uint8_t daysMask);
      bool ok = editSchedule((size_t)index, (uint8_t)ch, (uint8_t)hour, (uint8_t)minute, onFlag, (uint8_t)daysMask);
      sendResponse(client, "application/json", String(ok ? "{\"ok\":1}" : "{\"ok\":0}"));
      return;
    }

    // action=delete -> remove schedule by index
    if (action == "delete" && index >= 0) {
      extern bool removeSchedule(size_t index);
      bool ok = removeSchedule((size_t)index);
      sendResponse(client, "application/json", String(ok ? "{\"ok\":1}" : "{\"ok\":0}"));
      return;
    }
    // unsupported -> return 400
    String notfound = "Solicitud de horario inválida";
    client.print("HTTP/1.1 400 Bad Request\r\nContent-Length: ");
    client.print(notfound.length()); client.print("\r\nConnection: close\r\n\r\n");
    client.print(notfound);
    return;
  }

  // default 404
  String notfound = "No encontrado";
  client.print("HTTP/1.1 404 Not Found\r\nContent-Length: ");
  client.print(notfound.length()); client.print("\r\nConnection: close\r\n\r\n");
  client.print(notfound);
}

void webBegin() {
  WiFi.mode(WIFI_STA);
  logPrint(String("Connecting to WiFi '")); logPrint(String(WIFI_SSID)); logPrintln(String("'..."));
  // Configure static IP if defined in config.h
  if (WiFi.config(STATIC_IP, STATIC_GATEWAY, STATIC_SUBNET, STATIC_DNS)) {
    logPrintln(String("Static IP configured"));
  }
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long start = millis();
  // Blink green while attempting to connect
  unsigned long lastToggle = 0;
  bool ledOn = false;
  while (WiFi.status() != WL_CONNECTED && millis() - start < 30000) {
    unsigned long now = millis();
    if (now - lastToggle >= 500) {
      lastToggle = now;
      ledOn = !ledOn;
      if (ledOn) setPixelColorRGB(0, 255, 0);
      else setPixelColorRGB(0, 0, 0);
    }
    delay(200);
    logPrint(String("."));
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    logPrint(String("WiFi connected, IP: ")); logPrintln(WiFi.localIP().toString());
    // solid blue when connected
    setPixelColorRGB(0, 0, 255);
  } else {
    logPrintln(String("WiFi connection failed (timeout)"));
    // solid red on failure
    setPixelColorRGB(255, 0, 0);
  }
  server.begin();
  telnetServer.begin();
  logPrintln(String("Web server started on port 80"));
}

void webHandle() {
  WiFiClient client = server.available();
  if (!client) return;
  // wait for data
  unsigned long timeout = millis() + 1000;
  while (!client.available() && millis() < timeout) yield();
  if (!client.available()) { client.stop(); return; }
  String req = client.readStringUntil('\r');
  // req contains something like: GET /path?query HTTP/1.1
  // extract path
  int firstSpace = req.indexOf(' ');
  int secondSpace = req.indexOf(' ', firstSpace + 1);
  String path = "/";
  if (firstSpace != -1 && secondSpace != -1) {
    path = req.substring(firstSpace + 1, secondSpace);
  }
  handleRequest(client, path);
  // if this was an SSE connection we registered it and must not close here
  if (path.startsWith("/events")) {
    // don't stop the client; keep connection open for SSE
    return;
  }
  delay(1);
  client.stop();

  // Accept telnet clients (non-blocking)
  WiFiClient t = telnetServer.available();
  if (t) {
    // register in first free slot
    for (int i = 0; i < 2; ++i) {
      if (!telnetClients[i] || !telnetClients[i].connected()) {
        telnetClients[i] = t;
        telnetClients[i].print("Welcome to device serial log\r\n");
        break;
      }
    }
    // if no slot, reject
    if (t && t.connected()) t.stop();
  }
}
