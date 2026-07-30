/*
  EoRa Pi -- Outside BME280 Sensor Node (Optimized for Core 3.3.10)
  ESP32-NOW, ESP32 Core 3.3.10 / ESP32-S3
  July 30, 2026
*/

#define EoRa_PI_V1
#include "boards.h"     // Ebyte pin mappings and power rails
#include "utilities.h"  // Board helper functions
#include <RadioLib.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <ESP32_NOW.h>
#include <Wire.h>
#include <BME280I2C.h>
#include <SPI.h>
#include <driver/rtc_io.h>

// Configuration using OEM definitions from boards.h / utilities.h
#define WAKE_PIN GPIO_NUM_15  // Physical wire jump from DIO1 to RTC_GPIO15

#define USING_SX1262_868M
#if defined(USING_SX1262_868M)
uint8_t txPower = 2;
float radioFreq = 915.0;
SX1262 radio = new Module(RADIO_CS_PIN, RADIO_DIO1_PIN, RADIO_RST_PIN, RADIO_BUSY_PIN);
#endif

#define BME_SDA 48
#define BME_SCL 47

BME280I2C bme;

const float BME280_OUTSIDE_TEMP_CAL_OFFSET_F = +5.54;
uint8_t hubMAC[] = { 0x1C, 0xDB, 0xD4, 0x85, 0x6E, 0x9C };

#define HUB_WIFI_CHANNEL 11

// ESP-NOW Data Types
enum MessageType : uint8_t { MSG_BME280 = 0 };
struct __attribute__((packed)) BME280Data {
  MessageType type;
  float temperature;
  float humidity;
  float pressure;
};

// Peer class definition with exposed remove_from_system()
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

void goToSleep(void) {
  Serial.println(F("=== PREPARING FOR DEEP SLEEP ==="));
  Serial.flush();

  // 1. Arm Radio for Duty Cycle RX (WOR)
  radio.startReceiveDutyCycleAuto();
  delay(20);

  // 2. Tear down hardware buses to release driver_ng instances
  Wire.end();
  SPI.end();

  // 3. Power-gate peripherals if configured in boards.h
  #ifdef BOARD_PERIPH_OFF
    BOARD_PERIPH_OFF(); 
  #endif

  // 4. Pin isolation & high-impedance mode for non-RTC GPIOs
  pinMode(RADIO_BUSY_PIN, INPUT);
  pinMode(BME_SDA, INPUT);
  pinMode(BME_SCL, INPUT);
  pinMode(BOARD_LED, INPUT);

  // 5. Setup Wakeup Pin (EXT0)
  rtc_gpio_pulldown_en(WAKE_PIN);
  esp_sleep_enable_ext0_wakeup(WAKE_PIN, 1);

  // 6. Enter Deep Sleep
  esp_deep_sleep_start();
}

void setupLoRa() {
  SPI.begin(RADIO_SCLK_PIN, RADIO_MISO_PIN, RADIO_MOSI_PIN, RADIO_CS_PIN);

  // Note the last argument explicitly set to 'false' (useRegulatorLDO = false)
  int state = radio.begin(
    radioFreq, 500.0, 7, 7,
    RADIOLIB_SX126X_SYNC_WORD_PRIVATE,
    2, 512, 0.0, false); // <--- 'false' enables internal DC-DC converter

  if (state == RADIOLIB_ERR_NONE) {
    radio.setRegulatorDCDC(); // Explicitly enforce DC-DC buck mode
    Serial.println(F("[SX126x] Initialized with DC-DC Regulator!"));
  }
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

  // Local peer instantiation inside valid function scope
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

  // Clean, explicit driver teardown
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
    Serial.println(F("Core 3.3.10 failed to allocate I2C peripheral instance!"));
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
  Serial.begin(115200);
  delay(1500); 

  Serial.println(F("*** REACHED SETUP ***"));
  Serial.flush();

  setCpuFrequencyMhz(80);

  SPI.begin(RADIO_SCLK_PIN, RADIO_MISO_PIN, RADIO_MOSI_PIN, RADIO_CS_PIN);

  setupLoRa();
  delay(50);

  radio.standby();
  delay(20);
  radio.setPreambleLength(4096); // Scale preamble window up to ~1.05 seconds 
  delay(10);

  int state = radio.startReceiveDutyCycleAuto(); 

  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("[LoRa] WOR armed successfully using 512 symbols!"));
  } else {
    Serial.printf("[LoRa] WOR critical arming failure: code %d\n", state);
    Serial.flush();
    esp_deep_sleep_start();
  }

  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

#ifdef BENCH_TEST_FORCE_WAKE
  Serial.println(F("*** BENCH TEST MODE -- forcing wake/read/send path ***"));
  goToSleep();
  return;
#endif

  if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {
    Serial.println(F("Woke from hub WOR -- reading and sending BME280 data"));
    readAndSendBME280();
    goToSleep();
  }
  
  Serial.println(F("Power-on reset -- arming duty cycle and going to sleep"));
  goToSleep();
}

void loop() {
  // System operates purely on reset execution via EXT0 wakeup.
}
