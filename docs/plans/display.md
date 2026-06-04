# PLAN — Display module (`display.{h,cpp}`)

Branch: `plan/display` · Depends on: `config.h`, GxEPD2 · See `docs/ROADMAP.md`.

## Responsibility
Own the GDEP073E01 (E6 7C) panel via GxEPD2:
- SPI bring-up and driver init.
- **Paged** emission of the full 4bpp index frame from PSRAM.
- Panel power discipline (`powerOff`, `hibernate`) and the 24h refresh rule.

## Driver selection
```c
#include <GxEPD2_7C.h>
// GxEPD2_730c_GDEP073E01, 800x480, 7-color class driver used for the E6 panel.
GxEPD2_7C<GxEPD2_730c_GDEP073E01, GxEPD2_730c_GDEP073E01::HEIGHT>
  display(GxEPD2_730c_GDEP073E01(EPD_CS_PIN, EPD_DC_PIN, EPD_RST_PIN, EPD_BUSY_PIN));
```

## Public interface (proposed)
```c
bool display_init(void);                       // SPI.begin + display.init
void display_render_index(const uint8_t* idx); // paged paint from 4bpp PSRAM frame
void display_show_message(const char* text);   // simple text (first-boot/errors)
void display_power_off(void);                  // display.powerOff()
void display_hibernate(void);                  // display.hibernate() (lowest draw)
```

## Paged emit (the core of this module)
GxEPD2 7C cannot hold 800×480×color in MCU RAM, so it paints in **pages**:
```c
display.setFullWindow();
display.firstPage();
do {
  // for each pixel in the current page window:
  //   idx = unpack nibble from `idx` buffer
  //   display.drawPixel(x, y, palette_to_gx(idx));
} while (display.nextPage());
```
- The full frame lives in our PSRAM `idx` buffer; each page iteration reads the
  slice it needs. `MAX_DISPLAY_BUFFER_SIZE` (set in platformio.ini) bounds the
  per-page RAM.

## Palette → GxEPD2 color map
```c
// COL_BLACK→GxEPD_BLACK, COL_WHITE→GxEPD_WHITE, COL_RED→GxEPD_RED,
// COL_GREEN→GxEPD_GREEN, COL_BLUE→GxEPD_BLUE, COL_YELLOW→GxEPD_YELLOW
```

## Correctness traps this module OWNS
- [ ] **6 colors only** — the map above must never emit `GxEPD_ORANGE`.
- [ ] **Paged**: keep full frame in PSRAM, emit per page; never allocate a full
      framebuffer in internal RAM.
- [ ] `SPI.begin(EPD_SPI_SCK, EPD_SPI_MISO, EPD_SPI_MOSI, EPD_SPI_SS)` before
      `display.init`.
- [ ] **No rapid updates** — one full refresh per wake; E-Ink needs patience.
- [ ] After painting, `powerOff()`; before deep sleep, `hibernate()`.
- [ ] Support the **24h mandatory refresh** (driven by `main` via RTC time) so
      the panel stays healthy even when idle.
- [ ] Nibble unpack order must match what `plan/dither` packed.

## Implementation checklist
1. `display_init`: `SPI.begin(...)`; `display.init(115200)`; set rotation/origin.
2. `palette_to_gx(idx)` lookup table.
3. `display_render_index`: full-window paged loop with per-page coordinate math
   reading the 4bpp buffer.
4. `display_show_message`: Adafruit_GFX text for first-boot / low-batt / errors.
5. Power helpers wrap `powerOff`/`hibernate`.

## Test notes
- Bring-up: paint solid Black, then White, then each primary full-screen.
- Paint a 6-vertical-stripe test (one stripe per palette color) to confirm the
  color map and nibble unpacking.
- Confirm BUSY handling: refresh returns only after the panel finishes.
