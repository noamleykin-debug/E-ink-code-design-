#include "power.h"
#include "config.h"
#include <esp_sleep.h>
#include <esp_adc_cal.h>
#include <driver/adc.h>
#include <driver/rtc_io.h>

namespace Power {

static uint32_t s_battery_mv = 0;

void init() {
    // Any necessary initialization. ADC is handled on-demand.
}

WakeCause getWakeCause() {
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    
    if (cause == ESP_SLEEP_WAKEUP_TIMER) {
        return WakeCause::TIMER;
    }
    
    if (cause == ESP_SLEEP_WAKEUP_EXT1) {
        uint64_t ext1_mask = esp_sleep_get_ext1_wakeup_status();
        if (ext1_mask & (1ULL << TOUCH_ADVANCE_GPIO)) {
            return WakeCause::TOUCH_ADVANCE;
        } else if (ext1_mask & (1ULL << TOUCH_WIFI_GPIO)) {
            return WakeCause::TOUCH_WIFI;
        }
    }
    
    return WakeCause::UNKNOWN;
}

uint32_t getBatteryVoltageMv() {
    // Configure ADC1
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten((adc1_channel_t)BATT_ADC_CHANNEL, BATT_ADC_ATTEN);
    
    // Characterize ADC using eFuse Vref
    esp_adc_cal_characteristics_t adc_chars;
    esp_adc_cal_characterize(ADC_UNIT_1, BATT_ADC_ATTEN, ADC_WIDTH_BIT_12, 1100, &adc_chars);
    
    // Multisample to smooth out noise
    uint32_t raw_sum = 0;
    for (int i = 0; i < BATT_ADC_SAMPLES; i++) {
        raw_sum += adc1_get_raw((adc1_channel_t)BATT_ADC_CHANNEL);
    }
    uint32_t raw_avg = raw_sum / BATT_ADC_SAMPLES;
    
    // Convert raw reading to millivolts at the pin
    uint32_t pin_mv = esp_adc_cal_raw_to_voltage(raw_avg, &adc_chars);
    
    // Scale by the external resistor divider to get actual battery voltage
    s_battery_mv = (uint32_t)(pin_mv * BATT_ADC_DIVIDER);
    
    log_i("Battery ADC raw: %d, pin: %d mV, pack: %d mV", raw_avg, pin_mv, s_battery_mv);
    return s_battery_mv;
}

bool isBatteryOk() {
    // Ensure we have a fresh reading if it hasn't been taken yet
    if (s_battery_mv == 0) {
        getBatteryVoltageMv();
    }
    return s_battery_mv >= BATT_CUTOFF_MV;
}

void deepSleep(uint64_t sleep_sec) {
    log_i("Entering deep sleep for %llu seconds", sleep_sec);
    
    // Isolate battery ADC pin to prevent power leak through the voltage divider
    rtc_gpio_isolate((gpio_num_t)BATT_ADC_GPIO);
    
    // Configure EXT1 wakeup mask (ANY_HIGH) for capacitive touch pads
    esp_sleep_enable_ext1_wakeup(EXT1_WAKE_MASK, EXT1_WAKE_MODE);
    
    // Configure Timer wakeup if requested (positive duration)
    if (sleep_sec > 0) {
        esp_sleep_enable_timer_wakeup(sleep_sec * 1000000ULL); // microsec
    }
    
    esp_deep_sleep_start();
}

} // namespace Power
