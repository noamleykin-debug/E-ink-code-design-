#include "webportal.h"
#include "config.h"
#include "storage.h"
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <DNSServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

namespace WebPortal {

static AsyncWebServer server(WEB_PORT);
static DNSServer dnsServer;
static uint32_t s_last_activity_ms = 0;
static bool s_finished = false;

static void updateActivity() {
    s_last_activity_ms = millis();
}

// Chunked file upload handler
static void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
    updateActivity();
    
    String path = String(FS_IMAGE_DIR) + "/" + filename;
    
    if (index == 0) {
        log_i("Upload Start: %s", path.c_str());
        // Create or truncate the file
        File file = LittleFS.open(path, "w");
        if (!file) {
            log_e("Failed to open file for writing: %s", path.c_str());
            return;
        }
        file.close();
    }

    if (len > 0) {
        // Append chunk to the file
        // Yielding occurs naturally in AsyncWebServer between chunks, preventing WDT resets
        File file = LittleFS.open(path, "a");
        if (file) {
            file.write(data, len);
            file.close();
        } else {
            log_e("Failed to open file for appending");
        }
    }

    if (final) {
        log_i("Upload Complete: %s, size: %u bytes", path.c_str(), index + len);
        // Register the completed image payload into the playlist.json
        Storage::addImage(path);
    }
}

void init() {
    log_i("Initializing Web Portal SoftAP");
    
    WiFi.mode(WIFI_AP);
    IPAddress ip;
    ip.fromString(CAPTIVE_PORTAL_IP);
    IPAddress gateway;
    gateway.fromString(CAPTIVE_PORTAL_IP);
    IPAddress subnet(255, 255, 255, 0);
    
    WiFi.softAPConfig(ip, gateway, subnet);
    WiFi.softAP(AP_SSID, AP_PASSWORD, AP_CHANNEL, 0, AP_MAX_CONN);
    
    // DNS wildcard trap to force captive portal sheets open
    dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
    dnsServer.start(DNS_PORT, "*", ip);
    
    // Core OS captive portal probes
    server.on("/generate_204", HTTP_GET, [](AsyncWebServerRequest *request) {
        updateActivity();
        request->send(204);
    });
    server.on("/hotspot-detect.html", HTTP_GET, [](AsyncWebServerRequest *request) {
        updateActivity();
        request->redirect("/");
    });
    server.on("/connecttest.txt", HTTP_GET, [](AsyncWebServerRequest *request) {
        updateActivity();
        request->send(200, "text/plain", "Microsoft Connect Test");
    });
    
    // Serve stored photos so the Manage tab can render thumbnails. Registered
    // before the catch-all "/" handler so /img/* resolves to the gallery dir.
    server.serveStatic("/img", LittleFS, FS_IMAGE_DIR);

    // Serve frontend from LittleFS
    server.serveStatic("/", LittleFS, FS_WEB_DIR)
          .setDefaultFile("index.html");

    // Upload endpoint definition
    server.on("/api/upload", HTTP_POST, [](AsyncWebServerRequest *request) {
        updateActivity();
        request->send(200, "text/plain", "Upload Successful");
    }, handleUpload);

    // Optional "Show newest" action from the UI. Point the cursor at the freshly
    // uploaded photo and reboot into the image path. Rebooting is the only clean
    // hand-off: Wi-Fi and the decode/dither/refresh pipeline must never run in the
    // same wake (PSRAM/heap contention), so we end this wake entirely. Uploads on
    // their own never trigger this — it only fires when the user taps the button.
    server.on("/api/done", HTTP_GET, [](AsyncWebServerRequest *request) {
        updateActivity();
        log_i("Web Portal: Show-newest requested. Rebooting into image path.");
        Storage::jumpToLast();
        request->send(200, "text/plain", "OK");
        delay(300); // let the HTTP response flush before the reboot drops the link
        ESP.restart();
    });

    // Manage tab: list stored photos as a JSON array for the thumbnail grid.
    server.on("/api/list", HTTP_GET, [](AsyncWebServerRequest *request) {
        updateActivity();
        JsonDocument doc;
        JsonArray arr = doc.to<JsonArray>();
        for (const String& p : Storage::getPlaylist()) {
            arr.add(p);
        }
        String out;
        serializeJson(doc, out);
        request->send(200, "application/json", out);
    });

    // Manage tab: delete one photo (file + playlist entry). The frontend sends
    // one request per selected photo. Confine deletes to the image dir and reject
    // path traversal so a stray request can't reach anything else on the FS.
    server.on("/api/delete", HTTP_POST, [](AsyncWebServerRequest *request) {
        updateActivity();
        if (!request->hasParam("file")) {
            request->send(400, "text/plain", "missing file");
            return;
        }
        String f = request->getParam("file")->value();
        String prefix = String(FS_IMAGE_DIR) + "/";
        if (!f.startsWith(prefix) || f.indexOf("..") >= 0) {
            request->send(400, "text/plain", "bad path");
            return;
        }
        bool ok = Storage::deleteImage(f);
        log_i("Web Portal: delete %s -> %s", f.c_str(), ok ? "ok" : "fail");
        request->send(ok ? 200 : 500, "text/plain", ok ? "deleted" : "error");
    });
    
    // Fallback trap
    server.onNotFound([](AsyncWebServerRequest *request) {
        updateActivity();
        request->redirect("/");
    });

    server.begin();
    
    updateActivity();
    s_finished = false;
    log_i("Web Portal initialized. IP: %s", WiFi.softAPIP().toString().c_str());
}

void loop() {
    if (s_finished) return;
    
    // The AsyncWebServer runs on its own FreeRTOS thread via AsyncTCP,
    // but the DNSServer must be pumped manually.
    dnsServer.processNextRequest();
    
    // Inactivity Watchdog Evaluation
    if (millis() - s_last_activity_ms > WIFI_WATCHDOG_MS) {
        log_i("Web Portal Watchdog: Inactivity timeout. Shutting down.");
        s_finished = true;
        
        dnsServer.stop();
        server.end();
        WiFi.mode(WIFI_OFF);
    }
}

bool isFinished() {
    return s_finished;
}

} // namespace WebPortal
