#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#include <SPI.h>
#include <TFT_eSPI.h>
#include <TJpg_Decoder.h>

#include "secrets.h"
#include "wifi_enterprise.h"

// ================================================================
// Screen setup
// ================================================================
#define TFT_BL_PIN 21

TFT_eSPI tft = TFT_eSPI();

// Touch calibration.
// If touch is wrong, we can recalibrate these numbers later.
uint16_t calData[5] = { 275, 3620, 264, 3532, 7 };

// ================================================================
// JPEG buffer
// QVGA JPEG usually fits under this.
// If image fails, increase this to 70 * 1024.
// ================================================================
#define MAX_JPEG_SIZE (60 * 1024)
static uint8_t jpgBuf[MAX_JPEG_SIZE];

// ================================================================
// UI state
// ================================================================
String currentId = "";
String currentName = "";
String currentExpiry = "";
String currentOwner = "";
String lastMessage = "Starting...";

uint32_t lastFrameMs = 0;
uint32_t lastStatusMs = 0;
uint32_t lastTouchMs = 0;

const uint32_t FRAME_INTERVAL_MS = 200;
const uint32_t STATUS_INTERVAL_MS = 1500;

// Landscape CYD is 320x240
const int SAVE_X = 240;
const int SAVE_Y = 198;
const int SAVE_W = 76;
const int SAVE_H = 38;

// ================================================================
// JPEG draw callback
// ================================================================
bool tftOutput(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  if (x >= tft.width() || y >= tft.height()) {
    return false;
  }

  if (x + w > tft.width()) {
    w = tft.width() - x;
  }

  if (y + h > tft.height()) {
    h = tft.height() - y;
  }

  tft.pushImage(x, y, w, h, bitmap);
  return true;
}

// ================================================================
// UI helpers
// ================================================================
void showStatusScreen(const char* msg) {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(12, 105);
  tft.print(msg);
}

String shortText(const String& text, int maxChars) {
  if ((int)text.length() <= maxChars) {
    return text;
  }

  return text.substring(0, maxChars - 3) + "...";
}

bool shouldShowCameraFeed() {
  return lastMessage.indexOf("ready. PRESS JOYSTICK") >= 0 ||
         lastMessage.indexOf("Claude processing") >= 0 ||
         lastMessage.indexOf("Claude done") >= 0 ||
         lastMessage.indexOf("READY TO SAVE") >= 0;
}

void drawInstructionScreen() {
  tft.fillScreen(TFT_BLACK);

  // Force simple text mode
  tft.setTextDatum(TL_DATUM);
  tft.setTextFont(1);
  tft.setTextWrap(false);

  // Draw big white boxes first, then black text on top
  tft.fillRect(10, 40, 220, 35, TFT_WHITE);
  tft.fillRect(10, 85, 220, 35, TFT_WHITE);
  tft.fillRect(10, 130, 220, 35, TFT_WHITE);

  tft.setTextColor(TFT_BLACK, TFT_WHITE);
  tft.setTextSize(3);

  if (lastMessage.indexOf("USER") >= 0 || currentOwner.length() == 0) {
    tft.setCursor(20, 48);
    tft.print("PLEASE");

    tft.setCursor(20, 93);
    tft.print("SCAN");

    tft.setCursor(20, 138);
    tft.print("USER QR");
  }
  else if (lastMessage.indexOf("ITEM") >= 0 || currentId.length() == 0) {
    tft.setCursor(20, 48);
    tft.print("PLEASE");

    tft.setCursor(20, 93);
    tft.print("SCAN");

    tft.setCursor(20, 138);
    tft.print("ITEM QR");
  }
  else {
    tft.setCursor(20, 93);
    tft.print("WAIT");
  }

  // Bottom info
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);

  tft.setCursor(10, 190);
  tft.print("Owner: ");
  if (currentOwner.length() > 0) {
    tft.print(shortText(currentOwner, 20));
  } else {
    tft.print("None");
  }

  tft.setCursor(10, 210);
  tft.print("ID: ");
  if (currentId.length() > 0) {
    tft.print(shortText(currentId, 24));
  } else {
    tft.print("None");
  }

  Serial.println("Instruction screen drawn:");
  Serial.println(lastMessage);
}

void drawOverlay() {
  // Do nothing now.
  // This removes the black blinking rectangle over the camera image.
}

bool isInsideSaveButton(uint16_t x, uint16_t y) {
  return x >= SAVE_X && x <= SAVE_X + SAVE_W &&
         y >= SAVE_Y && y <= SAVE_Y + SAVE_H;
}

// ================================================================
// Wi-Fi
// ================================================================
void connectScreenWiFi() {
  showStatusScreen("Tufts WiFi...");

  bool ok = connectEnterpriseWiFi(
    WIFI_ENTERPRISE_SSID,
    SCREEN_EAP_IDENTITY,
    SCREEN_EAP_USERNAME,
    SCREEN_EAP_PASSWORD,
    45000
  );

  if (!ok) {
    showStatusScreen("WiFi failed");
    delay(3000);
    ESP.restart();
  }

  Serial.println();
  Serial.println("================================================");
  Serial.println("CYD connected to WiFi.");
  Serial.print("CYD IP: ");
  Serial.println(WiFi.localIP());
  Serial.println("Using camera URL:");
  Serial.println(CAMERA_BASE_URL);
  Serial.println("================================================");

  char ipStr[40];
  snprintf(ipStr, sizeof(ipStr), "%s", WiFi.localIP().toString().c_str());

  showStatusScreen(ipStr);
  delay(1200);
}

