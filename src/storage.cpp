#include "storage.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "config.h"

namespace Storage {

static int s_cursor = 0;
static std::vector<String> s_playlist;

static bool savePlaylist() {
    // Using ArduinoJson v7 elastic JsonDocument
    JsonDocument doc;
    doc["cursor"] = s_cursor;
    JsonArray arr = doc["images"].to<JsonArray>();
    for (const String& img : s_playlist) {
        arr.add(img);
    }

    String tempPath = String(FS_PLAYLIST_PATH) + ".tmp";
    File file = LittleFS.open(tempPath, "w");
    if (!file) {
        log_e("Failed to open temp playlist for writing");
        return false;
    }

    if (serializeJson(doc, file) == 0) {
        log_e("Failed to write playlist JSON");
        file.close();
        return false;
    }
    file.close();

    // Atomic rename to avoid corruption during power loss mid-write
    if (!LittleFS.rename(tempPath, FS_PLAYLIST_PATH)) {
        log_e("Failed to rename temp playlist");
        return false;
    }
    
    log_i("Playlist saved: %d images, cursor %d", s_playlist.size(), s_cursor);
    return true;
}

static void clampCursor() {
    if (s_playlist.empty()) {
        s_cursor = 0;
    } else if (s_cursor >= s_playlist.size()) {
        s_cursor = 0;
    } else if (s_cursor < 0) {
        s_cursor = 0;
    }
}

bool init() {
    // Requirement: LittleFS.begin(false) ONLY
    // Format-on-fail is strictly prohibited
    if (!LittleFS.begin(false)) {
        log_e("LittleFS Mount Failed");
        return false;
    }

    File file = LittleFS.open(FS_PLAYLIST_PATH, "r");
    if (!file) {
        log_w("Playlist not found, starting fresh");
        s_cursor = 0;
        s_playlist.clear();
        savePlaylist();
        return true;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) {
        log_w("Playlist JSON corrupt, recovering empty list: %s", error.c_str());
        s_cursor = 0;
        s_playlist.clear();
        savePlaylist();
        return true;
    }

    s_cursor = doc["cursor"] | 0;
    s_playlist.clear();
    
    JsonArray arr = doc["images"].as<JsonArray>();
    for (JsonVariant v : arr) {
        s_playlist.push_back(v.as<String>());
    }

    clampCursor();
    
    log_i("Storage init: %d images loaded, cursor at %d", s_playlist.size(), s_cursor);
    return true;
}

String getNextImage() {
    if (s_playlist.empty()) {
        return "";
    }
    
    clampCursor();
    String currentImage = s_playlist[s_cursor];
    
    // Advance cursor for next wake
    s_cursor++;
    clampCursor();
    
    // Persist cursor to survive deep sleep or power loss
    savePlaylist();
    
    return currentImage;
}

bool addImage(const String& filename) {
    for (const String& img : s_playlist) {
        if (img == filename) {
            return true; // Already exists
        }
    }
    s_playlist.push_back(filename);
    return savePlaylist();
}

bool removeImage(const String& filename) {
    bool found = false;
    for (auto it = s_playlist.begin(); it != s_playlist.end(); ) {
        if (*it == filename) {
            it = s_playlist.erase(it);
            found = true;
        } else {
            ++it;
        }
    }
    
    if (found) {
        // In case removal makes cursor out of bounds
        clampCursor();
        savePlaylist();
    }
    
    return found;
}

int getCursor() {
    return s_cursor;
}

std::vector<String> getPlaylist() {
    return s_playlist;
}

} // namespace Storage
