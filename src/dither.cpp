// ============================================================================
//  dither.cpp — see include/dither.h and docs/plans/dither.md
// ============================================================================
#include "dither.h"
#include "config.h"

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <string.h>
#include <limits.h>

static inline void rgb565_to_888(uint16_t c, int* r, int* g, int* b) {
  uint8_t r5 = (c >> 11) & 0x1F;
  uint8_t g6 = (c >> 5)  & 0x3F;
  uint8_t b5 =  c        & 0x1F;
  *r = (r5 << 3) | (r5 >> 2);   // replicate high bits for full 0..255 range
  *g = (g6 << 2) | (g6 >> 4);
  *b = (b5 << 3) | (b5 >> 2);
}

uint8_t dither_nearest_index(int r, int g, int b) {
  long best = LONG_MAX;
  uint8_t bi = 0;
  for (uint8_t i = 0; i < COL_COUNT; ++i) {
    long dr = (long)r - EPD_PALETTE[i].r;
    long dg = (long)g - EPD_PALETTE[i].g;
    long db = (long)b - EPD_PALETTE[i].b;
    long d = dr * dr + dg * dg + db * db;
    if (d < best) { best = d; bi = i; }
  }
  return bi;
}

static inline void pack_index(uint8_t* dstIdx, uint32_t pixel, uint8_t idx) {
  uint8_t* cell = &dstIdx[pixel >> 1];
  if (pixel & 1) *cell = (uint8_t)((*cell & 0xF0) | (idx & 0x0F));
  else           *cell = (uint8_t)((*cell & 0x0F) | (idx << 4));
}

void dither_rgb565_to_index(const uint16_t* src565, uint8_t* dstIdx, int w, int h) {
  // Two error rows (current + next), 3 channels each, in PSRAM.
  int16_t* errCur  = (int16_t*)heap_caps_calloc((size_t)w * 3, sizeof(int16_t), MALLOC_CAP_SPIRAM);
  int16_t* errNext = (int16_t*)heap_caps_calloc((size_t)w * 3, sizeof(int16_t), MALLOC_CAP_SPIRAM);

  // Fallback: if PSRAM error rows can't be had, quantize with no diffusion.
  if (!errCur || !errNext) {
    if (errCur)  heap_caps_free(errCur);
    if (errNext) heap_caps_free(errNext);
    for (int y = 0; y < h; ++y) {
      for (int x = 0; x < w; ++x) {
        uint32_t p = (uint32_t)y * w + x;
        int r, g, b;
        rgb565_to_888(src565[p], &r, &g, &b);
        pack_index(dstIdx, p, dither_nearest_index(r, g, b));
      }
    }
    return;
  }

  const size_t rowBytes = (size_t)w * 3 * sizeof(int16_t);

  for (int y = 0; y < h; ++y) {
    memset(errNext, 0, rowBytes);

    for (int x = 0; x < w; ++x) {
      uint32_t p = (uint32_t)y * w + x;
      int r, g, b;
      rgb565_to_888(src565[p], &r, &g, &b);

      r += errCur[x * 3 + 0];
      g += errCur[x * 3 + 1];
      b += errCur[x * 3 + 2];
      if (r < 0) r = 0; else if (r > 255) r = 255;
      if (g < 0) g = 0; else if (g > 255) g = 255;
      if (b < 0) b = 0; else if (b > 255) b = 255;

      uint8_t idx = dither_nearest_index(r, g, b);
      pack_index(dstIdx, p, idx);

      int er = r - EPD_PALETTE[idx].r;
      int eg = g - EPD_PALETTE[idx].g;
      int eb = b - EPD_PALETTE[idx].b;

      // Floyd-Steinberg distribution:  X 7/16 ; (3/16 5/16 1/16) next row
      if (x + 1 < w) {
        errCur[(x + 1) * 3 + 0] += er * 7 / 16;
        errCur[(x + 1) * 3 + 1] += eg * 7 / 16;
        errCur[(x + 1) * 3 + 2] += eb * 7 / 16;
      }
      if (x - 1 >= 0) {
        errNext[(x - 1) * 3 + 0] += er * 3 / 16;
        errNext[(x - 1) * 3 + 1] += eg * 3 / 16;
        errNext[(x - 1) * 3 + 2] += eb * 3 / 16;
      }
      errNext[x * 3 + 0] += er * 5 / 16;
      errNext[x * 3 + 1] += eg * 5 / 16;
      errNext[x * 3 + 2] += eb * 5 / 16;
      if (x + 1 < w) {
        errNext[(x + 1) * 3 + 0] += er * 1 / 16;
        errNext[(x + 1) * 3 + 1] += eg * 1 / 16;
        errNext[(x + 1) * 3 + 2] += eb * 1 / 16;
      }
    }

    int16_t* t = errCur; errCur = errNext; errNext = t;   // swap rows
  }

  heap_caps_free(errCur);
  heap_caps_free(errNext);
}
