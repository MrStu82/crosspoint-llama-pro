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

- [x] Button-hints row no longer reserves space it doesn't draw on touch devices
- [x] Home menu clamps to available height, scrolls, shows a chevron when more rows exist
- [x] Reader screen margin defaults to a sane value for new units
- [x] Reader screens support independent top + bottom status bars
- [x] Home menu's last row clears the bezel/physical-button area by a real buffer
- [x] Device exposes itself as USB mass storage

### Active (Milestone 2: #322096, dispatched 2026-08-10)

Phase 5 — small fixes, ship independently, don't hold behind Phase 6:
- [ ] Reading stats page: font choices, sizing, text clipping tidied up
- [ ] Landscape: bottom drawer opens ONLY from the physical bottom edge (was two edges)
- [ ] Top drawer (font settings): converted from full-screen view to a proper pulled-down drawer, reusing the bottom drawer's own implementation (DRY, no second drawer impl)

Phase 6 — Tamagotchi overhaul (Stuart: "a complete mess", "a big graphical overhaul"):
- [ ] Care-loop mechanics researched and implemented for real: hunger/happiness/discipline meters, life stages (egg→baby→child→teen→adult), evolution branching on care quality, sickness, poop, death/neglect
- [ ] UI restyled to match Tamagotchi Uni specifically: screen layout, icon row, sprite proportions, palette — whole game UI reorganised to that shape

### Out of Scope

