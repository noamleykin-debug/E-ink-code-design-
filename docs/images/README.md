# Images for the main README

| File | What it shows | Used in |
|------|---------------|---------|
| `hero.jpg` | The finished frame on the wall, showing a photo | Top of `README.md` |
| `dither-closeup.jpg` | Macro of the panel — the 6-color dither pattern | Gallery |
| `portal-tabs.png` | All three portal tabs side by side: Upload, Manage, Settings | Captive-portal section |

## Shooting notes

The panel is behind glass, so the two things that ruin a shot are **glare** and
**underexposure**. Shoot in daylight near a window rather than under a lamp, and
stand slightly off-axis so the glass doesn't mirror you or the light source.

Correcting exposure, white balance, and crop afterwards is fine and expected —
phone cameras consistently underexpose a dark panel on a white wall. What is
*not* fine is generative "enhancement": it smooths away the dither texture and
renders colors the panel physically cannot produce, which misrepresents the
hardware. The dither pattern is the point — keep it.

## Why the tab screenshots are one combined image

`portal-tabs.png` is a single strip of all three tabs rather than three separate
files in table cells. Three tall phone screenshots side by side make a markdown
table far wider than a phone screen; the GitHub mobile app then overflows the
table horizontally and won't load images in the clipped columns until you tap
them, so most of them render as broken-image placeholders.

One image sidesteps that entirely — it scales to any viewport and needs no
table. If you replace a tab screenshot, rebuild the strip rather than adding
individual files back into the table. Its gutters are transparent so it sits
correctly on both GitHub light and dark themes; keep that if you regenerate it
(`Image.quantize(method=FASTOCTREE)` preserves alpha, `MEDIANCUT` does not).

Phone screenshots are downscaled to 560 px per panel and palette-quantized to
keep the repo small while staying crisp at README display size.
