// ============================================================================
//  decode.h — TJpg_Decoder JPEG -> RGB565 (into PSRAM). See docs/plans/decode.md.
// ============================================================================
#pragma once

#include <stdint.h>

bool decode_init(void);     // alloc PSRAM workspace, set callback + scale
void decode_deinit(void);   // free PSRAM workspace

// Decode the JPEG at `path` (LittleFS) into a caller-owned RGB565 PSRAM buffer
// sized w*h (expected EPD_WIDTH x EPD_HEIGHT). Returns false on any error.
bool decode_jpeg_to_rgb565(const char* path, uint16_t* dst565, int w, int h);
