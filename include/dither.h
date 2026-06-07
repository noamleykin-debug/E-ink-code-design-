#pragma once
#include <Arduino.h>

namespace Dither {

// Process a decoded RGB565 frame into a 4bpp index frame using Floyd-Steinberg error diffusion.
// Both buffers must be pre-allocated in PSRAM.
void processFrame(const uint16_t* rgb565_in, uint8_t* index_out);

} // namespace Dither
