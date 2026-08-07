#include <Arduino.h>
#include <sys/time.h>
#include "config.h"
#include "power.h"
#include "storage.h"
#include "decode.h"
#include "dither.h"
#include "display.h"
#include "webportal.h"
#include <esp_heap_caps.h>

// RTC-domain persisted state survives deep sleep
RTC_DATA_ATTR uint32_t rtc_last_refresh_unix = 0;
RTC_DATA_ATTR uint32_t rtc_boot_count = 0;

static uint32_t get_unix_time() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec; // Automatically spans across deep sleeps on ESP32
}

void setup() {
    Serial.begin(115200);
    delay(500); // Allow serial monitor to attach
    
    log_i("\n=========================================");
    log_i(" E-Ink Photo Frame Firmware v%s", FW_VERSION);
    log_i(" Boot Count: %d", ++rtc_boot_count);
    log_i("=========================================\n");

    // Initialize foundational subsystems
    Power::init();
    
    // HARD REQUIREMENT: Battery Gate
    if (!Power::isBatteryOk()) {
        log_e("MAIN: Low battery detected (%u mV). Sleeping to protect hardware.",
              (unsigned)Power::getBatteryVoltageMv());
        // Pulse the display init just to issue a clean hibernate state
        // (hibernate powers the panel off before entering panel deep sleep)
        Display::init();
        Display::hibernate();

        // Deep sleep indefinitely until user recharges and triggers EXT1
        Power::deepSleep(0);
    }
    
    if (!Storage::init()) {
        log_e("MAIN: Storage failed to mount. Halting.");
        Power::deepSleep(0);
    }
    
    Power::WakeCause cause = Power::getWakeCause();
    log_i("MAIN: Wake cause decoded as %d", (int)cause);
    
    // ==========================================
    // PORTAL PATH (WIFI Tap)
    // ==========================================
    if (cause == Power::WakeCause::TOUCH_WIFI) {
        log_i("MAIN: Dispatching to PORTAL path");
        WebPortal::init();
        
        // Block and pump the portal until the 3-minute watchdog triggers
        while (!WebPortal::isFinished()) {
            WebPortal::loop();
            yield(); // Yield to FreeRTOS background tasks
        }
        
        log_i("MAIN: PORTAL path finished. Entering deep sleep.");
        Power::deepSleep(0); // Await physical touch
    }
    
    // ==========================================
    // IMAGE PATH (Advance Tap, Timer, or Cold Boot)
    // ==========================================
    log_i("MAIN: Dispatching to IMAGE path");
    
    uint32_t now = get_unix_time();
    
    // Lockout check to protect the e-ink microcapsules from rapid switching
    if (cause == Power::WakeCause::TOUCH_ADVANCE) {
        if (now - rtc_last_refresh_unix < PANEL_LOCKOUT_SEC) {
            log_w("MAIN: Ignored wake. Panel lockout active (%u sec remaining)",
                  (unsigned)(PANEL_LOCKOUT_SEC - (now - rtc_last_refresh_unix)));
            Power::deepSleep(MANDATORY_REFRESH_SEC);
        }
    }
    
    String nextImage = Storage::getNextImage();
    if (nextImage.isEmpty()) {
        log_w("MAIN: No images in playlist. Sleeping.");
        Power::deepSleep(MANDATORY_REFRESH_SEC);
    }
    
    // Memory Allocations - STRICTLY inside the Image path to prevent Portal OOM
    log_i("MAIN: Allocating %u bytes for RGB565 buffer in PSRAM", (unsigned)FRAME_RGB565_BYTES);
    uint16_t* rgb565_buffer = (uint16_t*)heap_caps_malloc(FRAME_RGB565_BYTES, MALLOC_CAP_SPIRAM);

    log_i("MAIN: Allocating %u bytes for 4bpp index buffer in PSRAM", (unsigned)FRAME_INDEX_BYTES);
    uint8_t* index_buffer = (uint8_t*)heap_caps_malloc(FRAME_INDEX_BYTES, MALLOC_CAP_SPIRAM);
    
    if (!rgb565_buffer || !index_buffer) {
        log_e("MAIN: Failed to allocate PSRAM buffers");
        if (rgb565_buffer) heap_caps_free(rgb565_buffer);
        if (index_buffer) heap_caps_free(index_buffer);
        Power::deepSleep(MANDATORY_REFRESH_SEC);
    }
    
    Display::init();
    
    // The Core Pipeline
    if (Decode::decodeFileToPSRAM(nextImage, rgb565_buffer)) {
        Dither::processFrame(rgb565_buffer, index_buffer);
        Display::paintFromIndexBuffer(index_buffer);
        rtc_last_refresh_unix = get_unix_time();
    } else {
        log_e("MAIN: Pipeline failed for %s", nextImage.c_str());
    }
    
    // Teardown & Deallocation
    heap_caps_free(rgb565_buffer);
    heap_caps_free(index_buffer);

    // hibernate() powers the panel off and then puts it into panel deep sleep;
    // no separate powerOff() call is needed (or possible) after it.
    Display::hibernate();

    log_i("MAIN: IMAGE path finished. Entering deep sleep.");
    Power::deepSleep(MANDATORY_REFRESH_SEC);
}

void loop() {
    // Left intentionally blank. The system FSM executes exclusively within setup()
    // and terminates the loop by transitioning to deep sleep.
}
