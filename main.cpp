#include <Arduino.h>
#include <ArduinoJson.h>   
#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <mbedtls/base64.h>
#include <stdlib.h>
#include <string.h>
#include "board_config.h"
#include "secrets.h"   // create this from secrets.h.example
#include "ESP32_OV5640_AF.h"
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

const int LCD_SDA_PIN = 1;
const int LCD_SCL_PIN = 2;

LiquidCrystal_I2C lcd(0x27, 16, 2);

static String clampTo16(String s);

static String padTo16(String s) {
  s = clampTo16(s);
  while (s.length() < 16) s += ' ';
  return s;
}

static void lcdShowTwoLines(const String& line1, const String& line2) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(padTo16(line1));
  lcd.setCursor(0, 1);
  lcd.print(padTo16(line2));
}
// ---------- CHANGE THESE ----------
const char* ssid = YOUR_WIFI_SSID;
const char* eap_identity = "";              // often empty string; sometimes same as username
const char* eap_username = YOUR_WIFI_USERNAME;
const char* eap_password = YOUR_WIFI_PASSWORD;
// ----------------------------------

const int BUTTON_PIN = 14;
const unsigned long SCAN_TIMEOUT_MS = 8000;
const char* ANTHROPIC_URL = "https://api.anthropic.com/v1/messages";
const char* ANTHROPIC_MODEL = "claude-haiku-4-5";  // faster + cheaper fallback
const unsigned long AF_SETTLE_MS = 250;

OV5640 ov5640;
bool ov5640Detected = false;
bool ov5640AutofocusReady = false;

// Shared state
volatile bool scanRequested = false;
volatile bool scanBusy = false;
volatile uint32_t currentScanId = 0;

// Frozen frame storage
uint8_t* frozenBuf = nullptr;
size_t frozenLen = 0;

// QR result from browser
volatile bool qrResultReady = false;
String qrResultStatus = "";
String qrResultPayload = "";
unsigned long scanStartedMs = 0;

void startCameraServer();
void handleCameraServerClient();

static bool initOV5640Autofocus();
static void logOv5640AutofocusState(const char* prefix);
static void waitForAutofocusToSettle();

static String jsonEscape(const String& in) {
  String out;
  out.reserve(in.length() + 32);

  for (size_t i = 0; i < in.length(); ++i) {
    const char c = in[i];
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default: out += c; break;
    }
  }

  return out;
}

static String decodeJsonStringAt(const String& json, int firstCharIndex) {
  String out;
  bool escape = false;

  for (int i = firstCharIndex; i < (int)json.length(); ++i) {
    char c = json[i];

    if (escape) {
      switch (c) {
        case 'n': out += '\n'; break;
        case 'r': out += '\r'; break;
        case 't': out += '\t'; break;
        case '"': out += '"'; break;
        case '\\': out += '\\'; break;
        case '/': out += '/'; break;
        default: out += c; break;
      }
      escape = false;
      continue;
    }

    if (c == '\\') {
      escape = true;
      continue;
    }

    if (c == '"') {
      break;
    }

    out += c;
  }

  return out;
}

static String extractFirstAssistantText(const String& responseBody) {
  int contentPos = responseBody.indexOf("\"content\"");
  if (contentPos < 0) return "";

  int textKeyPos = responseBody.indexOf("\"text\"", contentPos);
  if (textKeyPos < 0) return "";

  int colonPos = responseBody.indexOf(':', textKeyPos);
  if (colonPos < 0) return "";

  int firstQuotePos = responseBody.indexOf('"', colonPos + 1);
  if (firstQuotePos < 0) return "";

  return decodeJsonStringAt(responseBody, firstQuotePos + 1);
}

static String normalizeOneLine(String s) {
  s.replace("\r", " ");
  s.replace("\n", " ");
  s.replace("\t", " ");

  while (s.indexOf("  ") >= 0) {
    s.replace("  ", " ");
  }

  s.trim();
  return s;
}

static String clampTo16(String s) {
  s = normalizeOneLine(s);
  if (s.length() > 16) {
    s = s.substring(0, 16);
  }
  return s;
}

static void splitClaudeLines(const String& rawText, String& line1, String& line2) {
  String text = rawText;
  text.replace("\r\n", "\n");
  text.replace('\r', '\n');

  int firstBreak = text.indexOf('\n');
  if (firstBreak < 0) {
    line1 = clampTo16(text);
    line2 = "No details";
    return;
  }

  String a = text.substring(0, firstBreak);
  String rest = text.substring(firstBreak + 1);

  while (rest.startsWith("\n")) {
    rest.remove(0, 1);
  }

  int secondBreak = rest.indexOf('\n');
  if (secondBreak >= 0) {
    rest = rest.substring(0, secondBreak);
  }

  line1 = clampTo16(a);
  line2 = clampTo16(rest);

  if (line1.length() == 0) line1 = "Unknown object";
  if (line2.length() == 0) line2 = "Try again";
}

