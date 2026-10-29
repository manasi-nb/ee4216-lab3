/*
 * Task 1 - Sensor Node
 * Connects the ESP32-S3 sensor node to the gateway's Wi-Fi network
 * and displays the assigned sensor IP and gateway IP addresses.
 */

#include <WiFi.h>

const char* ssid = "";
const char* password = "";

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Configure the sensor node as a Wi-Fi station
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  unsigned long start = millis();

  // Wait up to 15 seconds for the gateway connection
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Connected to Gateway AP");

    // Display the IP address assigned to the sensor node
    Serial.print("Sensor Node IP: ");
    Serial.println(WiFi.localIP());

    Serial.print("Gateway IP: ");
    Serial.println(WiFi.gatewayIP());
  } else {
    Serial.println("Connection failed");
  }
}

void loop() {
  delay(1000);
}
