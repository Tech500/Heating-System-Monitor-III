/*
  EoRa Pi -- Outside BME280 Sensor Node (Optimized for Core 3.3.10)
  ESP32-NOW, ESP32 Core 3.3.10 / ESP32-S3
  Changed from Autoduty Cycle to Channel Activity Detection (CAD)
  for lower current draw average.
  [CAD Nordic Power Profiler Kit II Observations[(https://gist.github.com/Tech500/26b9f16bd595f98c8c41618e758c92f0)
  July 30, 2026
*/

/*
      SX1262 with Channel Activity Detection-Wake-on-Radio-Deep Sleep
      CAD_WOR_Deep_Sleep.ino
      Updated for ESP32 Arduino Core v3.x
*/

#define EoRa_PI_V1
#include <Arduino.h>
#include <WiFi.h>
#include <ESP32_NOW.h>
#include <esp_wifi.h>
#include <Wire.h>
#include <RadioLib.h>
#include <boards.h>
#include <BME280I2C.h>
#include <SPI.h>
#include "driver/rtc_io.h"

// --- Hardware & Network Definitions ---
#define WAKEUP_PIN GPIO_NUM_16
#define HUB_WIFI_CHANNEL 11
#define BME_SDA 48
#define BME_SCL 47

const float BME280_OUTSIDE_TEMP_CAL_OFFSET_F = +5.54;
uint8_t hubMAC[] = { 0x1C, 0xDB, 0xD4, 0x85, 0x6E, 0x9C };

const float radioFreq = 915.0;     // MHz
const float bandWidth = 125.0;     // kHz
const uint8_t spreadingFactor = 7;
const uint8_t codingRate = 7;
const uint8_t syncWord = RADIOLIB_SX126X_SYNC_WORD_PRIVATE;

SX1262 radio = new Module(RADIO_CS_PIN, RADIO_DIO1_PIN, RADIO_RST_PIN, RADIO_BUSY_PIN);
BME280I2C bme;

// --- Message / Packet Structures ---
enum MessageType : uint8_t {
  MSG_BME280       = 0,
  MSG_ALERT_FLAG   = 1,
  MSG_BLOWER_STATE = 2
};

struct __attribute__((packed)) BME280Data {
  MessageType type;
  float temperature;
  float humidity;
  float pressure;
};

struct __attribute__((packed)) BlowerData {
  MessageType type;
  bool         on;
  float        elapsedMinutes;
  float        dailyTotalMinutes;
};

struct __attribute__((packed)) AlertFlag {
  MessageType type;
  bool alert;
};

// --- ESP32 Core v3 ESP-NOW Peer Class ---
class HubPeer : public ESP_NOW_Peer {
public:
  HubPeer(const uint8_t *mac_addr, uint8_t channel)
    : ESP_NOW_Peer(mac_addr, channel, WIFI_IF_STA, NULL) {}
    
  bool add_to_system() {
    return ESP_NOW_Peer::add();
  }
  
  bool remove_from_system() {
    return ESP_NOW_Peer::remove();
  }

  bool sendData(const uint8_t *data, size_t len) {
    return send(data, len);
  }
};

void setupLoRa() {
  SPI.begin(RADIO_SCLK_PIN, RADIO_MISO_PIN, RADIO_MOSI_PIN, RADIO_CS_PIN);

  radio.standby();
  delay(50);

  radio.resetOnStartup = true;
  delay(50);
  radio.tcxoVoltage = 1.6; // Crucial 1.6V reference for Ebyte modules

  int state = radio.begin(
    radioFreq,
    125.0,
    7,
    7,
    RADIOLIB_SX126X_SYNC_WORD_PRIVATE,
    2,
    5000, // 5000-symbol preamble
    1.6,
    false
  );

  if (state == RADIOLIB_ERR_NONE) {
    radio.setRegulatorDCDC();
    radio.setPreambleLength(5000); 
    Serial.println(F("[SX1262] Battery Receiver Initialized with 5000-symbol Preamble."));
  } else {
    Serial.printf("[SX1262] Initialization failed, code %d\n", state);
  }
}

bool verifySignalWithBusyLoop(int totalPasses = 10, int requiredHits = 3) {
  Serial.println(F("\n--- [WAKE DIAGNOSTIC & MULTI-SCAN START] ---"));
  
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  Serial.printf("  Wakeup Cause Code: %d ", wakeup_reason);
  if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {
    Serial.println(F("(ESP_SLEEP_WAKEUP_EXT0 - DIO1 High)"));
  } else {
    Serial.println(F("(Other Wakeup Reason / Cold Boot)"));
  }

  int dio1State = digitalRead(WAKEUP_PIN);
  int busyState = digitalRead(RADIO_BUSY_PIN);
  Serial.printf("  Pin Levels at Boot -> DIO1 (GPIO %d): %d | BUSY (GPIO %d): %d\n", 
                RADIO_DIO1_PIN, dio1State, RADIO_BUSY_PIN, busyState);

  uint16_t irqFlags = radio.getIrqFlags();
  Serial.printf("  SX1262 Internal IRQ Register: 0x%04X\n", irqFlags);
  if (irqFlags & RADIOLIB_SX126X_IRQ_CAD_DETECTED) {
    Serial.println(F("  [IRQ CHECK] -> RADIOLIB_SX126X_IRQ_CAD_DETECTED is SET!"));
  }
  if (irqFlags & RADIOLIB_SX126X_IRQ_HEADER_VALID) {
    Serial.println(F("  [IRQ CHECK] -> RADIOLIB_SX126X_IRQ_HEADER_VALID is SET!"));
  }

  if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {
    Serial.println("Woke from hub WOR -- reading and sending BME280 data");
    digitalWrite(BOARD_LED, LED_ON);
  }  

  return false;
}

