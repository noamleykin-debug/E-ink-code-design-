# Prompt — Web / Captive Portal module

Copy the block below into a fresh chat connected to this repo.

---

```
You are an Expert Embedded C++ Firmware Engineer working on the ESP32-S3 E-Ink
photo frame in this repository.

FIRST read and follow: CLAUDE.md, docs/ROADMAP.md, docs/WORKFLOW.md,
docs/plans/webportal.md.

TASK: Implement/refine the Web/Captive Portal module — SoftAP captive portal that
serves the uploader UI, accepts chunked JPEG uploads to LittleFS, updates the
playlist, and self-terminates after the inactivity watchdog.

BRANCH: `plan/webportal` (cut from main). Never commit to main.

SCOPE (only these files):
- src/webportal.cpp
- include/webportal.h
- docs/plans/webportal.md (only if the interface changes)
- (web frontend assets if you add them: place under data/www/, not src/)

KEY REQUIREMENTS:
- ESPAsyncWebServer = mathieucarbou fork ONLY. Never WebServer.h.
- SoftAP + wildcard DNS to 192.168.4.1. Include OS-detection routes:
  /generate_204, /hotspot-detect.html, /connecttest.txt, plus onNotFound redirect.
- Uploads must be ASYNC/CHUNKED (request,filename,index,data,len,final) to avoid
  watchdog resets; register finished files via storage_add_image().
- 3-minute inactivity watchdog using millis() (allowed here — Wi-Fi keeps CPU awake).
- Never trigger decode/refresh from a request handler. This path runs ALONE.

DEFINITION OF DONE: `pio run` green; interface matches docs/plans/webportal.md;
draft PR into main describing verification (phone captive sheet opens; 60KB JPEG
upload succeeds with no WDT reset; idle 3 min -> exit).

Stop and ask if anything conflicts with the Golden Rules or hardware design.
```
