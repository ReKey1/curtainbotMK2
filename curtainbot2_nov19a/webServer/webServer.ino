#include <WiFi.h>
#include <WebServer.h>
#include <arduino_secrets.h>

const char* ssid = SECRET_SSID;
const char* password = SECRET_OPTIONAL_PASS;

WebServer server(80);

void handleOn() {
  digitalWrite(2, HIGH);
  server.send(200, "text/plain", "LED ON");
}

void handleOff() {
  digitalWrite(2, LOW);
  server.send(200, "text/plain", "LED OFF");
}

void setup() {
  pinMode(2, OUTPUT);
  WiFi.begin(ssid, password);
  Serial.begin(115200);

  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
  }

  server.on("/on", handleOn);
  server.on("/off", handleOff);
  server.begin();

  Serial.println(WiFi.localIP());
}

void loop() {
  server.handleClient();
}
