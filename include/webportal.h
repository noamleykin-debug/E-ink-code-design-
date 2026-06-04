// ============================================================================
//  webportal.h — SoftAP captive portal + async uploads. See docs/plans/webportal.md.
//  Runs ALONE: never while decode/refresh is active.
// ============================================================================
#pragma once

void portal_start(void);        // SoftAP + DNS + routes; seeds the watchdog
void portal_loop(void);         // process DNS; (watchdog uses millis())
bool portal_should_exit(void);  // true after WIFI_WATCHDOG_MS of inactivity
void portal_stop(void);         // tear down server + AP
