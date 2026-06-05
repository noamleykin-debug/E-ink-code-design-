#include "webportal.h"
#include "config.h"
#include "storage.h"
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <DNSServer.h>
#include <LittleFS.h>

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
    
    // Serve frontend from LittleFS
    server.serveStatic("/", LittleFS, FS_WEB_DIR)
          .setDefaultFile("index.html");

    // Upload endpoint definition
    server.on("/api/upload", HTTP_POST, [](AsyncWebServerRequest *request) {
        updateActivity();
        request->send(200, "text/plain", "Upload Successful");
    }, handleUpload);
    
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
