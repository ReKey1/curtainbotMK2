// webui.h — Web server: touch-friendly minimal control page + JSON API.
//   GET  /         the control page
//   GET  /status   current state as JSON
//   POST /command  {"cmd":"...", ...} -> dispatchCommand() (see commands.h)
#pragma once
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include "config.h"
#include "control.h"
#include "scheduler.h"
#include "commands.h"

WebServer server(HTTP_PORT);

const char HTML_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>CurtainBot</title>
<link rel="icon" type="image/svg+xml" href="data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCA2NCA2NCI+PHJlY3Qgd2lkdGg9IjY0IiBoZWlnaHQ9IjY0IiByeD0iMTQiIGZpbGw9IiMwYTg0ZmYiLz48cmVjdCB4PSIxMyIgeT0iMTMiIHdpZHRoPSIxNiIgaGVpZ2h0PSIzOCIgcng9IjQiIGZpbGw9IiNmZmYiLz48cmVjdCB4PSIzNSIgeT0iMTMiIHdpZHRoPSIxNiIgaGVpZ2h0PSIzOCIgcng9IjQiIGZpbGw9IiNmZmYiLz48L3N2Zz4=">
<style>
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body {
    font-family: system-ui, -apple-system, sans-serif;
    background: #f2f2f7;
    color: #1c1c1e;
    max-width: 480px;
    margin: 0 auto;
    padding: 16px;
  }
  h1 { font-size: 1.35rem; text-align: center; margin: 6px 0 16px; }
  .card {
    background: #fff;
    border-radius: 14px;
    padding: 16px;
    margin-bottom: 14px;
    box-shadow: 0 1px 3px rgba(0,0,0,0.08);
  }
  .row-top { display: flex; align-items: baseline; gap: 10px; margin-bottom: 12px; }
  .name { font-size: 1.05rem; font-weight: 600; }
  .state { font-weight: 700; }
  .state-OPEN { color: #34c759; }
  .state-CLOSED { color: #ff3b30; }
  .state-MOVING_OPEN, .state-MOVING_CLOSED { color: #ff9500; }
  .state-STOPPED { color: #8e8e93; }
  .pos { margin-left: auto; color: #8e8e93; font-variant-numeric: tabular-nums; }
  .btns { display: flex; gap: 10px; }
  button {
    flex: 1;
    min-height: 50px;
    font-size: 1rem;
    font-weight: 600;
    border: none;
    border-radius: 10px;
    background: #e5e5ea;
    color: #1c1c1e;
    touch-action: manipulation;
    cursor: pointer;
  }
  button:active { opacity: 0.6; }
  .b-open  { background: #d7f2dd; color: #1d7a35; }
  .b-close { background: #fadcda; color: #a92c22; }
  .goto { display: flex; align-items: center; gap: 12px; margin-top: 14px; }
  .goto input[type=range] { flex: 1; height: 34px; touch-action: manipulation; }
  .goto .val { width: 3em; text-align: right; color: #8e8e93; font-variant-numeric: tabular-nums; }
  .goto button { flex: 0 0 76px; }
  .sched { display: flex; align-items: center; gap: 12px; flex-wrap: wrap; }
  .sched .name { flex: 1 1 auto; }
  .sched .current { flex: 0 0 100%; color: #8e8e93; font-size: 0.85rem; }
  input[type=time] {
    font-size: 1.05rem;
    padding: 10px;
    border: 1px solid #d1d1d6;
    border-radius: 10px;
    background: inherit;
    color: inherit;
  }
  input[type=checkbox] { width: 26px; height: 26px; }
  .sched button { flex: 0 0 76px; }
  #clock { text-align: center; color: #8e8e93; font-size: 0.9rem; margin-top: 4px; }
  @media (prefers-color-scheme: dark) {
    body { background: #000; color: #f2f2f7; }
    .card { background: #1c1c1e; box-shadow: none; }
    button { background: #2c2c2e; color: #f2f2f7; }
    .b-open  { background: #12351c; color: #6fdd8b; }
    .b-close { background: #3d1512; color: #ff7b72; }
    input[type=time] { border-color: #3a3a3c; }
  }
</style>
</head>
<body>
<h1>CurtainBot</h1>

<div class="card">
  <div class="row-top">
    <span class="name">Left</span>
    <b class="state" id="left-state">?</b>
    <span class="pos" id="left-pos"></span>
  </div>
  <div class="btns">
    <button class="b-open" onclick="cmd('open_left')">Open</button>
    <button onclick="cmd('stop_left')">Stop</button>
    <button class="b-close" onclick="cmd('close_left')">Close</button>
  </div>
  <div class="goto">
    <input type="range" id="left-goto" min="0" max="100" value="50"
           oninput="sliderVal('left')" onpointerdown="markGotoTouched('left')">
    <span class="val" id="left-goto-val">50%</span>
    <button onclick="gotoPos('left')">Go</button>
  </div>
</div>

<div class="card">
  <div class="row-top">
    <span class="name">Right</span>
    <b class="state" id="right-state">?</b>
    <span class="pos" id="right-pos"></span>
  </div>
  <div class="btns">
    <button class="b-open" onclick="cmd('open_right')">Open</button>
    <button onclick="cmd('stop_right')">Stop</button>
    <button class="b-close" onclick="cmd('close_right')">Close</button>
  </div>
  <div class="goto">
    <input type="range" id="right-goto" min="0" max="100" value="50"
           oninput="sliderVal('right')" onpointerdown="markGotoTouched('right')">
    <span class="val" id="right-goto-val">50%</span>
    <button onclick="gotoPos('right')">Go</button>
  </div>
</div>

<div class="card sched">
  <span class="name">Morning open</span>
  <input type="time" id="morning-time">
  <input type="checkbox" id="morning-enabled">
  <button onclick="sendSched('morning')">Set</button>
  <div class="current" id="morning-current">Currently: --</div>
</div>

<div class="card sched">
  <span class="name">Night close</span>
  <input type="time" id="night-time">
  <input type="checkbox" id="night-enabled">
  <button onclick="sendSched('night')">Set</button>
  <div class="current" id="night-current">Currently: --</div>
</div>

<p id="clock">Device time: --</p>

<script>
function pad(n) { return String(n).padStart(2, '0'); }

function setState(id, st) {
  const el = document.getElementById(id);
  el.textContent = st.replace('_', ' ');
  el.className = 'state state-' + st;
}

function posText(p) { return (p >= 0) ? p + '%' : ''; }

// The goto slider stops tracking live position the moment it's touched
// (pointerdown), not just while focused — touch drags don't reliably keep
// "focus" on a range input the whole time, so a poll could otherwise still
// snap the thumb back mid-drag or in the gap before "Go" is pressed. It
// resumes tracking once "Go" sends the command. The live position is always
// visible separately in the state row (e.g. "50%") regardless.
const gotoTouched = { left: false, right: false };

function markGotoTouched(which) { gotoTouched[which] = true; }

function updateGoto(which, pos) {
  if (pos < 0 || gotoTouched[which]) return;
  document.getElementById(which + '-goto').value = pos;
  document.getElementById(which + '-goto-val').textContent = pos + '%';
}

// Schedule inputs are loaded once and never touched again after that, so a
// live poll can never stomp on an in-progress edit (native time pickers
// don't reliably register as "focused" while the user is mid-selection).
// The separate "Currently: ..." readout stays live instead.
const scheduleLoaded = { morning: false, night: false };

function updateSchedule(prefix, hour, min, enabled) {
  document.getElementById(prefix + '-current').textContent =
    'Currently: ' + pad(hour) + ':' + pad(min) + (enabled ? ' · On' : ' · Off');

  if (scheduleLoaded[prefix]) return;
  document.getElementById(prefix + '-time').value = pad(hour) + ':' + pad(min);
  document.getElementById(prefix + '-enabled').checked = enabled;
  scheduleLoaded[prefix] = true;
}

async function poll() {
  try {
    const s = await (await fetch('/status')).json();
    setState('left-state', s.leftState);
    setState('right-state', s.rightState);
    document.getElementById('left-pos').textContent  = posText(s.leftPos);
    document.getElementById('right-pos').textContent = posText(s.rightPos);
    updateGoto('left', s.leftPos);
    updateGoto('right', s.rightPos);
    document.getElementById('clock').textContent =
      s.timeSynced ? 'Device time: ' + s.time : 'Device time: not synced';
    updateSchedule('morning', s.morningHour, s.morningMin, s.morningEnabled);
    updateSchedule('night', s.nightHour, s.nightMin, s.nightEnabled);
  } catch (e) {}
}

async function send(obj) {
  try {
    await fetch('/command', {
      method: 'POST',
      headers: {'Content-Type': 'application/json'},
      body: JSON.stringify(obj)
    });
  } catch (e) {}
}

function cmd(c) { send({cmd: c}); }

function sliderVal(which) {
  document.getElementById(which + '-goto-val').textContent =
    document.getElementById(which + '-goto').value + '%';
}

function gotoPos(which) {
  send({cmd: 'goto_' + which,
        pos: Number(document.getElementById(which + '-goto').value)});
  gotoTouched[which] = false;  // resume tracking live position toward the new target
}

function sendSched(which) {
  const [h, m] = document.getElementById(which + '-time').value.split(':').map(Number);
  send({cmd: 'set_' + which, hour: h, min: m,
        enabled: document.getElementById(which + '-enabled').checked});
}

poll();
setInterval(poll, 2000);
</script>
</body>
</html>
)rawliteral";

const char* stateToString(Motor::State s) {
  switch (s) {
    case Motor::STOPPED:       return "STOPPED";
    case Motor::MOVING_OPEN:   return "MOVING_OPEN";
    case Motor::MOVING_CLOSED: return "MOVING_CLOSED";
    case Motor::OPEN:          return "OPEN";
    case Motor::CLOSED:        return "CLOSED";
  }
  return "UNKNOWN";
}

// 0-100 (% open), or -1 while the motor is uncalibrated.
int positionForJson(const Motor& m) {
  float p = m.positionPercent();
  return (p < 0) ? -1 : (int)(p + 0.5f);
}

void handleRoot() {
  server.send_P(200, "text/html", HTML_PAGE);
}

void handleStatus() {
  JsonDocument doc;
  doc["leftState"]      = stateToString(motorLeft.getState());
  doc["rightState"]     = stateToString(motorRight.getState());
  doc["leftPos"]        = positionForJson(motorLeft);
  doc["rightPos"]       = positionForJson(motorRight);
  doc["morningHour"]    = morningSched.hour;
  doc["morningMin"]     = morningSched.min;
  doc["morningEnabled"] = morningSched.enabled;
  doc["nightHour"]      = nightSched.hour;
  doc["nightMin"]       = nightSched.min;
  doc["nightEnabled"]   = nightSched.enabled;

  struct tm t;
  bool synced = getTimeNow(t);
  doc["timeSynced"] = synced;
  if (synced) {
    char timeBuf[6];
    snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", t.tm_hour, t.tm_min);
    doc["time"] = timeBuf;
  }

  char out[384];
  serializeJson(doc, out, sizeof(out));
  server.send(200, "application/json", out);
}

void handleCommand() {
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"no body\"}");
    return;
  }

  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain")) != DeserializationError::Ok) {
    server.send(400, "application/json", "{\"error\":\"bad json\"}");
    return;
  }

  const char* cmd = doc["cmd"] | "";
  Serial.print("[HTTP] Command: ");
  Serial.println(server.arg("plain"));

  switch (dispatchCommand(cmd, doc.as<JsonObjectConst>())) {
    case CmdResult::OK:
      server.send(200, "application/json", "{\"ok\":true}");
      break;
    case CmdResult::FAILED:
      server.send(400, "application/json", "{\"error\":\"invalid arguments\"}");
      break;
    case CmdResult::UNKNOWN:
      server.send(400, "application/json", "{\"error\":\"unknown command\"}");
      break;
  }
}

void webBegin() {
  server.on("/",        HTTP_GET,  handleRoot);
  server.on("/status",  HTTP_GET,  handleStatus);
  server.on("/command", HTTP_POST, handleCommand);
  server.begin();
}