static bool base64EncodeJpeg(const uint8_t* data, size_t dataLen, String& outBase64) {
  if (!data || dataLen == 0) return false;

  const size_t encodedCapacity = 4 * ((dataLen + 2) / 3) + 4;
  unsigned char* encoded = (unsigned char*)malloc(encodedCapacity);
  if (!encoded) {
    Serial.println("FAILED: malloc for base64 buffer");
    return false;
  }

  size_t actualLen = 0;
  int rc = mbedtls_base64_encode(encoded, encodedCapacity, &actualLen, data, dataLen);
  if (rc != 0) {
    free(encoded);
    Serial.printf("FAILED: base64 encode rc=%d\n", rc);
    return false;
  }

  encoded[actualLen] = '\0';
  outBase64 = String((char*)encoded);
  free(encoded);
  return true;
}

static bool describeFrozenImageWithClaude(String& line1, String& line2, String& debugMessage) {
  if (WiFi.status() != WL_CONNECTED) {
    debugMessage = "WiFi not connected";
    return false;
  }

  if (!frozenBuf || frozenLen == 0) {
    debugMessage = "No frozen image available";
    return false;
  }

  Serial.println("Preparing JPEG for Claude...");
  Serial.printf("Frozen JPEG bytes: %u\n", (unsigned)frozenLen);
  Serial.printf("Free heap before base64: %u\n", (unsigned)ESP.getFreeHeap());

  String base64Image;
  if (!base64EncodeJpeg(frozenBuf, frozenLen, base64Image)) {
    debugMessage = "Base64 encoding failed";
    return false;
  }

  Serial.printf("Base64 chars: %u\n", (unsigned)base64Image.length());
  Serial.printf("Free heap after base64: %u\n", (unsigned)ESP.getFreeHeap());

  const String prompt =
    // "You are helping an ESP32 with a 16x2 LCD. "
    // "Look at the attached image and return EXACTLY two lines and nothing else.\n"
    // "You are a structured data extraction system.\n"

    // "If blurry or uncertain, say:\n"
    // "Unclear image\n"
    // "Try again";
    "You are a structured data extraction system.\n"
    "Return ONLY valid JSON.\n"
    "No markdown.\n"
    "No explanation.\n"
    "Output format:\n"
    "{"
    "\"id\":\"FOOD001\","
    "\"name\":\"string\","
    "\"expiry_date\":\"YYYY-MM-DD\","
    "\"owner\":\"string\""
    "}\n"
    "If unclear return:\n"
    "{\"error\":\"invalid\"}";


  String body;
  body.reserve(base64Image.length() + prompt.length() + 768);
  body += "{\"model\":\"";
  body += ANTHROPIC_MODEL;
  body += "\",\"max_tokens\":80,\"messages\":[{\"role\":\"user\",\"content\":[";
  body += "{\"type\":\"image\",\"source\":{\"type\":\"base64\",\"media_type\":\"image/jpeg\",\"data\":\"";
  body += base64Image;
  body += "\"}},";
  body += "{\"type\":\"text\",\"text\":\"";
  body += jsonEscape(prompt);
  body += "\"}";
  body += "]}]}";

  Serial.printf("JSON body chars: %u\n", (unsigned)body.length());
  Serial.printf("Free heap before HTTPS POST: %u\n", (unsigned)ESP.getFreeHeap());

  WiFiClientSecure client;
  client.setInsecure();  // prototype only; later replace with proper certificate validation

  HTTPClient http;
  http.setConnectTimeout(15000);
  http.setTimeout(30000);

  if (!http.begin(client, ANTHROPIC_URL)) {
    debugMessage = "HTTP begin failed";
    return false;
  }

  http.addHeader("content-type", "application/json");
  http.addHeader("x-api-key", ANTHROPIC_API_KEY);
  http.addHeader("anthropic-version", "2023-06-01");

  Serial.println("Sending image to Claude...");
  const int httpCode = http.POST((uint8_t*)body.c_str(), body.length());
  const String responseBody = http.getString();
  http.end();

  Serial.printf("Claude HTTP status: %d\n", httpCode);

  if (httpCode != 200) {
    debugMessage = responseBody.length() ? responseBody : "Non-200 response";
    return false;
  }
  //connect to flask
  WiFiClientSecure flaskClient;
  flaskClient.setInsecure();

  // connect to flask
  WiFiClientSecure flaskClient;
  flaskClient.setInsecure();

  HTTPClient http;

  http.begin("https://team-black-1.onrender.com/add");
  http.addHeader("Content-Type", "application/json");

  String body =
    "{\"id\":\"" + id +
    "\",\"name\":\"" + name +
    "\",\"expiry\":\"" + expiry +
    "\",\"owner\":\"" + owner + "\"}";

  Serial.println("Sending JSON to Flask...");

  int code = http.POST(body);
  String res = http.getString();

  http.end();

  Serial.printf("Flask status: %d\n", code);
  Serial.println(res);

  
  // const String rawText = extractFirstAssistantText(responseBody);
  String json = extractFirstAssistantText(responseBody);
    json.trim();

    int start = json.indexOf('{');
    int end = json.lastIndexOf('}');

    if (start >= 0 && end >= 0) {
      json = json.substring(start, end + 1);
    }

    String id = extractField(json, "id");
    String name = extractField(json, "name");
    String expiry = extractField(json, "expiry_date");
    String owner = extractField(json, "owner");

    debugMessage = json;

    return true;
  }

  // ---------- JSON extractor (UNCHANGED) ----------
  static String extractFirstAssistantText(const String& responseBody) {
    int contentPos = responseBody.indexOf("\"content\"");
    if (contentPos < 0) return "";

    int textKeyPos = responseBody.indexOf("\"text\"", contentPos);
    if (textKeyPos < 0) return "";

    int colonPos = responseBody.indexOf(':', textKeyPos);
    if (colonPos < 0) return "";

    int firstQuotePos = responseBody.indexOf('"', colonPos + 1);
    if (firstQuotePos < 0) return "";

    String out;
    bool escape = false;

    for (int i = firstQuotePos + 1; i < (int)responseBody.length(); i++) {
      char c = responseBody[i];

      if (escape) {
        out += c;
        escape = false;
        continue;
      }

      if (c == '\\') {
        escape = true;
        continue;
      }

      if (c == '"') break;

      out += c;
    }

    return out;
  }




