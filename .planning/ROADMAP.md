# Roadmap: X4 Pro UI Layout Fixes

## Overview

Milestone 1 (Phases 1-4, complete): fix the three cheapest/lowest-risk layout
defects first, then add dual top/bottom status bars, then give the home menu's last row
real breathing room from the bezel, then flip on USB mass storage.

Milestone 2 (Phases 5-6, dispatch #322096, 2026-08-10): small fixes ship first and
independently (stats page tidy, landscape single-edge drawer, top drawer reuses the
bottom drawer's implementation), then the Tamagotchi overhaul (real care-loop mechanics,
then Tamagotchi Uni visual restyle) — Phase 6 must not hold up Phase 5's delivery.

## Phases

**Phase Numbering:** integer phases are the planned milestones from parent's dispatches, in the exact order given. No decimal/inserted phases. Phases 1-4 = Milestone 1 (dispatch msg 2876). Phases 5-6 = Milestone 2 (dispatch #322096, 2026-08-10).

- [x] **Phase 1: Three defects (§2)** - button-hints reservation, home-menu clamp, screen margin default
- [x] **Phase 2: Two status bars (§5.2)** - independent top + bottom status bars
- [x] **Phase 3: Home bottom buffer (§2.2)** - real clearance for the home menu's last row
- [x] **Phase 4: USB mass storage (§3.1)** - device enumerates as USB MSC
- [ ] **Phase 5: Small fixes (#322096 Phase 1)** - stats page tidy, landscape single-edge drawer, top drawer reuses bottom-drawer impl
- [ ] **Phase 6: Tamagotchi overhaul (#322096 Phase 2)** - real care-loop mechanics + Tamagotchi Uni visual restyle

## Phase Details

### Phase 1: Three defects (§2)
**Goal**: Fix the three lowest-risk layout defects Stuart hit on-device.
**Depends on**: Nothing (first phase)
**Requirements**: [LAY-01, LAY-02, LAY-03]
**Success Criteria**:
  1. Button-hints height is confirmed zero on touch devices at the point Stats reads it (verified, not a live bug at HEAD)
  2. Home menu with 7+ items clamps to the screen, scrolls in pages, and shows a chevron on the last visible row when more exist
  3. A factory-reset/new unit gets `screenMargin=20` by default; existing units' behavior and the settings-file trap are documented in the report
**Plans**: 1 plan (single commit covering all three defects, per parent's per-phase branch/commit instruction)

Plans:
- [x] 01-01: Investigate all three defects against current HEAD, fix what's still broken, verify in a built app image

### Phase 2: Two status bars (§5.2)
**Goal**: Reader screens support an independent top status bar alongside the existing bottom bar, zero-cost when empty.
**Depends on**: Phase 1
**Requirements**: [BAR-01, BAR-02, BAR-03, BAR-04]
**Success Criteria**:
  1. `Edge` (TOP/BOTTOM) threads through `UITheme::getStatusBarHeight`/`statusBarSpec` and all reader activities
  2. Top bar shows only chapter progress by default; bottom bar's existing default is unchanged
  3. An empty/hidden bar costs exactly 0px of page space; a populated bar pins hard to its panel edge
  4. `StatusBarSettingsActivity` configures either bar via one parameterized screen
**Plans**: 1 plan (single commit, matching Phase 1's pattern)

Plans:
- [x] 02-01: `Edge` enum + `StatusBarSpec` in CrossPointSettings, `UITheme::getStatusBarHeight(Edge)`/`getProgressBarHeight(Edge)`, EPUB/TXT/XTC reader activities render both bars independently, `StatusBarSettingsActivity` parameterized by edge — confirmed present at HEAD `8634a75d`, verified via a clean `pio run -e x4pro` on trantor (SUCCESS, 19.5s, no code changes needed). All four success criteria checked against source directly, not assumed.

### Phase 3: Home bottom buffer (§2.2)
**Goal**: Home menu's last row no longer sits flush against the bezel/physical home button.
**Depends on**: Phase 2
**Requirements**: [BUF-01]
**Success Criteria**:
  1. `homeBottomInset=24` is reserved below the last menu row, untouchable by touch input
  2. Six default menu rows still fit above it after the row-pitch reduction (`menuRowHeight` 64→60, `menuSpacing` 8→4)
  3. Seven-item overflow (OPDS enabled) is handled by Phase 1's clamp/scroll, not a special case here
**Plans**: 1 plan (single commit, matching Phase 1's pattern)

Plans:
- [x] 03-01: `homeBottomInset=24` + `menuRowHeight=60`/`menuSpacing=4` confirmed present in `LyraTheme.h`, dead-zone touch exclusion confirmed in `HomeActivity.cpp` (`menuTouchableHeight` subtracts `metrics.homeBottomInset`) — confirmed present at HEAD `8634a75d`, all three success criteria checked directly against source, no code changes needed.

### Phase 4: USB mass storage (§3.1)
**Goal**: X4 Pro enumerates as a USB mass storage device when connected to a host.
**Depends on**: Phase 3
**Requirements**: [USB-01]
**Success Criteria**:
  1. Build flags `FREEINK_CAP_USB_MSC=1`, `ARDUINO_USB_MODE=0`, `ARDUINO_USB_CDC_ON_BOOT=1` are set alongside the existing `USE_BLOCK_DEVICE_INTERFACE=1`
  2. Build succeeds with the new flags
  3. Any serial-logging impact from CDC-on-boot is flagged explicitly in the report, not silently absorbed
**Plans**: TBD

**RESOLVED (2026-08-07)**: `x4pro` env switched to precompiled framework variant `dio_opi` (`board_build.arduino.memory_type = dio_opi`), which is the only variant with TinyUSB compiled in (`qio_opi`, the prior default, has zero `CONFIG_TINYUSB_*` defines). Build succeeds.
  - Blocker that stopped the first attempt: CrossPoint's own `lib/Logging/Logging.h` hardcoded `static HWCDC& logSerial = Serial;`, guarded only on `ARDUINO_USB_CDC_ON_BOOT` — safe under every prior env (all pinned `ARDUINO_USB_MODE=1`) but broken under `x4pro`'s new `ARDUINO_USB_MODE=0`, where `Serial` resolves to `USBSerial` (type `USBCDC`, not `HWCDC`). Fixed with a 3-way branch mirroring the Arduino core's own `ARDUINO_USB_MODE` guard. Stale type comment in `HalPowerManager.cpp` fixed alongside it.
  - **Flash**: baseline (Phases 1-3, `qio_opi`) 5,446,218 / 6,553,600 bytes (83.1%). Phase 4 (`dio_opi` + MSC/CDC): 5,495,130 / 6,553,600 bytes (83.8%). Delta **+48,912 bytes**, 1,058,470 free.
  - **sdkconfig delta, verified by `nm`-scanning the built `.elf`** (proves linkage, not just config presence): Matter, RainMaker, Insights, camera driver — **0 symbols each**, confirmed dead code under `dio_opi` exactly as under `qio_opi`. Bluetooth controller — only zero-size linker boundary markers, no live driver. WiFi — present but pre-existing (already used for OPDS on every build). TinyUSB MSC callbacks (`tud_msc_*`) — present and linked, the actual feature. The entire flash delta is attributable to the variant switch + new USB stack, none of it to the stock Matter/RainMaker/camera config block.
  - **PSRAM**: re-verified against this build's actual baked `sdkconfig.h` — `CONFIG_SPIRAM_MODE_OCT/TYPE_AUTO/CLK_IO=30/CS_IO=26/SPEED_80M` byte-identical to `qio_opi`. 4 boot-init macros (`CONFIG_SPIRAM_BOOT_HW_INIT`, `CONFIG_SPIRAM_BOOT_INIT`, `CONFIG_SPIRAM_PRE_CONFIGURE_MEMORY_PROTECTION`, `CONFIG_SPIRAM_IGNORE_NOTFOUND`) present in `dio_opi`/absent in `qio_opi`, functionally unresolvable from source — parent accepted this given Stuart's dual-partition hardware setup makes a bad PSRAM bring-up a five-minute partition swap, not a bricked device.

Plans:
- [x] 04-01: dio_opi variant switch + Logging.h HWCDC/USBCDC fix, build green, parent go-ahead to commit

### Phase 5: Small fixes (#322096 Phase 1)
**Goal**: Three device-reported defects fixed, shipped as one deliverable, independent of Phase 6.
**Depends on**: Nothing (Milestone 1 is complete; this is a fresh dispatch)
**Requirements**: [STAT-01, DRW-01, DRW-02]
**Success Criteria**:
  1. Reading stats page: correct fonts, correct sizing, no clipped text — whole page tidied, not spot-patched
  2. Landscape orientation: the bottom drawer opens from exactly one edge — the physical edge that is "bottom" in landscape, not whichever edge was "bottom" in portrait
  3. Top drawer (font settings) delivers the drawer *feel* — pulled from the top edge, slides in, sits over the page, dismisses the same way as the bottom drawer. `TextSettingsActivity` keeps its own tab/list/preview state; DRY applies narrowly to edge-drag detection + slide/dismiss animation, shared only if it lifts out of `BrightnessSheet` cleanly (scope corrected 2026-08-10, see Key Decisions)
  4. Bottom drawer + brightness control behavior is unchanged except for the landscape single-edge fix in criterion 2
**Plans**: TBD

Scoping notes (2026-08-10, research only, no code changed):
1. STAT-01: `src/activities/home/StatsActivity.cpp` draws text at hardcoded baseline Y-coordinates and a hardcoded `textRightEdge = screenWidth - 16` with no text-width measurement guard — straightforward fix, add measurement-based clipping/wrap.
2. DRW-01: `src/MappedInputManager.cpp` lines ~120-121/268-332 — both top-menu and bottom-brightness-sheet edge gestures use a hardcoded height-fraction (14%) with no orientation check. In landscape the physical "bottom" edge is a different logical edge than what the fraction assumes, so both zones can fire. Fix: gate the edge-zone calculation on `renderer.getOrientation()`.
3. DRW-02: bottom drawer is `src/components/BrightnessSheet.cpp` — a lightweight non-Activity overlay component (open_/dragging state only, windowed `displayWindow()` render, `loop()` returns bool to claim input). Top drawer is currently `TextSettingsActivity` — a full stateful `Activity` (tab ring, list scroll, live preview, full `displayBuffer()` render). These are NOT drop-in compatible: making font settings a "BrightnessSheet-shaped" drawer means extracting its render/state out of the Activity lifecycle entirely — a real refactor, not a wrapper. Flagged to parent before starting (scope call, not a code decision).
**Plans**: TBD

Plans:
- [ ] 05-01: TBD

### Phase 6: Tamagotchi overhaul (#322096 Phase 2)
**Goal**: The Tamagotchi game actually implements real Tamagotchi mechanics, styled to match Tamagotchi Uni.
**Depends on**: Phase 5 (ships after, but is not gated on Phase 5's completion per parent's explicit "don't hold Phase 1 behind the overhaul" — sequencing here is about not blocking Phase 5, not a hard dependency)
**Requirements**: [TAMA-01, TAMA-02]
**Success Criteria**:
  1. Research produces a documented care-loop spec: hunger/happiness/discipline meters, life stages egg→baby→child→teen→adult, evolution branching on care quality, sickness, poop, death/neglect
  2. Research produces a documented Tamagotchi Uni visual reference: screen layout, icon row, sprite proportions, palette
  3. Implementation makes the care loop real (not cosmetic) within the ESP32-C3's RAM/flash constraints
  4. UI reorganised to match the Tamagotchi Uni shape identified in research
**Plans**: TBD (research plans precede implementation plans)

Plans:
- [ ] 06-01: TBD

## Progress

**Execution Order:** Phases execute in numeric order: 1 → 2 → 3 → 4 → 5 → 6. Phase 6 must not block Phase 5's delivery to Stuart.

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. Three defects (§2) | 1/1 | Complete | 2026-08-07 |
| 2. Two status bars (§5.2) | 1/1 | Complete | 2026-08-08 |
| 3. Home bottom buffer (§2.2) | 1/1 | Complete | 2026-08-08 |
| 4. USB mass storage (§3.1) | 1/1 | Complete | 2026-08-07 |
| 5. Small fixes (#322096 Phase 1) | 0/? | Not started | - |
| 6. Tamagotchi overhaul (#322096 Phase 2) | 0/? | Not started | - |
| 7. World Dungeon: Correctness | 1/1 | Complete | 2026-08-15 |
| 8. World Dungeon: Reclaim the frame | 1/1 | Complete | 2026-08-15 |
| 9. World Dungeon: Voice | 1/1 | Complete | 2026-08-15 |
| 10. World Dungeon: Decisions | 1/1 | Complete | 2026-08-16 |
| 11. World Dungeon: The Show | 0/? | Not started | - |
| 12. World Dungeon: The Co-star | 0/? | Not started | - |

### Phase 7: World Dungeon — Correctness
**Goal**: Loot, death/victory, monster balance, RNG determinism, save integrity, long-run stability all correct per the plan doc's Phase 0 requirements.
**Depends on**: Nothing (fresh milestone; existing `DungeonGenerator`/`AchievementBus` source is the starting point)
**Requirements**: see PROJECT.md Milestone 3 / Phase 7 checklist
**Gate**: HOST — all 7 requirements automated (run counts stated, e.g. 10,000-floor loot sweep, 1,000-floor monster-tier sweep, 70,000-turn soak), reusing the `ach_test/` harness pattern (forced positive/negative/boundary/persistence/no-double-fire style cases). GLASS — die once and read the death screen, force a win via seeded RNG and read the victory screen.
**Constraint carried into Phase 7's own build**: death/victory screens must not be a `clearScreen()` full repaint — Phase 8's dirty-rect work lands on top of them immediately after.
**Status**: Complete, closed by parent 2026-08-15 (prior session).
**Plans**: TBD

Plans:
- [x] 07-01: Correctness pass — closed.

### Phase 8: World Dungeon — Reclaim the frame (dirty-rect rendering)
**Goal**: Partial E-ink refresh via `FrameDirtyPlanner` — dirty-rect tracking replaces full-screen repaint for in-game rendering.
**Depends on**: Phase 7
**Requirements**: see PROJECT.md Milestone 3 / Phase 8 checklist
**Status**: Complete, closed by parent 2026-08-15 (prior session).
**Plans**: TBD

Plans:
- [x] 08-01: Dirty-rect planner — closed.

### Phase 9: World Dungeon — Voice ("make it Dungeon Crawler Carl")
**Goal**: Replace the game's Tolkien-pastiche register with a Dungeon Crawler Carl (System-dungeon/gameshow) voice — bestiary/item naming, flavour text, boxed System notifications, an achievements screen, and new achievement/event coverage. Parent's framing: "the phase where it stops being a Tolkien pastiche... highest feeling-per-byte in the whole plan and almost entirely text."
**Depends on**: Phase 8
**Work items**:
  1. Rewrite `MONSTER_DEFS` and `ITEM_DEFS` (`src/game/GameTypes.h`) into DCC-register names. Glyphs and stat blocks untouched — a string swap, not a balance pass.
  2. System flavour tables keyed by event and outcome band, drawn from the run's RNG stream (`Player::combatRngState`, proven persistent in Phase 7).
  3. Boxed System notifications — bordered box, inverted title bar — for level-up, achievement, floor entry, boss arrival, death.
  4. Achievements screen in the game menu: unlocked named, locked redacted with a hint.
  5. Four new achievements off events the bus already carries, plus one new `ItemPickedUp` event.
**Requirements** (each proven, not asserted):
  1. Zero Tolkien-derived proper nouns anywhere in source. Grep-proven with the pattern list in evidence, not eyeballed.
  2. Every combat outcome band has ≥3 flavour variants and never repeats twice consecutively. Proven by simulation over a long run, not by counting the table.
  3. All flavour text is `const char*` in flash. No per-turn string construction. Allocation trace required.
  4. Notification box renders inside the partial-refresh budget and does not trigger a full flash. Report windowCount and dirty area for a box appearing and dismissing, using the Phase 8 `FrameDirtyPlanner` instrument.
  5. Achievements screen lists every defined achievement; unlock state survives a reload; a locked one is never named.
  6. Each new achievement proven to fire AND proven not to fire — both directions, matching the Phase 7 fire/no-fire proof pattern.
**Gate**: HOST ONLY — grep proof, variant coverage, unlock/no-unlock matrix, allocation trace showing no per-turn heap churn. No glass gate this phase — per the revised delivery plan (2026-08-15), Stuart flashes once at the very end of the whole World Dungeon milestone, not per-phase.
**Status**: Closed 2026-08-15. All 5 work items landed (`c0d1525`, `6964d1f`, `a1d8f6e`, `259c556`), host gate evidence delivered and recorded in `.planning/evidence/phase9-gate.md`, closure committed in `071e65b`.
**Plans**: n/a — worked item-by-item off the dispatched spec (parent msg 3652), no formal plan doc written.

Plans:
- [x] 09-01: All 5 work items — closed, see evidence/phase9-gate.md.

### Phase 10: World Dungeon — Decisions (throw mechanic)
**Goal**: Throw mechanic for items, with the `Percussive Maintenance` achievement wired in during the build (not bolted on afterward) — per parent's explicit instruction (msgs 3748/3750/3752).
**Depends on**: Phase 9
**Reconnaissance** (2026-08-15, via Explore agent, no existing projectile/ranged/thrown system anywhere in the codebase — confirmed by grep): input dispatch is `GameActivity::loop()` (GameActivity.cpp:126-202) → `handleAction()` (354-508); melee damage formula is `max(1, strength + weaponBonus - defense ± variance)` (GameActivity.cpp:279-282); item-use lives in `GameMenuActivity::useInventoryItem()` (GameMenuActivity.cpp:300-400, potions/food/scrolls consume, weapons/armor toggle `ItemFlag::Equipped`); `ItemFlag` (GameTypes.h:48-53) has bit 3 free; `GameEventType`/`AchievementId` (Achievements.h:7-62) are event-driven via `AchievementBus::emit()` (AchievementBus.cpp:73-140), `ItemUsed` exists as an unused-for-achievements placeholder to model a new `ItemThrown` event on. Rendering is strictly turn-based with no animation loop (GameRenderer.h/.cpp) — no flight animation, impact-only visual, consistent with the project's YAGNI convention (Phase 9's flavour-table scope, cert-line font reuse).
**Work items**:
  1. `bool throwable` field on `ItemDef` (`GameTypes.h`) — corrected 2026-08-15 from the original `ItemFlag::Throwable` bit-flag idea: throwability is a property of an item's *definition/type* (all Daggers, all Potions of Healing), not a per-instance runtime flag on `Item`, so it belongs on `ItemDef`, not `ItemFlag`. Weapon and Potion `ITEM_DEFS` entries set `true`; Armor/Shield/Scroll/Food/Gold/Ring stay `false` (equipped-gear and non-physical items excluded).
  2. Throw-target input: from the inventory menu, a "Throw" action on a throwable item enters directional-targeting (reuse the existing 4-direction input, no new cursor system — matches the turn-based model, avoids inventing targeting UI this phase), confirms to throw in that direction, resolves against the nearest monster in line.
  3. Thrown-item damage formula, dexterity-based (distinct curve from melee's strength-based formula) plus item enchantment, reusing the existing `rollRangeInclusive` variance pattern. Thrown item is consumed — inventory count decrements, item removed at 0, same removal pattern as potion/food consumption.
  4. New `GameEventType::ItemThrown` (hit/miss, monster id/maxHp on hit) emitted from the throw-resolution call site, mirroring `MonsterKilled`'s emit-from-attack-site pattern.
  5. New `AchievementId::PercussiveMaintenance` (index 8, bumps `Count` to 9) — unlocks on a monster kill via `ItemThrown`, wired in `AchievementBus::emit()`'s `ItemThrown` case, same call-site pattern as `EscalationOfForce`.
**Requirements** (each proven, not asserted):
  1. Every throwable `ITEM_DEFS` subtype can be thrown from inventory; non-throwable items (equipped gear, quest item) cannot — grep/simulation proof, not eyeballed.
  2. Thrown-item damage formula proven via simulation to follow a dexterity-based curve distinct from melee's strength-based curve (not the same formula with different inputs by accident).
  3. Thrown item is consumed on throw — inventory count decrements correctly and the item is removed at 0, proven via inventory-count assertions across N throws (stacked and single-count cases).
  4. `PercussiveMaintenance` fires exactly once on the first thrown-item kill and never fires on a thrown miss, a thrown non-kill hit, or a melee kill — proven fire/no-fire both directions, matching the Phase 7/9 proof pattern.
  5. Throw resolution stays inside the existing turn-based dirty-rect render model — no new animation loop, no full-flash regression — proven via the Phase 8 `FrameDirtyPlanner` instrument, same technique as Phase 9 requirement 4.
**Gate**: HOST ONLY — grep proof, damage-curve simulation, inventory-count assertions, unlock/no-unlock matrix, dirty-rect instrument report. No glass gate this phase, per the standing delivery-plan.
**Status**: Closed 2026-08-16. All 5 work items landed in one commit-set (`b1b4f1e2`), build green (Trantor `x4pro`, 59.91s), all 5 requirements proven PASS, host gate evidence recorded in `.planning/evidence/phase10-gate.md`. Follow-up fix required by parent's ruling (msg 3758, same day): `handleThrow()`'s kill branch now also emits `MonsterKilled` (populating `damage`/`monsterMaxHp`) so a thrown boss overkill can trigger `EscalationOfForce` alongside `PercussiveMaintenance`; `AchievementBus`'s single-unlock-slot design was widened to a bounded FIFO (`MAX_PENDING_UNLOCKS=3`) so both surface instead of the second being silently swallowed. New Scenario 14 (thrown-overkill double-unlock) added to the `ach_test` harness; full 68-assertion re-run clean (`=== ALL PASS (0 failure(s)) ===`), catching and fixing two pre-existing stale Phase-9-era assertions along the way. Landed as commit `72eac4b4acb343cc5df016dbb82e70a3dcc4f762`, Trantor build green (39.79s), pushed `d5dc2d9..72eac4b`. No open items remain on Phase 10.
**Plans**: n/a — worked directly off the dispatched spec (parent msgs 3748/3750/3752/3756/3758), no formal plan doc written.

Plans:
- [x] 10-01: Throw mechanic + PercussiveMaintenance — closed, see evidence/phase10-gate.md.
- [x] 10-02: Follow-up fix (MonsterKilled emit on thrown kills + AchievementBus multi-unlock queue) per parent msg 3758 — closed, commit `72eac4b4acb343cc5df016dbb82e70a3dcc4f762`, see evidence/phase10-gate.md.

### Phase 11: World Dungeon — The Show / reward economy
**Goal**: Per-run achievement economy plus loot boxes.
**Status**: Closed 2026-08-21. The append-only 250-entry achievement pool deals 50 ids per run from the full pool; rewards reuse the existing run-scoped buff/skill machinery. Four Common/Uncommon/Rare/Legendary loot-box tiers use exact 50/30/15/5 weighting and one compact existing-definition reward table per tier. Four visible box records occupy one logical shared-loot slot. Detailed proof: `.planning/evidence/job-phase4-loot-box-gate.md` and `.planning/evidence/milestone3-phase11-12-final-gate.md`.

### Phase 12: World Dungeon — The Co-star / companion
**Goal**: Two corpse-loot streams and a run-identity companion.
**Status**: Closed 2026-08-21. Every eligible kill rolls independent player-visible and companion-only streams. The companion is independently rolled at run creation, levels with the player, forages its private stream, auto-equips its best gear and has a profile/progress/gear surface. Save and occupancy boundaries are hardened and migration-tested. Final clean-source gate: loot-box harness PASS, ASan/UBSan companion-save harness PASS, full host suite 149/149, X4 Pro production build SUCCESS. Final bare app image recorded in `.planning/evidence/milestone3-phase11-12-final-gate.md`. Standing flash cadence honoured: no interim Phase 11 image; one final image after Phase 12 closure.


### Phase 13: Audited upstream bug fixes
**Goal:** Port every compatible runtime fix identified in `audits/crosspoint-reader-upstream-crossink-audit.md`; no blind merge or feature sweep.
**Status:** In progress.

### Phase 14: Approved reader capabilities
**Goal:** Native Lexend Deca/Bitter selection, 10/12/14/16pt sizes, limited symbols, redaction, simple tables, optional Bionic/Guide Dots, force paragraph indents; retain existing strikethrough/thick underline/hr behavior.
**Status:** Pending Phase 13.

### Phase 15: Pinned sleep favourite
**Goal:** Explicit Pinned/Unpinned state and missing-file fallback in the existing sleep-image flow.
**Status:** Pending Phase 14.

### Phase 16: Simulator and release gate
**Goal:** `simulator_x4_pro` deterministic input/framebuffer proof, one local Trantor X4 Pro build, release provenance, Pixel/Gauge single gate.
**Status:** Pending Phase 15.

### Phase 17: Approved InkPointX Home / Now Reading
**Goal:** Implement the approved 480×800 benchmark `00af7165a57d22140fd2c520a04111906f488bccd19b2dce0882a1b79bb1b348` with truthful per-book stats, cover-only resume, shared Caveat heading token, deterministic daily quote and six-tab persistent navigation.
**Depends on:** Hardcover beta GraphQL search, resolved by Stuart; dedicated `read:catalog:search` PAT provisioned through device web settings.
**Constraint:** Existing partition table, dual OTA, rollback and SPIFFS remain unchanged. One implementation build only; simulator proof, then one Pixel/Gauge gate. No implementation starts while the rating source is undecided.
**Plan:** `.planning/INKPOINTX_HOME_IMPLEMENTATION_BRIEF.md`.
**Status:** Implementation in progress; provider isolated, atomic last-good cache and simulator gate underway.
