#include "storage.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "config.h"

namespace Storage {

static int s_cursor = 0;
static std::vector<String> s_playlist;
static Settings s_settings = { SLIDESHOW_DEFAULT_ENABLED, SLIDESHOW_DEFAULT_SEC };

static void clampSettings(Settings& s) {
    if (s.slideshowIntervalSec < SLIDESHOW_MIN_SEC) s.slideshowIntervalSec = SLIDESHOW_MIN_SEC;
    if (s.slideshowIntervalSec > SLIDESHOW_MAX_SEC) s.slideshowIntervalSec = SLIDESHOW_MAX_SEC;
}

static bool saveSettings() {
    JsonDocument doc;
    doc["slideshow"] = s_settings.slideshowEnabled;
    doc["interval_sec"] = s_settings.slideshowIntervalSec;

    String tempPath = String(FS_SETTINGS_PATH) + ".tmp";
    File file = LittleFS.open(tempPath, "w");
    if (!file) {
        log_e("Failed to open temp settings for writing");
        return false;
    }
    if (serializeJson(doc, file) == 0) {
        log_e("Failed to write settings JSON");
        file.close();
        return false;
    }
    file.close();

    // Atomic rename, same pattern as the playlist
    if (!LittleFS.rename(tempPath, FS_SETTINGS_PATH)) {
        log_e("Failed to rename temp settings");
        return false;
    }
    log_i("Settings saved: slideshow=%d interval=%u s",
          (int)s_settings.slideshowEnabled, (unsigned)s_settings.slideshowIntervalSec);
    return true;
}

static void loadSettings() {
    s_settings = { SLIDESHOW_DEFAULT_ENABLED, SLIDESHOW_DEFAULT_SEC };

    File file = LittleFS.open(FS_SETTINGS_PATH, "r");
    if (!file) {
        log_i("No settings file, using defaults");
        return;
    }
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    if (error) {
        log_w("Settings JSON corrupt (%s), using defaults", error.c_str());
        return;
    }
    s_settings.slideshowEnabled = doc["slideshow"] | SLIDESHOW_DEFAULT_ENABLED;
    s_settings.slideshowIntervalSec = doc["interval_sec"] | (uint32_t)SLIDESHOW_DEFAULT_SEC;
    clampSettings(s_settings);
    log_i("Settings loaded: slideshow=%d interval=%u s",
          (int)s_settings.slideshowEnabled, (unsigned)s_settings.slideshowIntervalSec);
}

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
    
    log_i("Playlist saved: %u images, cursor %d", (unsigned)s_playlist.size(), s_cursor);
    return true;
}

static void clampCursor() {
    if (s_playlist.empty()) {
        s_cursor = 0;
    } else if (s_cursor >= (int)s_playlist.size()) {
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

    // Ensure the image directory exists. LittleFS.open(path, "w") fails silently
    // if the parent dir is missing, which would drop every upload while still
    // recording phantom entries in playlist.json.
    if (!LittleFS.exists(FS_IMAGE_DIR)) {
        LittleFS.mkdir(FS_IMAGE_DIR);
    }

    loadSettings();

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

    log_i("Storage init: %u images loaded, cursor at %d", (unsigned)s_playlist.size(), s_cursor);
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
    // Iterate backwards so erasing doesn't shift the indices still to visit,
    // and shift the cursor down for every removed entry in front of it so it
    // keeps pointing at the same upcoming image.
    for (int i = (int)s_playlist.size() - 1; i >= 0; i--) {
        if (s_playlist[i] == filename) {
            s_playlist.erase(s_playlist.begin() + i);
            found = true;
            if (i < s_cursor) s_cursor--;
        }
    }

    if (found) {
        // In case removal makes cursor out of bounds
        clampCursor();
        savePlaylist();
    }

    return found;
}

bool reorder(const std::vector<String>& newOrder) {
    // Remember which image the cursor points at so the reorder doesn't change
    // what shows on the next wake.
    String current = (s_cursor >= 0 && s_cursor < (int)s_playlist.size())
                         ? s_playlist[s_cursor] : String();

    auto contains = [](const std::vector<String>& v, const String& s) {
        for (const String& e : v) if (e == s) return true;
        return false;
    };

    std::vector<String> rebuilt;
    rebuilt.reserve(s_playlist.size());

    // Take the client's order, but only entries that actually exist in the
    // current playlist (drops stale/foreign paths), deduplicated.
    for (const String& p : newOrder) {
        if (contains(s_playlist, p) && !contains(rebuilt, p)) {
            rebuilt.push_back(p);
        }
    }
    // Append anything the client didn't know about (e.g. a photo uploaded
    // after the UI fetched the list) so nothing silently disappears.
    for (const String& p : s_playlist) {
        if (!contains(rebuilt, p)) {
            rebuilt.push_back(p);
        }
    }

    s_playlist = rebuilt;

    s_cursor = 0;
    if (!current.isEmpty()) {
        for (size_t i = 0; i < s_playlist.size(); i++) {
            if (s_playlist[i] == current) { s_cursor = (int)i; break; }
        }
    }
    clampCursor();

    return savePlaylist();
}

Settings getSettings() {
    return s_settings;
}

bool setSettings(const Settings& s) {
    s_settings = s;
    clampSettings(s_settings);
    return saveSettings();
}

void jumpToLast() {
    if (!s_playlist.empty()) {
        s_cursor = (int)s_playlist.size() - 1;
        savePlaylist();
    }
}

void jumpTo(const String& filename) {
    for (size_t i = 0; i < s_playlist.size(); i++) {
        if (s_playlist[i] == filename) {
            s_cursor = i;
            savePlaylist();
            break;
        }
    }
}

bool deleteImage(const String& filename) {
    // Erase the physical JPEG from flash, not just the playlist entry. A
    // playlist-only removal would orphan the file and slowly fill LittleFS.
    // An already-missing file counts as success so the list can self-heal.
    bool fileGone = !LittleFS.exists(filename) || LittleFS.remove(filename);
    bool listGone = removeImage(filename); // drops entry, clamps cursor, saves
    return fileGone && listGone;
}

int getCursor() {
    return s_cursor;
}

std::vector<String> getPlaylist() {
    return s_playlist;
}

} // namespace Storage
