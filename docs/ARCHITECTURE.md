# Architecture

How the firmware for the ESP32-S3 6-color E-Ink photo frame is put together.
The authoritative pinout, palette, ADC math, and timing constants live in
[`include/config.h`](../include/config.h); nothing here redefines them.

---

## 1. System overview

The frame is battery powered, so its normal state is deep sleep. A wake event
(capacitive tap via EXT1, or an RTC timer) runs exactly one job, then the
firmware returns to deep sleep. There is no long-running loop.

```
                 ┌──────────────── deep sleep ────────────────┐
                 │                                             │
   EXT1 wake (GPIO1 advance / GPIO2 wifi)  or  timer wake      │
   (slideshow interval / 24h mandatory refresh)                │
                 ▼                                             │
        ┌─────────────────┐   battery gate (read GPIO6 FIRST)  │
        │   main / FSM     │──────────────┐                    │
        └─────────────────┘              low → hibernate+sleep ┘
            │          │
   ADVANCE/ │          │ WIFI tap
   timer    │          │
            ▼          ▼
   ┌──────────────┐  ┌──────────────────────┐
   │ IMAGE path   │  │ PORTAL path           │
   │ storage →    │  │ SoftAP + DNS +        │
   │ decode →     │  │ async web server      │
   │ dither →     │  │ (upload / manage /    │
   │ display      │  │  settings, watchdog)  │
   └──────────────┘  └──────────────────────┘
            │                 │
            └──── sleep ◄──────┘
```

### Hard rule: mutually exclusive subsystems

Wi-Fi/web server and JPEG decode/refresh **never run in the same wake**. Both
need large amounts of heap and PSRAM, and on battery their combined current
draw risks a brownout through the buck-boost. The FSM picks exactly one path
per wake; the portal hands control to the image path by scheduling a clean
reboot (see §6).

---

## 2. Modules

Each module is one `.cpp`/`.h` pair with a small public interface.

| Module | Files | Responsibility |
|---|---|---|
| main / FSM | `src/main.cpp` | Decode the wake cause, dispatch to one path, sleep |
| power | `src/power.cpp` | Wake causes, battery ADC + calibration, deep-sleep entry |
| storage | `src/storage.cpp` | LittleFS, `playlist.json` + `settings.json` (atomic writes) |
| decode | `src/decode.cpp` | JPEG from LittleFS → RGB565 in PSRAM (TJpg_Decoder) |
| dither | `src/dither.cpp` | Serpentine Floyd-Steinberg → packed 4bpp palette indices |
| display | `src/display.cpp` | GxEPD2 paged rendering to the GDEP073E01 panel |
| webportal | `src/webportal.cpp` | SoftAP, captive portal, upload/manage/settings API |

Dependency order (arrows = "depends on"): everything depends on `config.h`;
`decode` and `webportal` additionally depend on `storage`; `main` depends on
all of them.

---

## 3. The image path

1. **Battery gate**: `Power::isBatteryOk()` runs before anything touches the
   panel. (Currently stubbed via `BATT_MONITOR_ENABLED 0` until the battery
   sense line is wired; the real cutoff check is behind the flag.)
2. **Playlist**: `Storage::getNextImage()` returns the image at the cursor and
   advances it. The cursor is persisted in `playlist.json`, so ordering
   survives power loss.
3. **Decode**: TJpg_Decoder streams the JPEG into a full-frame RGB565 buffer
   in PSRAM. Only exact 800×480 files are accepted (the web app guarantees
   this), and decode errors abort the refresh instead of painting garbage.
4. **Dither**: error diffusion quantizes each pixel to the nearest of the 6
   panel colors. Two user-selectable algorithms (Settings tab): classic
   Floyd-Steinberg (weights 7/16, 3/16, 5/16, 1/16, all error kept) and
   Atkinson (1/8 to six neighbors, 2/8 discarded, which keeps flat areas
   free of stray speckles). Rows alternate scan direction (serpentine) in
   both modes to avoid directional "worm" artifacts. Output is a packed
   4-bit-per-pixel index buffer.
