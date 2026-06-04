# Prompt — Main / State Machine

Copy the block below into a fresh chat connected to this repo.

---

```
You are an Expert Embedded C++ Firmware Engineer working on the ESP32-S3 E-Ink
photo frame in this repository.

FIRST read and follow: CLAUDE.md, docs/ROADMAP.md, docs/WORKFLOW.md,
docs/plans/main.md.

TASK: Implement/refine the orchestrator (main.cpp) — on each wake: establish RTC
time, gate on battery, decode the wake reason, run exactly ONE path, deep sleep.

BRANCH: `plan/main` (cut from main). Merge LAST. Never commit to main.

SCOPE: src/main.cpp (+ docs/plans/main.md only if behavior changes). main.cpp
includes every module header — assume the other modules are present in main.

KEY REQUIREMENTS:
- Battery read (power_*) BEFORE any refresh; low battery -> message + deep sleep.
- Wake dispatch: WIFI tap -> PORTAL path; ADVANCE tap or timer -> IMAGE path;
  cold boot -> show current image.
- IMAGE and PORTAL paths are MUTUALLY EXCLUSIVE (PSRAM/heap). Allocate the big
  PSRAM buffers (RGB565 768KB, 4bpp 192KB) only on the IMAGE path; free before sleep.
- RTC-domain timing (RTC_DATA_ATTR + gettimeofday) for PANEL_LOCKOUT_SEC and the
  24h mandatory refresh. millis() only inside the portal loop.
- Always exit via power_enter_deep_sleep() (isolates GPIO6, arms EXT1 + timer).

DEFINITION OF DONE: `pio run` green (with all modules merged into main); behavior
matches docs/plans/main.md; draft PR into main describing verification (double
ADVANCE respects lockout; WIFI tap allocates no image buffers; timer wake refreshes;
low battery -> message then sleep).

Stop and ask if anything conflicts with the Golden Rules or hardware design.
```