// ================================================================
// Fetch camera status
// ================================================================
void fetchStatus() {
  String url = String(CAMERA_BASE_URL) + "/status?t=" + String(millis());

  HTTPClient http;
  http.setTimeout(2500);
  http.begin(url);

  int code = http.GET();

  if (code == 200) {
    String payload = http.getString();

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);

    if (!err) {
      if (doc["id"].is<const char*>()) {
        currentId = doc["id"].as<const char*>();
      }

      if (doc["name"].is<const char*>()) {
        currentName = doc["name"].as<const char*>();
      }

      if (doc["expiry_date"].is<const char*>()) {
        currentExpiry = doc["expiry_date"].as<const char*>();
      }

      if (doc["owner"].is<const char*>()) {
        currentOwner = doc["owner"].as<const char*>();
      }

      if (doc["last_save_status"].is<const char*>()) {
        lastMessage = doc["last_save_status"].as<const char*>();
      }
    } else {
      lastMessage = "JSON error";
    }
  } else {
    lastMessage = "Status HTTP " + String(code);
  }

  http.end();
}

// ================================================================
// Fetch camera JPEG
// ================================================================
void fetchAndDrawJpg() {
  if (!shouldShowCameraFeed()) {
    drawInstructionScreen();
    return;
  }
  String url = String(CAMERA_BASE_URL) + "/jpg?t=" + String(millis());

  HTTPClient http;
  http.setTimeout(5000);
  http.begin(url);

  int code = http.GET();

  if (code != 200) {
    lastMessage = "Image HTTP " + String(code);
    http.end();
    drawOverlay();
    return;
  }

  int len = http.getSize();

  if (len <= 0) {
    lastMessage = "Bad image size";
    http.end();
    drawOverlay();
    return;
  }

  if (len > MAX_JPEG_SIZE) {
    lastMessage = "Image too large";
    http.end();
    drawOverlay();
    return;
  }

  WiFiClient* stream = http.getStreamPtr();

  int bytesRead = 0;
  uint32_t startMs = millis();

  while (http.connected() && bytesRead < len && millis() - startMs < 5000) {
    int availableBytes = stream->available();

    if (availableBytes > 0) {
      int toRead = min(availableBytes, len - bytesRead);
      int actualRead = stream->readBytes(jpgBuf + bytesRead, toRead);
      bytesRead += actualRead;
    } else {
      delay(1);
    }
  }

  http.end();

  if (bytesRead != len) {
    Serial.print("Partial JPEG. Expected ");
    Serial.print(len);
    Serial.print(" got ");
    Serial.println(bytesRead);
    return;
  }

  // QVGA 320x240 draws full-screen in landscape.
  TJpgDec.drawJpg(0, 0, jpgBuf, bytesRead);

  drawOverlay();

  Serial.print("Drawn JPEG bytes: ");
  Serial.println(bytesRead);
}

// ================================================================
// Confirm item
// ================================================================
void confirmItem() {
  lastMessage = "Saving...";
  drawOverlay();

  String url = String(CAMERA_BASE_URL) + "/confirm";

  HTTPClient http;
  http.setTimeout(7000);
  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  JsonDocument doc;
  doc["id"] = currentId;
  doc["name"] = currentName;
  doc["expiry_date"] = currentExpiry;
  doc["owner"] = currentOwner;

  String payload;
  serializeJson(doc, payload);

  int code = http.POST(payload);
  String response = http.getString();

  Serial.println();
  Serial.print("Confirm HTTP code: ");
  Serial.println(code);
  Serial.print("Confirm response: ");
  Serial.println(response);

  if (code >= 200 && code < 300) {
    lastMessage = "Saved!";
  } else {
    lastMessage = "Save failed " + String(code);
  }

  http.end();
  drawOverlay();
}

// ================================================================
// Touch handling
// ================================================================
void handleTouch() {
  if (millis() - lastTouchMs < 350) {
    return;
  }

  uint16_t x = 0;
  uint16_t y = 0;

  bool touched = tft.getTouch(&x, &y, 600);

  if (!touched) {
    return;
  }

  Serial.print("Touch x=");
  Serial.print(x);
  Serial.print(" y=");
  Serial.println(y);

  if (isInsideSaveButton(x, y)) {
    lastTouchMs = millis();
    confirmItem();
  }
}

// ================================================================
// Setup
// ================================================================
void setup() {
  Serial.begin(115200);
  delay(800);

  Serial.println();
  Serial.println("CYD Tufts WiFi Camera Screen starting...");

  pinMode(TFT_BL_PIN, OUTPUT);
  digitalWrite(TFT_BL_PIN, HIGH);

  tft.init();
  tft.setRotation(1);
  tft.setTouch(calData);
  tft.fillScreen(TFT_BLACK);

  Serial.printf("TFT width=%d height=%d\n", tft.width(), tft.height());
  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(true);
  TJpgDec.setCallback(tftOutput);

  connectScreenWiFi();

  showStatusScreen("Connecting cam...");
  fetchStatus();
  delay(500);
}

// ================================================================
// Loop
// ================================================================
void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    showStatusScreen("WiFi lost");
    delay(2000);
    ESP.restart();
  }

  handleTouch();

  uint32_t now = millis();

  if (now - lastStatusMs >= STATUS_INTERVAL_MS) {
    fetchStatus();
    lastStatusMs = now;
  }

  if (now - lastFrameMs >= FRAME_INTERVAL_MS) {
    fetchAndDrawJpg();
    lastFrameMs = now;
  }
}