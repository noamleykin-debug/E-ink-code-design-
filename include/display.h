#pragma once
#include <Arduino.h>

namespace Display {

// Initialize SPI and the GxEPD2 panel driver
void init();

// Renders the image to the panel using paged updates.
// The index_buffer must be exactly FRAME_INDEX_BYTES large,
// containing packed 4-bit per pixel indices mapping to EpdColorIndex.
void paintFromIndexBuffer(const uint8_t* index_buffer);

// Safely power down the panel driver to prevent damage
void hibernate();
void powerOff();

} // namespace Display
