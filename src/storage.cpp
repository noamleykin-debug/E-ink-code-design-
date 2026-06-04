// ============================================================================
//  storage.cpp — see include/storage.h and docs/plans/storage.md
// ============================================================================
#include "storage.h"
#include "config.h"

#include <LittleFS.h>
#include <ArduinoJson.h>   // v7

static JsonDocument s_doc;     // elastic document (ArduinoJson v7)
static int          s_cursor = 0;

static void make_default_playlist(void) {
  s_doc.clear();
  s_doc["version"] = 1;
  s_doc["cursor"]  = 0;
  s_doc["items"].to<JsonArray>();
  s_cursor = 0;
}

static void clamp_cursor(void) {
  int n = storage_count();
  if (n <= 0) { s_cursor = 0; return; }
  s_cursor = ((s_cursor % n) + n) % n;   // wrap, tolerate negatives
}

bool storage_init(void) {
  // TRAP: begin(false) ONLY. A transient mount error must not wipe the gallery.
  if (!LittleFS.begin(false)) {
    return false;
  }
  if (!LittleFS.exists(FS_IMAGE_DIR)) {
    LittleFS.mkdir(FS_IMAGE_DIR);
  }
  return storage_load_playlist();
}

bool storage_load_playlist(void) {
  File f = LittleFS.open(FS_PLAYLIST_PATH, "r");
  if (!f) {
    make_default_playlist();
    return storage_save_playlist();
  }
  DeserializationError err = deserializeJson(s_doc, f);
  f.close();
  if (err) {
    // Corrupt JSON: recover with a valid empty playlist (do NOT format FS).
    make_default_playlist();
    return storage_save_playlist();
  }
  s_cursor = s_doc["cursor"] | 0;
  clamp_cursor();
  return true;
}

bool storage_save_playlist(void) {
  s_doc["cursor"] = s_cursor;

  // Write to a temp file then rename, so a power loss mid-write can't corrupt
  // the live playlist.
  const char* tmp = "/playlist.tmp";
  File f = LittleFS.open(tmp, "w");
  if (!f) return false;
  if (serializeJson(s_doc, f) == 0) { f.close(); return false; }
  f.close();

  LittleFS.remove(FS_PLAYLIST_PATH);
  return LittleFS.rename(tmp, FS_PLAYLIST_PATH);
}

int storage_count(void) {
  JsonArray items = s_doc["items"].as<JsonArray>();
  return items.isNull() ? 0 : (int)items.size();
}

String storage_current_path(void) {
  int n = storage_count();
  if (n == 0) return String();
  JsonArray items = s_doc["items"].as<JsonArray>();
  const char* p = items[s_cursor].as<const char*>();
  return p ? String(p) : String();
}

String storage_advance_and_get(void) {
  int n = storage_count();
  if (n == 0) return String();
  s_cursor = (s_cursor + 1) % n;
  storage_save_playlist();
  return storage_current_path();
}

bool storage_add_image(const String& path) {
  JsonArray items = s_doc["items"].as<JsonArray>();
  if (items.isNull()) items = s_doc["items"].to<JsonArray>();
  items.add(path);
  return storage_save_playlist();
}

bool storage_remove_image(const String& path) {
  JsonArray items = s_doc["items"].as<JsonArray>();
  if (items.isNull()) return false;
  for (size_t i = 0; i < items.size(); ++i) {
    const char* p = items[i].as<const char*>();
    if (p && path == p) {
      items.remove(i);
      LittleFS.remove(path);
      clamp_cursor();
      return storage_save_playlist();
    }
  }
  return false;
}
