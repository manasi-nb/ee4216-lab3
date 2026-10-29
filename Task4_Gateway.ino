/*
 * Task 4 - Gateway
 * Uploads ultrasonic distance measurements to ThingSpeak and polls
 * for remote LED-control commands. Valid commands are forwarded to
 * the sensor node, and acknowledgments are used to prevent repeated
 * execution of successfully completed commands.
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

const char* sensorWriteKey = "";
const char* ledChannelID = "";
const char* ledReadKey = "";

WebServer server(80);

float latestDistance = -1;
unsigned long receivedAt = 0;
bool hasReading = false;

IPAddress sensorIP;
bool sensorKnown = false;

long lastCompletedEntry = 0;

unsigned long lastUpload = 0;
unsigned long lastPoll = 0;
unsigned long lastReconnect = 0;

// Receive sensor measurements and identify the sensor node's IP
void handleSensor() {
  JsonDocument doc;

  if (deserializeJson(doc, server.arg("plain")) || !doc.is<JsonObject>()) {
    server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }

  JsonVariantConst value = doc["distance"];

  // Validate the distance before storing it
  if ((!value.is<float>() && !value.is<int>()) || !isfinite(value.as<float>()) || value.as<float>() < 2 || value.as<float>() > 400) {
    server.send(400, "application/json", "{\"error\":\"Invalid distance\"}");
    return;
  }

  latestDistance = value.as<float>();
  receivedAt = millis();
  hasReading = true;

  // Obtain the sensor node IP from the incoming HTTP request
  sensorIP = server.client().remoteIP();
  sensorKnown = true;

  Serial.printf("Received: %.2f cm from %s\n", latestDistance, sensorIP.toString().c_str());

  server.send(200, "application/json", "{\"status\":\"ok\"}");
}

// Upload the latest valid measurement to ThingSpeak
void uploadDistance() {
  if (WiFi.status() != WL_CONNECTED || !hasReading) return;

  // Skip readings older than 15 seconds
  if (millis() - receivedAt > 15000) return;

  WiFiClientSecure client;

  // Disable certificate verification for initial lab testing
  client.setInsecure();

  HTTPClient http;
  http.setTimeout(5000);

  if (!http.begin(client, "https://api.thingspeak.com/update"))
    return;

  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  String payload = "api_key=" + String(sensorWriteKey) + "&field1=" + String(latestDistance, 2);

  // Send the measurement to the Sensor Data channel
  int status = http.POST(payload);
  String response = http.getString();
  response.trim();

  Serial.printf("ThingSpeak upload: HTTP %d | Entry: %s\n", status, response.c_str());

  http.end();
}

// Forward an LED command and verify the sensor node acknowledgment
bool forwardLED(int id, int r, int g, int b) {
  if (!sensorKnown) {
    Serial.println("Sensor IP not known yet");
    return false;
  }

  String url = "http://" + sensorIP.toString() + "/api/led";

  HTTPClient http;
  http.setTimeout(3000);

  if (!http.begin(url)) return false;

  http.addHeader("Content-Type", "application/json");

  // Prepare the LED index and RGB values as JSON
  JsonDocument doc;
  doc["index"] = id;
  doc["r"] = r;
  doc["g"] = g;
  doc["b"] = b;

  String payload;
  serializeJson(doc, payload);

  // Forward the command to the sensor node
  int status = http.POST(payload);
  String response = http.getString();

  http.end();

  if (status != 200) {
    Serial.printf("LED forwarding failed: %d\n", status);
    return false;
  }

  // Verify that the sensor node executed the correct command
  JsonDocument ack;

  if (deserializeJson(ack, response) || ack["status"] != "ok" || !ack["index"].is<int>() || ack["index"].as<int>() != id) {
    Serial.println("Invalid acknowledgment");
    return false;
  }

  return true;
}

// Retrieve the latest LED-control command from ThingSpeak
void pollLED() {
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClientSecure client;
  client.setInsecure();

  String url = "https://api.thingspeak.com/channels/" + String(ledChannelID) + "/feeds/last.json?api_key=" + String(ledReadKey);

  HTTPClient http;
  http.setTimeout(5000);

  if (!http.begin(client, url)) return;

  // Request the latest command using HTTPS GET
  int status = http.GET();
  String response = http.getString();
  http.end();

  if (status != 200) {
    Serial.printf("LED poll failed: %d\n", status);
    return;
  }

  JsonDocument doc;

  if (deserializeJson(doc, response) || !doc.is<JsonObject>()) {
    Serial.println("Invalid cloud JSON");
    return;
  }

  long entry = doc["entry_id"] | 0L;
  String indexText = doc["field1"].as<String>();
  String colorText = doc["field2"].as<String>();

  // Validate the command ID, LED index and colour code
  if (entry <= 0 || indexText.length() != 1 || colorText.length() != 1 || indexText[0] < '0' || indexText[0] > '7' || colorText[0] < '0' || colorText[0] > '3') {
    Serial.println("Invalid cloud command");
    return;
  }

  // Ignore commands that have already been completed
  if (entry <= lastCompletedEntry) return;

  int id = indexText.toInt();
  int color = colorText.toInt();

  // Convert colour code into RGB values
  int r = 0, g = 0, b = 0;

  if (color == 1) r = 255;
  else if (color == 2) g = 255;
  else if (color == 3) b = 255;

  Serial.printf("Command: entry=%ld LED=%d color=%d\n", entry, id, color);

  // Mark completed only after a successful acknowledgment
  if (forwardLED(id, r, g, b)) {
    lastCompletedEntry = entry;
    Serial.println("LED updated successfully");
  } else {
    Serial.println("Command pending, will retry");
  }
}

void setup() {
  Serial.begin(115200);

  // Enable local AP and upstream STA connectivity
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(apSSID, apPassword, 1, 0, 2);
  WiFi.begin(staSSID, staPassword);
  WiFi.setAutoReconnect(true);

  // Register the local sensor endpoint
  server.on("/api/sensor", HTTP_POST, handleSensor);
  server.begin();

  Serial.print("Gateway AP IP: ");
  Serial.println(WiFi.softAPIP());
}

void loop() {
  // Process incoming sensor measurements
  server.handleClient();

  // Reconnect to the upstream network when necessary
  if (WiFi.status() != WL_CONNECTED && millis() - lastReconnect >= 5000) {
    lastReconnect = millis();
    WiFi.reconnect();
  }

  // Display the gateway STA IP when connected
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

  // Upload sensor data every 20 seconds
  if (millis() - lastUpload >= 20000) {
    lastUpload = millis();
    uploadDistance();
  }

  // Check ThingSpeak for LED commands every 5 seconds
  if (millis() - lastPoll >= 5000) {
    lastPoll = millis();
    pollLED();
  }
}
