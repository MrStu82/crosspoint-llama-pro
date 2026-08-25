---
gsd_state_version: '1.0'
status: in_progress
progress:
  total_phases: 17
  completed_phases: 16
  total_plans: 5
  completed_plans: 5
  percent: 94
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-08-15)

**Core value:** Every screen renders correctly and fully on-panel on the X4 Pro, with no clipped/overflowing content and no touch targets crowded against the bezel.
**Current focus:** Phase 17 implementation — approved InkPointX Home benchmark `00af7165…1b348`, Hardcover beta GraphQL rating lookup with device-bound scoped PAT, atomic last-good cache, simulator proof, and one X4 Pro release build.

**2026-08-25 final safe revision:** partition expansion is cancelled. The
original dual-OTA/SPIFFS table is preserved. Lexend Deca and Bitter use the
existing downloadable SD `.cpfont` pipeline at 10/12/14/16 pt, with only the
bounded offline preview/fallback retained in firmware. One authorised X4 Pro
release build is the next action.

## Current Position

Phase: 17 of 17 — host + simulator gates green; one X4 Pro release build pending
Plan: `.planning/INKPOINTX_HOME_IMPLEMENTATION_BRIEF.md`. Stuart selected Hardcover Search API: ISBN first, exact title+author fallback; cache last-good atomically; never blank stale data.
**Chained self-continuation job (parent msg 4140, 2026-08-18, separate local 1-5 numbering, runs inside/ahead of Phase 11 scoping):** Job Phase 1 (`drawLine` multi-width overload fix — thick vertical lines were stretching instead of widening because the old code always offset y regardless of orientation) is DONE and independently verified: commit `7e447bc56b66958a1ec3a0ed88a0fb7483453c6f` (`git rev-parse HEAD` + `wc -c`=41 confirmed), Trantor firmware build green (`x4pro`, 39.43s, SUCCESS), host gtest suite 143/143 passed including all 5 new `ThickLineOffsetInX`/`ThickLineShape` cases proving both the axis decision and the widened-band-not-stretched-length shape. Chain proceeding immediately, no stop, into Job Phase 2 (corpse loot: dual player/pet-addressed drop streams off the same floor/monster-scaled table with a rare tail; pet stream never enters player inventory, never renders on map, not player-pickable). Job Phase 3 = the pet (rolled at creation, levels with player, autonomous foraging of the Phase 2 pet stream, auto-equip, profile/gear screen). Job Phase 4 = loot-box DESIGN NOTE ONLY (six tiers, no safe-room gate, key-to-race drops, some unstable — box-type count and per-tier tables left for Stuart), no code/commit, chain halts after sending the note. Job Phase 5 (progress screen + character naming w/ Random button) is explicitly NOT auto-chained — separate dispatch from parent required.

**Job Phase 6 (added by parent msg 4142, 2026-08-18, queued AFTER Phase 5, does not interrupt in-progress Phase 2 work):** pressed-state feedback on the Action/Menu on-screen buttons. Pixel-reviewed, spec ratified, build as written: geometry from `drawControls()` (`GameRenderer.cpp:534-545`, `buttonX=188, buttonW=292, controlsY=632, buttonH=84`); down/release via `MappedInputManager::rowTouch()` (`MappedInputManager.cpp:205`, `top=632, rowStep=84, rowCount=2, xStart=188, xEnd=480, rowHeight=84`, `Down`=press-begin/`Tap`=resolved release); pressed look = invert only (`fillRect(...,true)` + `drawText(..., black=false)`, no border/offset/shadow/animation); refresh = `displayWindow()` on that rect only (never a full refresh); new `pressedControlButton` (Action/Menu/none) field on `GameActivity`, same pattern as existing `upHeld` flags, set on Down/cleared on resolution; Action release restores fill+displayWindow then dispatches `handleAction()`, Menu release skips restore and dispatches `openGameMenu()` immediately (screen supersedes it); drag-off abort (lift without resolving to Tap inside the button) restores identically on both buttons — non-negotiable, a stuck-inverted button is the exact defect this phase prevents. Gate: Trantor `pio run -e x4pro` green + gtest suite, then continue chain, no stop.

