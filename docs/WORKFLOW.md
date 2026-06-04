# WORKFLOW — Branching, PRs, and Definition of Done

This repo is built by multiple independent agents/sessions working in parallel.
These rules keep that orderly and conflict-free.

## Branch model

- **`main`** — integration branch. Always buildable. **Never committed to
  directly**; it only changes through reviewed PRs.
- **`plan/<module>`** — the home branch for one module (power, storage, display,
  dither, decode, webportal, main). All work on that module lands here first.
- **`feat/<topic>`** — net-new work that doesn't belong to an existing module.
  Cut from `main`.
- **`fix/<topic>`** — bug fixes. Cut from `main` (or from the module branch if
  the fix is module-local and not yet merged).

Always branch **from `main`**:
```bash
git checkout main && git pull
git checkout -b plan/<module>      # or feat/<topic>
```

## The one rule that prevents merge messes

**Keep each branch's changes scoped to its own files.** Module branches must only
touch their own `src/<module>.cpp`, `include/<module>.h`, `docs/plans/<module>.md`,
and `docs/prompts/<module>.md`. Shared files (`include/config.h`,
`platformio.ini`, `partitions.csv`) are changed rarely and deliberately — if you
must change one, say so loudly in the PR and expect to coordinate, because
multiple branches editing a shared file is the main source of conflicts.

Two branches that each only create/modify their own files **never conflict**, and
can be merged into `main` in any order.

## Recommended merge order

Integrate in dependency order so `main` compiles at every step:

```
power → storage → display → dither → decode → webportal → main (last)
```

`main.cpp` is merged last because it includes every module's header.

## Commit messages

- Imperative, scoped, one logical change: `Implement storage playlist save`.
- Reference the module. Keep noise out (no model names, no secrets).

## Pull requests

1. Open as **draft** into `main`.
2. Body must state: **what changed**, **why**, and **how you verified it**
   (e.g. "`pio run` passes", or hardware test notes).
3. Resolve CI before requesting review. **CI green is mandatory.**
4. Mark "Ready for review" only when the Definition of Done is met.

## Definition of Done (every PR)

- [ ] Scoped to the module's own files (no incidental edits to shared files).
- [ ] `pio run` compiles (CI green).
- [ ] Honors all Golden Rules in `CLAUDE.md` §2.
- [ ] Public interface matches `docs/plans/<module>.md` (update the plan if the
      interface intentionally changed).
- [ ] PR body documents verification.

## When to stop and ask the human

- A change would alter the finalized hardware, pinout, or partitions.
- A requested change conflicts with a Golden Rule.
- You need to edit a shared file in a way that affects other modules.
- A library API differs from what the plan assumed and the right fix is ambiguous.
