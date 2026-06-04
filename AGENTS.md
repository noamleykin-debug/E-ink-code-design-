# AGENTS.md

Entry point for autonomous coding agents (Claude Code, and any tool that reads
`AGENTS.md`). This repo's full operating manual is **[`CLAUDE.md`](./CLAUDE.md)** —
read it before doing anything.

## TL;DR for agents

- **Project:** ESP32-S3 firmware for a 6-color E-Ink photo frame. Hardware is
  FINAL; constants live in `include/config.h` (source of truth).
- **Before coding:** read `CLAUDE.md` (Golden Rules), `docs/ROADMAP.md`, and your
  task's `docs/plans/<module>.md` + `docs/prompts/<module>.md`.
- **Workflow:** one module = one branch (`plan/<module>`) = one draft PR into
  `main`. **Never commit to `main` directly.** See `docs/WORKFLOW.md`.
- **Build check:** `pio run` must pass. CI runs it on every PR.

## Hard constraints (do not violate — see CLAUDE.md §2 for the full list)

- 6 colors only, never orange.
- `LittleFS.begin(false)` only.
- RTC-domain timing across sleep, not `millis()`.
- Battery ADC read before any refresh; isolate GPIO6 before sleep.
- Wi-Fi and decode/refresh never run together.
- Big buffers in PSRAM via `heap_caps_malloc`.
- GPIO4/5/7 are reserved for V2.

If a requested change conflicts with these or alters the finalized hardware,
stop and ask the human.
