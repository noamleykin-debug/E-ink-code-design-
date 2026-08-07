# Web API reference

The captive portal exposes a small JSON/HTTP API on `http://192.168.4.1`
(port 80) while a portal session is active. The web app in
[`data/www/index.html`](../data/www/index.html) is its only intended client,
but the endpoints are plain HTTP and easy to script against.

All state-changing endpoints operate on the playlist/settings files described
in [`ARCHITECTURE.md`](ARCHITECTURE.md) §5.

## Endpoints

### `POST /api/upload`
Multipart file upload. Each file must be a JPEG at exactly 800×480 (the web
app produces these; other sizes are rejected later at display time). The
filename is reduced to a safe basename and stored under `/img/`; the photo is
appended to the playlist. Re-uploading the same filename overwrites the file
(used by the crop editor's re-send).

### `GET /api/list`
Returns the playlist in display order plus the cursor (the index that shows
on the next wake):

```json
{ "images": ["/img/img_abc.jpg", "/img/img_def.jpg"], "cursor": 1 }
```

### `POST /api/reorder`
Body: a JSON array of image paths in the desired order, e.g.
`["/img/img_def.jpg", "/img/img_abc.jpg"]`. Entries not in the current
playlist are dropped; current entries missing from the body (e.g. uploaded
after the client fetched the list) are appended. The cursor keeps pointing at
the same image. Body size is capped at 16 KB.

### `POST /api/delete?file=<path>`
Deletes one photo: the file on flash and its playlist entry. The path must be
inside `/img/` and free of `..` traversal. Returns 200 on success.

### `POST /api/show?file=<path>`
Points the cursor at the given photo and schedules a graceful reboot into the
image path (the response is sent first; the AP shuts down ~1.5 s later).
Same path validation as delete.

### `GET /api/done`
"Show newest": points the cursor at the last playlist entry and schedules the
same graceful reboot. Used by the upload tab after a batch upload.

### `GET /api/settings`
Returns the current settings and the firmware's interval floor:

```json
{ "slideshow": false, "interval_sec": 3600, "interval_min_sec": 300 }
```

### `POST /api/settings?slideshow=<0|1>&interval_sec=<seconds>`
Updates settings; both parameters are optional. The interval is clamped in
firmware to `[SLIDESHOW_MIN_SEC, SLIDESHOW_MAX_SEC]` (5 minutes to 24 hours),
so no client can set a panel-damaging refresh rate.

### `GET /api/ping`
Heartbeat; returns 204. Any API call resets the portal's 3-minute inactivity
watchdog, and the web app calls this every 30 s while the page is open so the
portal doesn't shut down mid-use.

## Static routes

| Route | Purpose |
|---|---|
| `/` | The web app (`/www` on LittleFS, `index.html` default) |
| `/img/*` | Stored photos, served with `Cache-Control: public, max-age=86400` |
| `/generate_204`, `/hotspot-detect.html`, `/connecttest.txt` | OS captive-portal probes (Android, Apple, Windows) |
| anything else | Redirects to `/` (captive-portal trap, wildcard DNS) |
