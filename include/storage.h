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

// Accessors for external use (UI/debugging)
int getCursor();
std::vector<String> getPlaylist();

} // namespace Storage
