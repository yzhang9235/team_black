// #include <Arduino.h>
// #include "esp_camera.h"
// #include "ESP32_OV5640_AF.h"
// #include <WiFi.h>
// #include <WiFiUdp.h>

// // =============================================
// // Camera creates its own WiFi network
// // =============================================
// const char* AP_SSID = "PlastibotCam";
// const char* AP_PASS = "plastibot123";

// // =============================================
// // Freenove ESP32-S3 WROOM Camera Pin Definitions
// // =============================================
// #define PWDN_GPIO_NUM     -1
// #define RESET_GPIO_NUM    -1
// #define XCLK_GPIO_NUM     15
// #define SIOD_GPIO_NUM     4
// #define SIOC_GPIO_NUM     5
// #define Y9_GPIO_NUM       16
// #define Y8_GPIO_NUM       17
// #define Y7_GPIO_NUM       18
// #define Y6_GPIO_NUM       12
// #define Y5_GPIO_NUM       10
// #define Y4_GPIO_NUM       8
// #define Y3_GPIO_NUM       9
// #define Y2_GPIO_NUM       11
// #define VSYNC_GPIO_NUM    6
// #define HREF_GPIO_NUM     7
// #define PCLK_GPIO_NUM     13

// // =============================================
// // UDP config
// // =============================================
// #define UDP_PORT        5005
// #define DISC_PORT       (UDP_PORT - 1)
// #define MAX_PACKET_SIZE 1200
// #define HEADER_SIZE     10

// WiFiUDP udp;
// WiFiUDP discUdp;
// OV5640 ov5640;

// IPAddress cydIP;
// bool cydKnown = false;
// uint32_t frameID = 0;

// void startCameraAP() {
//   Serial.printf("Starting camera hotspot: %s\n", AP_SSID);

//   WiFi.mode(WIFI_AP);
//   WiFi.setSleep(false);

//   bool ok = WiFi.softAP(AP_SSID, AP_PASS, 1, 0, 1); // channel 1, visible, max 1 station
//   if (!ok) {
//     Serial.println("SoftAP start failed");
//     while (true) delay(1000);
//   }

//   delay(500);
//   Serial.println("Camera hotspot started!");
//   Serial.printf("Camera AP IP: %s\n", WiFi.softAPIP().toString().c_str());
// }

// bool initCamera() {
//   camera_config_t config;
//   config.ledc_channel = LEDC_CHANNEL_0;
//   config.ledc_timer   = LEDC_TIMER_0;
//   config.pin_d0       = Y2_GPIO_NUM;
//   config.pin_d1       = Y3_GPIO_NUM;
//   config.pin_d2       = Y4_GPIO_NUM;
//   config.pin_d3       = Y5_GPIO_NUM;
//   config.pin_d4       = Y6_GPIO_NUM;
//   config.pin_d5       = Y7_GPIO_NUM;
//   config.pin_d6       = Y8_GPIO_NUM;
//   config.pin_d7       = Y9_GPIO_NUM;
//   config.pin_xclk     = XCLK_GPIO_NUM;
//   config.pin_pclk     = PCLK_GPIO_NUM;
//   config.pin_vsync    = VSYNC_GPIO_NUM;
//   config.pin_href     = HREF_GPIO_NUM;
//   config.pin_sccb_sda = SIOD_GPIO_NUM;
//   config.pin_sccb_scl = SIOC_GPIO_NUM;
//   config.pin_pwdn     = PWDN_GPIO_NUM;
//   config.pin_reset    = RESET_GPIO_NUM;
//   config.xclk_freq_hz = 24000000;           // push clock harder
//   config.pixel_format = PIXFORMAT_JPEG;

//   if (psramFound()) {
//     config.frame_size   = FRAMESIZE_QQVGA;  // 160x120, will be scaled to full screen
//     config.jpeg_quality = 28;               // more compression = smaller frames = faster
//     config.fb_count     = 2;
//     config.grab_mode    = CAMERA_GRAB_LATEST;
//     Serial.println("PSRAM found");
//   } else {
//     config.frame_size   = FRAMESIZE_QQVGA;
//     config.jpeg_quality = 28;
//     config.fb_count     = 1;
//     config.grab_mode    = CAMERA_GRAB_WHEN_EMPTY;
//     Serial.println("No PSRAM");
//   }

