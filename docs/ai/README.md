# AI-assisted development

This project was built with AI coding agents (Claude Code) working under
human direction, and this folder documents how that was organized. Nothing
about it is hidden: the process is part of the project.

## How it was run

The core idea was to make the AI operate like a disciplined team member
instead of a code firehose:

1. **An operating manual, not vibes.** The repo root carries
   [`CLAUDE.md`](../../CLAUDE.md), a contract every agent session reads first.
   Its "Golden Rules" encode the hardware and physics constraints (the
   6-color palette limit, PSRAM-only big buffers, the Wi-Fi/refresh mutual
   exclusion, e-paper refresh discipline) so no session could "helpfully"
   break the hardware. [`AGENTS.md`](../../AGENTS.md) is the short entry
   point for any tool that reads that convention.

2. **One module, one branch, one PR.** Each firmware module (power, storage,
   decode, dither, display, webportal, main) was specified and implemented
   independently, then integrated through reviewed pull requests with CI
   compiling every one. The [`prompts/`](prompts/) folder preserves the
   actual per-module prompts that were handed to fresh agent sessions.

3. **A human in the loop for everything irreversible.** Hardware decisions,
   feature scope, and merges stayed human-owned. Field testing on the real
   device fed bugs back in (the portal watchdog killing active sessions and
   the thumbnail-concurrency overload were both found by using the frame,
   not by review).

## What's in this folder

| Path | Contents |
|---|---|
| [`prompts/`](prompts/) | The per-module prompt given to each agent session, plus the template they were written from |

The prompts are preserved verbatim, exactly as they were run. They reference
documentation paths from before the repo was reorganized (`docs/ROADMAP.md`,
`docs/WORKFLOW.md`, `docs/plans/`), which today correspond to
[`ARCHITECTURE.md`](../ARCHITECTURE.md) and [`DEVELOPMENT.md`](../DEVELOPMENT.md).

The manual itself ([`CLAUDE.md`](../../CLAUDE.md)) and the agent entry point
([`AGENTS.md`](../../AGENTS.md)) stay at the repo root because that's where
agent tooling looks for them.
