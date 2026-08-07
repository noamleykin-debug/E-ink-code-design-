#pragma once

#include <Arduino.h>
#include <vector>

namespace Storage {

// User-tunable settings, persisted in /settings.json (atomic tmp+rename).
// Values are clamped on load and on set, so callers can trust the ranges.
struct Settings {
    bool slideshowEnabled;
    uint32_t slideshowIntervalSec;   // clamped to [SLIDESHOW_MIN_SEC, SLIDESHOW_MAX_SEC]
    uint8_t ditherMode;              // Dither::DitherMode value, clamped to [0, DITHER_MODE_MAX]
};

// Initialize LittleFS and load the playlist + settings
bool init();

// Playlist operations
// Returns the current image and advances the cursor
String getNextImage();
bool addImage(const String& filename);
bool removeImage(const String& filename);
bool deleteImage(const String& filename);
void jumpToLast();
void jumpTo(const String& filename);

// Replace the playlist order. Entries not present in the current playlist are
// dropped; current entries missing from newOrder (e.g. uploaded after the UI
// fetched the list) are appended at the end. The cursor keeps pointing at the
// same image it pointed at before the reorder.
bool reorder(const std::vector<String>& newOrder);

// Settings
Settings getSettings();
bool setSettings(const Settings& s);   // clamps, persists, returns save result

// Accessors for external use (UI/debugging)
int getCursor();
std::vector<String> getPlaylist();

} // namespace Storage
