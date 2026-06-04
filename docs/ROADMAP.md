# E-Ink Photo Frame — Firmware Roadmap

Master plan for the ESP32-S3 7.3" 6-color E-Ink photo frame. Each **module**
below has its own branch (`plan/<module>`) carrying a scoped `docs/PLAN.md`.
This file is the shared map they all descend from.

> Hardware design is FINAL. See `include/config.h` for the authoritative pinout,
> palette, ADC math, and timing constants. Do not redefine these per-module.

---

## 1. System overview

Battery-powered frame. Normal life = deep sleep. A capacitive tap wakes it via
EXT1, it does exactly one job, then returns to deep sleep.

```
                 ┌──────────────── deep sleep ────────────────┐
                 │                                             │
   EXT1 wake (GPIO1 advance / GPIO2 wifi)  or  timer/mandatory │
                 ▼                                             │
        ┌─────────────────┐   battery gate (read GPIO6 FIRST)  │
        │   main / FSM     │──────────────┐                    │
        └─────────────────┘              low → hibernate+sleep ┘
            │          │
   ADVANCE/ │          │ WIFI tap
   timer    │          │
            ▼          ▼
   ┌──────────────┐  ┌──────────────────┐
   │ IMAGE path   │  │ PORTAL path       │
   │ storage→     │  │ SoftAP + DNS +    │
   │ decode→      │  │ async webserver   │
   │ dither→      │  │ (uploads, 3-min   │
   │ display      │  │  watchdog)        │
   └──────────────┘  └──────────────────┘
            │                 │
            └──── sleep ◄──────┘
```

### Hard rule: mutually exclusive subsystems
Wi-Fi/WebServer and JPEG decode/refresh **never run at the same time**
(PSRAM/heap contention). The FSM picks exactly one path per wake.

---

## 2. Module map & dependency order

Build/integrate bottom-up; arrows = "depends on".

```
config.h  (done)
   ▲
   ├── power      (no deps)            ── ADC gate, EXT1, deep sleep
   ├── storage    (no deps)            ── LittleFS, playlist.json
   ├── display    (config)             ── GxEPD2 7C, paged emit
   ├── dither     (config)             ── FS error diffusion → 6 colors
   ├── decode     (config, storage)    ── TJpg → RGB565 in PSRAM
   ├── webportal  (storage)            ── captive portal, uploads
   └── main       (ALL of the above)   ── wake dispatch / orchestration
```

| Module | Files | Branch |
|---|---|---|
| Power & sleep   | `power.{h,cpp}`     | `plan/power` |
| Storage         | `storage.{h,cpp}`   | `plan/storage` |
| JPEG decode     | `decode.{h,cpp}`    | `plan/decode` |
| Dither          | `dither.{h,cpp}`    | `plan/dither` |
| Display         | `display.{h,cpp}`   | `plan/display` |
| Web/captive     | `webportal.{h,cpp}` | `plan/webportal` |
| Main / FSM      | `main.cpp`          | `plan/main` |

---

## 3. Shared PSRAM buffer plan

Allocated once (in the IMAGE path only) with `heap_caps_malloc(..., MALLOC_CAP_SPIRAM)`:

| Buffer | Size | Owner | Notes |
|---|---|---|---|
| TJpg workspace      | `TJPG_WORKSPACE_SIZE` (64 KB) | decode  | scratch for decompress |
| RGB565 frame        | `FRAME_RGB565_BYTES` (768 KB) | decode→dither | 800×480×2 |
| 4bpp index frame    | `FRAME_INDEX_BYTES` (192 KB)  | dither→display | 1 nibble/pixel, palette idx 0–5 |

The PORTAL path allocates none of these — Wi-Fi gets the heap to itself.

---

## 4. RTC-persisted state (survives deep sleep)

Stored in `RTC_DATA_ATTR` (and/or RTC-domain `gettimeofday`). **Never** use
`millis()` for cross-sleep timing.

| State | Purpose |
|---|---|
| `last_refresh_unix`   | enforce `PANEL_LOCKOUT_SEC` and `MANDATORY_REFRESH_SEC` |
| `playlist_cursor`     | which image is next |
| `boot_count`          | diagnostics / first-boot detection |
| `rtc_clock_valid`     | whether RTC time has been set this power cycle |

---

## 5. Correctness traps (global checklist)

- [ ] Dither to **6 colors only** — Black/White/Red/Green/Blue/Yellow, **no orange**.
- [ ] TJpg outputs **RGB565**; convert before/within the dither.
- [ ] GxEPD2 7C is **paged**: hold full frame in PSRAM, emit per page.
- [ ] `LittleFS.begin(false)` — **never** `true` (would wipe the gallery).
- [ ] Cross-sleep timing via **RTC domain**, not `millis()`.
- [ ] Read battery ADC (GPIO6) **before** any refresh; `rtc_gpio_isolate(GPIO_NUM_6)` before sleep; calibrate via `esp_adc_cal`.
- [ ] **Never** run Wi-Fi/WebServer and decode/refresh simultaneously.
- [ ] Captive portal: `/generate_204`, `/hotspot-detect.html`, `/connecttest.txt` + wildcard DNS → 192.168.4.1.
- [ ] TJpg workspace + image buffers in **PSRAM** (`heap_caps_malloc`), not internal heap.
- [ ] E-Ink discipline: `powerOff()`/`hibernate()`, 24h mandatory refresh, no rapid loops.

---

## 6. Integration order

1. `power` + `storage` (independent, testable in isolation).
2. `display` (verify a hardcoded solid-color frame paints).
3. `dither` (verify against a known test image off-device if possible).
4. `decode` (wire LittleFS JPEG → RGB565).
5. Stitch IMAGE path in `main`: storage → decode → dither → display → sleep.
6. `webportal` (PORTAL path), then add WIFI-tap dispatch to `main`.
7. End-to-end: phone uploads → playlist → tap advances → refresh.
