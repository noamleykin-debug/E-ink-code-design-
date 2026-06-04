# PLAN — Main / State Machine (`main.cpp`)

Branch: `plan/main` · Depends on: **all** modules · See `docs/ROADMAP.md`.

## Responsibility
The orchestrator. On each wake it: establishes time, gates on battery, decodes
the wake reason, runs **exactly one** path (IMAGE or PORTAL or maintenance),
then returns to deep sleep. There is effectively no `loop()` for the IMAGE path
— it's a wake → act → sleep cycle.

## RTC-persisted state
```c
RTC_DATA_ATTR uint32_t boot_count;
RTC_DATA_ATTR time_t   last_refresh_unix;   // for lockout + 24h rule
RTC_DATA_ATTR bool     rtc_clock_valid;
// playlist cursor lives in playlist.json (storage), survives full power loss
```

## setup() flow
```
1. Serial/log init. boot_count++.
2. power_adc_init();  mv = power_read_battery_mv();
3. if (!power_battery_ok()):
       display_init(); display_show_message("Low battery");
       display_hibernate();
       power_enter_deep_sleep(0);          // wake only on EXT1
4. cause = power_wake_cause(); gpio = power_wake_gpio();
5. dispatch:
     - EXT1 & gpio==2 (WIFI)      -> PORTAL path
     - EXT1 & gpio==1 (ADVANCE)   -> IMAGE path (advance cursor)
     - TIMER / mandatory-refresh  -> IMAGE path (re-refresh current/next)
     - cold boot (no wake)        -> first-boot: show current or "tap WiFi to add"
6. After path completes -> power_enter_deep_sleep(timer for 24h rule).
```

## IMAGE path
```
if (now - last_refresh_unix < PANEL_LOCKOUT_SEC) -> skip refresh, sleep
storage_init();           // begin(false)
path = storage_advance_and_get();   // (or current, for timer refresh)
// allocate PSRAM: rgb565 (768K), idx (192K)
decode_init();
decode_jpeg_to_rgb565(path, rgb565, 800, 480);
dither_rgb565_to_index(rgb565, idx, 800, 480);
decode_deinit(); free(rgb565);
display_init();
display_render_index(idx);
display_power_off(); display_hibernate();
free(idx);
last_refresh_unix = now; (update RTC)
```

## PORTAL path
```
portal_start();                     // SoftAP + DNS + async server
while (!portal_should_exit()) {     // millis() watchdog OK here
    portal_loop();
    delay(short);
}
portal_stop();
// NOTE: do NOT decode/refresh here. New images take effect on next ADVANCE wake.
```

## Time handling
- Maintain RTC-domain time with `gettimeofday`. If `!rtc_clock_valid`, seed a
  baseline (e.g. 0 epoch) and set valid; absolute wall-clock isn't required —
  only **elapsed** time for lockout / 24h matters, and that survives sleep.
- 24h mandatory refresh: arm `power_enter_deep_sleep(timer_us)` with
  `MANDATORY_REFRESH_SEC` so an idle frame still refreshes daily.

## Correctness traps this module OWNS
- [ ] **Battery read BEFORE any refresh** (step 2 precedes all display work).
- [ ] **Mutually exclusive paths** — never start the portal and the decode/
      refresh pipeline in the same wake.
- [ ] **RTC-domain timing** for lockout + 24h; `millis()` only inside PORTAL.
- [ ] Always end every branch by isolating GPIO6 + EXT1 config + deep sleep
      (via `power_enter_deep_sleep`).
- [ ] Free PSRAM buffers before sleeping; verify SPIRAM baseline restored.
- [ ] Respect `PANEL_LOCKOUT_SEC` so repeated taps across sleeps don't thrash
      the panel.

## Implementation checklist
1. Wire RTC_DATA_ATTR state + time seeding.
2. Battery gate + low-batt early exit.
3. Wake dispatch switch (EXT1 gpio decode, timer, cold boot).
4. IMAGE path with PSRAM alloc/free ordering exactly as above.
5. PORTAL path loop with watchdog.
6. Single unified sleep exit.

## Test notes
- Tap ADVANCE twice quickly across a sleep → second tap respects lockout.
- Tap WIFI → portal up, no PSRAM image buffers allocated.
- Leave idle 24h (or shorten constant) → timer wake refreshes.
- Drain battery below cutoff → low-batt message, then sleep, no refresh.
