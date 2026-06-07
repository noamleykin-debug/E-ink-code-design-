#pragma once
#include <Arduino.h>

namespace WebPortal {

// Initializes SoftAP, DNS, and AsyncWebServer
void init();

// Called repeatedly from main() to pump DNS requests and monitor inactivity
void loop();

// Returns true if the WIFI_WATCHDOG_MS timeout has elapsed
bool isFinished();

} // namespace WebPortal
