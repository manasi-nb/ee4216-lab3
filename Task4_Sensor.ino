/*
 * Task 4 - Sensor Node
 * Sends ultrasonic distance measurements to the gateway every
 * 5 seconds and receives HTTP commands to control individual
 * RGB LEDs, returning an acknowledgment after each valid update.
 */

#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>
#include <math.h>

const char* ssid = "";
const char* password = "";

const char* gatewayURL = "http://192.168.4.1/api/sensor";

#define TRIG_PIN 5
#define ECHO_PIN 18
#define LED_PIN 4
#define NUM_LEDS 8

WebServer server(80);
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

unsigned long lastSend = 0;
unsigned long lastReconnect = 0;

// Measure distance using the HC-SR04 ultrasonic sensor
float measureDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // Measure echo duration with a 30 ms timeout
  unsigned long duration = pulseIn(ECHO_PIN, HIGH, 30000UL);

  if (duration == 0) return -1;

  // Convert round-trip echo time to distance in centimetres
  return duration * 0.0343f / 2.0f;
}

// Check whether an RGB value is an integer between 0 and 255
bool validByte(JsonVariantConst value) {
  return value.is<int>() && value.as<int>() >= 0 && value.as<int>() <= 255;
}

// Receive and execute LED-control commands from the gateway
void handleLED() {
  JsonDocument doc;

  // Reject malformed JSON requests
  if (deserializeJson(doc, server.arg("plain")) || !doc.is<JsonObject>()) {
    server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }

  // Validate the LED index and RGB values
  if (!doc["index"].is<int>() || doc["index"].as<int>() < 0 || doc["index"].as<int>() >= NUM_LEDS || !validByte(doc["r"]) || !validByte(doc["g"]) || !validByte(doc["b"])) {
    server.send(400, "application/json", "{\"error\":\"Invalid LED values\"}");
    return;
  }

  int id = doc["index"].as<int>();
  int r = doc["r"].as<int>();
  int g = doc["g"].as<int>();
  int b = doc["b"].as<int>();

  // Update only the selected LED
  strip.setPixelColor(id, strip.Color(r, g, b));
  strip.show();

  Serial.printf("LED %d: R=%d G=%d B=%d\n", id, r, g, b);

  // Acknowledge successful LED actuation
  JsonDocument reply;
  reply["status"] = "ok";
  reply["index"] = id;

  String response;
  serializeJson(reply, response);
  server.send(200, "application/json", response);
}

// Measure and send valid distance readings to the gateway
void sendDistance() {
  if (WiFi.status() != WL_CONNECTED) return;

  float distance = measureDistance();

  // Skip invalid or out-of-range measurements
  if (!isfinite(distance) || distance < 2 || distance > 400) {
    Serial.println("Invalid distance");
    return;
  }

  // Prepare the sensor measurement as JSON
  JsonDocument doc;
  doc["node"] = "sensor01";
  doc["distance"] = distance;

  String payload;
  serializeJson(doc, payload);

  HTTPClient http;
  http.setTimeout(3000);

  if (!http.begin(gatewayURL)) return;

  http.addHeader("Content-Type", "application/json");

  // Send the measurement through HTTP POST
  int status = http.POST(payload);

  Serial.printf("Distance: %.2f cm | HTTP: %d\n", distance, status);

  http.end();
}

void setup() {
  Serial.begin(115200);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  digitalWrite(TRIG_PIN, LOW);

  // Initialize the eight-LED NeoPixel bar
  strip.begin();
  strip.clear();
  strip.show();

  // Connect the sensor node to the gateway AP
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  // Register the LED-control endpoint and start the HTTP server
  server.on("/api/led", HTTP_POST, handleLED);
  server.begin();

  Serial.println("Sensor node started");
}

void loop() {
  // Process incoming LED-control requests
  server.handleClient();

  // Attempt Wi-Fi reconnection when disconnected
  if (WiFi.status() != WL_CONNECTED && millis() - lastReconnect >= 5000) {
    lastReconnect = millis();
    WiFi.reconnect();
  }

  // Send ultrasonic measurements every 5 seconds
  if (millis() - lastSend >= 5000) {
    lastSend = millis();
    sendDistance();
  }
}