**Job Phase 3 spec, tightened (parent msg 4144, 2026-08-18, 9:15 PM — Stuart direct, verbatim intent "From random pool of pets/animals/companions, random name, random attributes"):** KISS, three tables + a roll, no pet-generation framework.
- Species: flat append-only array (same shape as the achievement pool, id = index), a pool of pets/animals/companions, DCC-flavoured, deliberately varied/silly entries welcome, rolled once at character creation.
- Name: rolled independently off a separate flat name table (not per-species lists) — a weasel named Bartholomew is the intended outcome, not a bug.
- Attributes: rolled fresh at creation the same way the player's are rolled — never copied from a species template. Species may cheaply bias the roll; it must never fix/override it.
- Everything previously ruled still stands unchanged: levels with the player, autonomous foraging of the Job-Phase-2 pet loot stream, auto-equips best gear, visible on the profile/gear screen matching the existing character menu look, its own invisible corpse-loot stream (never in inventory/never rendered/never pickable, from Phase 2).

**Job Phase "Centre-on-player restore" (parent msgs 4146 then 4148, 2026-08-18, 9:16-9:17 PM — 4148 supersedes 4146, both apply, sequenced ahead of Job Phase 3 in this session given a live on-handset complaint from Stuart):** NOT a regression hunt — 4148 corrects 4146: dropping centre-on-player was a deliberate perf tradeoff (a centred view shifts every cell, forcing a full-window redraw per step), made stale by a later reduction in visible map size. Brief:
1. Measure today's redraw cost of a centred step at the *current* (smaller) map size, before touching anything — establishes the baseline the fix is judged against.
2. Inspect what a step actually pushes to the panel today (partial-refresh/dirty-rect path, `FrameDirtyPlanner`/`displayWindow()` usage in the map render path) — if partial refresh can be limited to the newly-exposed edge strip + genuinely dirty cells instead of the whole viewport, that is what makes centring affordable; if it can't, report the number instead of shipping a stutter.
3. If affordable: restore centre-on-player against the current smaller viewport, with edge clamping only where the map genuinely runs out (player goes off-centre at map edges rather than showing void).
4. Add/keep a host gtest that pins the centring behaviour so it can't silently regress a third time.
5. Gate: Trantor `pio run -e x4pro` green + gtest suite. Continue chain after, no permission needed. Ordering: this phase runs before Job Phase 6 (pressed-state) per parent's explicit instruction; scheduled ahead of Job Phase 3 in this session's execution order as a live user-facing issue, not a re-ordering of the standing phase numbers.
Status: **Phase 10 is fully closed, including the follow-up fix.** Requirement 1 (throwable/non-throwable correctness) proven by `ITEM_DEFS[]` inspection. Requirement 2 (damage curve distinct from melee) proven by a 462-case host simulation (442 non-coincidental mismatches, confirmed distinct example + variance-width check). Requirement 3 (stack-aware consumption) proven by a 13-assertion host simulation mirroring `handleThrow()`'s consumption block verbatim. Requirement 4 (PercussiveMaintenance fire/no-fire matrix) proven via the `ach_test/` harness, extended twice: once for the original throw-kill scenario, then again per parent's msg 3758 ruling with a new "thrown overkill" Scenario 14 (thrown kill now also emits `MonsterKilled`, unlocking both `PercussiveMaintenance` and `EscalationOfForce` in one turn, both surfaced via a new bounded FIFO pending-unlock queue instead of the prior single-slot design that silently swallowed the second). Full re-run after the fix: 68/68 assertions PASS (`=== ALL PASS (0 failure(s)) ===`), catching and fixing two pre-existing stale Phase-9-era assertions (hardcoded `AchievementId::Count == 8`, now 9) along the way — proof that this harness needs re-running in full, not spot-checked, per parent's explicit instruction. Requirement 5 (no dirty-rect regression) proven by source inspection — `handleThrow()` reuses the existing single-`requestUpdate()`-per-handler pattern and the same bounded `showNotification()`/`FrameDirtyPlanner` mechanism as every other handler, no new redraw path. Full evidence in `.planning/evidence/phase10-gate.md`.
Also landed this session, off the game_title_render cert-line fix thread (not a Phase 9/10 work item, but same branch/session): `9eb56a0` (cert-line UI_10 fix, both wordmark-border and tick-ring collisions resolved, parent + Pixel signed off) and `4718cab` (host-render-harness README, build recipe verified from a genuine clean-room rebuild, catching 3 real recipe bugs before documenting).
No open items remain on Phase 10. The previously-flagged judgment call (`handleThrow()`'s kill branch not emitting `MonsterKilled`, so a thrown kill couldn't trigger `EscalationOfForce`) was resolved by parent's msg 3758 ruling — "emit `MonsterKilled` as well" — and implemented in commit `72eac4b4acb343cc5df016dbb82e70a3dcc4f762`.

