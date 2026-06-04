// ============================================================================
//  display.cpp — see include/display.h and docs/plans/display.md
//
//  NOTE: confirm the driver class name matches your installed GxEPD2 version.
//  Recent GxEPD2 exposes the 7.3" E6 panel as GxEPD2_730c_GDEP073E01.
// ============================================================================
#include "display.h"
#include "config.h"

#include <Arduino.h>
#include <SPI.h>
#define ENABLE_GxEPD2_GFX 0
#include <GxEPD2_7C.h>

static GxEPD2_7C<GxEPD2_730c_GDEP073E01, GxEPD2_730c_GDEP073E01::HEIGHT>
  display(GxEPD2_730c_GDEP073E01(EPD_CS_PIN, EPD_DC_PIN, EPD_RST_PIN, EPD_BUSY_PIN));

// Palette index -> GxEPD2 color constant. STRICTLY 6 colors; never GxEPD_ORANGE.
static uint16_t palette_to_gx(uint8_t idx) {
  switch (idx) {
    case COL_BLACK:  return GxEPD_BLACK;
    case COL_WHITE:  return GxEPD_WHITE;
    case COL_RED:    return GxEPD_RED;
    case COL_GREEN:  return GxEPD_GREEN;
    case COL_BLUE:   return GxEPD_BLUE;
    case COL_YELLOW: return GxEPD_YELLOW;
    default:         return GxEPD_WHITE;
  }
}

// Unpack a palette index: even-x -> high nibble, odd-x -> low nibble
// (must match the packing in dither.cpp).
static inline uint8_t idx_at(const uint8_t* idx, uint32_t pixel) {
  uint8_t byte = idx[pixel >> 1];
  return (pixel & 1) ? (uint8_t)(byte & 0x0F) : (uint8_t)(byte >> 4);
}

bool display_init(void) {
  SPI.begin(EPD_SPI_SCK, EPD_SPI_MISO, EPD_SPI_MOSI, EPD_SPI_SS);
  display.init(115200);
  display.setRotation(0);
  return true;
}

void display_render_index(const uint8_t* idx) {
  // 7C is paged: keep the full frame in PSRAM (idx) and emit per page.
  // We redraw the whole frame each page; GxEPD2 clips to the active page window.
  display.setFullWindow();
  display.firstPage();
  do {
    for (int y = 0; y < EPD_HEIGHT; ++y) {
      for (int x = 0; x < EPD_WIDTH; ++x) {
        uint32_t p = (uint32_t)y * EPD_WIDTH + x;
        display.drawPixel(x, y, palette_to_gx(idx_at(idx, p)));
      }
    }
  } while (display.nextPage());
  // Caller is responsible for powerOff()/hibernate() afterwards.
}

void display_show_message(const char* text) {
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);
    display.setTextSize(3);
    display.setCursor(40, EPD_HEIGHT / 2);
    display.print(text);
  } while (display.nextPage());
}

void display_power_off(void) { display.powerOff(); }
void display_hibernate(void) { display.hibernate(); }
