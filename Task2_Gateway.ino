/*
 * Task 2 - Gateway
 * Hosts an HTTP server to receive ultrasonic distance measurements
 * from the sensor node, validate the JSON data, and store the latest
 * valid reading along with its reception time.
 */

#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>

const char* apSSID = "";
const char* apPassword = "";
const char* staSSID = "";
const char* staPassword = "";

WebServer server(80);

float latestDistance = -1;
unsigned long receivedAt = 0;
String latestNode = "";

// Receive and validate sensor measurements through HTTP POST
void handleSensor() {
  JsonDocument doc;

  // Reject malformed JSON requests
  if (deserializeJson(doc, server.arg("plain")) || !doc.is<JsonObject>()) {
    server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }

  JsonVariantConst value = doc["distance"];

  // Ensure the distance is a numeric value
  if (!value.is<float>() && !value.is<int>()) {
    server.send(400, "application/json", "{\"error\":\"Invalid distance\"}");
    return;
  }

  float d = value.as<float>();

  // Reject measurements outside the sensor's valid range
  if (!isfinite(d) || d < 2 || d > 400) {
    server.send(400, "application/json", "{\"error\":\"Distance out of range\"}");
    return;
  }

  // Store the latest valid measurement and reception time
  latestDistance = d;
  receivedAt = millis();
  latestNode = doc["node"].as<String>();

  Serial.print("Received distance: ");
  Serial.print(latestDistance);
  Serial.println(" cm");

  Serial.print("Node: ");
  Serial.println(latestNode);

  Serial.print("Received at millis: ");
  Serial.println(receivedAt);

  // Acknowledge successful reception
  server.send(200, "application/json", "{\"status\":\"ok\"}");
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Start the gateway in AP+STA mode
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(apSSID, apPassword, 1, 0, 2);
  WiFi.begin(staSSID, staPassword);

  // Register the sensor data endpoint and start the HTTP server
  server.on("/api/sensor", HTTP_POST, handleSensor);
  server.begin();

  Serial.print("Gateway AP IP: ");
  Serial.println(WiFi.softAPIP());
}

void loop() {
  // Process incoming HTTP requests from the sensor node
  server.handleClient();
}
