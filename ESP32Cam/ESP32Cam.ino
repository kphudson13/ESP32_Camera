/* 
A script to send pictures from an esp32-cam via email
You need a credentials file to make this work 
This esp32 also collects data from another module via ESP-NOW
Jan 2026 
Kyle Hudson
Live laugh love
*/

#include "esp_camera.h"       // include library for the camera
#include <WiFi.h>             // for wifi connection
#include <ESP_Mail_Client.h>  // for mail client
#include <esp_now.h>          // To talk to other esp32
#include <RTClib.h>           // To convert data packet from unix time
#include "credentials.h"      // personal credentials file, not tracked with Git

// set pin configurations - #define replaces all the first value with the second before it hits the compiler
#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27
#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22

// Delay email send
unsigned long lastSendTime = 0;
const unsigned long sendInterval = 14400000;  // 4 hours

// For backup incase no temp data is recieved
unsigned long packetWaitStart = 0;
const unsigned long packetTimeout = 300000;  // 5 minutes

// SMTP objects
SMTPSession smtp;
ESP_Mail_Session session;

// Replace with the partner ESP32 MAC address
//uint8_t partnerMac[] = { 0xE4, 0xB0, 0x63, 0xB4, 0x2F, 0xE8 };

// structure for data from other esp
typedef struct {
  float temperature;
  float humidity;
  uint32_t unixTime;
} sensor_packet_t;

sensor_packet_t lastPacket;
bool hasPacket = false;

// Camera function
bool initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;

  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  config.frame_size = FRAMESIZE_VGA;
  config.jpeg_quality = 12;
  config.fb_count = 1;  // Allows only one frame cached

  return esp_camera_init(&config) == ESP_OK;
}

// WiFi function
uint8_t WiFiConnect(const char* nSSID, const char* nPassword) {

  Serial.print("Connecting to ");
  Serial.println(nSSID);
  WiFi.begin(nSSID, nPassword);

  uint8_t i = 0;
  while (WiFi.status() != WL_CONNECTED && i++ < 50) {
    delay(250);
    Serial.print(".");
  }
  Serial.println();  // Just make a new line after loading dots

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi connection failed");
    return false;
  }
  Serial.println("WiFi connected");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  return true;
}

void onReceive(const esp_now_recv_info_t* info,
               const uint8_t* data,
               int len) {

  Serial.println("ESP-NOW PACKET RECEIVED");

  memcpy(&lastPacket, data, sizeof(lastPacket));
  hasPacket = true;
}

void setup() {

  Serial.begin(115200);
  delay(1000);  // initial delay 1sec
  Serial.println("Setup started!");

  // Initialize ESP-NOW
  WiFi.mode(WIFI_STA);

  if (!WiFiConnect(ssid, password))
    return;

  // Wait for WiFi to start
  while (WiFi.macAddress() == "00:00:00:00:00:00") {
    delay(100);
  }
  Serial.print("MAC: ");
  Serial.println(WiFi.macAddress());

  // IMPORTANT: re-init ESP-NOW after Wi-Fi is connected
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    return;
  }

  esp_now_register_recv_cb(onReceive);

  // initialize camera
  Serial.println("Starting camera initialization...");
  if (!initCamera()) {
    Serial.println("Camera init failed");
    return;
  }
  Serial.println("Camera initialized!");
  delay(500);  // Give camera time to stabilize

  // configure camera
  Serial.println("Modifying photo sensor orientation");
  sensor_t* s = esp_camera_sensor_get();
  s->set_vflip(s, 1);    // vertical flip becasue photo is upside down
  s->set_hmirror(s, 1);  // horizontal mirror (optional)

  lastSendTime = millis() - sendInterval;
}

void loop() {

   // Check if 4 hours have passed since last email
  if (millis() - lastSendTime < sendInterval) {
    Serial.println("Waiting for email delay");
    delay(3000);  // Keep latest packet, wait for next window
    return;
  }

  // Check for data packet
  if (packetWaitStart == 0) {
    packetWaitStart = millis();
  }

  // If no packet yet AND timeout not reached → wait
  if (!hasPacket && millis() - packetWaitStart < packetTimeout) {
    Serial.println("Waiting for ESP-NOW data...");
    delay(5000);
    return;
  }

  // If timeout reached with no packet → continue anyway
  bool packetTimedOut = false;
  if (!hasPacket) {
    Serial.println("ESP-NOW timeout reached");
    packetTimedOut = true;
  }

  // Double check wifi is connected
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.reconnect();
    delay(5000);
  }

  // Capture the photo
  Serial.println("Attempting to capture photo...");
  camera_fb_t* fb = esp_camera_fb_get();
  if (fb) esp_camera_fb_return(fb);  // discard stale frame
  delay(100);                        // allow new capture
  fb = esp_camera_fb_get();          // This is the fresh photo
  if (!fb) {
    Serial.println("Camera capture failed");
    return;
  }
  Serial.println("Photo captured!");

  // SMTP CONFIG
  session.server.host_name = "smtp.gmail.com";
  session.server.port = 465;
  session.login.email = smtp_email;
  session.login.password = smtp_password;
  session.login.user_domain = "";

  // MESSAGE
  SMTP_Message message;
  message.sender.name = "ESP32-CAM";
  message.sender.email = smtp_email;
  message.subject = "Automated Apt. Data";
  message.addRecipient("Kyle", recip_email);
  message.text.content = "Photo attached.\n";

  if (packetTimedOut) {
    message.text.content += "Temperature data is not available.\n";
  } else {
    DateTime timestamp(lastPacket.unixTime);
    message.text.content += "Data from : " + String(timestamp.year()) + "-" + String(timestamp.month()) + "-" + String(timestamp.day()) + " " + String(timestamp.hour()) + ":" + String(timestamp.minute()) + ":" + String(timestamp.second()) + "\n";
    message.text.content += "Temp: " + String(lastPacket.temperature, 1) + "C Hum: " + String(lastPacket.humidity, 0) + "%";
  }
  message.text.charSet = "us-ascii";
  SMTP_Attachment att;
  att.descr.filename = "photo.jpg";
  att.descr.mime = "image/jpeg";
  att.blob.data = fb->buf;
  att.blob.size = fb->len;
  att.descr.transfer_encoding = Content_Transfer_Encoding::enc_base64;
  message.addAttachment(att);

  // SEND
  if (!smtp.connect(&session)) {
    Serial.println("SMTP connect failed");
    esp_camera_fb_return(fb);  // clear camera buffer incase smtp fails
    return;
  }
  if (!MailClient.sendMail(&smtp, &message))
    Serial.println(smtp.errorReason());
  else
    Serial.println("Email sent successfully");

  lastSendTime = millis();  // reset timer ONLY AFTER sending email

  esp_camera_fb_return(fb);  // clear frame buffer for RAM management
  smtp.closeSession();       // Add after sending email

  hasPacket = false;  // Reset flag to wait for new packet
  packetWaitStart = 0;

  Serial.println("Delay till next cycle...");
}