### Milestone 1/2 history (prior, not current focus)

Milestone 1 (Phases 1-4, X4 Pro UI Layout Fixes): complete 2026-08-07, confirmed at HEAD `8634a75d`.
Milestone 2 (Phases 5-6, dispatch #322096, 2026-08-10): Phase 5 (small fixes: stats page, landscape drawer edge, top-drawer feel) shipped and confirmed on origin. Phase 6 (Tamagotchi overhaul) reached a working state — care-loop mechanics rebuilt with real Bandai-style controls, poop/sickness, sleep cycle, discipline-as-stat, and care-mistake evolution gating, plus a sprite-based Care Menu UI — before Milestone 3 (World Dungeon) was dispatched and became the active focus. Milestone 2's remaining item (TAMA-02 Tamagotchi Uni visual restyle) is paused, not abandoned — pick back up if parent reopens it.

**Phase 5 — complete, confirmed on origin:**
- STAT-01 (stats page font/sizing/clipping): fixed in `StatsActivity.cpp` (baseline/top-edge conversion fix + numeric-overflow guard). Committed+pushed `cdb2daf2`.
- DRW-01 (landscape bottom drawer opens from two edges): root cause — `wasBrightnessGesture()` (left edge) and `wasBrightnessSheetGesture()` (bottom edge) zones overlapped in the bottom-left corner; `ActivityManager::loop()` checked the left gesture first, so a corner swipe opened the full settings page instead of the drawer. Fixed by excluding the shared corner from `wasBrightnessGesture()`. Committed+pushed `83e4f1f3`.
- DRW-02 (top drawer feel): dropped the `clearScreen()` white-flash, added a `DRAWER_MIN_PEEK_PX=24` fit-check, swapped to a windowed `displayWindow()` push that excludes the dead `bottomReserved` band on touch hardware so the reader page peeks through underneath — matches the bottom drawer's feel without touching the content layout budget. Landed as two commits: `f94262b4` (fix full-screen-flush regression) and `b7329077` (fix bleed-through + outside-tap dismiss, plus a follow-on STAT-01 clipping fix from Stuart's hardware report).
- Confirmed via `git log origin/phase4-usb-msc` (not just local branch state) — all four shas present on origin.

