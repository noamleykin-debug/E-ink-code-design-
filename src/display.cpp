#include "display.h"
#include "config.h"
#include <SPI.h>
// GxEPD2_7C.h pulls in all 7-color panel drivers (incl. GDEP073E01) from its
// epd7c/ subfolder. Do NOT include <GxEPD2_730c_GDEP073E01.h> directly — that
// header is not on the root include path and the build fails to find it.
#include <GxEPD2_7C.h>

namespace Display {

// Instantiate the display driver.
// Using the specified class and hardware constants from config.h.
//
// GxEPD2_7C's second template parameter is the per-page buffer HEIGHT (a
// uint16_t), NOT a byte size. Feeding MAX_DISPLAY_BUFFER_SIZE (65536) directly
// overflows uint16_t and fails to compile. Derive a valid page height from the
// byte budget instead: bytes / 2 (4bpp -> 2px per byte across WIDTH) bounded by
// the panel height. This keeps the internal page buffer small (~32 KB) and
// drives GxEPD2 page-by-page, per Golden Rule #3 (no full framebuffer in RAM).
#define MAX_HEIGHT_7C(EPD) \
    ((EPD::HEIGHT) <= (MAX_DISPLAY_BUFFER_SIZE) / 2 / ((EPD::WIDTH) / 2) \
        ? (EPD::HEIGHT) \
        : (MAX_DISPLAY_BUFFER_SIZE) / 2 / ((EPD::WIDTH) / 2))

static GxEPD2_7C<GxEPD2_730c_GDEP073E01, MAX_HEIGHT_7C(GxEPD2_730c_GDEP073E01)> display(
    GxEPD2_730c_GDEP073E01(EPD_CS_PIN, EPD_DC_PIN, EPD_RST_PIN, EPD_BUSY_PIN)
);

// Map our 6 valid colors to GxEPD2 hardware colors.
// Strictly NEVER map to GxEPD_ORANGE.
static const uint16_t COLOR_MAP[6] = {
    GxEPD_BLACK,
    GxEPD_WHITE,
    GxEPD_RED,
    GxEPD_GREEN,
    GxEPD_BLUE,
    GxEPD_YELLOW
};

void init() {
    log_i("Initializing SPI and Display");
    // Standard hardware SPI mapping for this board
    SPI.begin(EPD_SPI_SCK, EPD_SPI_MISO, EPD_SPI_MOSI, EPD_SPI_SS);
    
    // Init GxEPD2: serial diagnostic rate, initial reset, reset duration, pulldown
    display.init(115200, true, 2, false);
}

void paintFromIndexBuffer(const uint8_t* index_buffer) {
    if (!index_buffer) {
        log_e("Null buffer passed to paintFromIndexBuffer");
        return;
    }

    log_i("Starting paged refresh");
    display.setFullWindow();
    display.firstPage();
    do {
        // Iterate over the entire frame. GxEPD2's Adafruit_GFX implementation
        // will inherently clip and ignore drawPixel calls outside the current page.
        // On an ESP32-S3, this nested loop takes negligible time per page.
        for (int16_t y = 0; y < EPD_HEIGHT; y++) {
            for (int16_t x = 0; x < EPD_WIDTH; x++) {
                uint32_t byte_idx = (y * EPD_WIDTH + x) / 2;
                uint8_t packed = index_buffer[byte_idx];
                
                // Nibble unpack: even-x is high nibble, odd-x is low nibble.
                uint8_t col_idx = (x % 2 == 0) ? (packed >> 4) & 0x0F : (packed & 0x0F);
                
                if (col_idx >= COL_COUNT) {
                    col_idx = COL_BLACK; // Fallback
                }
                
                display.drawPixel(x, y, COLOR_MAP[col_idx]);
            }
        }
    } while (display.nextPage());
    log_i("Paged refresh complete");
}

void hibernate() {
    display.hibernate();
}

void powerOff() {
    display.powerOff();
}

} // namespace Display
