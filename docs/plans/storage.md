# PLAN — Storage & Playlist module (`storage.{h,cpp}`)

Branch: `plan/storage` · Depends on: `config.h`, ArduinoJson, LittleFS ·
See `docs/ROADMAP.md`.

## Responsibility
Own LittleFS and the `playlist.json` model:
- Safe mount of the gallery filesystem.
- Read/write the ordered playlist of JPEGs.
- Advance the cursor and resolve "the next image to show".
- Add/remove images on behalf of the web portal.

## Data model — `playlist.json`
```json
{
  "version": 1,
  "cursor": 0,
  "items": ["/img/0001.jpg", "/img/0002.jpg", "/img/0003.jpg"]
}
```
`cursor` is the index of the image to display next. After a successful refresh,
`main` advances it (wrap around). Persist `cursor` here so it survives even a
full power loss (RTC memory does not).

## Public interface (proposed)
```c
bool   storage_init(void);                 // LittleFS.begin(false) — NEVER true
bool   storage_load_playlist(void);        // parse FS_PLAYLIST_PATH
bool   storage_save_playlist(void);        // serialize back to FS

int    storage_count(void);
String storage_current_path(void);         // items[cursor]
String storage_advance_and_get(void);      // bump cursor (wrap), save, return path

bool   storage_add_image(const String& path);     // append, save
bool   storage_remove_image(const String& path);  // remove + delete file, save
bool   storage_reorder(const int* order, int n);  // portal drag-reorder
```

## Correctness traps this module OWNS
- [ ] **`LittleFS.begin(false)` only.** A transient mount failure must NOT
      format and wipe the user's gallery. On failure: log + report up, do not
      retry with `true`.
- [ ] **ArduinoJson v7** semantics (`JsonDocument`, no fixed `StaticJsonDocument`).
- [ ] Tolerate a **missing/empty/corrupt** playlist: synthesize an empty,
      valid document rather than crashing.
- [ ] Keep `cursor` in-bounds whenever `items` shrinks (clamp/wrap on load).
- [ ] Free `String`/doc memory promptly — this module may run in the PORTAL
      path where Wi-Fi already pressures the heap.

## Implementation checklist
1. `storage_init`: `LittleFS.begin(false)`; if false → return false (caller decides).
   Ensure `FS_IMAGE_DIR` exists (`mkdir` if needed).
2. Playlist load: open `FS_PLAYLIST_PATH`, `deserializeJson`; on error build a
   default `{version,cursor:0,items:[]}`.
3. Playlist save: `serializeJson` to a temp file then rename (atomic-ish) to
   avoid corrupting on power loss mid-write.
4. `storage_advance_and_get`: `cursor = (cursor+1) % count`, save, return path.
5. Image add/remove used by `webportal`; deletes also unlink the JPEG file.

## Test notes
- Boot with no `playlist.json` → must produce a valid empty playlist, no crash.
- Corrupt the JSON by hand → must recover, not format.
- Confirm cursor wraps and persists across a power cycle.
