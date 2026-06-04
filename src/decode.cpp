// ============================================================================
//  decode.cpp — see include/decode.h and docs/plans/decode.md
//
//  The large PSRAM consumer in this pipeline is the RGB565 frame buffer (768 KB),
//  which the IMAGE path allocates with heap_caps_malloc(MALLOC_CAP_SPIRAM) and
//  passes into decode_jpeg_to_rgb565(). We also reserve a PSRAM workspace here
//  per the architecture guardrail; the Bodmer TJpg_Decoder build manages its own
//  small internal work pool, so this reservation guarantees PSRAM headroom for
//  the decode stage rather than being handed to the library directly.
// ============================================================================
#include "decode.h"
#include "config.h"

#include <Arduino.h>
#include <TJpg_Decoder.h>
#include <LittleFS.h>
#include <esp_heap_caps.h>

static uint16_t* s_dst = nullptr;   // active destination frame
static int       s_w   = 0;
static int       s_h   = 0;
static uint8_t*  s_workspace = nullptr;

// TJpg tile callback: copy each decoded MCU block into the RGB565 frame,
// clipping anything outside the destination bounds.
static bool tile_cb(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  if (!s_dst) return false;
  for (uint16_t row = 0; row < h; ++row) {
    int dy = y + row;
    if (dy < 0 || dy >= s_h) continue;
    for (uint16_t col = 0; col < w; ++col) {
      int dx = x + col;
      if (dx < 0 || dx >= s_w) continue;
      s_dst[(uint32_t)dy * s_w + dx] = bitmap[(uint32_t)row * w + col];
    }
  }
  return true;   // keep decoding
}

bool decode_init(void) {
  if (!s_workspace) {
    s_workspace = (uint8_t*)heap_caps_malloc(TJPG_WORKSPACE_SIZE, MALLOC_CAP_SPIRAM);
    if (!s_workspace) return false;
  }
  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(true);     // keep RGB565 byte order consistent with dither/display
  TJpgDec.setCallback(tile_cb);
  return true;
}

void decode_deinit(void) {
  if (s_workspace) {
    heap_caps_free(s_workspace);
    s_workspace = nullptr;
  }
}

bool decode_jpeg_to_rgb565(const char* path, uint16_t* dst565, int w, int h) {
  if (!dst565 || !path) return false;

  s_dst = dst565; s_w = w; s_h = h;

  uint16_t jw = 0, jh = 0;
  if (TJpgDec.getFsJpgSize(&jw, &jh, path, LittleFS) != JDR_OK) {
    s_dst = nullptr;
    return false;
  }
  // Images are pre-sized to 800x480 on the phone; reject unexpected dimensions
  // rather than smearing a mis-sized decode across the frame.
  if (jw != (uint16_t)w || jh != (uint16_t)h) {
    s_dst = nullptr;
    return false;
  }

  JRESULT r = TJpgDec.drawFsJpg(0, 0, path, LittleFS);
  s_dst = nullptr;
  return (r == JDR_OK);
}
