#include "decode.h"
#include "config.h"
#include <LittleFS.h>
#include <TJpg_Decoder.h>

namespace Decode {

static uint16_t* s_dest_buffer = nullptr;

// TJpg_Decoder callback
// Receives MCU blocks (e.g. 16x16 or 8x8) and blits them into our full-frame PSRAM buffer.
static bool tjpgd_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
    if (!s_dest_buffer) {
        return 0; // Abort
    }
    
    // Strict bounds check: reject anything completely outside our geometry
    if (x >= EPD_WIDTH || y >= EPD_HEIGHT) {
        return 1; // Ignore and continue
    }
    
    int16_t draw_w = w;
    int16_t draw_h = h;
    
    // Clip right and bottom edges if the MCU block overhangs
    if (x + draw_w > EPD_WIDTH) {
        draw_w = EPD_WIDTH - x;
    }
    if (y + draw_h > EPD_HEIGHT) {
        draw_h = EPD_HEIGHT - y;
    }
    
    // Blit the block into the PSRAM destination buffer
    for (int16_t row = 0; row < draw_h; row++) {
        uint32_t dest_idx = (y + row) * EPD_WIDTH + x;
        uint32_t src_idx = row * w; // src bitmap width is always w
        memcpy(&s_dest_buffer[dest_idx], &bitmap[src_idx], draw_w * sizeof(uint16_t));
    }
    
    return 1; // Continue decoding
}

bool decodeFileToPSRAM(const String& filename, uint16_t* rgb565_out) {
    if (!rgb565_out) {
        log_e("Decode: Null destination buffer");
        return false;
    }
    
    // Ensure file exists
    if (!LittleFS.exists(filename)) {
        log_e("Decode: File not found %s", filename.c_str());
        return false;
    }
    
    // Read headers to enforce resolution gate (800x480 ONLY)
    uint16_t jpg_w = 0, jpg_h = 0;
    JRESULT jres = TJpgDec.getFsJpgSize(&jpg_w, &jpg_h, filename.c_str(), LittleFS);
    if (jres != JDR_OK) {
        log_e("Decode: Failed to read JPEG header for %s (JRESULT %d)", filename.c_str(), (int)jres);
        return false;
    }

    if (jpg_w != EPD_WIDTH || jpg_h != EPD_HEIGHT) {
        log_e("Decode: Dimension mismatch. Expected %dx%d, got %dx%d", EPD_WIDTH, EPD_HEIGHT, jpg_w, jpg_h);
        return false;
    }

    // Setup TJpg_Decoder (it manages its own internal workspace)
    s_dest_buffer = rgb565_out;
    TJpgDec.setJpgScale(1);

    // Do NOT byte-swap. The ditherer reads each pixel as a native uint16_t and
    // extracts channels by shifting (r = (color >> 11) & 0x1F), so it needs the
    // pixel in normal RGB565 layout. setSwapBytes(true) reverses the bytes of
    // every pixel and scrambles the colors.
    TJpgDec.setSwapBytes(false);

    TJpgDec.setCallback(tjpgd_output);

    log_i("Decode: Starting JPEG decompress");
    jres = TJpgDec.drawFsJpg(0, 0, filename.c_str(), LittleFS);
    s_dest_buffer = nullptr;

    if (jres != JDR_OK) {
        log_e("Decode: Decompress failed for %s (JRESULT %d)", filename.c_str(), (int)jres);
        return false;
    }

    log_i("Decode: Complete");
    return true;
}

} // namespace Decode
