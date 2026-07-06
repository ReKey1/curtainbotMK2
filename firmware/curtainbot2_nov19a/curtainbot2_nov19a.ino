/*
  CurtainBot MARK2 — local web server edition

  ESP32 curtain controller: two L298N-driven motors with encoder stall
  detection (control.h), controlled by physical buttons and a barebones
  web UI served from the ESP32 itself (webui.h). Time comes from NTP
  (scheduler.h); morning/night schedules open/close both curtains.

  File map:
    config.h      — every tunable (WiFi timeout, NTP/timezone, motor tuning, schedule defaults)
    control.h     — Motor class (self-tuning stall detection, position tracking,
                    go-to-position), buttons, LEDs, controlSetup()/controlLoop()
    calibration.h — persists learned travel span / direction / endpoint to flash
    scheduler.h   — NTP sync + two-stage morning/night routines (half at T-5 min,
                    fully open/closed at the set time)
    commands.h    — command registry; add a row to COMMANDS[] to add an action
    webui.h       — HTML page + GET /status + POST /command

  Setup: copy arduino_secrets.h.template to arduino_secrets.h and fill in WiFi
  credentials. Timezone (JST) and all tuning live in config.h.
*/
#include <WiFi.h>
#include "arduino_secrets.h"
#include "config.h"
#include "control.h"
#include "calibration.h"
#include "scheduler.h"
#include "commands.h"
#include "webui.h"

const int LED_PIN = 2;  // onboard LED: WiFi connection indicator

void wifiConnect() {
  Serial.print("Connecting to WiFi");
  WiFi.mode(WIFI_STA);
  WiFi.begin(SECRET_SSID, SECRET_OPTIONAL_PASS);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_TIMEOUT_MS) {
    delay(300);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Connected. Open http://");
    Serial.print(WiFi.localIP());
    Serial.println(" in a browser.");
  } else {
    Serial.println("WiFi timed out — buttons still work; radio keeps retrying in background.");
  }
}

void setup() {
  controlSetup();       // Serial @115200, motors, encoders, buttons, LEDs
  calibrationBegin();   // restore learned travel span / position from flash
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  wifiConnect();
  schedulerBegin();  // start NTP sync
  webBegin();        // register routes, start HTTP server
}

void loop() {
  controlLoop();            // motor ramp/stall updates + physical buttons
  server.handleClient();    // web UI / API
  schedulerUpdate();        // routine sweeps + fire morning/night when due
  calibrationUpdate();      // persist calibration when it changes
  digitalWrite(LED_PIN, WiFi.status() == WL_CONNECTED ? HIGH : LOW);
}
