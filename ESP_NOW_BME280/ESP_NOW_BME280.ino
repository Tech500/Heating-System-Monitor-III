/*
  EoRa Pi Foundation --Outside BME280 Sensor Node
  Wakes on incoming LoRa WOR from hub -> reads BME280 -> sends reading to
  hub via ESP-NOW -> sleeps.
  July 26, 2026

 */

/*
  EoRa Pi Foundation -- Outside BME280 Sensor Node
  Wakes on incoming LoRa WOR from hub -> reads BME280 -> sends reading to
  hub via ESP-NOW -> sleeps.

  New direction for project; now using CAD detection of LoRa preamble to awaken deep sleeping BME280
  ESP-NOW node to send BME280 readings back to Receiver node.  Advantage of CAD is a much lower current
  requirement than radio.startAutoDutyCycle().
*/

#define EoRa_PI_V1
#include <RadioLib.h>
#include "boards.h"
#include <WiFi.h>
#include <esp_wifi.h>
#include <ESP32_NOW.h>
#include <Wire.h>
#include <BME280I2C.h>  
#include <SPI.h>
#include <FS.h>
#include <LittleFS.h>
#include <rom/rtc.h>
#include <driver/rtc_io.h>

// Global Variables & Pin Configurations
int symbols = 512;
#define WAKE_PIN GPIO_NUM_16  // Direct insulated jumper from GPIO33 (DIO1)

#define USING_SX1262_868M
#if defined(USING_SX1262_868M)
uint8_t txPower = 2;
float radioFreq = 915.0;
SX1262 radio = new Module(RADIO_CS_PIN, RADIO_DIO1_PIN, RADIO_RST_PIN, RADIO_BUSY_PIN);
#endif

#define BME_SDA 35
#define BME_SCL 36

BME280I2C bme;  // default constructor uses address 0x76

// ESP-NOW Setup
enum MessageType : uint8_t {
  MSG_BME280 = 0,
  MSG_ALERT_FLAG = 1,
  MSG_BLOWER_STATE = 2
};

struct __attribute__((packed)) BME280Data {
  MessageType type;
  float temperature;  
  float humidity;     
  float pressure;     
};

uint8_t hubMAC[] = { 0x1C, 0xDB, 0xD4, 0x85, 0x6E, 0x9C };
#define HUB_WIFI_CHANNEL 11

class HubPeer : public ESP_NOW_Peer {
public:
  HubPeer(const uint8_t *mac_addr, uint8_t channel, wifi_interface_t iface = WIFI_IF_STA, const uint8_t *lmk = NULL)
    : ESP_NOW_Peer(mac_addr, channel, iface, lmk) {}

  bool add_to_system() { return ESP_NOW_Peer::add(); }
  bool sendData(const uint8_t *data, size_t len) { return send(data, len); }
};

HubPeer hub(hubMAC, HUB_WIFI_CHANNEL, WIFI_IF_STA);

// ─────────────────────────────────────────────────────────────────────────────
// SYSTEM DEEP SLEEP (The 22uA Floor & Battery Leak Protection Goes Here)
// ─────────────────────────────────────────────────────────────────────────────
void goToSleep(void) {
  Serial.println("=== PREPARING FOR DEEP SLEEP ===");
  Serial.flush();

  // 1. Completely shut down I2C so BME280 pull-ups don't drain the battery
  Wire.end(); 

  // 2. Set the radio to pass-through deep sleep mode 
  radio.sleep(true); 

  // 3. Terminate active SPI interface completely before touching pins
  SPI.end(); 

  // 4. Ebyte Base Pin Isolation: Drive CS High, lock out clock/data leakage
  pinMode(RADIO_CS_PIN, OUTPUT);
  digitalWrite(RADIO_CS_PIN, HIGH);
  pinMode(RADIO_SCLK_PIN, INPUT_PULLDOWN);
  pinMode(RADIO_MOSI_PIN, INPUT_PULLDOWN);
  pinMode(RADIO_MISO_PIN, INPUT_PULLDOWN);

  // 5. Battery Leak Protection: Secure the UART RX/TX pins so they don't float
  pinMode(1, INPUT_PULLDOWN); // ESP32 Hardware RX
  pinMode(3, INPUT_PULLDOWN); // ESP32 Hardware TX

  // 6. Secure the Direct Copper Jumper Wire Line against noise
  rtc_gpio_init(WAKE_PIN);
  rtc_gpio_set_direction(WAKE_PIN, RTC_GPIO_MODE_INPUT_ONLY);
  rtc_gpio_pullup_dis(WAKE_PIN);
  rtc_gpio_pulldown_en(WAKE_PIN); 

  // Wake up when SX1262 DIO1 drives GPIO16 HIGH (Active-HIGH / Unity non-inverting match)
  esp_sleep_enable_ext0_wakeup(WAKE_PIN, 1); 

  // 7. Power down ESP32 core
  esp_deep_sleep_start();
}

