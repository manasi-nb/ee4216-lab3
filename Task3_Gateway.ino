/*
 * Task 3 - Gateway
 * Receives and validates ultrasonic distance readings from the sensor
 * node and uploads the latest valid measurement to ThingSpeak
 * approximately every 20 seconds using HTTPS POST.
 */

#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <math.h>

const char* apSSID = "";
const char* apPassword = "";

const char* staSSID = "";
const char* staPassword = "";

const char* writeAPIKey = "";

WebServer server(80);

float latestDistance = -1;
bool hasReading = false;
unsigned long receivedAt = 0;
unsigned long lastUpload = 0;

// Receive and validate distance measurements from the sensor node
void handleSensor() {
  JsonDocument doc;

  if (deserializeJson(doc, server.arg("plain")) || !doc.is<JsonObject>()) {
    server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }

  JsonVariantConst value = doc["distance"];

  if (!value.is<float>() && !value.is<int>()) {
    server.send(400, "application/json", "{\"error\":\"Invalid distance\"}");
    return;
  }

  float distance = value.as<float>();

  // Accept only finite distances within the sensor's valid range
  if (!isfinite(distance) || distance < 2 || distance > 400) {
    server.send(400, "application/json", "{\"error\":\"Distance out of range\"}");
    return;
  }

  // Store the latest valid reading and its reception time
  latestDistance = distance;
  receivedAt = millis();
  hasReading = true;

  Serial.print("Received distance: ");
  Serial.print(latestDistance, 2);
  Serial.println(" cm");

  server.send(200, "application/json", "{\"status\":\"ok\"}");
}

// Upload the latest valid distance measurement to ThingSpeak
void uploadToThingSpeak() {
  if (WiFi.status() != WL_CONNECTED || !hasReading) return;

  // Do not upload stale measurements
  if (millis() - receivedAt > 15000) return;

  WiFiClientSecure client;

  // Disable certificate verification for initial lab testing
  client.setInsecure();

  HTTPClient http;
  http.setTimeout(5000);

  if (!http.begin(client, "https://api.thingspeak.com/update")) {
    Serial.println("HTTPS connection failed");
    return;
  }

  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  // Prepare ThingSpeak API key and distance field
  String payload = "api_key=" + String(writeAPIKey) + "&field1=" + String(latestDistance, 2);

  // Upload the measurement using HTTPS POST
  int status = http.POST(payload);
  String response = http.getString();
  response.trim();

  Serial.print("ThingSpeak HTTP: ");
  Serial.println(status);

  Serial.print("ThingSpeak response: ");
  Serial.println(response);

  // A positive entry ID confirms a successful upload
  if (status == 200 && response.toInt() > 0) {
    Serial.println("Upload successful");
  } else {
    Serial.println("Upload failed");
  }

  http.end();
}

void setup() {
  Serial.begin(115200);

  // Configure the gateway as both AP and STA
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(apSSID, apPassword, 1, 0, 2);
  WiFi.begin(staSSID, staPassword);
  WiFi.setAutoReconnect(true);

  // Start the HTTP server for incoming sensor data
  server.on("/api/sensor", HTTP_POST, handleSensor);
  server.begin();

  Serial.print("Gateway AP IP: ");
  Serial.println(WiFi.softAPIP());
}

void loop() {
  // Handle local sensor requests
  server.handleClient();

  // Attempt upstream Wi-Fi reconnection when disconnected
  static unsigned long lastReconnect = 0;

  if (WiFi.status() != WL_CONNECTED &&
      millis() - lastReconnect >= 5000) {
    lastReconnect = millis();
    WiFi.reconnect();
  }

  // Display the gateway STA IP after connecting
  static bool printedIP = false;

  if (WiFi.status() == WL_CONNECTED) {
    if (!printedIP) {
      Serial.print("Gateway STA IP: ");
      Serial.println(WiFi.localIP());
      printedIP = true;
    }
  } else {
    printedIP = false;
  }

  // Upload the latest valid measurement every 20 seconds
  if (millis() - lastUpload >= 20000) {
    lastUpload = millis();
    uploadToThingSpeak();
  }
}
