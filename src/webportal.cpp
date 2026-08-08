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
static volatile uint32_t s_last_activity_ms = 0;
static uint32_t s_session_start_ms = 0;
static volatile bool s_finished = false;

// Deferred-reboot machinery. HTTP handlers run on the AsyncTCP task, so they
// must never block or restart the chip themselves (the old code delay()ed and
// rebooted mid-connection, which dropped the AP with requests still in
// flight). Handlers just arm this flag; loop() — on the main task — performs
// an orderly teardown after the grace period so the response reaches the
// phone and connected clients get a clean deauth instead of a vanished AP.
static volatile bool s_reboot_pending = false;
static volatile uint32_t s_reboot_at_ms = 0;

static void scheduleReboot() {
    s_reboot_at_ms = millis() + PORTAL_REBOOT_GRACE_MS;
    s_reboot_pending = true;
}

// Orderly portal teardown: stop answering, close the server, deauth clients,
// then drop the radio.
static void shutdownPortal() {
    dnsServer.stop();
    server.end();
    WiFi.softAPdisconnect(true);   // sends deauth so phones drop off cleanly
    WiFi.mode(WIFI_OFF);
}

static void updateActivity() {
    s_last_activity_ms = millis();
}

// Reduce a client-supplied upload filename to a safe basename. Strips any
// directory components so a crafted name (e.g. "../../playlist.json") cannot
// escape FS_IMAGE_DIR. Returns "" if nothing usable remains.
static String sanitizeFilename(const String& raw) {
    String name = raw;
    int slash = name.lastIndexOf('/');
    if (slash >= 0) name = name.substring(slash + 1);
    slash = name.lastIndexOf('\\');
    if (slash >= 0) name = name.substring(slash + 1);
    if (name.isEmpty() || name == "." || name == "..") return "";
    return name;
}

