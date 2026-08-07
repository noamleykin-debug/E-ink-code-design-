# Development guide

Branching, pull requests, and the definition of done for this repo.

## Building

Requires [PlatformIO](https://platformio.org/).

```bash
pio run                 # compile (the primary "does it build?" check)
pio run -t upload       # flash firmware over USB
pio run -t uploadfs     # upload the LittleFS image (web app in data/www)
pio device monitor      # serial console @ 115200
pio check               # static analysis (optional)
```

Firmware and filesystem are flashed separately: changes under `data/www/`
only reach the device via `uploadfs`.

## Branch model

- **`main`**: integration branch. Always buildable. Never committed to
  directly; it only changes through reviewed PRs.
- **`feat/<topic>`**: new features, cut from `main`.
- **`fix/<topic>`**: bug fixes, cut from `main`.

During the initial build-out each firmware module also had a dedicated
`plan/<module>` branch (one module = one branch = one PR); see
[`docs/ai/`](ai/) for how that workflow was run.

## Keeping merges clean

Keep each branch's changes scoped to the files it owns. The shared files
(`include/config.h`, `platformio.ini`, `partitions.csv`) change rarely and
deliberately. If a PR must touch one, call it out in the PR body, because
parallel edits to shared files are the main source of conflicts.

## Pull requests

1. Open as **draft** into `main`.
2. The body states **what changed**, **why**, and **how it was verified**
   (e.g. "`pio run` passes", or hardware test notes).
3. CI compiles every PR (`.github/workflows/build.yml`). **CI green is
   mandatory** before review.
4. Mark ready for review only when the definition of done is met.

## Definition of done

- [ ] Scoped to the files the change owns (no incidental edits to shared files).
- [ ] `pio run` compiles; CI green.
- [ ] Honors the Golden Rules in [`CLAUDE.md`](../CLAUDE.md) §2; they encode
      hardware and physics constraints, not preferences.
- [ ] Documentation updated if the behavior or API changed
      ([`ARCHITECTURE.md`](ARCHITECTURE.md), [`API.md`](API.md)).
- [ ] PR body documents verification.

## When to stop and ask

- A change would alter the finalized hardware, pinout, or partitions.
- A requested change conflicts with a Golden Rule.
- A shared file needs editing in a way that affects other modules.
- A library API differs from what was assumed and the right fix is ambiguous.
