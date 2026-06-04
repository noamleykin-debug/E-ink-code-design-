// ============================================================================
//  config.h  —  Single source of truth for the E-Ink photo frame
//  ESP32-S3-DevKitC-1 | GDEP073E01 (E6) | DESPI-C73
//
//  Hardware design is FINAL. Do not change pins/values without updating the
//  hardware doc. GPIO4 / GPIO5 / GPIO7 are reserved for V2 and intentionally
//  absent here and from the EXT1 wake mask.
// ============================================================================
#pragma once

#include <stdint.h>
#include <esp_sleep.h>
#include <hal/gpio_types.h>

// ----------------------------------------------------------------------------
//  Firmware identity
// ----------------------------------------------------------------------------
#define FW_VERSION              "0.1.0"

// ----------------------------------------------------------------------------
//  Display: GDEP073E01 (E6 7C panel) over hardware SPI via DESPI-C73
//  Bus bring-up:  SPI.begin(SCK=40, MISO=-1, MOSI=41, SS=42)
// ----------------------------------------------------------------------------
#define EPD_SPI_SCK             40
#define EPD_SPI_MISO            -1          // unused (panel is write-only)
#define EPD_SPI_MOSI            41
#define EPD_SPI_SS              42          // = CS

#define EPD_CS_PIN              42
#define EPD_DC_PIN              39
#define EPD_RST_PIN             47
#define EPD_BUSY_PIN            21

// Native panel geometry. JPEGs are pre-sized to this on the phone.
#define EPD_WIDTH               800
#define EPD_HEIGHT              480
#define EPD_PIXELS              ((uint32_t)EPD_WIDTH * EPD_HEIGHT)   // 384000

// ----------------------------------------------------------------------------
//  Capacitive touch buttons (idle-LOW, active-HIGH) -> EXT1 wake
//    GPIO1 = ADVANCE (next photo)
//    GPIO2 = WIFI     (start captive portal)
//  Wake on ANY_HIGH so either pad lifts us out of deep sleep.
// ----------------------------------------------------------------------------
#define TOUCH_ADVANCE_GPIO      GPIO_NUM_1
#define TOUCH_WIFI_GPIO         GPIO_NUM_2

#define EXT1_WAKE_MASK          ((1ULL << TOUCH_ADVANCE_GPIO) | (1ULL << TOUCH_WIFI_GPIO))
#define EXT1_WAKE_MODE          ESP_EXT1_WAKEUP_ANY_HIGH

// ----------------------------------------------------------------------------
//  Battery failsafe: GPIO6 = ADC1_CH5, external resistor divider ÷2.
//  ADC pin maxes near ~2.1V (=> ~4.2V pack). Read BEFORE every refresh;
//  isolate the pin before deep sleep with rtc_gpio_isolate(GPIO_NUM_6).
// ----------------------------------------------------------------------------
#define BATT_ADC_GPIO           GPIO_NUM_6
#define BATT_ADC_CHANNEL        ADC1_CHANNEL_5     // ADC1_CH5 == GPIO6
#define BATT_ADC_DIVIDER        2.0f               // Vbatt = Vadc * divider
#define BATT_ADC_SAMPLES        16                 // averaged per reading
#define BATT_ADC_ATTEN          ADC_ATTEN_DB_12    // full-scale ~3.1-3.3V

// Voltage gates (millivolts, at the BATTERY, i.e. after multiplying by divider).
// Below CUTOFF we refuse to refresh, hibernate the panel, and deep sleep.
#define BATT_CUTOFF_MV          3300               // hard floor: do not refresh
#define BATT_LOW_WARN_MV        3500               // soft warning threshold

// ----------------------------------------------------------------------------
//  6-color E6 palette — STRICTLY these six. No ACeP orange (mapping breaks).
//  Stored as RGB888 reference points for the Floyd-Steinberg quantizer.
//  GxEPD2 color constants are applied at emit time (display layer).
// ----------------------------------------------------------------------------
enum EpdColorIndex : uint8_t {
  COL_BLACK = 0,
  COL_WHITE,
  COL_RED,
  COL_GREEN,
  COL_BLUE,
  COL_YELLOW,
  COL_COUNT                    // == 6
};

struct RgbRef { uint8_t r, g, b; };

// Reference palette in RGB888. Tune these to the panel's measured primaries
// later; nearest-color search in the ditherer keys off this table.
static const RgbRef EPD_PALETTE[COL_COUNT] = {
  {   0,   0,   0 },   // COL_BLACK
  { 255, 255, 255 },   // COL_WHITE
  { 255,   0,   0 },   // COL_RED
  {   0, 255,   0 },   // COL_GREEN
  {   0,   0, 255 },   // COL_BLUE
  { 255, 255,   0 },   // COL_YELLOW
};

// ----------------------------------------------------------------------------
//  Timing & power policy
//
//  TRAP: cross-sleep timing (panel lockout) MUST use RTC-domain time
//  (gettimeofday + RTC-persisted timestamp), NOT millis() — millis() resets
//  every deep sleep. The Wi-Fi watchdog MAY use millis() because Wi-Fi keeps
//  the CPU awake for its whole lifetime.
// ----------------------------------------------------------------------------
// Minimum seconds between physical panel refreshes (debounce repeated taps
// across sleep cycles). Enforced via RTC clock.
#define PANEL_LOCKOUT_SEC       30

// E-Ink health: force a full refresh at least once per 24h even if idle.
#define MANDATORY_REFRESH_SEC   (24UL * 60UL * 60UL)

// Wi-Fi / captive-portal session auto-shutdown (millis is fine here).
#define WIFI_WATCHDOG_MS        (3UL * 60UL * 1000UL)   // 3 minutes

// ----------------------------------------------------------------------------
//  Networking — SoftAP captive portal
// ----------------------------------------------------------------------------
#define AP_SSID                 "EinkFrame-Setup"
#define AP_PASSWORD             ""              // open AP (captive portal)
#define AP_CHANNEL              1
#define AP_MAX_CONN             4
#define DNS_PORT                53
#define WEB_PORT                80
// Wildcard DNS target for the captive portal (SoftAP default gateway).
#define CAPTIVE_PORTAL_IP       "192.168.4.1"

// ----------------------------------------------------------------------------
//  Filesystem layout (LittleFS) — mount with LittleFS.begin(false) ONLY.
// ----------------------------------------------------------------------------
#define FS_PLAYLIST_PATH        "/playlist.json"
#define FS_IMAGE_DIR            "/img"          // JPEGs land here, e.g. /img/0007.jpg
#define FS_WEB_DIR              "/www"          // captive-portal frontend assets

// ----------------------------------------------------------------------------
//  PSRAM buffers (allocated at runtime with MALLOC_CAP_SPIRAM)
//
//  TJpg_Decoder workspace must live in PSRAM (heap_caps_malloc), not the
//  internal heap. The full image is held in PSRAM and emitted page-by-page.
// ----------------------------------------------------------------------------
#ifndef TJPG_WORKSPACE_SIZE
#define TJPG_WORKSPACE_SIZE     (64 * 1024)     // TJpg scratch (PSRAM)
#endif

// Full-frame RGB565 scratch used during decode + dither (PSRAM).
//   800 * 480 * 2 bytes = 768000 bytes
#define FRAME_RGB565_BYTES      ((size_t)EPD_PIXELS * 2)

// Packed 4-bit-per-pixel palette-index framebuffer fed to the pager (PSRAM).
//   384000 px / 2 = 192000 bytes
#define FRAME_INDEX_BYTES       ((size_t)((EPD_PIXELS + 1) / 2))
