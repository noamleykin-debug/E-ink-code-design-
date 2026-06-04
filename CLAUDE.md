# CLAUDE.md — Agent Operating Manual

> **Read this first, every session.** It is the contract for working in this
> repo. If anything you're about to do conflicts with the **Golden Rules**
> below, stop and reconsider — those rules encode hardware/physics constraints,
> not preferences.

---

## 1. Mission

Firmware for a **battery-powered 7.3" 6-color E-Ink photo frame**.

- **MCU:** ESP32-S3-DevKitC-1 — 16 MB flash, 8 MB **OPI (Octal) PSRAM**.
- **Display:** Good Display **GDEP073E01** (E6, 6-color) via **GxEPD2** on a DESPI-C73.
- **Flow:** phone resizes photos to 800×480 JPEGs → uploads to the frame over a
  captive portal → frame wakes on a touch, decodes a JPEG into PSRAM, dithers it
  to 6 colors, paints the panel, and goes back to deep sleep.

The hardware design is **FINAL**. Do not change pins, partitions, or the panel
driver. The authoritative hardware/config values live in **`include/config.h`** —
treat that file as the single source of truth and never hard-code constants that
duplicate it.

---

## 2. Golden Rules (correctness traps — non-negotiable)

1. **6 colors only:** Black, White, Red, Green, Blue, Yellow. **Never** emit
   ACeP orange (`GxEPD_ORANGE`) — the panel mapping fails.
2. **JPEG decode outputs RGB565.** Convert correctly before/within the dither.
3. **GxEPD2 7C is paged.** Keep the full frame in PSRAM; emit page-by-page.
   Never allocate a full framebuffer in internal RAM.
4. **`LittleFS.begin(false)` ONLY.** Never `true` — format-on-fail would wipe the
   user's gallery on a transient mount error.
5. **Cross-sleep timing uses the RTC domain** (`gettimeofday` + `RTC_DATA_ATTR`),
   **never `millis()`**. (`millis()` is allowed *only* inside the Wi-Fi/portal
   session, which stays awake.)
6. **Read the battery ADC (GPIO6) BEFORE any refresh.** Calibrate with
   `esp_adc_cal`. If below cutoff: show a message, `display.hibernate()`, deep sleep.
7. **Isolate GPIO6 before sleep:** `rtc_gpio_isolate(GPIO_NUM_6)`.
8. **Wi-Fi and image decode/refresh are mutually exclusive** — never run the
   web server and the decode/dither/refresh pipeline in the same wake
   (PSRAM/heap contention).
9. **Big buffers live in PSRAM** via `heap_caps_malloc(..., MALLOC_CAP_SPIRAM)`,
   never plain `malloc`.
10. **Captive portal must include** OS-detection routes (`/generate_204`,
    `/hotspot-detect.html`, `/connecttest.txt`) and a wildcard DNS to
    `192.168.4.1`.
11. **E-Ink discipline:** one full refresh per wake (respect `PANEL_LOCKOUT_SEC`),
    a 24 h mandatory refresh, `powerOff()`/`hibernate()` when idle. No rapid loops.
12. **GPIO4 / GPIO5 / GPIO7 are reserved for V2.** Do not use them or add them to
    the EXT1 wake mask.

The **EXT1 wake mask is exactly** `(1ULL<<1)|(1ULL<<2)` with
`ESP_EXT1_WAKEUP_ANY_HIGH` (GPIO1 = advance, GPIO2 = wifi).

---

## 3. Repo map

```
platformio.ini          Build config: 16MB flash, OPI PSRAM, libs, partitions.
partitions.csv          3MB app0 / 3MB app1 / ~9.9MB LittleFS / coredump.
include/                Public headers (config.h is the source of truth).
src/                    Module implementations (one .cpp per module) + main.cpp.
docs/ROADMAP.md         System architecture, module map, data flow, RTC state.
docs/WORKFLOW.md        Branch/PR rules, merge order, definition of done.
docs/plans/<module>.md  Per-module spec: responsibility, interface, traps, checklist.
docs/prompts/<module>.md Copy-paste prompt to hand a module to a fresh agent.
.github/workflows/      CI (PlatformIO build).
```

## 4. Modules & ownership

Each module is independently developed on its own branch and integrated into
`main` via PR. **One module = one branch = one PR.**

| Module    | Files                          | Branch          | Depends on        |
|-----------|--------------------------------|-----------------|-------------------|
| power     | `power.{h,cpp}`                | `plan/power`    | config            |
| storage   | `storage.{h,cpp}`              | `plan/storage`  | config            |
| display   | `display.{h,cpp}`              | `plan/display`  | config            |
| dither    | `dither.{h,cpp}`               | `plan/dither`   | config            |
| decode    | `decode.{h,cpp}`               | `plan/decode`   | config, storage   |
| webportal | `webportal.{h,cpp}`            | `plan/webportal`| config, storage   |
| main      | `main.cpp`                     | `plan/main`     | ALL of the above  |

---

## 5. Workflow (how to ship work)

Full detail in **`docs/WORKFLOW.md`**. The short version:

1. **Never commit to `main` directly.** `main` is integration-only; it changes
   via reviewed PRs.
2. Work on the module's branch (`plan/<module>`) or a new `feat/<topic>` branch
   cut **from `main`**.
3. Keep changes scoped to that module's own files so branches never collide.
4. Build locally (§6). Make it compile.
5. Commit with a clear message, push with `git push -u origin <branch>`.
6. Open a **draft PR into `main`**. Fill in what changed and how you verified it.
7. Recommended integration order: power → storage → display → dither → decode →
   webportal → **main last**.

## 6. Build & validate

```bash
pio run                 # compile firmware (the primary "does it build?" check)
pio run -t upload       # flash firmware over USB
pio run -t uploadfs     # upload the LittleFS image (web assets in data/)
pio device monitor      # serial @ 115200
pio check               # static analysis (optional)
```
CI runs `pio run` on every PR — **a PR is not done until CI is green** (plus the
module's own Definition of Done in its prompt).

---

## 7. Start-here checklist for a fresh agent

1. Read this file (Golden Rules especially).
2. Read `docs/ROADMAP.md` for the system picture.
3. Identify your task. If you were handed a module, open its
   `docs/plans/<module>.md` (the spec) and `docs/prompts/<module>.md` (your prompt).
4. Check out the right branch (or create `feat/<topic>` from `main`).
5. Implement within scope, honor the Golden Rules, build, push, open a draft PR.
6. If something is ambiguous or would change the finalized hardware/architecture,
   **ask the human** instead of guessing.