5. **Paint**: GxEPD2 refreshes the panel in pages; the full frame stays in
   PSRAM while only a ~32 KB page buffer lives in internal RAM.
6. **Sleep**: with a timer armed for the slideshow interval (if enabled) or
   the 24 h mandatory refresh, whichever is sooner.

### PSRAM buffers

Allocated only in the image path, via `heap_caps_malloc(..., MALLOC_CAP_SPIRAM)`:

| Buffer | Size | Flow |
|---|---|---|
| RGB565 frame | `FRAME_RGB565_BYTES` (768 KB) | decode → dither |
| 4bpp index frame | `FRAME_INDEX_BYTES` (192 KB) | dither → display |

The portal path allocates neither; Wi-Fi gets the heap to itself.

---

## 4. The portal path

Waking with the Wi-Fi pad starts an open SoftAP (`EinkFrame-Setup`) with:

- **Wildcard DNS** to `192.168.4.1` plus the OS captive-portal probe routes
  (`/generate_204`, `/hotspot-detect.html`, `/connecttest.txt`), so phones
  auto-open the interface on join.
- **The web app** served from LittleFS (`/www/index.html`, fully self-contained).
- **The JSON API**: see [`API.md`](API.md).

Session lifetime: an inactivity watchdog closes the portal after 3 minutes.
The web app sends a heartbeat (`/api/ping`) every 30 s while open, so the
watchdog only fires once the user actually leaves. Stored photos are served
with long cache headers (filenames are unique per upload) and the web app
throttles thumbnail loading to two concurrent requests, because LittleFS
cannot feed a browser's default six-plus parallel connections.

---

## 5. Persistence

| File | Contents | Written |
|---|---|---|
| `/playlist.json` | Image list (display order) + cursor | On upload, delete, reorder, advance |
| `/settings.json` | Slideshow enabled + interval, dither mode | On settings save |
| `/img/*.jpg` | The photos, exactly 800×480 | On upload |
| `/www/*` | Web app assets | By `pio run -t uploadfs` |

Both JSON files are written atomically (write to `.tmp`, then rename) so a
power cut mid-write can't corrupt them. The filesystem is mounted with
`LittleFS.begin(false)`; format-on-fail is prohibited, so a transient mount
error can never wipe the gallery.

### RTC-persisted state (survives deep sleep, not power loss)

| Variable | Purpose |
|---|---|
| `rtc_last_refresh_unix` | Enforces `PANEL_LOCKOUT_SEC` between refreshes |
| `rtc_boot_count` | Diagnostics |

Cross-sleep timing uses the RTC domain (`gettimeofday`); `millis()` resets on
every deep sleep and is only valid within the portal session.

---

## 6. Portal → image hand-off

"Show this photo now" requires leaving the portal (rule in §1). The API
handlers must not block or reboot from the AsyncTCP task, so they:

1. Point the playlist cursor at the requested photo.
2. Answer the HTTP request normally.
3. Arm a deferred reboot that the main-task loop executes after a short grace
   period (`PORTAL_REBOOT_GRACE_MS`), closing the server and deauthing
   clients (`softAPdisconnect(true)`) before restarting.

The reboot lands in the image path (unknown wake cause), which paints the
photo at the cursor.

---

## 7. E-Ink discipline

Rules that protect the panel, all enforced in firmware:

- **6 colors only**: black, white, red, green, blue, yellow. Never ACeP
  orange; the GDEP073E01 mapping breaks.
- One full refresh per wake; repeated taps inside `PANEL_LOCKOUT_SEC` (30 s)
  are ignored.
- A mandatory refresh at least every 24 h (`MANDATORY_REFRESH_SEC`) for panel
  health, even when idle.
- Slideshow intervals are clamped to a 5-minute floor (`SLIDESHOW_MIN_SEC`).
- `hibernate()` after every refresh; the panel powers off and holds the
  image for free.
