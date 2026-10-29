/*
 * Task 1 - Gateway
 * Configures the ESP32-S3 gateway in AP+STA mode to create a local
 * Wi-Fi network while connecting to an external router.
 * Displays the gateway IP addresses and connected client count.
 */

#include <WiFi.h>

const char* apSSID = "";
const char* apPassword = "";

const char* staSSID = "";
const char* staPassword = "";

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Enable Access Point and Station modes simultaneously
  WiFi.mode(WIFI_AP_STA);

  // Start the gateway AP with a maximum of two clients
  bool apOK = WiFi.softAP(apSSID, apPassword, 1, 0, 2);

  Serial.print("AP started: ");
  Serial.println(apOK ? "YES" : "NO");

  Serial.print("Gateway AP IP: ");
  Serial.println(WiFi.softAPIP());

  // Connect the gateway to an external Wi-Fi network
  WiFi.begin(staSSID, staPassword);

  unsigned long start = millis();

  // Wait up to 15 seconds for the STA connection
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Upstream Wi-Fi connected");

    Serial.print("Gateway STA IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("Upstream connection failed");
  }
}

void loop() {
  // Display the number of clients connected to the gateway AP
  Serial.print("Sensor/client connections: ");
  Serial.println(WiFi.softAPgetStationNum());
  delay(3000);
}
