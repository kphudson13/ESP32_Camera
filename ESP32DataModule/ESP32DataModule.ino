/* 
A script to send data from this esp32 to another esp32 with a camera to further pass
Jan 2026 
Kyle Hudson
Live laugh love
*/

#include <DHT.h>
#include <RTClib.h>
#include <Wire.h>
#include <esp_now.h>
#include <WiFi.h>

#define DHTPIN 19
#define DHTTYPE DHT22

RTC_DS3231 rtc;
DHT dht(DHTPIN, DHTTYPE);

// Replace with your ESP32-CAM MAC address
uint8_t partnerMac[] = { 0x88, 0x57, 0x21, 0xC1, 0x98, 0x40 };

typedef struct {
  float temperature;
  float humidity;
  uint32_t unixTime;
} sensor_packet_t;

sensor_packet_t packet;

// to confirm data sent
void onSend(const wifi_tx_info_t *info, esp_now_send_status_t status) {
  Serial.println("Packet sent (ack may fail due to WiFi mode)");
}

void setup() {
  Serial.begin(115200);

  // Start I2C on custom pins
  Wire.begin(15, 16);  // SDA, SCL

  dht.begin();

  // ESP-NOW setup
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    return;
  }

  // Wait for WiFi to start
  while (WiFi.macAddress() == "00:00:00:00:00:00") {
    delay(100);
  }
  Serial.print("MAC: ");
  Serial.println(WiFi.macAddress());

  if (!rtc.begin()) {
    Serial.println("Couldn't find DS3231");
    while (1)
      ;
  }

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, partnerMac, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }

  esp_now_register_send_cb(onSend);

  // set RTC 
  if (rtc.lostPower()) {
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  delay(200);  // Minor delay to let modules stablize
}

void loop() {

  DateTime now = rtc.now();

  packet.temperature = dht.readTemperature();
  packet.humidity = dht.readHumidity();
  packet.unixTime = now.unixtime();

  if (isnan(packet.temperature) || isnan(packet.humidity)) {
    Serial.println("DHT read failed");
    delay(4000);
    return;
  }

  esp_err_t result = esp_now_send(partnerMac,
                                  (uint8_t *)&packet,
                                  sizeof(packet));

  if (result == ESP_OK) {
    Serial.println("ESP-NOW send OK");
  } else {
    Serial.println("ESP-NOW send FAIL");
  }

  Serial.print(now.year());
  Serial.print("-");
  Serial.print(now.month());
  Serial.print("-");
  Serial.print(now.day());
  Serial.print(" ");
  Serial.print(now.hour());
  Serial.print(":");
  Serial.print(now.minute());
  Serial.print(":");
  Serial.print(now.second());
  Serial.println();
  Serial.print("Temp: ");
  Serial.println(packet.temperature);
  Serial.print("Hum: ");
  Serial.println(packet.humidity);
  Serial.println("--------------------");
  delay(6000);
}