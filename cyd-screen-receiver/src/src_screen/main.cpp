#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <TJpg_Decoder.h>
#include <WiFi.h>
#include <WiFiUDP.h>
#include "esp_wpa2.h"
#include "secrets.h"

// =============================================
// UDP config
// =============================================
#define UDP_PORT  5005
#define DISC_PORT (UDP_PORT - 1)

// =============================================
// Packet reassembly
// =============================================
#define HEADER_SIZE      10
#define MAX_JPEG_SIZE    (16 * 1024)
#define MAX_UDP_PAYLOAD  1200
#define MAX_CHUNKS       32

#define TFT_BL_PIN 21

TFT_eSPI tft = TFT_eSPI();
WiFiUDP udp;
WiFiUDP discUdp;

static uint8_t  jpegBuf[MAX_JPEG_SIZE];
static bool     chunkReceived[MAX_CHUNKS];
static uint32_t currentFrameID = 0xFFFFFFFF;
static uint16_t expectedChunks = 0;
static uint16_t chunksReceived = 0;
static uint8_t  pktBuf[MAX_UDP_PAYLOAD];

IPAddress camIP;
bool camKnown = false;

bool tftOutput(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  if (x >= tft.width() || y >= tft.height()) return false;
  if (x + w > tft.width())  w = tft.width() - x;
  if (y + h > tft.height()) h = tft.height() - y;
  tft.pushImage(x, y, w, h, bitmap);
  return true;
}

void showStatus(const char* msg) {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 110);
  tft.print(msg);
}

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

  showStatus("WiFi...");

  uint32_t startMs = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    Serial.print(".");
    if (millis() - startMs > 20000) {
      Serial.println("\nWiFi timeout - rebooting");
      ESP.restart();
    }
  }

  Serial.println("\nWiFi connected!");
  Serial.printf("CYD IP: %s\n", WiFi.localIP().toString().c_str());

  char ipStr[32];
  snprintf(ipStr, sizeof(ipStr), "%s", WiFi.localIP().toString().c_str());
  showStatus(ipStr);
  delay(800);
}

void discoveryLoop() {
  showStatus("Finding cam...");
  uint32_t lastHello = 0;

  while (!camKnown) {
    if (millis() - lastHello > 1000) {
      discUdp.beginPacket(IPAddress(255,255,255,255), DISC_PORT);
      discUdp.write((const uint8_t*)"HELLO", 5);
      discUdp.endPacket();
      Serial.println("Sent HELLO broadcast...");
      lastHello = millis();
    }

    int sz = discUdp.parsePacket();
    if (sz > 0) {
      char buf[8] = {0};
      discUdp.read(buf, sizeof(buf) - 1);
      if (strncmp(buf, "ACK", 3) == 0) {
        camIP = discUdp.remoteIP();
        camKnown = true;
        Serial.printf("Camera found at %s\n", camIP.toString().c_str());
      }
    }
    delay(20);
  }

  showStatus("Camera found!");
  delay(300);
  tft.fillScreen(TFT_BLACK);
}

void resetFrame(uint32_t newFrameID, uint16_t totalChunks) {
  currentFrameID = newFrameID;
  expectedChunks = totalChunks;
  chunksReceived = 0;
  memset(chunkReceived, 0, sizeof(chunkReceived));
}

void processPacket(uint8_t* data, int len) {
  if (len < HEADER_SIZE) return;

  uint32_t frameID     = ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16)
                       | ((uint32_t)data[2] <<  8) |  (uint32_t)data[3];
  uint16_t chunkIdx    = ((uint16_t)data[4] << 8) | data[5];
  uint16_t totalChunks = ((uint16_t)data[6] << 8) | data[7];
  uint16_t chunkLen    = ((uint16_t)data[8] << 8) | data[9];

  if (totalChunks == 0 || totalChunks > MAX_CHUNKS) return;
  if (chunkIdx >= totalChunks) return;
  if (chunkLen == 0 || (HEADER_SIZE + chunkLen) > (uint16_t)len) return;

  if (frameID != currentFrameID) {
    resetFrame(frameID, totalChunks);
  }

  if (chunkReceived[chunkIdx]) return;

  uint32_t payloadSize = MAX_UDP_PAYLOAD - HEADER_SIZE;
  uint32_t offset = (uint32_t)chunkIdx * payloadSize;

  if (offset + chunkLen > MAX_JPEG_SIZE) return;

  memcpy(jpegBuf + offset, data + HEADER_SIZE, chunkLen);
  chunkReceived[chunkIdx] = true;
  chunksReceived++;

  if (chunksReceived == expectedChunks) {
    uint32_t fullChunks = expectedChunks - 1;
    uint32_t jpegSize = fullChunks * payloadSize + chunkLen;

    tft.fillScreen(TFT_BLACK);

    int drawX = (tft.width()  - 160) / 2;
    int drawY = (tft.height() - 120) / 2;

    TJpgDec.drawJpg(drawX, drawY, jpegBuf, jpegSize);

    currentFrameID = 0xFFFFFFFF;
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(TFT_BL_PIN, OUTPUT);
  digitalWrite(TFT_BL_PIN, HIGH);

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(true);
  TJpgDec.setCallback(tftOutput);

  connectWiFi();

  udp.begin(UDP_PORT);
  discUdp.begin(DISC_PORT);

  discoveryLoop();

  Serial.println("Streaming...");
}

void loop() {
  if (!camKnown) {
    discoveryLoop();
    return;
  }

  int pktSize = udp.parsePacket();
  if (pktSize > 0) {
    int len = udp.read(pktBuf, sizeof(pktBuf));
    if (len > 0) {
      processPacket(pktBuf, len);
    }
  }
}