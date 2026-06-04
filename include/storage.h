// ============================================================================
//  storage.h — LittleFS mount + playlist.json model. See docs/plans/storage.md.
// ============================================================================
#pragma once

#include <Arduino.h>

// Mount LittleFS (begin(false) — NEVER format) and load the playlist.
// Returns false on mount failure (caller decides what to do; do NOT reformat).
bool storage_init(void);

bool storage_load_playlist(void);
bool storage_save_playlist(void);

int    storage_count(void);
String storage_current_path(void);      // items[cursor], or "" if empty
String storage_advance_and_get(void);   // bump cursor (wrap), persist, return path

bool storage_add_image(const String& path);     // append + persist
bool storage_remove_image(const String& path);  // remove entry + delete file
