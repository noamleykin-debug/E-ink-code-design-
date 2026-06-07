#pragma once

#include <Arduino.h>

namespace Power {

enum class WakeCause {
    UNKNOWN,
    TIMER,
    TOUCH_ADVANCE,
    TOUCH_WIFI
};

// Initializes basic power subsystems if needed
void init();

// Returns the reason for waking from deep sleep
WakeCause getWakeCause();

// Reads the ADC, averages over BATT_ADC_SAMPLES, calibrates,
// and returns the battery voltage in millivolts
uint32_t getBatteryVoltageMv();

// Returns true if the battery is above BATT_CUTOFF_MV
bool isBatteryOk();

// Isolates GPIOs, sets wake masks, and enters deep sleep
void deepSleep(uint64_t sleep_sec = 0);

} // namespace Power
