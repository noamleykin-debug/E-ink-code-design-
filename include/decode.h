#pragma once
#include <Arduino.h>

namespace Decode {

// Decodes a LittleFS JPEG into the provided PSRAM RGB565 buffer.
// Returns true on success, false if dimensions mismatch or decoding fails.
bool decodeFileToPSRAM(const String& filename, uint16_t* rgb565_out);

} // namespace Decode
