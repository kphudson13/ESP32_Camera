/*
  ESP32 DHT22 ESP-NOW Sender
  Sends temperature and humidity to a receiver ESP32
  Jan 2026
  Kyle Hudson
  live laugh love
*/

#include <WiFi.h>
#include <esp_now.h>
#include <DHT.h>

// ---------------- CONFIG ----------------
#define DHTPIN 4
#define DHTTYPE DHT22
#define SEND_INTERVAL_MS 60000  // send every minute

// Replace with RECEIVER ESP32 MAC ADDRESS
uint8_t receiverMAC[] = { 0x24, 0x6F, 0x28, 0xAA, 0xBB, 0xCC };
// ----------------------------------------

DHT dht(DHTPIN, DHTTYPE);

// Data structure sent via ESP-NOW
typedef struct struct_message {
  float temperature;
  float humidity;
  uint32_t uptime_ms;
} struct_message;

struct_message sensorData;

unsigned long lastSend = 0;

// Callback when data is sent
void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("ESP-NOW send status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "SUCCESS" : "FAIL");
}

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println("Starting DHT22 ESP-NOW sender");

  // Initialize DHT
  dht.begin();

  // Set WiFi mode (required for ESP-NOW)
  WiFi.mode(WIFI_STA);
  Serial.print("Sender MAC: ");
  Serial.println(WiFi.macAddress());

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    ESP.restart();
  }

  esp_now_register_send_cb(onDataSent);

  // Register receiver
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, receiverMAC, 6);
  peerInfo.channel = 0;     // same WiFi channel
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    ESP.restart();
  }

  Serial.println("ESP-NOW initialized");
}

void loop() {
  unsigned long now = millis();

  if (now - lastSend >= SEND_INTERVAL_MS) {
    lastSend = now;

    float humidity = dht.readHumidity();
    float temperature = dht.readTemperature(); // Celsius

    if (isnan(humidity) || isnan(temperature)) {
      Serial.println("Failed to read from DHT sensor");
      return;
    }

    sensorData.temperature = temperature;
    sensorData.humidity = humidity;
    sensorData.uptime_ms = now;

    esp_err_t result = esp_now_send(receiverMAC,
                                    (uint8_t *)&sensorData,
                                    sizeof(sensorData));

    Serial.printf(
      "Sent -> Temp: %.2f C | Hum: %.2f %% | Result: %s\n",
      temperature,
      humidity,
      result == ESP_OK ? "OK" : "ERROR"
    );
  }
}

