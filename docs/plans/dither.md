# PLAN — Dither module (`dither.{h,cpp}`)

Branch: `plan/dither` · Depends on: `config.h` (palette) · See `docs/ROADMAP.md`.

## Responsibility
Convert the decoded **RGB565** frame into a packed **4-bit-per-pixel palette
index** frame using **Floyd-Steinberg error diffusion**, quantizing strictly to
the **6 E6 colors** in `EPD_PALETTE[]`.

## Public interface (proposed)
```c
// src565: EPD_WIDTH*EPD_HEIGHT RGB565 (PSRAM).
// dstIdx: packed 4bpp, FRAME_INDEX_BYTES (PSRAM); high/low nibble = palette idx.
void dither_rgb565_to_index(const uint16_t* src565, uint8_t* dstIdx,
                            int w, int h);

// Helpers (exposed for unit testing)
static inline void rgb565_to_888(uint16_t c, int* r, int* g, int* b);
uint8_t dither_nearest_index(int r, int g, int b);   // -> COL_* (0..5)
```

## Algorithm
Standard Floyd-Steinberg, left-to-right per row, error pushed to neighbors:
```
        X     7/16
  3/16  5/16  1/16
```
1. Expand each RGB565 pixel to RGB888, add accumulated error (clamp 0..255).
2. `dither_nearest_index` = argmin over the 6 palette entries of a weighted
   distance (use perceptual weights, e.g. 0.299/0.587/0.114, or squared
   Euclidean to start).
3. Compute per-channel quant error = (input - palette[idx]).
4. Distribute error to the 4 neighbors with the FS weights.
5. Pack the chosen index into the destination nibble.

Use **two `int16_t` error rows** (current + next) in PSRAM rather than a full
float error plane — keeps memory bounded and avoids drift.

## Correctness traps this module OWNS
- [ ] **Exactly 6 colors. No orange.** Only ever emit indices `COL_BLACK..COL_YELLOW`.
- [ ] **Input is RGB565**, not RGB888 — do the 5/6/5 → 8/8/8 expansion
      correctly (replicate high bits, e.g. `r8 = (r5<<3)|(r5>>2)`).
- [ ] Match the **byte order** the decode module produced (see `plan/decode`).
- [ ] Clamp channel+error to `[0,255]` before nearest-color search.
- [ ] Packing: even x → one nibble, odd x → the other; be consistent with what
      `display` unpacks (document nibble order once, keep fixed).
- [ ] Operate entirely on PSRAM buffers; no 384 KB stack/heap surprises.

## Implementation checklist
1. `rgb565_to_888` expansion + `dither_nearest_index` with the chosen metric.
2. Allocate two `int16_t[w][3]` error rows in PSRAM (or one rolling pair).
3. Main loop: read src → add error → nearest → write nibble → spread error.
4. Swap error rows at end of each line; zero the new "next" row.

## Test notes
- Off-device: run on a known gradient, confirm only 6 indices appear and that
  flat color regions map to the nearest primary (no orange leak).
- Visual: a smooth sky should diffuse to blue/white speckle, not banding.
