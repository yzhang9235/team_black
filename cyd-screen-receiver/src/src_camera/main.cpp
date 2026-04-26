#include <Arduino.h>
#include "esp_camera.h"
#include "ESP32_OV5640_AF.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "esp_wpa2.h"
#include "secrets.h"

// =============================================
// Freenove ESP32-S3 WROOM Camera Pin Definitions
// =============================================
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     15
#define SIOD_GPIO_NUM     4
#define SIOC_GPIO_NUM     5
#define Y9_GPIO_NUM       16
#define Y8_GPIO_NUM       17
#define Y7_GPIO_NUM       18
#define Y6_GPIO_NUM       12
#define Y5_GPIO_NUM       10
#define Y4_GPIO_NUM       8
#define Y3_GPIO_NUM       9
#define Y2_GPIO_NUM       11
#define VSYNC_GPIO_NUM    6
#define HREF_GPIO_NUM     7
#define PCLK_GPIO_NUM     13

// =============================================
// Joystick pins
// =============================================
#define JOY_X_PIN   40
#define JOY_Y_PIN   39
#define JOY_SW_PIN  41

// =============================================
// Server config
// =============================================
#define SERVER_HOST "team-black-1.onrender.com"
#define SERVER_PORT 443
#define SERVER_PATH "/upload"

OV5640 ov5640;

void connectWiFi() {
  Serial.println("Connecting to Tufts_Secure...");

  WiFi.disconnect(true);
  delay(500);
  WiFi.mode(WIFI_STA);

  // Enterprise WPA2
  esp_wifi_sta_wpa2_ent_set_identity((uint8_t *)WIFI_USERNAME, strlen(WIFI_USERNAME));
  esp_wifi_sta_wpa2_ent_set_username((uint8_t *)WIFI_USERNAME, strlen(WIFI_USERNAME));
  esp_wifi_sta_wpa2_ent_set_password((uint8_t *)WIFI_PASSWORD, strlen(WIFI_PASSWORD));
  //esp_wifi_sta_enterprise_enable();
  esp_wifi_sta_wpa2_ent_enable();

  WiFi.begin("Tufts_Secure");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected!");
  Serial.printf("IP: %s\n", WiFi.localIP().toString().c_str());
}

bool initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0       = Y2_GPIO_NUM;
  config.pin_d1       = Y3_GPIO_NUM;
  config.pin_d2       = Y4_GPIO_NUM;
  config.pin_d3       = Y5_GPIO_NUM;
  config.pin_d4       = Y6_GPIO_NUM;
  config.pin_d5       = Y7_GPIO_NUM;
  config.pin_d6       = Y8_GPIO_NUM;
  config.pin_d7       = Y9_GPIO_NUM;
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;
  config.xclk_freq_hz = 24000000;
  config.pixel_format = PIXFORMAT_JPEG;

  if (psramFound()) {
    config.frame_size   = FRAMESIZE_QVGA;  // 320x240, better for food recognition
    config.jpeg_quality = 10;
    config.fb_count     = 2;
    config.grab_mode    = CAMERA_GRAB_LATEST;
  } else {
    config.frame_size   = FRAMESIZE_QVGA;
    config.jpeg_quality = 10;
    config.fb_count     = 1;
    config.grab_mode    = CAMERA_GRAB_WHEN_EMPTY;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed: 0x%x\n", err);
    return false;
  }

  sensor_t* s = esp_camera_sensor_get();
  if (s) {
    s->set_brightness(s, 0);
    s->set_contrast(s, 1);
    s->set_saturation(s, 0);
    s->set_hmirror(s, 1);
    s->set_vflip(s, 0);

    ov5640.start(s);
    if (ov5640.focusInit() == 0) {
      ov5640.autoFocusMode();
    }
  }

  Serial.println("Camera ready");
  return true;
}

void sendPhotoToServer(const String& foodId) {
  Serial.println("Taking photo...");

  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    return;
  }

  Serial.printf("Photo taken: %u bytes\n", fb->len);
  Serial.println("Sending to server...");

  WiFiClientSecure client;
  client.setInsecure(); // skip SSL cert check

  if (!client.connect(SERVER_HOST, SERVER_PORT)) {
    Serial.println("Connection to server failed");
    esp_camera_fb_return(fb);
    return;
  }

  // Build multipart form body
  String boundary = "----ESP32Boundary";
  String bodyStart = "--" + boundary + "\r\n";
  bodyStart += "Content-Disposition: form-data; name=\"id\"\r\n\r\n";
  bodyStart += foodId + "\r\n";
  bodyStart += "--" + boundary + "\r\n";
  bodyStart += "Content-Disposition: form-data; name=\"image\"; filename=\"food.jpg\"\r\n";
  bodyStart += "Content-Type: image/jpeg\r\n\r\n";

  String bodyEnd = "\r\n--" + boundary + "--\r\n";

  int contentLength = bodyStart.length() + fb->len + bodyEnd.length();

  // Send HTTP request
  client.printf("POST %s HTTP/1.1\r\n", SERVER_PATH);
  client.printf("Host: %s\r\n", SERVER_HOST);
  client.printf("Content-Type: multipart/form-data; boundary=%s\r\n", boundary.c_str());
  client.printf("Content-Length: %d\r\n", contentLength);
  client.println("Connection: close\r\n");

  client.print(bodyStart);
  client.write(fb->buf, fb->len);
  client.print(bodyEnd);

  esp_camera_fb_return(fb);

  // Read response
  String response = "";
  long timeout = millis() + 5000;
  while (client.connected() && millis() < timeout) {
    if (client.available()) {
      response += client.readString();
      break;
    }
  }

  Serial.println("Server response: " + response);
  client.stop();
}

void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(JOY_SW_PIN, INPUT_PULLUP);

  connectWiFi();

  if (!initCamera()) {
    Serial.println("FATAL: camera init failed");
    while (true) delay(1000);
  }

  Serial.println("Ready! Press joystick to take photo.");
}

void loop() {
  // Check joystick button press
  if (digitalRead(JOY_SW_PIN) == LOW) {
    Serial.println("Joystick pressed!");

    // Get next available food ID from server
    WiFiClientSecure client;
    client.setInsecure();

    String foodId = "FOOD001"; // fallback

    if (client.connect(SERVER_HOST, SERVER_PORT)) {
      client.println("GET /next_id HTTP/1.1");
      client.printf("Host: %s\r\n", SERVER_HOST);
      client.println("Connection: close\r\n");

      long timeout = millis() + 3000;
      String response = "";
      while (client.connected() && millis() < timeout) {
        if (client.available()) {
          response = client.readString();
          break;
        }
      }
      client.stop();

      // Parse food_id from response body
      int idx = response.indexOf("FOOD");
      if (idx >= 0) {
        foodId = response.substring(idx, idx + 7);
      }
    }

    Serial.printf("Using food ID: %s\n", foodId.c_str());
    sendPhotoToServer(foodId);

    // Debounce
    delay(2000);
  }

  delay(50);
}