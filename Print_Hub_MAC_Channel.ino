/*
  Print WiFi MAC + Channel
  Flash this to the HUB board, connect to your WiFi network, and read
  the two values off the serial monitor. Copy them into the outside
  node's hubMAC[] and HUB_WIFI_CHANNEL.
*/

#include <WiFi.h>

const char* ssid     = "ssid";
const char* password = "password";

void setup() {
  Serial.begin(115200);
  delay(1000);

  WiFi.mode(WIFI_MODE_STA);
  WiFi.begin(ssid, password);

  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }
  Serial.println("\nConnected.");

  String mac = WiFi.macAddress();  // format: "AA:BB:CC:DD:EE:FF"
  int ch = WiFi.channel();

  Serial.println("\n=== Copy these into the outside node sketch ===");
  Serial.println("MAC address: " + mac);
  Serial.printf("WiFi channel: %d\n", ch);

  // Pre-formatted as a C array, ready to paste directly into
  // uint8_t hubMAC[] = { ... };
  Serial.print("\nAs a C array:\nuint8_t hubMAC[] = { ");
  for (int i = 0; i < 6; i++) {
    String byteStr = mac.substring(i * 3, i * 3 + 2);
    Serial.print("0x");
    Serial.print(byteStr);
    if (i < 5) Serial.print(", ");
  }
  Serial.println(" };");
  Serial.printf("#define HUB_WIFI_CHANNEL %d\n", ch);
}

void loop() {
  // nothing -- one-shot report on boot
}