static bool initOV5640Autofocus() {
  sensor_t* s = esp_camera_sensor_get();
  if (!s) {
    Serial.println("AF: sensor handle not available");
    return false;
  }

  ov5640Detected = ov5640.start(s);
  if (!ov5640Detected) {
    Serial.println("AF: current sensor is not OV5640");
    return false;
  }

  const uint8_t initRc = ov5640.focusInit();
  if (initRc != 0) {
    Serial.printf("AF: focusInit failed (%u)\n", (unsigned)initRc);
    return false;
  }

  const uint8_t afRc = ov5640.autoFocusMode();
  if (afRc != 0) {
    Serial.printf("AF: autoFocusMode failed (%u)\n", (unsigned)afRc);
    return false;
  }

  ov5640AutofocusReady = true;
  logOv5640AutofocusState("AF: continuous autofocus enabled");
  return true;
}

static void logOv5640AutofocusState(const char* prefix) {
  if (!ov5640Detected) return;
  const uint8_t fw = ov5640.getFWStatus();
  Serial.print(prefix);
  Serial.print(" | FW_STATUS=0x");
  Serial.println(fw, HEX);
}

static void waitForAutofocusToSettle() {
  if (!ov5640AutofocusReady) return;

  delay(AF_SETTLE_MS);

  camera_fb_t* throwaway = esp_camera_fb_get();
  if (throwaway) {
    esp_camera_fb_return(throwaway);
  }

  logOv5640AutofocusState("AF: settled before capture");
}

bool initPreviewCamera() {
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

  if (psramFound()) {
    config.frame_size = FRAMESIZE_VGA;
    config.jpeg_quality = 8;
    config.fb_count = 2;
    Serial.println("PSRAM found");
  } else {
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
    Serial.println("PSRAM not found");
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x\n", err);
    return false;
  }

  sensor_t* s = esp_camera_sensor_get();
  if (s) {
    s->set_brightness(s, 1);
    s->set_contrast(s, 2);
    s->set_saturation(s, 0);
  }

  initOV5640Autofocus();
  return true;
}

bool saveFrozenFrame(camera_fb_t* fb) {
  if (!fb || !fb->buf || fb->len == 0) return false;

  uint8_t* newBuf = (uint8_t*)malloc(fb->len);
  if (!newBuf) {
    Serial.println("FAILED: malloc for frozen frame");
    return false;
  }

  memcpy(newBuf, fb->buf, fb->len);

  if (frozenBuf) {
    free(frozenBuf);
    frozenBuf = nullptr;
  }

  frozenBuf = newBuf;
  frozenLen = fb->len;
  return true;
}

void setQrDecodeResult(uint32_t scanId, const String& status, const String& payload) {
  if (!scanBusy) return;
  if (scanId != currentScanId) return;
  if (qrResultReady) return;

  qrResultStatus = status;
  qrResultPayload = payload;
  qrResultReady = true;
}