- ~~Quick-settings swipe sheet (§5.3)~~ — resolved in Milestone 1 (proven on hardware 2026-08-09, sheet itself not authorised — see REQUIREMENTS.md)
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
| Home-menu clamp implemented as page-based scroll (jump by page, not smooth) | Matches the pattern `RoundedRaffTheme` already uses for the same widget — consistent UX across themes, minimal new code | ✓ Shipped, Milestone 1 |
| Chevron drawn via `renderer.fillPolygon` (8x8 triangle), not a new bitmap asset | No existing chevron primitive/icon; a 3-point polygon is the smallest correct fix — avoids new asset/build-bloat | ✓ Shipped, Milestone 1 |
| Phase 5 top drawer delivers the bottom drawer's *feel* (edge-pull, slide, dismiss), not a shared base class; `TextSettingsActivity` keeps its own state; DRY applies narrowly to edge-drag + animation if it lifts out cleanly | Parent corrected the original "reuse the implementation" wording after flagging it would force ~200 lines of convergence between two structurally different components (2026-08-10) | — Pending |
| Phase 6 gated on real research before implementation (items 4/5 are research-then-build, not just item 5's styling) | Parent: "needs to BE a Tamagotchi, not merely look like one" — mechanics correctness first, Tamagotchi Uni visual style second | — Pending |

### Active (Milestone 3: World Dungeon, plan doc dispatched 2026-08-15)

Source: `/workspace/inbox/a2a-1786788310195-g5xaw8/world-dungeon-build-plan.html`. Six-phase build for the World Dungeon game component (existing `src/game/DungeonGenerator.*`, `AchievementBus.*`). Requirements per phase confirmed as written by parent 2026-08-15, msg 3618 — no negotiation, build to them.

Phase 7 (World Dungeon Phase 0, "Correctness") — CLOSED by parent 2026-08-15:
- [x] No victory item (Master Key/Ring of Power) in the random loot pool across 10,000 generated floors — placed on boss defeat instead
- [x] Blocking death screen (cause, floor, turns, kills, level, achievements this run), dismissed only by explicit tap
- [x] Blocking victory screen, same data shape, doesn't eject to launcher
- [x] Depth-weighted monster selection: mean tier at depth 26 measurably higher than depth 5 over 1,000 floors
- [x] One persistent RNG stream on `GameState`, seeded once per run, serialised with the save — identical seed replays identically, no repeated combat roll on the same tile/turn
- [x] `turnCount` → `uint32_t`; opened-door state persists through save/reload
- [x] Turn 70,000 reached with regen still firing at correct cadence

Phase 8 (World Dungeon Phase 1, "Reclaim the frame" — dirty-rect rendering) — CLOSED by parent 2026-08-15: `FrameDirtyPlanner`-based partial E-ink refresh shipped, host-gated. Constraint honored from Phase 7's own build: death/victory screens were not written as an unconditional `clearScreen()` full repaint, so Phase 8's dirty-rect work landed directly on top of them (parent, msg 3622).

Phase 9 (World Dungeon Phase 2, "Voice — make it Dungeon Crawler Carl") — dispatched 2026-08-15 (parent msg 3652), HOST GATE CLOSED:
- [x] Work item 1: `MONSTER_DEFS`/`ITEM_DEFS` (`src/game/GameTypes.h`) rewritten into DCC-register names, glyphs/stats/array-order untouched; matching comments fixed in `GameTheme.h` and `GameMenuActivity.cpp`. Requirement 1 grep re-run clean (word-boundary scan across `src/game/`, `src/activities/game/`).
- [x] Work item 2: System flavour tables (`src/game/FlavorText.h`/`.cpp`) — 12 categories (4 hit bands, 3 damaged bands, MonsterKilled, PlayerDeath, LevelUp, FloorEntry, BossArrival), 3 `constexpr const char*` variants each, drawn via `GameState::rollRange` (persistent `Player::combatRngState`) with non-repeat-consecutive tracking. Wired into all 7 `GameActivity.cpp` message sites (kill, hit, damaged, death, level-up, floor-entry, boss-arrival via new `BOSS_MONSTER_TYPE` check on floor load). `FLAVOR_TEXT.resetRun()` wired into `GameState::newGame()` alongside `ACHIEVEMENTS.resetRun()`.
- [x] Work item 3: Boxed System notifications (`GameRenderer::showNotification`/`drawNotification`, bordered box + inverted title bar) — wired at all 9 `GameActivity.cpp` call sites (achievement ×5, floor-entry, boss-arrival, death, level-up).
- [x] Work item 4: Achievements screen in the game menu (`GameMenuActivity::renderAchievements()`) — unlocked shown via `achievementShortName()`, locked shown as `STR_DM_ACHIEVEMENT_LOCKED` + `achievementHint()` category teaser (never the real name).
- [x] Work item 5: Four new achievements (`PackRat`, `SpeedRunner`, `DeepDiver`, `MaxedOut`) + new `ItemPickedUp` event, wired in `AchievementBus.cpp`/`GameActivity.cpp`.
- [x] Requirement 2: ≥3 flavour variants per outcome band (all 12 categories carry exactly 3), no consecutive repeat — proven by standalone host simulation mirroring `FlavorTextTracker::pick()`'s exact algorithm, 10,000 draws/category, zero consecutive repeats, full variant coverage every category. Evidence: `.planning/evidence/phase9-gate.md`.
- [x] Requirement 3: all flavour text `const char*` in flash (`inline constexpr const char*[]`), no per-turn string construction — allocation-trace grep across `FlavorText.{h,cpp}`, `AchievementBus.cpp`, `Achievements.h`, and all `showNotification()` call sites in `GameActivity.cpp` found zero `new`/`malloc`/`std::string`/`std::vector`; every notification body copies into a fixed 96-byte member buffer (`GameRenderer::NOTIFICATION_BODY_LEN`) via `snprintf`.
- [x] Requirement 4: notification box stays inside partial-refresh budget — `FrameDirtyPlanner::FramePlan::windows[4]` is a fixed-size array (never heap), notification occupies one bounded `DirtyWindow` slot (`GameRenderer::notificationRect()`), appended only when `!plan.fullClear && windowCount<4`, so it always rides the existing partial-refresh path and never forces a full clear.
- [x] Requirement 5: achievements screen completeness/reload-survival/redaction proof — screen loop iterates `AchievementId::Count` (=8, matches enum exactly, no drift possible); unlock state persisted via `AchievementBus::save()`/`load()` to `/.crosspoint/game/achievements.bin`, loaded on every `GameActivity::onEnter()` so it survives app restart; locked-row render path only ever calls `tr(STR_DM_ACHIEVEMENT_LOCKED)` + `achievementHint()`, never `achievementShortName()` — real name is structurally unreachable while locked.
- [x] Requirement 6: each new achievement proven to fire AND not fire — code-trace matrix (fire condition / no-fire condition) for `PackRat`, `SpeedRunner`, `DeepDiver`, `MaxedOut`. Evidence: `.planning/evidence/phase9-gate.md`.

Phases 10-12 (World Dungeon Phases 3-5: Decisions, The Show, The Co-star): requirements TBD, pulled from plan doc as each phase opens.

## Milestones

- **Milestone 1 (Phases 1-4, X4 Pro UI Layout Fixes)**: Complete 2026-08-07. All 6 requirements shipped and reported.
- **Milestone 2 (Phases 5-6, dispatch #322096, 2026-08-10)**: In progress. Bottom drawer + brightness control (prior deliverable, separate from this milestone) confirmed working by Stuart — do not touch their behavior except where Phase 5's landscape-edge fix requires it.
- **Milestone 3 (Phases 7-12, World Dungeon, dispatched 2026-08-15)**: In progress, Phases 7-8 closed, Phase 9 opened. **Flash cadence superseded 2026-08-15 (parent msg 3650, direct from Stuart): no interim images at all.** Stuart wants exactly one finished flash at the very end of the whole milestone (Phase 12 close), not the earlier three-paired-image plan (msg 3622, now void). Every phase still runs its own full host-gate loop with machine-verified evidence delivered to parent per phase — only the on-glass flash-to-Stuart step is deferred to the very end, since he now has zero chance to catch anything on-device before then. The 4 touch-gap-audit fixes and the outstanding `LOG_INF` instrumentation for `wasHomeKeyBackGesture()`, previously slated to ride the 7+8 image, now ride whatever the final image turns out to be.

---
*Last updated: 2026-08-15 — Milestone 3 (World Dungeon) added as Phases 7-12, flash cadence set to 3 paired images*
