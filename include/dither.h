#pragma once
#include <Arduino.h>

namespace Dither {

// Selectable error-diffusion algorithm (persisted in settings, see config.h).
enum DitherMode : uint8_t {
    DITHER_FLOYD_STEINBERG = 0,   // crisper, classic; diffuses all error
    DITHER_ATKINSON        = 1    // softer; diffuses 6/8, cleaner flat areas
};

// Process a decoded RGB565 frame into a 4bpp index frame using error
// diffusion. Both buffers must be pre-allocated in PSRAM.
void processFrame(const uint16_t* rgb565_in, uint8_t* index_out,
                  DitherMode mode = DITHER_FLOYD_STEINBERG);

} // namespace Dither
