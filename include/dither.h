// ============================================================================
//  dither.h — Floyd-Steinberg RGB565 -> 6-color packed 4bpp index frame.
//  See docs/plans/dither.md.
// ============================================================================
#pragma once

#include <stdint.h>

// src565 : w*h RGB565 pixels (PSRAM).
// dstIdx : packed 4bpp palette indices (PSRAM), size (w*h+1)/2 bytes.
//          even-x pixel -> high nibble, odd-x pixel -> low nibble.
// Quantizes strictly to the 6 colors in EPD_PALETTE (no orange).
void dither_rgb565_to_index(const uint16_t* src565, uint8_t* dstIdx, int w, int h);

// Nearest palette index (COL_*) for an RGB888 input. Exposed for testing.
uint8_t dither_nearest_index(int r, int g, int b);
