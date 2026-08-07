#pragma once

#include <Arduino.h>
#include <vector>

namespace Storage {

// Initialize LittleFS and load the playlist
bool init();

// Playlist operations
// Returns the current image and advances the cursor
String getNextImage();
bool addImage(const String& filename);
bool removeImage(const String& filename);
bool deleteImage(const String& filename);
void jumpToLast();
void jumpTo(const String& filename);

// Point the cursor at the newest image so the next getNextImage() returns it.
// Used by the portal's optional "Show newest" action before a reboot.
void jumpToLast();

// Delete an image: erases both the JPEG from flash and its playlist entry.
bool deleteImage(const String& filename);

// Accessors for external use (UI/debugging)
int getCursor();
std::vector<String> getPlaylist();

} // namespace Storage
