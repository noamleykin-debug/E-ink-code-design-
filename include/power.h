// ============================================================================
//  power.h — Battery gate, wake-cause decode, and deep-sleep entry.
//  See docs/plans/power.md.
// ============================================================================
#pragma once

#include <stdint.h>
#include <esp_sleep.h>

// Characterize ADC1_CH5 (GPIO6) via esp_adc_cal. Safe to call more than once.
void power_adc_init(void);

// Averaged battery voltage in millivolts (battery side, i.e. ×BATT_ADC_DIVIDER).
uint32_t power_read_battery_mv(void);

// True if battery is above the hard cutoff and a refresh is allowed.
bool power_battery_ok(void);

// True if battery is below the soft warning threshold (still OK to refresh).
bool power_battery_low_warn(void);

// Raw wake cause from the last deep sleep.
esp_sleep_wakeup_cause_t power_wake_cause(void);

// If woken by EXT1, returns the GPIO that triggered (1 = advance, 2 = wifi),
// otherwise -1.
int power_wake_gpio(void);

// Isolate GPIO6, arm EXT1 (and optional timer), then enter deep sleep.
// timer_us == 0 disables the timer wake.
void power_enter_deep_sleep(uint64_t timer_us);
