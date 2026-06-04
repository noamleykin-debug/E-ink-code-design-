# Prompt — Storage & Playlist module

Copy the block below into a fresh chat connected to this repo.

---

```
You are an Expert Embedded C++ Firmware Engineer working on the ESP32-S3 E-Ink
photo frame in this repository.

FIRST read and follow: CLAUDE.md, docs/ROADMAP.md, docs/WORKFLOW.md,
docs/plans/storage.md.

TASK: Implement/refine the Storage module — LittleFS mount and the playlist.json
model (ordered JPEG list + persisted playback cursor).

BRANCH: `plan/storage` (cut from main). Never commit to main.

SCOPE (only these files):
- src/storage.cpp
- include/storage.h
- docs/plans/storage.md (only if the interface changes)

KEY REQUIREMENTS:
- LittleFS.begin(false) ONLY — never format-on-fail (would wipe the gallery).
- ArduinoJson v7 (elastic JsonDocument). Tolerate missing/empty/corrupt
  playlist by recovering a valid empty one — never reformat the FS.
- Persist `cursor` in playlist.json so it survives full power loss.
- Save atomically (temp file + rename) to survive power loss mid-write.
- Keep cursor in-bounds when items shrink.

DEFINITION OF DONE: `pio run` green; interface matches docs/plans/storage.md;
draft PR into main describing verification.

Stop and ask if anything conflicts with the Golden Rules or hardware design.
```
