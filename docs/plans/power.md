# PLAN — Power & Sleep module (`power.{h,cpp}`)

Branch: `plan/power` · Depends on: `config.h` only · See `docs/ROADMAP.md`.

## Responsibility
Own everything about energy and wake/sleep lifecycle:
- Battery voltage measurement + the refresh gate.
- Wake-cause detection (which touch pad, timer, or cold boot).
- Configuring EXT1 and entering deep sleep cleanly.

This module is the **first** thing `main` calls after boot and the **last**
thing it calls before sleep.

## Public interface (proposed)
```c
void      power_adc_init(void);           // esp_adc_cal calibration for ADC1_CH5
uint32_t  power_read_battery_mv(void);    // averaged, ×BATT_ADC_DIVIDER, battery-side mV
bool      power_battery_ok(void);         // >= BATT_CUTOFF_MV
bool      power_battery_low_warn(void);   // < BATT_LOW_WARN_MV (still OK to refresh)

esp_sleep_wakeup_cause_t power_wake_cause(void);
int       power_wake_gpio(void);          // 1, 2, or -1  (decodes EXT1 status bitmask)

void      power_enter_deep_sleep(uint64_t timer_us /*0 = none*/);
// Internally: rtc_gpio_isolate(BATT_ADC_GPIO), configure EXT1
// (EXT1_WAKE_MASK, EXT1_WAKE_MODE), optional timer wake, esp_deep_sleep_start().
```

## Correctness traps this module OWNS
- [ ] **Read ADC before any refresh.** Expose `power_battery_ok()` so `main`
      gates the IMAGE path on it.
- [ ] **Calibrate** with `esp_adc_cal_characterize` (ADC1, `BATT_ADC_ATTEN`,
      12-bit). Convert raw→mV with `esp_adc_cal_raw_to_voltage`, then
      ×`BATT_ADC_DIVIDER`. Average `BATT_ADC_SAMPLES`.
- [ ] **Isolate GPIO6 before sleep:** `rtc_gpio_isolate(GPIO_NUM_6)` to stop
      the divider leaking current through the ADC pad during deep sleep.
- [ ] **EXT1 mask exactly** `(1ULL<<1)|(1ULL<<2)` with `ESP_EXT1_WAKEUP_ANY_HIGH`.
      Do NOT add GPIO4/5/7 (V2 reserved).
- [ ] On low battery, `main` must `display.hibernate()` then call us to sleep —
      we do not touch the display, but document the handshake.

## Implementation checklist
1. `power_adc_init`: `adc1_config_width(ADC_WIDTH_BIT_12)`,
   `adc1_config_channel_atten(BATT_ADC_CHANNEL, BATT_ADC_ATTEN)`, characterize.
2. `power_read_battery_mv`: loop `BATT_ADC_SAMPLES` × `adc1_get_raw`, average,
   `esp_adc_cal_raw_to_voltage`, multiply by divider.
3. `power_wake_cause` / `power_wake_gpio`: `esp_sleep_get_wakeup_cause`,
   `esp_sleep_get_ext1_wakeup_status` → map bit 1/2 to the action.
4. `power_enter_deep_sleep`: isolate GPIO6 → `esp_sleep_enable_ext1_wakeup` →
   optional `esp_sleep_enable_timer_wakeup` → `esp_deep_sleep_start`.

## Test notes
- Log `power_read_battery_mv()` against a bench supply through the real divider.
- Verify EXT1 status decodes the correct pad on each pad tap.
- Confirm deep-sleep current drops after `rtc_gpio_isolate`.
