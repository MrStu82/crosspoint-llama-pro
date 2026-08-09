# X4 Pro UI Layout Fixes

## What This Is

CrossPoint firmware fixes for the Xteink X4 Pro e-reader (480×800 1-bit e-ink,
GT911 touch, ESP32-C3): four defects in the touch-device layout path
(button-hints reservation, home-menu overflow, screen margin, dual status
bars, home bottom buffer, USB mass storage) called out in Stuart's device
review of the current build.

## Core Value

Every screen renders correctly and fully on-panel on the X4 Pro, with no
clipped/overflowing content and no touch targets crowded against the bezel.

## Requirements

### Validated

(None yet — ship to validate)

### Active

- [ ] Button-hints row no longer reserves space it doesn't draw on touch devices
- [ ] Home menu clamps to available height, scrolls, shows a chevron when more rows exist
- [ ] Reader screen margin defaults to a sane value for new units
- [ ] Reader screens support independent top + bottom status bars
- [ ] Home menu's last row clears the bezel/physical-button area by a real buffer
- [ ] Device exposes itself as USB mass storage

### Out of Scope

- ~~Quick-settings swipe sheet (§5.3)~~ — pulled back into scope by Stuart (2026-08-09); gated on proving §5.3's own [device]-marked windowed-partial-refresh assumption before the sheet itself is built
- Stats screen redesign (§4.5) — separate future work, not part of this dispatch
- Any [device]-marked item from the source spec — needs Stuart's hardware, not buildable/verifiable here

## Context

- Repo: MrStu82/crosspoint-llama-pro, base branch `feat/reading-stats-rtc-tracker` @ 900a0ce.
- Source spec: parent's dispatch (msg 2876), itself derived from
  `/workspace/inbox/a2a-1786117567245-36vd27/X4Pro-Firmware-Design-Proposal.html`.
- Investigation finding (Phase 1, item 1): the button-hints-reservation bug as literally
  described (forces StatsActivity to a 0.819 scale) is **not reproducible at current HEAD**.
  `UITheme::getMetrics()` already zeroes `buttonHintsHeight` on touch devices (fix landed in
  `f42fab1`, already an ancestor of HEAD), and a numeric trace of `StatsActivity`'s own
  scale-compression code (naturalHeight=574 vs availableHeight=681 with the fix, or 641
  without it) shows the content fits with room to spare either way. The spec text appears
  stale against current source. No code change was needed for this specific item; verified,
  not patched.
- Build policy: PlatformIO, **app image only** — the flasher rejects a merged bootloader blob.
- Delivery model: not a GitHub PR flow. Each phase is a separate branch/commit, verified in a
  built app image, and reported to parent directly. `/gsd-ship`'s PR-review assumption and
  `/gsd-verify-work`'s UAT-as-test-coverage assumption are both replaced — see Constraints.

## Constraints

- **Build**: App image only (PlatformIO `x4pro` env) — never a merged bootloader blob.
- **Hardware**: No device access here. Stuart's unit is the only real test rig; anything
  needing silicon routes to him through parent. Gauge provides independent execution-proof
  where possible (build succeeds, logic traced/verified in source and, where feasible, in a
  running image); Stuart's on-device sign-off is the actual acceptance gate for visual/feel items.
- **Process**: Skip `/gsd-ship` entirely (no PR review loop here). `/gsd-verify-work`'s UAT gate
  maps to Gauge's independent verification + Stuart's silicon sign-off, not automated test
  coverage — this repo has none of the latter.
- **Style**: C++20, no exceptions/RTTI, `LOG_*` macros only, `makeUniqueNoThrow`/`new(std::nothrow)`
  mandatory, YAGNI/DRY/KISS — behavior-preserving, minimal-diff fixes only.

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Phase 1 item 1 needs no code change | Numeric trace shows it's already fixed at HEAD (see Context) | ✓ Good — verified, reported honestly rather than patched-for-appearance |
| Home-menu clamp implemented as page-based scroll (jump by page, not smooth) | Matches the pattern `RoundedRaffTheme` already uses for the same widget — consistent UX across themes, minimal new code | — Pending Stuart's on-device feel check |
| Chevron drawn via `renderer.fillPolygon` (8x8 triangle), not a new bitmap asset | No existing chevron primitive/icon; a 3-point polygon is the smallest correct fix — avoids new asset/build-bloat | — Pending |

---
*Last updated: 2026-08-07 after Phase 1 investigation and fix*
