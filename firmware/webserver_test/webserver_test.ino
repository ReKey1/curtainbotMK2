#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include "arduino_secrets.h"

// ---- Mock curtain state -----------------------------------------------

enum class CurtainState { OPEN, CLOSED, MOVING_OPEN, MOVING_CLOSED };

CurtainState leftState  = CurtainState::CLOSED;
CurtainState rightState = CurtainState::CLOSED;

int  morningHour    = 7;
int  morningMin     = 30;
bool morningEnabled = false;

int  nightHour    = 22;
int  nightMin     = 0;
bool nightEnabled = false;

// ---- Server -----------------------------------------------------------

WebServer server(80);

// ---- Embedded HTML page -----------------------------------------------

const char HTML_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>CurtainBot Control</title>
  <style>
    *, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif;
      background: #111;
      color: #eee;
      min-height: 100vh;
      padding: 16px;
    }
    header {
      display: flex;
      align-items: center;
      justify-content: space-between;
      margin-bottom: 20px;
    }
    h1 { font-size: 1.3rem; font-weight: 600; }
    #conn-status {
      display: flex;
      align-items: center;
      gap: 6px;
      font-size: 0.8rem;
    }
    #conn-dot {
      width: 10px; height: 10px;
      border-radius: 50%;
      background: #555;
      transition: background 0.3s;
    }
    #conn-dot.ok  { background: #4caf50; }
    #conn-dot.err { background: #f44336; }
    .card {
      background: #1e1e1e;
      border-radius: 12px;
      padding: 16px;
      margin-bottom: 14px;
    }
    .card h2 {
      font-size: 0.75rem;
      font-weight: 600;
      margin-bottom: 12px;
      color: #777;
      text-transform: uppercase;
      letter-spacing: 0.08em;
    }
    .curtain-row {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 12px;
    }
    .curtain-card {
      background: #252525;
      border-radius: 10px;
      padding: 14px;
      text-align: center;
    }
    .curtain-name { font-size: 0.8rem; color: #888; margin-bottom: 8px; }
    .curtain-state {
      font-size: 1rem;
      font-weight: 700;
      margin-bottom: 12px;
      min-height: 1.4em;
    }
    .state-OPEN          { color: #4caf50; }
    .state-CLOSED        { color: #f44336; }
    .state-MOVING_OPEN   { color: #ff9800; }
    .state-MOVING_CLOSED { color: #ff9800; }
    .btn-row { display: flex; gap: 8px; justify-content: center; }
    .btn {
      flex: 1;
      padding: 12px 6px;
      border: none;
      border-radius: 8px;
      font-size: 0.85rem;
      font-weight: 600;
      cursor: pointer;
      transition: opacity 0.15s, transform 0.1s;
      min-height: 44px;
    }
    .btn:active { transform: scale(0.96); }
    .btn-open  { background: #2e7d32; color: #fff; }
    .btn-close { background: #b71c1c; color: #fff; }
    .schedule-row {
      display: flex;
      align-items: center;
      gap: 10px;
      flex-wrap: wrap;
    }
    .schedule-row label { font-size: 0.85rem; color: #888; min-width: 56px; }
    .time-input {
      background: #333;
      border: 1px solid #444;
      border-radius: 6px;
      color: #eee;
      font-size: 1rem;
      padding: 8px 10px;
      width: 110px;
    }
    .time-input:focus { outline: none; border-color: #888; }
    .toggle-wrap { display: flex; align-items: center; gap: 8px; margin-left: auto; }
    .toggle-wrap span { font-size: 0.8rem; color: #888; }
    .toggle { position: relative; width: 44px; height: 24px; }
    .toggle input { opacity: 0; width: 0; height: 0; }
    .slider {
      position: absolute; inset: 0;
      background: #444; border-radius: 24px;
      cursor: pointer; transition: background 0.25s;
    }
    .slider::before {
      content: ''; position: absolute;
      width: 18px; height: 18px; left: 3px; top: 3px;
      background: #fff; border-radius: 50%;
      transition: transform 0.25s;
    }
    .toggle input:checked + .slider { background: #4caf50; }
    .toggle input:checked + .slider::before { transform: translateX(20px); }
    .btn-set {
      background: #1565c0; color: #fff;
      border: none; border-radius: 8px;
      padding: 10px 14px; font-size: 0.85rem;
      font-weight: 600; cursor: pointer;
      min-height: 44px; transition: opacity 0.15s;
    }
    .btn-set:active { opacity: 0.75; }
  </style>
</head>
<body>

  <header>
    <h1>CurtainBot</h1>
    <div id="conn-status">
      <div id="conn-dot" class="err"></div>
      <span id="conn-label">Connecting...</span>
    </div>
  </header>

  <div class="card">
    <h2>Curtains</h2>
    <div class="curtain-row">
      <div class="curtain-card">
        <div class="curtain-name">Left</div>
        <div class="curtain-state state-CLOSED" id="left-state">CLOSED</div>
        <div class="btn-row">
          <button class="btn btn-open"  onclick="cmd('open_left')">Open</button>
          <button class="btn btn-close" onclick="cmd('close_left')">Close</button>
        </div>
      </div>
      <div class="curtain-card">
        <div class="curtain-name">Right</div>
        <div class="curtain-state state-CLOSED" id="right-state">CLOSED</div>
        <div class="btn-row">
          <button class="btn btn-open"  onclick="cmd('open_right')">Open</button>
          <button class="btn btn-close" onclick="cmd('close_right')">Close</button>
        </div>
      </div>
    </div>
  </div>

  <div class="card">
    <h2>Morning Schedule</h2>
    <div class="schedule-row">
      <label>Open at</label>
      <input class="time-input" type="time" id="morning-time" value="07:30">
      <div class="toggle-wrap">
        <span>Enable</span>
        <label class="toggle">
          <input type="checkbox" id="morning-enabled">
          <span class="slider"></span>
        </label>
      </div>
      <button class="btn-set" onclick="sendMorning()">Set</button>
    </div>
  </div>

  <div class="card">
    <h2>Night Schedule</h2>
    <div class="schedule-row">
      <label>Close at</label>
      <input class="time-input" type="time" id="night-time" value="22:00">
      <div class="toggle-wrap">
        <span>Enable</span>
        <label class="toggle">
          <input type="checkbox" id="night-enabled">
          <span class="slider"></span>
        </label>
      </div>
      <button class="btn-set" onclick="sendNight()">Set</button>
    </div>
  </div>

  <script>
    const dot   = document.getElementById('conn-dot');
    const label = document.getElementById('conn-label');

    function applyState(s) {
      setStateEl('left-state',  s.leftState);
      setStateEl('right-state', s.rightState);
      if (s.morningHour !== undefined) {
        document.getElementById('morning-time').value    = pad(s.morningHour) + ':' + pad(s.morningMin);
        document.getElementById('morning-enabled').checked = s.morningEnabled;
      }
      if (s.nightHour !== undefined) {
        document.getElementById('night-time').value    = pad(s.nightHour) + ':' + pad(s.nightMin);
        document.getElementById('night-enabled').checked = s.nightEnabled;
      }
    }

    function setStateEl(id, stateStr) {
      const el = document.getElementById(id);
      if (!el || !stateStr) return;
      el.textContent = stateStr.replace('_', ' ');
      el.className   = 'curtain-state state-' + stateStr;
    }

    function pad(n) { return String(n).padStart(2, '0'); }

    async function pollStatus() {
      try {
        const r = await fetch('/status');
        if (!r.ok) throw new Error(r.status);
        applyState(await r.json());
        dot.className     = 'ok';
        label.textContent = 'Connected';
      } catch(e) {
        dot.className     = 'err';
        label.textContent = 'Unreachable';
      }
    }

    async function sendCommand(obj) {
      try {
        await fetch('/command', {
          method: 'POST',
          headers: {'Content-Type': 'application/json'},
          body: JSON.stringify(obj)
        });
      } catch(e) { console.warn('Command failed', e); }
    }

    function cmd(c)       { sendCommand({cmd: c}); }
    function sendMorning() {
      const [h, m] = document.getElementById('morning-time').value.split(':').map(Number);
      sendCommand({ cmd: 'set_morning', hour: h, min: m,
                    enabled: document.getElementById('morning-enabled').checked });
    }
    function sendNight() {
      const [h, m] = document.getElementById('night-time').value.split(':').map(Number);
      sendCommand({ cmd: 'set_night', hour: h, min: m,
                    enabled: document.getElementById('night-enabled').checked });
    }

    pollStatus();
    setInterval(pollStatus, 1000);
  </script>
</body>
</html>
)rawliteral";

// ---- Helpers ----------------------------------------------------------

const char* stateToString(CurtainState s) {
  switch (s) {
    case CurtainState::OPEN:          return "OPEN";
    case CurtainState::CLOSED:        return "CLOSED";
    case CurtainState::MOVING_OPEN:   return "MOVING_OPEN";
    case CurtainState::MOVING_CLOSED: return "MOVING_CLOSED";
  }
  return "UNKNOWN";
}

void buildStateJson(char* buf, size_t len) {
  JsonDocument doc;
  doc["leftState"]      = stateToString(leftState);
  doc["rightState"]     = stateToString(rightState);
  doc["morningHour"]    = morningHour;
  doc["morningMin"]     = morningMin;
  doc["morningEnabled"] = morningEnabled;
  doc["nightHour"]      = nightHour;
  doc["nightMin"]       = nightMin;
  doc["nightEnabled"]   = nightEnabled;
  serializeJson(doc, buf, len);
}

// ---- HTTP handlers ----------------------------------------------------

void handleRoot() {
  server.send_P(200, "text/html", HTML_PAGE);
}

void handleStatus() {
  char buf[256];
  buildStateJson(buf, sizeof(buf));
  server.send(200, "application/json", buf);
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
  Serial.print("[HTTP] Received: ");
  Serial.println(server.arg("plain"));

  if (strcmp(cmd, "open_left") == 0) {
    leftState = CurtainState::OPEN;
  } else if (strcmp(cmd, "close_left") == 0) {
    leftState = CurtainState::CLOSED;
  } else if (strcmp(cmd, "open_right") == 0) {
    rightState = CurtainState::OPEN;
  } else if (strcmp(cmd, "close_right") == 0) {
    rightState = CurtainState::CLOSED;
  } else if (strcmp(cmd, "set_morning") == 0) {
    morningHour    = doc["hour"]    | morningHour;
    morningMin     = doc["min"]     | morningMin;
    morningEnabled = doc["enabled"] | morningEnabled;
  } else if (strcmp(cmd, "set_night") == 0) {
    nightHour    = doc["hour"]    | nightHour;
    nightMin     = doc["min"]     | nightMin;
    nightEnabled = doc["enabled"] | nightEnabled;
  } else {
    Serial.print("[HTTP] Unknown command: ");
    Serial.println(cmd);
    server.send(400, "application/json", "{\"error\":\"unknown command\"}");
    return;
  }

  Serial.printf("[HTTP] State updated -> cmd=%s\n", cmd);
  server.send(200, "application/json", "{\"ok\":true}");
}

// ---- Setup & loop -----------------------------------------------------

void setup() {
  Serial.begin(115200);
  delay(100);

  Serial.print("Connecting to WiFi");
  WiFi.begin(SECRET_SSID, SECRET_OPTIONAL_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  server.on("/",        HTTP_GET,  handleRoot);
  server.on("/status",  HTTP_GET,  handleStatus);
  server.on("/command", HTTP_POST, handleCommand);
  server.begin();

  Serial.print("Server started. Open http://");
  Serial.print(WiFi.localIP());
  Serial.println(" in a browser.");
}

void loop() {
  server.handleClient();
}
