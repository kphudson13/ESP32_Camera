/* 
A script to send pictures from an esp32-cam via email
You need a credentials file to make this work 
Jan 2026 
Kyle Hudson
Live laugh love
*/

#include "esp_camera.h"       // include library for the camera
#include <WiFi.h>             // for wifi connection
#include <ESP_Mail_Client.h>  // for mail client
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

// SMTP objects
SMTPSession smtp;
ESP_Mail_Session session;

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
  config.fb_count = 1;

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

void setup() {

  Serial.begin(115200);
  Serial.println("Setup started!");  // Add this line
  if (!WiFiConnect(ssid, password))
    return;

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
}

void loop() {

  delay(1000);  // initial delay 1sec

  // Capture the photo
  Serial.println("Attempting to capture photo...");
  camera_fb_t* fb = esp_camera_fb_get();
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
  message.subject = "Automated Photo";
  message.addRecipient("Kyle", recip_email);
  message.text.content = "Photo attached.";
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
    return;
  }
  if (!MailClient.sendMail(&smtp, &message))
    Serial.println(smtp.errorReason());
  else
    Serial.println("Email sent successfully");

  esp_camera_fb_return(fb);  // clear frame buffer for RAM management
  smtp.closeSession();       // Add after sending email

  Serial.println("Delay till next cycle...");

  delay(14400000);  // 4 hours
}
