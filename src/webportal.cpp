// ============================================================================
//  webportal.cpp — see include/webportal.h and docs/plans/webportal.md
//
//  ESPAsyncWebServer = mathieucarbou fork ONLY (never WebServer.h).
//  Uploads are async/chunked to avoid watchdog resets.
// ============================================================================
#include "webportal.h"
#include "config.h"
#include "storage.h"

#include <WiFi.h>
#include <DNSServer.h>
#include <LittleFS.h>
#include <ESPAsyncWebServer.h>

static AsyncWebServer s_server(WEB_PORT);
static DNSServer      s_dns;
static uint32_t       s_last_activity_ms = 0;
static uint32_t       s_upload_counter   = 0;

static void touch_activity(void) { s_last_activity_ms = millis(); }

// Minimal fallback uploader page if /www/index.html is absent.
static const char FALLBACK_HTML[] PROGMEM =
  "<!doctype html><meta name=viewport content='width=device-width,initial-scale=1'>"
  "<h2>E-Ink Frame</h2>"
  "<form method=POST action=/upload enctype=multipart/form-data>"
  "<input type=file name=img accept=image/jpeg><button>Upload</button></form>";

// ---- Captive-portal OS detection + wildcard redirect -----------------------
static void redirect_to_portal(AsyncWebServerRequest* req) {
  touch_activity();
  req->redirect(String("http://") + CAPTIVE_PORTAL_IP + "/");
}

// ---- Chunked upload handler ------------------------------------------------
static File s_upload_file;
static String s_upload_path;

static void handle_upload(AsyncWebServerRequest* req, const String& filename,
                          size_t index, uint8_t* data, size_t len, bool final) {
  touch_activity();
  if (index == 0) {
    s_upload_path = String(FS_IMAGE_DIR) + "/" +
                    String(s_upload_counter++) + ".jpg";
    s_upload_file = LittleFS.open(s_upload_path, "w");
  }
  if (s_upload_file) {
    if (len) s_upload_file.write(data, len);
  }
  if (final) {
    if (s_upload_file) s_upload_file.close();
    storage_add_image(s_upload_path);
  }
}

void portal_start(void) {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, (strlen(AP_PASSWORD) ? AP_PASSWORD : nullptr),
              AP_CHANNEL, 0, AP_MAX_CONN);

  // Wildcard DNS: every lookup -> the portal IP (192.168.4.1).
  s_dns.start(DNS_PORT, "*", WiFi.softAPIP());

  // Frontend
  s_server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
    touch_activity();
    if (LittleFS.exists(String(FS_WEB_DIR) + "/index.html")) {
      req->send(LittleFS, String(FS_WEB_DIR) + "/index.html", "text/html");
    } else {
      req->send_P(200, "text/html", FALLBACK_HTML);
    }
  });

  // Async chunked upload
  s_server.on("/upload", HTTP_POST,
    [](AsyncWebServerRequest* req) { req->send(200, "text/plain", "OK"); },
    handle_upload);

  // Playlist API (read)
  s_server.on("/api/playlist", HTTP_GET, [](AsyncWebServerRequest* req) {
    touch_activity();
    req->send(LittleFS, FS_PLAYLIST_PATH, "application/json");
  });

  // OS captive-portal probes
  s_server.on("/generate_204",        HTTP_GET, redirect_to_portal); // Android
  s_server.on("/hotspot-detect.html", HTTP_GET, redirect_to_portal); // iOS/macOS
  s_server.on("/connecttest.txt",     HTTP_GET, redirect_to_portal); // Windows
  s_server.onNotFound(redirect_to_portal);

  s_server.begin();
  touch_activity();
}

void portal_loop(void) {
  s_dns.processNextRequest();
}

bool portal_should_exit(void) {
  // millis() is fine: Wi-Fi keeps the CPU awake for the whole session.
  return (millis() - s_last_activity_ms) > WIFI_WATCHDOG_MS;
}

void portal_stop(void) {
  s_server.end();
  s_dns.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
}
