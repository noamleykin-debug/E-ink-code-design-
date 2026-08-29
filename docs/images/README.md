# Images for the main README

| File | What it shows | Used in |
|------|---------------|---------|
| `hero.jpg` | The finished frame on the wall, showing a photo | Top of `README.md` |
| `dither-closeup.jpg` | Macro of the panel — the 6-color dither pattern | Gallery |
| `portal-upload.png` | Portal Upload tab, per-file progress | Captive-portal section |
| `portal-manage.png` | Portal Manage tab, thumbnails + multi-select | Captive-portal section |
| `portal-settings.png` | Portal Settings tab, slideshow + dither style | Captive-portal section |

## Shooting notes

The panel is behind glass, so the two things that ruin a shot are **glare** and
**underexposure**. Shoot in daylight near a window rather than under a lamp, and
stand slightly off-axis so the glass doesn't mirror you or the light source.

Correcting exposure, white balance, and crop afterwards is fine and expected —
phone cameras consistently underexpose a dark panel on a white wall. What is
*not* fine is generative "enhancement": it smooths away the dither texture and
renders colors the panel physically cannot produce, which misrepresents the
hardware. The dither pattern is the point — keep it.

Phone screenshots are downscaled to 780 px wide and palette-quantized to keep
the repo small while staying crisp at README display size.
