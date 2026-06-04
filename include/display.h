// ============================================================================
//  display.h — GxEPD2 7C (GDEP073E01 / E6) paged renderer.
//  See docs/plans/display.md.
// ============================================================================
#pragma once

#include <stdint.h>

bool display_init(void);                          // SPI.begin + driver init

// Paint a full frame from a packed 4bpp palette-index buffer (PSRAM).
void display_render_index(const uint8_t* idx);

void display_show_message(const char* text);      // simple centered text
void display_power_off(void);                      // display.powerOff()
void display_hibernate(void);                      // display.hibernate()
