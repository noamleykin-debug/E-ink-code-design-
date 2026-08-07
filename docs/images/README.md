# Images for the main README

## Already here

| File | What it shows |
|------|---------------|
| `hero.jpg` | The finished frame on the wall, displaying Van Gogh's *Starry Night*, the first thing visitors see |
| `frame-front.jpg` | Front view of the frame with a black-and-white photograph on the panel |
| `dither-closeup.jpg` | Crop into the panel showing the error-diffusion dither grain |

## Still wanted (optional)

| File | What to shoot |
|------|---------------|
| `portal-upload.png` | Phone screenshot of the portal's Upload tab, crop editor open |
| `portal-manage.png` | Phone screenshot of the Manage tab with several photos in the playlist |

Tips: shoot the frame in daylight (e-paper looks best lit from the front), and
crop phone screenshots to just the browser content. Keep committed images under
roughly 500 KB: resize the long edge to about 1600 px and save at JPEG quality
85, which is plenty for GitHub's rendering width.

Nothing in the README currently points at the two missing files, so the page has
no broken images. If you add them, wire them into the [Gallery](../../README.md#gallery)
section yourself.

---

## Adding the demo video

**Do not commit video files to the repo.** Git stores every version of a binary
forever, and a phone clip will bloat the clone for everyone. Let GitHub host it
instead. It gives you a permanent CDN URL that renders as an inline player.

1. Start a new issue on the repo. **You never have to submit it**; the upload
   happens as soon as you drop the file in.
   <https://github.com/noamleykin-debug/E-ink-code-design-/issues/new>
2. Drag the `.mp4` or `.mov` into the comment box and wait for the upload bar to
   finish. Size limits: **10 MB** per file on free accounts, 100 MB on Pro.
   `.mov` straight off an iPhone is usually way over, so re-encode first:

   ```bash
   # roughly 10x smaller: 720p wide, heavier compression, audio stripped
   ffmpeg -i IMG_2201.mov -vf "scale=720:-2" -c:v libx264 -crf 30 -an demo.mp4
   ```

   If it is still too large, cut it down as well: add `-ss 3 -t 20` to keep 20
   seconds starting at the 3-second mark.
3. GitHub replaces the file with a URL like
   `https://github.com/user-attachments/assets/<uuid>`. Copy it.
4. Close the issue draft without submitting. The uploaded file stays live.
5. Paste the URL **on its own line** in the README's *Demo video* section. A bare
   attachment URL on its own line renders as a video player. Do not wrap it in
   Markdown image or link syntax, that breaks the embed.

### If you would rather have an animated GIF

A GIF works everywhere (including places that do not render GitHub's player) and
can be committed, but keep it short and small:

```bash
ffmpeg -i demo.mp4 -vf "fps=10,scale=600:-1:flags=lanczos,split[a][b];[a]palettegen[p];[b][p]paletteuse" \
  docs/images/demo.gif
```

Check the result is under ~5 MB before committing it, then reference it in the
README the same way as any other image.