void beginScanFromFrozenFrame() {
  Serial.println("=== STARTING SCAN ===");
  waitForAutofocusToSettle();

  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("SCAN FAILED: camera capture failed");
    return;
  }

  if (!saveFrozenFrame(fb)) {
    Serial.println("SCAN FAILED: could not save frozen frame");
    esp_camera_fb_return(fb);
    return;
  }

  esp_camera_fb_return(fb);

  currentScanId++;
  qrResultReady = false;
  qrResultStatus = "";
  qrResultPayload = "";
  scanStartedMs = millis();

  Serial.printf("Frozen image captured: %u bytes\n", (unsigned)frozenLen);
  Serial.printf("SCAN_ID: %lu\n", (unsigned long)currentScanId);
  Serial.println("Waiting for browser QR decode...");
}

static void printClaudeResultToSerial(const String& reasonLabel) {
  String line1;
  String line2;
  String debugMessage;

  bool ok = describeFrozenImageWithClaude(line1, line2, debugMessage);
  if (ok) {
    Serial.println(reasonLabel);
    Serial.println("SCENE_DETECTED");
    Serial.print("LINE1: ");
    Serial.println(line1);
    Serial.print("LINE2: ");
    Serial.println(line2);
    Serial.print("RAW_CLAUDE_TEXT: ");
    Serial.println(debugMessage);
    lcdShowTwoLines(line1, line2);
  } else {
    Serial.println(reasonLabel);
    Serial.println("CLAUDE_FALLBACK_FAILED");
    Serial.print("DETAIL: ");
    Serial.println(debugMessage);
    lcdShowTwoLines("Scan failed", "Try again");
  }
}

void finalizeScanIfReady() {
  if (!scanBusy) return;

  if (qrResultReady) {
    if (qrResultStatus == "ok") {
      Serial.println("QR_DETECTED");
      Serial.print("PAYLOAD: ");
      Serial.println(qrResultPayload);
      lcdShowTwoLines("QR detected", clampTo16(qrResultPayload));
    } else if (qrResultStatus == "not_found") {
      Serial.println("NO_QR_DETECTED");
      printClaudeResultToSerial("CLAUDE_FALLBACK_AFTER_NO_QR");
    } else if (qrResultStatus == "unsupported_browser") {
      Serial.println("UNSUPPORTED_BROWSER_FOR_QR");
      printClaudeResultToSerial("CLAUDE_FALLBACK_AFTER_UNSUPPORTED_BROWSER");
    } else {
      Serial.println("QR_DECODE_ERROR");
      Serial.print("DETAIL: ");
      Serial.println(qrResultPayload);
      printClaudeResultToSerial("CLAUDE_FALLBACK_AFTER_QR_ERROR");
    }

    scanBusy = false;
    Serial.println("=== SCAN COMPLETE ===");
    Serial.println("READY FOR NEXT SCAN");
    return;
  }

  if (millis() - scanStartedMs > SCAN_TIMEOUT_MS) {
    Serial.println("SCAN_TIMEOUT");
    printClaudeResultToSerial("CLAUDE_FALLBACK_AFTER_TIMEOUT");
    scanBusy = false;
    Serial.println("=== SCAN COMPLETE ===");
    Serial.println("READY FOR NEXT SCAN");
  }
}

void setup() {
  Serial.begin(115200);
  Wire.begin(LCD_SDA_PIN, LCD_SCL_PIN, 100000);

  lcd.init();
  lcd.backlight();
  lcdShowTwoLines("Booting...", "Please wait");
  delay(1000);

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  Serial.println();
  Serial.println("Booting camera...");

  if (!initPreviewCamera()) {
    while (true) {
      delay(1000);
    }
  }

  if (ov5640AutofocusReady) {
    lcdShowTwoLines("OV5640 AF ready", "Open browser");
    delay(800);
  } else if (ov5640Detected) {
    lcdShowTwoLines("OV5640 no AF", "Check AF VCC");
    delay(1200);
  }

  Serial.print("Connecting to enterprise Wi-Fi");

  WiFi.disconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, WPA2_AUTH_PEAP, eap_identity, eap_username, eap_password);

  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 60) {
    delay(500);
    Serial.print(".");
    tries++;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println();
    Serial.println("Enterprise Wi-Fi connection failed");
    while (true) {
      delay(1000);
    }
  }

  Serial.println();
  Serial.println("Wi-Fi connected");
  Serial.print("Open this in your browser: http://");
  Serial.println(WiFi.localIP());

  startCameraServer();
  Serial.println("Camera server started");
}

void loop() {
  handleCameraServerClient();

  if (!scanBusy && !scanRequested && digitalRead(BUTTON_PIN) == LOW) {
    scanRequested = true;
    Serial.println("BUTTON PRESSED");
    delay(250);
  }

  if (scanRequested && !scanBusy) {
    scanRequested = false;
    scanBusy = true;
    Serial.println("SCAN REQUEST DETECTED");
    beginScanFromFrozenFrame();
  }

  finalizeScanIfReady();
  delay(2);
}