//   esp_err_t err = esp_camera_init(&config);
//   if (err != ESP_OK) {
//     Serial.printf("Camera init failed: 0x%x\n", err);
//     return false;
//   }

//   sensor_t* s = esp_camera_sensor_get();
//   if (s) {
//     s->set_brightness(s, 0);
//     s->set_contrast(s, 1);
//     s->set_saturation(s, 0);
//     s->set_hmirror(s, 1);
//     s->set_vflip(s, 0);

//     ov5640.start(s);
//     if (ov5640.focusInit() == 0) {
//       Serial.println("OV5640 AF init OK");
//       ov5640.autoFocusMode();
//     } else {
//       Serial.println("OV5640 AF init failed (continuing anyway)");
//     }
//   }

//   Serial.println("Camera ready");
//   return true;
// }

// void sendFrame(camera_fb_t* fb) {
//   if (!cydKnown) return;

//   const uint32_t len = fb->len;
//   const uint16_t payloadSize = MAX_PACKET_SIZE - HEADER_SIZE;
//   const uint16_t totalChunks =
//       (len + payloadSize - 1) / payloadSize;

//   uint8_t pkt[MAX_PACKET_SIZE];
//   uint32_t offset = 0;

//   for (uint16_t i = 0; i < totalChunks; i++) {
//     const uint16_t chunkLen = min((uint32_t)payloadSize, len - offset);

//     pkt[0] = (frameID >> 24) & 0xFF;
//     pkt[1] = (frameID >> 16) & 0xFF;
//     pkt[2] = (frameID >>  8) & 0xFF;
//     pkt[3] = (frameID      ) & 0xFF;
//     pkt[4] = (i >> 8) & 0xFF;
//     pkt[5] =  i       & 0xFF;
//     pkt[6] = (totalChunks >> 8) & 0xFF;
//     pkt[7] =  totalChunks       & 0xFF;
//     pkt[8] = (chunkLen >> 8) & 0xFF;
//     pkt[9] =  chunkLen       & 0xFF;

//     memcpy(pkt + HEADER_SIZE, fb->buf + offset, chunkLen);

//     udp.beginPacket(cydIP, UDP_PORT);
//     udp.write(pkt, HEADER_SIZE + chunkLen);
//     int ok = udp.endPacket();

//     if (!ok) {
//       // tiny pause if send buffer chokes
//       delayMicroseconds(400);
//     }

//     offset += chunkLen;

//     // small pacing helps real throughput on ESP32 UDP
//     delayMicroseconds(150);
//   }

//   frameID++;
// }

// void checkDiscovery() {
//   if (cydKnown) return;

//   int sz = discUdp.parsePacket();
//   if (sz > 0) {
//     char buf[16] = {0};
//     discUdp.read(buf, sizeof(buf) - 1);

//     if (strncmp(buf, "HELLO", 5) == 0) {
//       cydIP = discUdp.remoteIP();
//       cydKnown = true;
//       Serial.printf("CYD discovered at %s\n", cydIP.toString().c_str());

//       discUdp.beginPacket(cydIP, DISC_PORT);
//       discUdp.write((const uint8_t*)"ACK", 3);
//       discUdp.endPacket();
//     }
//   }
// }

// void setup() {
//   Serial.begin(115200);
//   delay(500);
//   Serial.println("\nESP32-S3 Camera (WiFi AP UDP) starting...");

//   startCameraAP();

//   discUdp.begin(DISC_PORT);
//   udp.begin(UDP_PORT);

//   if (!initCamera()) {
//     Serial.println("FATAL: camera init failed");
//     while (true) delay(1000);
//   }

//   Serial.println("Waiting for CYD discovery packet...");
// }

// void loop() {
//   if (!cydKnown) {
//     checkDiscovery();
//     delay(5);
//     return;
//   }

//   camera_fb_t* fb = esp_camera_fb_get();
//   if (!fb) {
//     Serial.println("Frame capture failed");
//     delay(2);
//     return;
//   }

//   uint32_t frameLen = fb->len;
//   sendFrame(fb);
//   esp_camera_fb_return(fb);

//   static uint32_t frames = 0, lastMs = 0;
//   frames++;
//   if (millis() - lastMs >= 1000) {
//     Serial.printf("TX FPS: %u | last frame: %u bytes\n", frames, frameLen);
//     frames = 0;
//     lastMs = millis();
//   }
// }

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
  esp_wifi_sta_enterprise_enable();

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