// Chunked file upload handler
static void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
    updateActivity();

    String safeName = sanitizeFilename(filename);
    if (safeName.isEmpty()) {
        log_e("Upload rejected: unusable filename '%s'", filename.c_str());
        return;
    }
    String path = String(FS_IMAGE_DIR) + "/" + safeName;
    
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
    
    // Core OS captive portal probes.
    //
    // Deliberately NOT counted as activity: phone OSes re-fire these probes
    // in the background for as long as they stay associated with the AP. A
    // phone parked next to the frame overnight would reset the watchdog
    // forever, keeping the portal awake and, since deepSleep() is never
    // reached, silently disabling the slideshow timer. Only deliberate app
    // traffic (the /api/* routes, including the page's ping heartbeat)
    // counts as activity.
    server.on("/generate_204", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(204);
    });
    server.on("/hotspot-detect.html", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->redirect("/");
    });
    server.on("/connecttest.txt", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/plain", "Microsoft Connect Test");
    });
    
    // Keep-alive heartbeat. Static file requests (the page itself, the photo
    // thumbnails) never touch the API handlers, so without this the inactivity
    // watchdog fires while the user is actively browsing photos. The frontend
    // pings every 30s while the page is open.
    server.on("/api/ping", HTTP_GET, [](AsyncWebServerRequest *request) {
        updateActivity();
        request->send(204);
    });

    // Serve stored photos so the Manage tab can render thumbnails. Registered
    // before the catch-all "/" handler so /img/* resolves to the gallery dir.
    // Filenames are unique per upload (img_<uid>.jpg), so aggressive caching is
    // safe and spares the slow LittleFS reads on every Manage-tab open.
    server.serveStatic("/img", LittleFS, FS_IMAGE_DIR)
          .setCacheControl("public, max-age=86400");

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
        log_i("Web Portal: Show-newest requested. Scheduling reboot into image path.");
        Storage::jumpToLast();
        request->send(200, "text/plain", "OK");
        scheduleReboot();
    });

    server.on("/api/show", HTTP_POST, [](AsyncWebServerRequest *request) {
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
        log_i("Web Portal: Show specific photo requested (%s). Scheduling reboot.", f.c_str());
        Storage::jumpTo(f);
        request->send(200, "text/plain", "OK");
        scheduleReboot();
    });

    // Manage tab: playlist (in display order) + the cursor, i.e. which entry
    // shows on the next wake. Shape: {"images":[...], "cursor":N}
    server.on("/api/list", HTTP_GET, [](AsyncWebServerRequest *request) {
        updateActivity();
        JsonDocument doc;
        JsonArray arr = doc["images"].to<JsonArray>();
        for (const String& p : Storage::getPlaylist()) {
            arr.add(p);
        }
        doc["cursor"] = Storage::getCursor();
        String out;
        serializeJson(doc, out);
        request->send(200, "application/json", out);
    });

    // Manage tab: replace the playlist order. Body is a JSON array of image
    // paths in the desired order. Entries are validated against the current
    // playlist inside Storage::reorder(); photos uploaded after the client
    // fetched the list are kept (appended), unknown paths are dropped.
    server.on("/api/reorder", HTTP_POST,
        [](AsyncWebServerRequest *request) {
            updateActivity();
            const char* body = (const char*)request->_tempObject;
            if (!body) {
                request->send(400, "text/plain", "missing body");
                return;
            }
            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, body);
            if (err || !doc.is<JsonArray>()) {
                request->send(400, "text/plain", "bad json");
                return;
            }
            std::vector<String> order;
            for (JsonVariant v : doc.as<JsonArray>()) {
                order.push_back(v.as<String>());
            }
            bool ok = Storage::reorder(order);
            log_i("Web Portal: reorder (%u entries) -> %s", (unsigned)order.size(), ok ? "ok" : "fail");
            request->send(ok ? 200 : 500, "text/plain", ok ? "ok" : "error");
        },
        nullptr,
        // Body accumulator. _tempObject is free()d by AsyncWebServerRequest's
        // destructor, so a malloc'd buffer never leaks even on aborted requests.
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            updateActivity();
            if (total == 0 || total > 16384) return;   // sanity cap ~500 photos
            if (index == 0 && !request->_tempObject) {
                request->_tempObject = calloc(1, total + 1);
            }
            if (request->_tempObject && index + len <= total) {
                memcpy((uint8_t*)request->_tempObject + index, data, len);
            }
        });

    // Settings: GET returns the current values, POST (query params) updates
    // them. Interval is clamped to [SLIDESHOW_MIN_SEC, SLIDESHOW_MAX_SEC] by
    // Storage::setSettings, so a hostile/buggy client can't set a panel-
    // damaging refresh rate.
    server.on("/api/settings", HTTP_GET, [](AsyncWebServerRequest *request) {
        updateActivity();
        Storage::Settings s = Storage::getSettings();
        JsonDocument doc;
        doc["slideshow"] = s.slideshowEnabled;
        doc["interval_sec"] = s.slideshowIntervalSec;
        doc["interval_min_sec"] = (uint32_t)SLIDESHOW_MIN_SEC;
        doc["dither"] = s.ditherMode;
        String out;
        serializeJson(doc, out);
        request->send(200, "application/json", out);
    });

    server.on("/api/settings", HTTP_POST, [](AsyncWebServerRequest *request) {
        updateActivity();
        Storage::Settings s = Storage::getSettings();
        if (request->hasParam("slideshow")) {
            s.slideshowEnabled = request->getParam("slideshow")->value().toInt() != 0;
        }
        if (request->hasParam("interval_sec")) {
            long v = request->getParam("interval_sec")->value().toInt();
            if (v > 0) s.slideshowIntervalSec = (uint32_t)v;
        }
        if (request->hasParam("dither")) {
            long v = request->getParam("dither")->value().toInt();
            if (v >= 0 && v <= DITHER_MODE_MAX) s.ditherMode = (uint8_t)v;
        }
        bool ok = Storage::setSettings(s);
        log_i("Web Portal: settings slideshow=%d interval=%u dither=%u -> %s",
              (int)s.slideshowEnabled, (unsigned)s.slideshowIntervalSec,
              (unsigned)s.ditherMode, ok ? "ok" : "fail");
        request->send(ok ? 200 : 500, "text/plain", ok ? "ok" : "error");
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
    
    // Fallback trap. Also not activity: wildcard DNS funnels every stray
    // background request from any associated device here.
    server.onNotFound([](AsyncWebServerRequest *request) {
        request->redirect("/");
    });

    server.begin();
    
    updateActivity();
    s_session_start_ms = millis();
    s_finished = false;
    s_reboot_pending = false;
    log_i("Web Portal initialized. IP: %s", WiFi.softAPIP().toString().c_str());
}

void loop() {
    if (s_finished) return;

    // Deferred reboot armed by /api/done or /api/show. Runs here on the main
    // task, after the grace period let the HTTP response reach the phone.
    if (s_reboot_pending && (int32_t)(millis() - s_reboot_at_ms) >= 0) {
        log_i("Web Portal: Grace period over. Restarting into image path.");
        shutdownPortal();
        delay(100);       // let the deauth frames leave the radio
        ESP.restart();
    }

    // The AsyncWebServer runs on its own FreeRTOS thread via AsyncTCP,
    // but the DNSServer must be pumped manually.
    dnsServer.processNextRequest();

    // Inactivity Watchdog Evaluation
    uint32_t now = millis();
    if (now >= s_last_activity_ms && (now - s_last_activity_ms > WIFI_WATCHDOG_MS)) {
        log_i("Web Portal Watchdog: Inactivity timeout. Shutting down.");
        s_finished = true;
        shutdownPortal();
        return;
    }

    // Hard session cap: no amount of traffic may keep the portal open past
    // this ceiling. This is the guarantee that the frame always gets back to
    // deep sleep and re-arms its slideshow / 24h-refresh timer.
    if (now - s_session_start_ms > PORTAL_MAX_SESSION_MS) {
        log_i("Web Portal: Session cap reached. Shutting down.");
        s_finished = true;
        shutdownPortal();
    }
}

bool isFinished() {
    return s_finished;
}

} // namespace WebPortal
