# PLAN — Web / Captive Portal module (`webportal.{h,cpp}`)

Branch: `plan/webportal` · Depends on: `config.h`, `storage`,
mathieucarbou/ESPAsyncWebServer + AsyncTCP, DNSServer · See `docs/ROADMAP.md`.

## Responsibility
The WIFI-tap path. Bring up a SoftAP captive portal that:
- Serves the phone-side HTML/JS uploader frontend.
- Accepts chunked JPEG uploads into LittleFS and updates the playlist.
- Satisfies OS captive-portal detection so the page auto-pops.
- Self-terminates after the watchdog window and signals `main` to sleep.

> Runs **alone** — never while decode/refresh is active (heap/PSRAM contention).

## Public interface (proposed)
```c
void portal_start(void);        // WiFi.softAP + DNS + routes; resets watchdog
void portal_loop(void);         // dnsServer.processNextRequest(); check watchdog
bool portal_should_exit(void);  // true after WIFI_WATCHDOG_MS of inactivity
void portal_stop(void);         // server.end(), WiFi.softAPdisconnect(true)
```

## SoftAP + DNS
- `WiFi.mode(WIFI_AP); WiFi.softAP(AP_SSID, AP_PASSWORD, AP_CHANNEL, 0, AP_MAX_CONN);`
- `dnsServer.start(DNS_PORT, "*", IPAddress(192,168,4,1));`  // wildcard → portal IP
- Default SoftAP gateway is `CAPTIVE_PORTAL_IP` (192.168.4.1).

## Routes
| Route | Purpose |
|---|---|
| `GET /`                    | serve uploader UI (`FS_WEB_DIR/index.html`) |
| `POST /upload`             | chunked JPEG upload → `FS_IMAGE_DIR`, then `storage_add_image` |
| `GET /api/playlist`        | return playlist JSON |
| `POST /api/playlist`       | reorder / delete (calls storage) |
| `GET /generate_204`        | Android probe → redirect/204 to trigger portal |
| `GET /hotspot-detect.html` | iOS/macOS probe |
| `GET /connecttest.txt`     | Windows probe |
| `onNotFound` (wildcard)    | redirect to `http://192.168.4.1/` |

## Chunked upload handler
Use the AsyncWebServer upload signature
`(request, filename, index, data, len, final)`:
- `index==0`: open the target file (`/img/NNNN.jpg`) for write.
- each call: append `data,len`.
- `final`: close file, register with `storage_add_image`, respond.
- Reset the watchdog timer on every chunk.

## Correctness traps this module OWNS
- [ ] **mathieucarbou fork only** — never `WebServer.h`.
- [ ] **Async chunked** uploads (no buffering the whole file) to avoid WDT resets.
- [ ] **All three OS-detection routes** + wildcard DNS, or the portal won't auto-open.
- [ ] **3-min watchdog** — `millis()` is allowed here (Wi-Fi keeps CPU awake).
      Reset on any client activity; when it expires, `portal_should_exit()`.
- [ ] Never trigger decode/refresh from inside a request handler.
- [ ] `storage` uses `LittleFS.begin(false)`; portal must surface mount failure,
      not reformat.

## Implementation checklist
1. `portal_start`: SoftAP, DNS wildcard, register all routes + upload handler,
   `server.begin()`, seed `lastActivityMs = millis()`.
2. Static file serving from `FS_WEB_DIR` (fallback to an embedded minimal page).
3. Upload handler as above; bump `lastActivityMs` each chunk.
4. `portal_loop`: `dnsServer.processNextRequest()`; nothing blocking.
5. `portal_should_exit`: `millis() - lastActivityMs > WIFI_WATCHDOG_MS`.
6. `portal_stop`: end server, stop DNS, `softAPdisconnect(true)`.

## Test notes
- Phone connects → captive sheet auto-opens on Android/iOS/Windows.
- Upload a 60 KB JPEG → lands in `/img`, appears in playlist, no WDT reset.
- Idle 3 min → `portal_should_exit()` true; `main` then sleeps.
