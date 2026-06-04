# PLAN — JPEG Decode module (`decode.{h,cpp}`)

Branch: `plan/decode` · Depends on: `config.h`, `storage` (for the file path),
TJpg_Decoder · See `docs/ROADMAP.md`.

## Responsibility
Decode a LittleFS JPEG (~60 KB, pre-sized to 800×480 on the phone) into a
full-frame **RGB565** buffer in PSRAM, ready for the ditherer.

## Public interface (proposed)
```c
bool decode_init(void);     // allocate TJpg workspace in PSRAM, set callback + scale
void decode_deinit(void);   // free workspace

// Decode `path` into a caller-provided PSRAM RGB565 buffer (EPD_WIDTH×EPD_HEIGHT).
// Returns false on open/decode/dimension error.
bool decode_jpeg_to_rgb565(const char* path, uint16_t* dst565, int w, int h);
```

## Correctness traps this module OWNS
- [ ] **Workspace in PSRAM:**
      `uint8_t* ws = (uint8_t*)heap_caps_malloc(TJPG_WORKSPACE_SIZE, MALLOC_CAP_SPIRAM);`
      Never plain `malloc` — internal heap can't hold this on OPI-PSRAM builds.
- [ ] **Output is RGB565.** Hand off the format honestly; the dither module is
      written to consume RGB565 and must not assume RGB888.
- [ ] **Bounds-safe callback.** TJpg's tile callback must clip writes to the
      800×480 destination; drop/clip any tile that would overflow.
- [ ] Use `TJpgDec.setSwapBytes(...)` consistently with what the dither and
      display layers expect for RGB565 byte order — document the chosen
      endianness here once and keep it fixed.
- [ ] Set scale so the decode lands at native 800×480 (input is already sized;
      reject or center-crop unexpected dimensions rather than smearing).

## Implementation checklist
1. `decode_init`: alloc PSRAM workspace; `TJpgDec.setJpgScale(1)`;
   `TJpgDec.setCallback(tile_cb)`; configure byte order.
2. `tile_cb(x,y,w,h,bitmap)`: copy each row into
   `dst565[(y+row)*EPD_WIDTH + (x)]`, clipped to frame bounds.
3. `decode_jpeg_to_rgb565`: open file via LittleFS, query
   `TJpgDec.getJpgSize`, validate ≈800×480, then `TJpgDec.drawJpg`/`drawFsJpg`.
4. `decode_deinit`: `heap_caps_free(ws)`.

## Memory contract (see ROADMAP §3)
- TJpg workspace: 64 KB PSRAM (this module allocs/frees).
- RGB565 destination: 768 KB PSRAM (allocated by `main`/IMAGE path, passed in).

## Test notes
- Decode a known 800×480 test JPEG; spot-check corner pixels' RGB565 values.
- Feed a wrong-size JPEG → must reject cleanly, not corrupt the buffer.
- Confirm `heap_caps_get_free_size(MALLOC_CAP_SPIRAM)` returns to baseline
  after `decode_deinit`.
