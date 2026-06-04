// ============================================================================
//  main.cpp — wake dispatch + orchestration. See docs/plans/main.md.
//
//  Wake -> establish time -> battery gate -> pick ONE path -> deep sleep.
//  IMAGE path and PORTAL path are mutually exclusive (PSRAM/heap contention).
// ============================================================================
#include <Arduino.h>
#include <sys/time.h>
#include <esp_heap_caps.h>

#include "config.h"
#include "power.h"
#include "storage.h"
#include "decode.h"
#include "dither.h"
#include "display.h"
#include "webportal.h"

// ---- RTC-persisted state (survives deep sleep) -----------------------------
RTC_DATA_ATTR static uint32_t rtc_boot_count        = 0;
RTC_DATA_ATTR static time_t   rtc_last_refresh_unix  = 0;
RTC_DATA_ATTR static bool     rtc_clock_valid        = false;

static time_t now_unix(void) {
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  return tv.tv_sec;
}

static void ensure_clock(void) {
  // Absolute wall-clock isn't needed; only elapsed time (which the RTC keeps
  // across deep sleep) matters for the lockout and the 24h rule.
  if (!rtc_clock_valid) {
    struct timeval tv = { .tv_sec = 0, .tv_usec = 0 };
    settimeofday(&tv, nullptr);
    rtc_clock_valid = true;
    rtc_last_refresh_unix = 0;
  }
}

static void sleep_now(void) {
  // Re-arm the 24h mandatory refresh as a timer wake alongside EXT1.
  power_enter_deep_sleep((uint64_t)MANDATORY_REFRESH_SEC * 1000000ULL);
}

// ---- IMAGE path ------------------------------------------------------------
static void run_image_path(bool advance) {
  time_t now = now_unix();

  // Panel lockout: debounce repeated taps across sleep cycles (RTC-domain).
  if (advance && rtc_last_refresh_unix != 0 &&
      (now - rtc_last_refresh_unix) < PANEL_LOCKOUT_SEC) {
    return;   // too soon; skip refresh
  }

  if (!storage_init()) {
    display_init();
    display_show_message("Storage error");
    display_hibernate();
    return;
  }

  String path = advance ? storage_advance_and_get() : storage_current_path();
  if (path.length() == 0) {
    display_init();
    display_show_message("No photos. Tap Wi-Fi to add.");
    display_hibernate();
    return;
  }

  // Allocate the big PSRAM buffers only on the IMAGE path.
  uint16_t* rgb565 = (uint16_t*)heap_caps_malloc(FRAME_RGB565_BYTES, MALLOC_CAP_SPIRAM);
  uint8_t*  idx    = (uint8_t*) heap_caps_malloc(FRAME_INDEX_BYTES,  MALLOC_CAP_SPIRAM);
  if (!rgb565 || !idx) {
    if (rgb565) heap_caps_free(rgb565);
    if (idx)    heap_caps_free(idx);
    display_init();
    display_show_message("Out of memory");
    display_hibernate();
    return;
  }

  bool ok = decode_init() &&
            decode_jpeg_to_rgb565(path.c_str(), rgb565, EPD_WIDTH, EPD_HEIGHT);
  decode_deinit();

  if (ok) {
    dither_rgb565_to_index(rgb565, idx, EPD_WIDTH, EPD_HEIGHT);
    heap_caps_free(rgb565);
    rgb565 = nullptr;

    display_init();
    display_render_index(idx);
    display_power_off();
    display_hibernate();

    rtc_last_refresh_unix = now;     // record refresh time (RTC-domain)
  } else {
    heap_caps_free(rgb565);
    rgb565 = nullptr;
    display_init();
    display_show_message("Decode failed");
    display_hibernate();
  }

  if (idx) heap_caps_free(idx);
}

// ---- PORTAL path -----------------------------------------------------------
static void run_portal_path(void) {
  // No image buffers here — Wi-Fi gets the heap to itself.
  if (!storage_init()) {
    // Still start the portal so the user can see/upload, but log the failure.
  }
  portal_start();
  while (!portal_should_exit()) {
    portal_loop();
    delay(10);
  }
  portal_stop();
}

void setup() {
  Serial.begin(115200);
  rtc_boot_count++;
  ensure_clock();

  // TRAP: read battery BEFORE any refresh.
  power_adc_init();
  if (!power_battery_ok()) {
    display_init();
    display_show_message("Low battery");
    display_hibernate();
    sleep_now();
    return;
  }

  esp_sleep_wakeup_cause_t cause = power_wake_cause();
  int gpio = power_wake_gpio();

  if (cause == ESP_SLEEP_WAKEUP_EXT1 && gpio == (int)TOUCH_WIFI_GPIO) {
    run_portal_path();                 // WIFI tap
  } else if (cause == ESP_SLEEP_WAKEUP_EXT1 && gpio == (int)TOUCH_ADVANCE_GPIO) {
    run_image_path(/*advance=*/true);  // ADVANCE tap
  } else if (cause == ESP_SLEEP_WAKEUP_TIMER) {
    run_image_path(/*advance=*/true);  // 24h mandatory refresh -> next image
  } else {
    run_image_path(/*advance=*/false); // cold boot -> show current
  }

  sleep_now();
}

void loop() {
  // Unused: every wake completes in setup() and returns to deep sleep.
}
