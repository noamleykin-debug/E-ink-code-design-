# Hardware

> **Status: documentation in progress.** The electronics and enclosure are
> built and working; the write-ups below are being added.

## Overview

| Part | Notes |
|---|---|
| ESP32-S3-DevKitC-1 | 16 MB flash, 8 MB OPI PSRAM |
| Good Display GDEP073E01 | 7.3", 800×480, 6-color (E6 / Spectra 6) e-paper panel |
| DESPI-C73 | Panel adapter board (SPI) |
| 2× capacitive touch pads | Wake pads: GPIO1 = next photo, GPIO2 = Wi-Fi portal |
| LiPo battery + buck-boost converter | Untethered power |

The complete pin map lives in [`include/config.h`](../../include/config.h);
it is the single source of truth the firmware builds on.

## Planned documentation

- [ ] Wiring diagram / schematic
- [ ] Bill of materials with part links
- [ ] Power measurements (deep-sleep current, refresh energy, battery life)
- [ ] Battery sense divider (GPIO6) install + calibration notes
- [ ] Assembly guide

## Enclosure & 3D design

The frame's enclosure was designed and 3D printed by **Noam Shmazion**, who
turned a set of rough requirements into a clean, print-ready design and did
an excellent job of it. Enclosure documentation (models, print settings,
assembly) will be added here and credited accordingly.
