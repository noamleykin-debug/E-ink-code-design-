#include "decode.h"
#include "config.h"
#include <LittleFS.h>
#include <TJpg_Decoder.h>
#include <esp_heap_caps.h>

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
    TJpgDec.getFsJpgSize(&jpg_w, &jpg_h, filename.c_str(), LittleFS);
    
    if (jpg_w != EPD_WIDTH || jpg_h != EPD_HEIGHT) {
        log_e("Decode: Dimension mismatch. Expected %dx%d, got %dx%d", EPD_WIDTH, EPD_HEIGHT, jpg_w, jpg_h);
        return false;
    }
    
    log_i("Decode: Allocating TJpg workspace in PSRAM (%d bytes)", TJPG_WORKSPACE_SIZE);
    
    // Allocate the scratch workspace strictly in PSRAM
    uint8_t* workspace = (uint8_t*)heap_caps_malloc(TJPG_WORKSPACE_SIZE, MALLOC_CAP_SPIRAM);
    if (!workspace) {
        log_e("Decode: Failed to allocate workspace in PSRAM");
        return false;
    }
    
    // Setup TJpg_Decoder
    s_dest_buffer = rgb565_out;
    TJpgDec.setJpgScale(1);
    
    // Set swap bytes to true to normalize endianness for the downstream ditherer.
    // The ESP32-S3 is little-endian, so swapping ensures the RGB565 bits are 
    // laid out predictably for our shifting logic: r = (color >> 11) & 0x1F
    TJpgDec.setSwapBytes(true); 
    
    TJpgDec.setCallback(tjpgd_output);
    
    // Depending on the TJpg_Decoder fork, the workspace might need to be assigned.
    // For safety, we allocate it manually here to ensure heap space is tracked,
    // and let the internal TJpgDec utilize dynamic allocation or global scope.
    
    log_i("Decode: Starting JPEG decompress");
    TJpgDec.drawFsJpg(0, 0, filename.c_str(), LittleFS);
    log_i("Decode: Complete");
    
    // Cleanup state and restore PSRAM ceiling
    s_dest_buffer = nullptr;
    heap_caps_free(workspace);
    
    return true;
}

} // namespace Decode
