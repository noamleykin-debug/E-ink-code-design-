// ============================================================================
//  power.cpp — see include/power.h and docs/plans/power.md
// ============================================================================
#include "power.h"
#include "config.h"

#include <Arduino.h>
#include <driver/adc.h>
#include <esp_adc_cal.h>
#include <driver/rtc_io.h>

static esp_adc_cal_characteristics_t s_adc_chars;
static bool s_adc_ready = false;

void power_adc_init(void) {
  if (s_adc_ready) return;
  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten(BATT_ADC_CHANNEL, BATT_ADC_ATTEN);
  // Uses eFuse Two-Point / Vref calibration when present, else the default
  // reference (1100 mV). This is the required esp_adc_cal calibration step.
  esp_adc_cal_characterize(ADC_UNIT_1, BATT_ADC_ATTEN, ADC_WIDTH_BIT_12,
                           1100, &s_adc_chars);
  s_adc_ready = true;
}

uint32_t power_read_battery_mv(void) {
  if (!s_adc_ready) power_adc_init();

  uint32_t acc = 0;
  for (int i = 0; i < BATT_ADC_SAMPLES; ++i) {
    acc += (uint32_t)adc1_get_raw(BATT_ADC_CHANNEL);
  }
  uint32_t raw = acc / BATT_ADC_SAMPLES;

  uint32_t mv_at_pin = esp_adc_cal_raw_to_voltage(raw, &s_adc_chars);
  return (uint32_t)(mv_at_pin * BATT_ADC_DIVIDER);
}

bool power_battery_ok(void) {
  return power_read_battery_mv() >= BATT_CUTOFF_MV;
}

bool power_battery_low_warn(void) {
  return power_read_battery_mv() < BATT_LOW_WARN_MV;
}

esp_sleep_wakeup_cause_t power_wake_cause(void) {
  return esp_sleep_get_wakeup_cause();
}

int power_wake_gpio(void) {
  if (esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_EXT1) return -1;
  uint64_t status = esp_sleep_get_ext1_wakeup_status();
  if (status & (1ULL << TOUCH_ADVANCE_GPIO)) return (int)TOUCH_ADVANCE_GPIO;
  if (status & (1ULL << TOUCH_WIFI_GPIO))    return (int)TOUCH_WIFI_GPIO;
  return -1;
}

void power_enter_deep_sleep(uint64_t timer_us) {
  // Stop the resistor divider from leaking current through the ADC pad.
  rtc_gpio_isolate(BATT_ADC_GPIO);

  // Wake on either capacitive pad (GPIO1 advance / GPIO2 wifi).
  esp_sleep_enable_ext1_wakeup(EXT1_WAKE_MASK, EXT1_WAKE_MODE);

  if (timer_us > 0) {
    esp_sleep_enable_timer_wakeup(timer_us);
  }

  esp_deep_sleep_start();
}