void setupLoRa() {
  Serial.print(F("[SX126x] Initializing ... "));
  int state = radio.begin(radioFreq, 500.0, 7, 7, RADIOLIB_SX126X_SYNC_WORD_PRIVATE, 2, 512, 0.0, true);
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("success!"));
  } else {
    Serial.print(F("failed, code "));
    Serial.println(state);
    while (true);
  }
}

bool setupESPNOW() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();  
  esp_wifi_set_channel(HUB_WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);

  if (!ESP_NOW.begin()) {
    Serial.println("ESP-NOW init failed");
    return false;
  }
  if (!hub.add_to_system()) {
    Serial.println("Failed to bind hub peer");
    return false;
  }
  return true;
}

bool readAndSendBME280() {
  Wire.end();
  delay(50);
  Wire.setPins(BME_SDA, BME_SCL);
  if (!Wire.begin(BME_SDA, BME_SCL)) {
    Serial.println("I2C peripheral allocation failed!");
  }
  delay(50);

  if (!bme.begin()) {
    Serial.println("BME280 not found");
    return false;
  }

  float tempF = NAN, humidity = NAN, pressureHPa = NAN;
  BME280::TempUnit tempUnit(BME280::TempUnit_Fahrenheit);
  BME280::PresUnit presUnit(BME280::PresUnit_hPa);
  bme.read(pressureHPa, tempF, humidity, tempUnit, presUnit);

  if (isnan(tempF) || isnan(pressureHPa)) {
    Serial.println("Telemetry corruption.");
    return false;
  }

  if (!setupESPNOW()) {
    return false;
  }

  BME280Data pkt;
  pkt.type = MSG_BME280;
  pkt.temperature = tempF;
  pkt.humidity = humidity;
  pkt.pressure = pressureHPa;  

  bool sent = hub.sendData((uint8_t *)&pkt, sizeof(BME280Data));
  WiFi.mode(WIFI_OFF);  
  return sent;
}

// ─────────────────────────────────────────────────────────────────────────────
// MAIN EXECUTION LOOP
// ─────────────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);

  // 1. Restore standard functional pin modes before restarting SPI
  pinMode(RADIO_CS_PIN, OUTPUT);
  digitalWrite(RADIO_CS_PIN, HIGH);
  SPI.begin(RADIO_SCLK_PIN, RADIO_MISO_PIN, RADIO_MOSI_PIN, RADIO_CS_PIN);

  // 2. Initialize LoRa parameters
  setupLoRa();

  // 3. Process execution ONLY if woken up by the direct jumper interrupt
  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0) {
    Serial.println(F("[WAKEUP] Hub packet received! Collecting telemetry..."));
    readAndSendBME280();
  } else {
    Serial.println(F("[INITIAL BOOT] Cold start. Grounding registers..."));
  }

  // 4. Return immediately to the low power profile
  goToSleep();
}

void loop() {
  // Loop is bypassed entirely by deep sleep execution
}