void enterLowPowerWOR() {

  radio.clearIrqFlags(RADIOLIB_SX126X_IRQ_ALL);

  int state = radio.startReceiveDutyCycleAuto(
      5000,
      0,
      RADIOLIB_SX126X_IRQ_PREAMBLE_DETECTED |
      RADIOLIB_SX126X_IRQ_HEADER_VALID |
      RADIOLIB_SX126X_IRQ_RX_DONE,

      RADIOLIB_SX126X_IRQ_PREAMBLE_DETECTED |
      RADIOLIB_SX126X_IRQ_HEADER_VALID |
      RADIOLIB_SX126X_IRQ_RX_DONE
  );

  if (state != RADIOLIB_ERR_NONE) {
    Serial.printf("[SX1262] DutyCycleAuto failed, code: %d\n", state);
    return;
  }

  pinMode(WAKEUP_PIN, INPUT);
  esp_sleep_enable_ext0_wakeup(WAKEUP_PIN, 1);

  Serial.printf("DIO1 GPIO %d before sleep = %d\n", WAKEUP_PIN, digitalRead(WAKEUP_PIN));
  Serial.println(F("=== SX1262 WOR armed - ESP32-S3 entering deep sleep ==="));
  Serial.flush();

  esp_deep_sleep_start();
}

bool sendTelemetryViaESPNOW(float tempF, float humidity, float pressureHPa) {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  esp_wifi_set_channel(HUB_WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);

  if (!ESP_NOW.begin()) {
    Serial.println(F("ESP-NOW init failed"));
    WiFi.mode(WIFI_OFF);
    return false;
  }

  HubPeer localHub(hubMAC, HUB_WIFI_CHANNEL);
  if (!localHub.add_to_system()) {
    Serial.println(F("Failed to bind hub peer"));
    ESP_NOW.end();
    WiFi.mode(WIFI_OFF);
    return false;
  }

  BME280Data pkt;
  pkt.type = MSG_BME280;
  pkt.temperature = tempF;
  pkt.humidity = humidity;
  pkt.pressure = pressureHPa;

  bool sent = localHub.sendData((uint8_t *)&pkt, sizeof(BME280Data));
  Serial.printf("[ESP-NOW] Send to hub: %s\n", sent ? "OK" : "FAILED");

  localHub.remove_from_system();
  ESP_NOW.end();
  WiFi.mode(WIFI_OFF);
  esp_wifi_stop();

  return sent;
}

bool readAndSendBME280() {
  Wire.end();
  delay(50);
  Wire.setPins(BME_SDA, BME_SCL);
  if (!Wire.begin(BME_SDA, BME_SCL)) {
    Serial.println(F("Failed to allocate I2C peripheral instance!"));
  }
  delay(50);

  if (!bme.begin()) {
    Serial.println(F("BME280 not found -- check wiring/address"));
    return false;
  }

  float tempF = NAN, humidity = NAN, pressureHPa = NAN;
  BME280::TempUnit tempUnit(BME280::TempUnit_Fahrenheit);
  BME280::PresUnit presUnit(BME280::PresUnit_hPa);

  delay(500);

  bme.read(pressureHPa, tempF, humidity, tempUnit, presUnit);
  tempF += BME280_OUTSIDE_TEMP_CAL_OFFSET_F;

  if (isnan(tempF) || isnan(pressureHPa)) {
    Serial.println(F("Error reading BME280 telemetry."));
    return false;
  }

  Serial.printf("BME280 -> Temp: %.2f F  Hum: %.2f %%  Pres: %.4f hPa\n",
                tempF, humidity, pressureHPa);

  return sendTelemetryViaESPNOW(tempF, humidity, pressureHPa);
}

void setup() {
  initBoard();
  delay(1500);

  Serial.begin(115200);
  delay(1500);

  Serial.println(F("\n=============================================="));
  Serial.println(F("--- ESP32-S3 WOR NODE ---"));

  setupLoRa();
  Serial.println(F("Executing mandatory boot-up radio diagnostic..."));
  verifySignalWithBusyLoop(10, 3);

  readAndSendBME280();
  delay(50);

  enterLowPowerWOR();
}

void loop() {
  // Unused
}
