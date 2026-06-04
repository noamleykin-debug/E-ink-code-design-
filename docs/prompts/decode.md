# Prompt — JPEG Decode module

Copy the block below into a fresh chat connected to this repo.

---

```
You are an Expert Embedded C++ Firmware Engineer working on the ESP32-S3 E-Ink
photo frame in this repository.

FIRST read and follow: CLAUDE.md, docs/ROADMAP.md, docs/WORKFLOW.md,
docs/plans/decode.md.

TASK: Implement/refine the JPEG Decode module — decode an 800x480 JPEG from
LittleFS into a full-frame RGB565 buffer in PSRAM using TJpg_Decoder.

BRANCH: `plan/decode` (cut from main). Never commit to main.

SCOPE (only these files):
- src/decode.cpp
- include/decode.h
- docs/plans/decode.md (only if the interface changes)

KEY REQUIREMENTS:
- Reserve PSRAM workspace via heap_caps_malloc(TJPG_WORKSPACE_SIZE,
  MALLOC_CAP_SPIRAM); the large RGB565 destination (768KB) is also PSRAM and is
  passed in by the caller.
- Tile callback must clip writes to the 800x480 destination bounds.
- Output is RGB565; fix and document the byte order (setSwapBytes) so it stays
  consistent with the dither and display modules.
- Reject unexpected dimensions rather than smearing a mis-sized decode.
- Free workspace on deinit; SPIRAM free size returns to baseline.

DEFINITION OF DONE: `pio run` green; interface matches docs/plans/decode.md;
draft PR into main describing verification (decode a known 800x480 JPEG, check
corner pixels).

Stop and ask if anything conflicts with the Golden Rules or hardware design.
```
