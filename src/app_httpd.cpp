#include <Arduino.h>
#include "esp_camera.h"
#include <WebServer.h>

WebServer server(80);

extern volatile bool scanBusy;
extern uint8_t* frozenBuf;
extern size_t frozenLen;
extern volatile uint32_t currentScanId;

void setQrDecodeResult(uint32_t scanId, const String& status, const String& payload);

static const char INDEX_HTML[] = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>ESP32-S3 Camera</title>
  <style>
    body { font-family: Arial; text-align: center; background:#111; color:#fff; }
    img  { max-width: 90vw; height: auto; border: 2px solid #555; border-radius: 8px; margin-top: 12px; transform: scaleX(-1)}
    #status { margin-top: 12px; color: #bbb; }
    canvas { display: none; }
  </style>
  <script src="https://cdn.jsdelivr.net/npm/jsqr@1.4.0/dist/jsQR.js"></script>
</head>
<body>
  <h2>ESP32-S3 Camera Preview</h2>
  <p>Press the physical button to freeze and scan the current view.</p>
  <div id="status">Live preview</div>
  <img id="cam" alt="camera frame">
  <canvas id="qrCanvas"></canvas>

  <script>
    const cam = document.getElementById("cam");
    const statusText = document.getElementById("status");
    const qrCanvas = document.getElementById("qrCanvas");
    const qrCtx = qrCanvas.getContext("2d", { willReadFrequently: true });

    let timer = null;
    let decodeAttemptedForScanId = -1;

    function scheduleNext(ms) {
      if (timer) clearTimeout(timer);
      timer = setTimeout(() => {
        cam.src = "/jpg?t=" + Date.now();
      }, ms);
    }

    async function getScanState() {
      const res = await fetch("/scan_state?t=" + Date.now(), { cache: "no-store" });
      return await res.json();
    }

    async function reportQr(scanId, status, payload) {
      const url =
        "/report_qr?scan_id=" + encodeURIComponent(scanId) +
        "&status=" + encodeURIComponent(status) +
        "&payload=" + encodeURIComponent(payload || "") +
        "&t=" + Date.now();

      await fetch(url, { cache: "no-store" });
    }

    async function decodeWithBarcodeDetector(scanId) {
      if (!("BarcodeDetector" in window)) return false;

      try {
        const detector = new BarcodeDetector({ formats: ["qr_code"] });
        const bitmap = await createImageBitmap(cam);
        const results = await detector.detect(bitmap);
        if (bitmap.close) bitmap.close();

        if (results && results.length > 0 && results[0].rawValue) {
          await reportQr(scanId, "ok", results[0].rawValue);
        } else {
          await reportQr(scanId, "not_found", "");
        }
        return true;
      } catch (err) {
        return false;
      }
    }

    async function decodeWithJsQR(scanId) {
      try {
        qrCanvas.width = cam.naturalWidth || cam.width;
        qrCanvas.height = cam.naturalHeight || cam.height;

        if (!qrCanvas.width || !qrCanvas.height) {
          await reportQr(scanId, "decode_error", "image_not_ready");
          return;
        }

        qrCtx.drawImage(cam, 0, 0, qrCanvas.width, qrCanvas.height);
        const imageData = qrCtx.getImageData(0, 0, qrCanvas.width, qrCanvas.height);

        const code = jsQR(imageData.data, imageData.width, imageData.height, {
          inversionAttempts: "dontInvert"
        });

        if (code && code.data) {
          await reportQr(scanId, "ok", code.data);
        } else {
          await reportQr(scanId, "not_found", "");
        }
      } catch (err) {
        await reportQr(scanId, "decode_error", String(err));
      }
    }

    async function decodeCurrentImage(scanId) {
      const usedNative = await decodeWithBarcodeDetector(scanId);
      if (!usedNative) {
        await decodeWithJsQR(scanId);
      }
    }

    cam.onload = async function() {
      try {
        const state = await getScanState();

        if (state.busy) {
          statusText.textContent = "Frozen frame - scanning...";
          if (decodeAttemptedForScanId !== state.scan_id) {
            decodeAttemptedForScanId = state.scan_id;
            await decodeCurrentImage(state.scan_id);
          }
          scheduleNext(10);
        } else {
          statusText.textContent = "Live preview";
          decodeAttemptedForScanId = -1;
          scheduleNext(10);
        }
      } catch (e) {
        statusText.textContent = "State check failed";
        scheduleNext(10);
      }
    };

    cam.onerror = function() {
      statusText.textContent = "Reconnecting...";
      scheduleNext(10);
    };

    window.onload = function() {
      cam.src = "/jpg?t=" + Date.now();
    };
  </script>
</body>
</html>
)rawliteral";

static void sendJpegBytes(WiFiClient& client, const uint8_t* buf, size_t len) {
  client.printf(
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: image/jpeg\r\n"
    "Content-Length: %u\r\n"
    "Cache-Control: no-cache, no-store, must-revalidate\r\n"
    "Connection: close\r\n\r\n",
    (unsigned)len
  );
  client.write(buf, len);
  client.stop();
}

void handleRoot() {
  server.send(200, "text/html", INDEX_HTML);
}

void handleJpg() {
  WiFiClient client = server.client();

  if (scanBusy && frozenBuf && frozenLen > 0) {
    sendJpegBytes(client, frozenBuf, frozenLen);
    return;
  }

  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    client.print(
      "HTTP/1.1 500 Internal Server Error\r\n"
      "Content-Type: text/plain\r\n"
      "Connection: close\r\n\r\n"
      "Camera capture failed"
    );
    client.stop();
    return;
  }

  client.printf(
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: image/jpeg\r\n"
    "Content-Length: %u\r\n"
    "Cache-Control: no-cache, no-store, must-revalidate\r\n"
    "Connection: close\r\n\r\n",
    (unsigned)fb->len
  );

  client.write(fb->buf, fb->len);
  esp_camera_fb_return(fb);
  client.stop();
}

void handleScanState() {
  String json = "{";
  json += "\"busy\":";
  json += (scanBusy ? "true" : "false");
  json += ",";
  json += "\"scan_id\":";
  json += String((unsigned long)currentScanId);
  json += "}";

  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.send(200, "application/json", json);
}

void handleReportQr() {
  if (!server.hasArg("scan_id") || !server.hasArg("status")) {
    server.send(400, "text/plain", "Missing scan_id or status");
    return;
  }

  uint32_t scanId = (uint32_t) server.arg("scan_id").toInt();
  String status = server.arg("status");
  String payload = server.hasArg("payload") ? server.arg("payload") : "";

  setQrDecodeResult(scanId, status, payload);
  server.send(200, "text/plain", "OK");
}

void handleNotFound() {
  server.send(404, "text/plain", "Not found");
}

void startCameraServer() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/jpg", HTTP_GET, handleJpg);
  server.on("/scan_state", HTTP_GET, handleScanState);
  server.on("/report_qr", HTTP_GET, handleReportQr);
  server.onNotFound(handleNotFound);
  server.begin();
}

void handleCameraServerClient() {
  server.handleClient();
}