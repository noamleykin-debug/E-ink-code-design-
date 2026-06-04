# Prompt — Display module

Copy the block below into a fresh chat connected to this repo.

---

```
You are an Expert Embedded C++ Firmware Engineer working on the ESP32-S3 E-Ink
photo frame in this repository.

FIRST read and follow: CLAUDE.md, docs/ROADMAP.md, docs/WORKFLOW.md,
docs/plans/display.md.

TASK: Implement/refine the Display module — drive the GDEP073E01 (E6 7C) panel
via GxEPD2, painting from a packed 4bpp PSRAM palette-index frame.

BRANCH: `plan/display` (cut from main). Never commit to main.

SCOPE (only these files):
- src/display.cpp
- include/display.h
- docs/plans/display.md (only if the interface changes)

KEY REQUIREMENTS:
- GxEPD2_7C + GxEPD2_730c_GDEP073E01. SPI.begin(40,-1,41,42); CS=42 DC=39 RST=47
  BUSY=21 (all from config.h).
- 7C is PAGED: full frame stays in PSRAM; emit per page (firstPage/nextPage).
- Map palette index -> GxEPD2 color for the SIX colors only. NEVER GxEPD_ORANGE.
- Nibble unpack order MUST match dither.cpp packing (even-x high, odd-x low).
- Provide powerOff()/hibernate() helpers; no rapid updates.
- VERIFY the driver class name compiles against the installed GxEPD2 version; if
  it differs, report it (do not silently swap panels).

DEFINITION OF DONE: `pio run` green; interface matches docs/plans/display.md;
draft PR into main describing verification (bring-up: solid colors + 6-stripe test).

Stop and ask if anything conflicts with the Golden Rules or hardware design.
```