**Phase 6 — in progress, six commits on origin (18:29 2026-08-10 – 2026-08-11):**
- `79122d7` — rebuilt Tamagotchi from scratch with real Bandai A/B/C controls, poop/sickness cycle, and care-mistake-gated evolution (foundational commit — `TamagotchiActivity.h`/`.cpp`, RTC-time-driven meters, versioned flash-persisted state).
- `a8f0793` — fix TAMA-02 `careMistakes` evolve dead-end and a Discipline exploit: an age-ready pet with too many care mistakes was previously frozen in its stage forever with no way to recover; Discipline could also be spammed to resolve any attention call for free, defeating the evolution care-mistake gate.
- `5737a5b` — fix: evolve-fail must reset the stage clock, not just `careMistakes` — a failed evolve check now costs a full stage window served with good care, rather than only clearing the mistake counter.
- `c8babe9` — fix Tamagotchi layout defects: icon glyphs, dead space, popup collision (visual/layout pass on the Care Menu grid and food submenu popup).
- `9852b3f` — Tamagotchi Care Menu on A (physical/tap A button now opens the Care Menu from Main), sprite-based rendering for pet + icons (Stuart supplied the sprite art).
- `5ba2040` — non-Tamagotchi (docs fix + dormant game-mode LUT hook), reconciliation baseline for the gap analysis.
- `532f8a9` (2026-08-11) — real sleep cycle: `isAsleep` state on an RTC day/night schedule (21:00–07:00), hunger/happiness decay 3x slower and energy recovers instead of draining while asleep, new attention calls suppressed while asleep, `toggleLight()` rewritten to actually toggle sleep (early-wake penalty: care mistake + energy cost, matches parent's build-order item 1 verbatim). State bumped to v3. Built green on Trantor, delivered to parent as `crosspoint-sleep-cycle.bin`.
- `cb84e1f` (2026-08-11) — fix: parent caught an infinite-nap exploit on review — `toggleLight()` outside the night window set `isAsleep` with no edge for `tick()` to ever wake it (wake path is `wasNight && !nightNow`, can't fire during the day), letting the pet nap indefinitely at 3x slower decay with free energy recovery, a permanent care-mistake shield. Added `kManualNapDurationSeconds` (30 min): a manual daytime nap self-wakes on duration from `sleepStartEpoch`, gated on `!nightNow` so real night sleep (woken by the edge) is untouched. Built green on Trantor, delivered to parent as `crosspoint-sleep-cycle-napfix.bin`. Parent verified off origin and released this bin to Stuart.
- `7f1c743` (2026-08-11) — feat: build-order item 2, `disciplineLevel` as a persisted care-quality stat. New `uint8_t` field (0-100, starts 50), gains +5 on a genuinely-resolved call (`resolveCall`), loses -8 on an ignored/timed-out call (`maybeExpireCall`), gates `evolveIfReady` alongside `careMistakes` (threshold `kMinDisciplineToEvolve=40`). Persists across stage evolutions (unlike `careMistakes`, not reset on evolve/evolve-fail). Deliberately untouched by the existing Discipline icon/button (`resolveAnyCall`'s catch-all scold) so the stat can't be farmed — "a stat, not a button" per parent's spec. State bumped v3→v4. Built green on Trantor, `version.txt` `v1.5.0-11-g7f1c743` (no `-dirty`), delivered to parent as `crosspoint-discipline.bin`. Parent verified the economics but caught a defect: the stat was invisible, no draw call anywhere — held the bin back from Stuart.
- `bdcf5d3` (2026-08-11) — fix: surface `disciplineLevel` to the player. Status screen gained a 6th meter row reusing `drawHeartPips` (same idiom as hunger/happiness/energy, no new sprite/screen, per parent's explicit instruction). When an age-ready pet's evolve check fails specifically on `disciplineLevel < kMinDisciplineToEvolve` (new UI-only `disciplineBlockedEvolve` flag, recomputed every real tick in `evolveIfReady`, not persisted), a "D!" mark now appears on the pet itself on the Main screen — same no-new-sprite overlay pattern already used for the sick cross/call "!"/asleep "Z" in `drawCreature()` — plus a matching "!" beside the discipline row on Status. Built green on Trantor, `version.txt` `v1.5.0-13-gbdcf5d3` (no `-dirty`), delivered to parent as `crosspoint-discipline-visible.bin`.
- All confirmed present on `origin/phase4-usb-msc` via `git log`; local branch matches origin at HEAD `bdcf5d3`.
Last activity: 2026-08-11 — build-order item 2 (discipline as a persisted stat) shipped, then re-shipped with the visibility fix parent required before release. Awaiting parent's review of `bdcf5d3`. Item 3 (evolution branching) stays parked until parent draws a second adult sprite. LUT suspend/resume gap (`gameLutActive` intent-vs-currently-loaded split) remains open, not yet started.

### DRW-02 implementation notes (2026-08-10, superseded the earlier open question)

- The earlier "carve a gap from the content budget" open question was resolved without touching the content budget at all: `BaseTheme::drawButtonHints()` is a complete no-op on touch hardware (`if (gpio.hasTouch()) { return; }`), so `bottomReserved` was already blank/unused pixels there. The fix simply stops flushing that already-dead band to the panel — the reader page underneath shows through it for free, satisfying parent's "sliver of page peeking through IS the requirement" ruling without shrinking any existing region or risking a new overflow.
- Fit-check uses `LOG_ERR` (not `assert()`), per repo CLAUDE.md's error-handling hierarchy — this is a configuration-sanity check (theme metrics could change bottomReserved in the future), not a fatal "impossible state".

Progress: [██████████] 100% (12/12 phases complete)

## Performance Metrics

**Velocity:**
- Total plans completed: 4 (Milestone 1)
- Average duration: N/A
- Total execution time: N/A

**By Phase:**

| Phase | Plans | Total | Avg/Plan |
|-------|-------|-------|----------|
| - | - | - | - |

**Recent Trend:**
- Last 5 plans: N/A
- Trend: N/A

## Accumulated Context

### Decisions

Decisions are logged in PROJECT.md Key Decisions table.
Recent decisions affecting current work:

- Milestone 3 delivery plan changed 2026-08-15 (parent msg 3650, direct from Stuart): no interim on-glass flashes for Phases 9-11 — build straight through, host-gate every phase exactly as strictly as before, ship one final image only when Phase 12 closes. Supersedes the earlier 3-paired-image plan (msg 3622).
- Phase 9 work item 1 (MONSTER_DEFS/ITEM_DEFS rename) is a pure string swap: array order/index, glyphs, and stat blocks are untouched, since `GameTheme.h`'s `kThemeCarl.monsters[]` art-slot array is positionally (not name-) matched to `MONSTER_DEFS` — reordering would silently desync monster art slots.
- Milestone 2 Phase 5 top drawer delivers the bottom drawer's *feel* (edge-pull, slide, dismiss), not a shared base class; `TextSettingsActivity` keeps its own tab/list/preview state; DRY applies narrowly to edge-drag detection + slide/dismiss animation if it lifts out cleanly (parent corrected the original "reuse the implementation" wording 2026-08-10, see PROJECT.md)
- Milestone 2 Phase 6 is research-then-build: care-loop mechanics and Tamagotchi Uni visual reference must be researched and documented before implementation, per parent's "needs to BE a Tamagotchi, not merely look like one" — mechanics-first pass shipped (real RTC-driven meters, poop/sickness, care-mistake evolution gating); the UNI visual-style pass (TAMA-02's screen-layout/icon-ring/palette match) is paused, not started

### Pending Todos

- Phase 9 work items 2-5: flavour tables, boxed System notifications, achievements screen, 4 new achievements + `ItemPickedUp` event
- Phase 9 host gate harnesses for all 6 requirements (grep proof, variant-coverage sim, allocation trace, notification-box dirty-rect report, achievements-screen proof, fire/no-fire matrix)
- Milestone 2 paused item: TAMA-02 (Tamagotchi Uni visual restyle) not started — pick back up only if parent reopens Milestone 2

### Blockers/Concerns

- None currently blocking Phase 9. `GameActivity.cpp`'s exact item-pickup call site (needed for the new `ItemPickedUp` emit) not yet located — routine lookup, not a blocker.
- Standing lesson (from Milestone 2): STATE.md can drift stale relative to real repo/session state if not actively kept current — cross-check against `git log`/session context before reporting status, don't just trust this file's prose. Applies doubly now that Phase 9's host-only gate means there's no glass checkpoint forcing a status re-check.

## Deferred Items

Items acknowledged and carried forward from previous milestone close:

| Category | Item | Status | Deferred At |
|----------|------|--------|-------------|
| UI | Quick-settings swipe sheet (§5.3) | Resolved — proven on hardware 2026-08-09, sheet not authorised by Stuart, closed unless he asks by name | Dispatch (2026-08-07) |
| UI | Stats screen redesign (§4.5) | Now in scope as STAT-01, Milestone 2 Phase 5 | Dispatch (2026-08-07) → reactivated 2026-08-10 |

## Session Continuity

**RULE (parent, 2026-08-19, binding on every future write to this section): "unfinished phase in the marker" + "pending task: none" is an illegal combination.** Writing "Stopped at: [in-progress/partial work]" and writing "Resume file: None" / leaving no armed continuation in the same edit is a defect, full stop — it doesn't matter what the surrounding prose says. Every write to this section MUST fill in **all three** fields below together, in the same commit/edit, no exceptions:
- **Stopped at:** — exact state, phase, commit sha if applicable.
- **Next actionable item:** — either (a) a specific next phase/step this agent will pick up unprompted, or (b) an explicit statement that the chain is parent-gated (naming exactly what's being waited on: a ruling, a dispatch, an approval) — "None" / blank is never valid; "waiting on parent" is valid ONLY when named with what for.
- **Resume file:** — a path, or "None (see Next actionable item)" — never a bare "None" with nothing else armed.

Last session: 2026-08-25
Stopped at: Phase 17 implementation started at HEAD 632acc579e1a2247ba9e4af5c50eb4af1a81b045; approved benchmark 00af7165…1b348.
Next actionable item: Commit the host/simulator-green Phase 17 tree, run the single Trantor X4 Pro release build, then one Pixel/Gauge gate.
Resume file: `.planning/INKPOINTX_HOME_IMPLEMENTATION_BRIEF.md`
