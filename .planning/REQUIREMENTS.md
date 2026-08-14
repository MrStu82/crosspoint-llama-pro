# Requirements: X4 Pro UI Layout Fixes

**Defined:** 2026-08-07
**Core Value:** Every screen renders correctly and fully on-panel on the X4 Pro, with no clipped/overflowing content and no touch targets crowded against the bezel.

## v1 Requirements

### Layout Defects (§2)

- [x] **LAY-01**: `UITheme::getMetrics()` reports zero button-hints height on touch devices, so screens that reserve space for it (e.g. Stats) don't over-reserve — verified already true at HEAD, no code change needed
- [ ] **LAY-02**: Home menu clamps rows to available height, scrolls in pages, and shows an 8x8 chevron on the last visible row when more items exist below
- [ ] **LAY-03**: `screenMargin` setting defaults to 20 for new/factory-reset units (documented settings-file trap for existing units)

### Status Bars (§5.2)

- [ ] **BAR-01**: Reader screens (EPUB/TXT/XTC) support an independent top status bar in addition to the existing bottom bar
- [ ] **BAR-02**: Top bar defaults to showing only chapter progress; bottom bar keeps its existing book-progress + text lane default
- [ ] **BAR-03**: Both bars pin hard to their panel edge and cost zero page space when empty/hidden
- [ ] **BAR-04**: Settings screen lets the user configure each bar independently (same screen, parameterized by edge — no second screen)

### Home Bottom Buffer (§2.2)

- [ ] **BUF-01**: Home menu's last row clears the bezel by a real buffer (`homeBottomInset`), not 4px

### USB Mass Storage (§3.1)

- [ ] **USB-01**: Device exposes itself as a USB mass storage device to a host PC

## Milestone 2 Requirements (dispatch #322096, 2026-08-10)

### Phase 5 — Small fixes (ship first, independent of Phase 6)

- [x] **STAT-01**: Reading stats page — font choices, sizing, and text clipping all fixed; whole page tidied. Shipped `cdb2daf2`, follow-on clipping fix folded into `b7329077`.
- [x] **DRW-01**: Landscape bottom drawer opens from exactly one edge (the physical "bottom" edge in landscape), not two. Shipped `83e4f1f3`.
- [x] **DRW-02**: Top drawer (font settings) delivers the drawer *feel* — pulled from the top edge, slides in, sits over the page, dismisses the same way as the bottom drawer. `TextSettingsActivity` keeps its own tab ring/list/live-preview state (legitimately more state than the bottom drawer's single scalar) — no forced convergence. DRY applies narrowly: lift edge-drag detection and slide/dismiss animation out of `BrightnessSheet` into something shared if it comes out cleanly; if not, leave it in place and say so explicitly. Scope corrected 2026-08-10 — parent's original "reuse the implementation" wording was written before either component had been read. Shipped `f94262b4` + `b7329077`.

### Phase 6 — Tamagotchi overhaul

- [x] **TAMA-01**: Real Tamagotchi care-loop mechanics researched and implemented — hunger/happiness/energy meters (RTC-time-driven decay), egg→baby→child→adult life stages, care-mistake-gated evolution, sickness, poop, death/neglect, attention-call system. Shipped `79122d7`, `a8f0793`, `5737a5b`. **Gap remaining** (see Phase 6 gap analysis): discipline is an action only, not a tracked stat; evolution is strictly linear, no branching on care-quality history; no real sleep-state machine (Light icon is an instant energy top-up, not a day/night cycle).
- [ ] **TAMA-02**: UI restyled to match Tamagotchi Uni specifically (screen layout, icon row, sprite proportions, palette), whole game UI reorganised to that shape. Partial progress only: sprite-based pet + icon rendering shipped (`9852b3f`) and layout defects fixed (`c8babe9`), but the Care Menu is still a generic 4-column icon grid with heart-pip meters, not yet restyled to Tamagotchi Uni's specific screen shape.

## v2 Requirements

Deferred, not in this dispatch.

### Deferred UI

- ~~**DEF-01**: Quick-settings swipe sheet (§5.3)~~ — resolved, Milestone 1 (proven on hardware 2026-08-09, sheet not authorised by Stuart — see "Back in Scope" below)
- ~~**DEF-02**: Stats screen redesign (§4.5)~~ — now in scope as STAT-01, Milestone 2 Phase 5

## Back in Scope

- **DEF-01 / Quick-settings swipe sheet (§5.3)**: pulled back into scope by Stuart (2026-08-09) — top-edge swipe must land on text/typography settings. Build order: (1) confirm what `EpubReaderMenuActivity` already exposes for text settings, (2) prove §5.3's own `[device]`-marked assumption — SSD1677 windowed partial refresh of a 480×160 region without a full-panel flash — on Stuart's hardware, (3) only build the sheet itself if that proof passes.
  - **Step 2 PROVEN, on hardware, 2026-08-09**: a one-shot debug trigger (long-press the Settings header) drew a striped test pattern into a 480×160 bottom strip and windowed-refreshed just that region on Stuart's actual SSD1677 panel. Stuart's verbatim result: *"I didn't see the screen flash. I don't think it would matter if it did."* The windowed-refresh assumption behind §5.3 holds — confirmed, not assumed. `HalDisplay::displayWindow`/`GfxRenderer::displayWindow` are proven-working plumbing and remain in the codebase; the one-shot debug trigger itself has been stripped out of `SettingsActivity` (its only purpose was this proof) since a long-press easter egg drawing stripes over the Settings screen is a liability if rediscovered later with no context.
  - **Step 3 NOT authorised**: despite the proof passing, Stuart did not ask for the sheet — he already has text options on the top swipe. Do not build it. This entry stays closed unless he asks by name.

## Out of Scope

| Feature | Reason |
|---------|--------|
| Any [device]-marked spec item | Needs Stuart's hardware to build/verify, not actionable here |
| Stats redesign | Explicitly deferred by parent's dispatch |

## Traceability

| Requirement | Phase | Status |
|-------------|-------|--------|
| LAY-01 | Phase 1 | Complete (verified, no change needed) |
| LAY-02 | Phase 1 | In Progress |
| LAY-03 | Phase 1 | In Progress |
| BAR-01 | Phase 2 | Pending |
| BAR-02 | Phase 2 | Pending |
| BAR-03 | Phase 2 | Pending |
| BAR-04 | Phase 2 | Pending |
| BUF-01 | Phase 3 | Complete |
| USB-01 | Phase 4 | Complete |
| STAT-01 | Phase 5 | Complete |
| DRW-01 | Phase 5 | Complete |
| DRW-02 | Phase 5 | Complete |
| TAMA-01 | Phase 6 | Complete (gap analysis identifies remaining authenticity work) |
| TAMA-02 | Phase 6 | In Progress (sprites/layout shipped, Uni-specific restyle not started) |

**Coverage:**
- v1 requirements (Milestone 1): 9 total, all complete
- Milestone 2 requirements: 5 total, mapped to phases 5-6 — Phase 5 (3/3) complete, Phase 6 (1/2 complete, 1/2 in progress)
- Unmapped: 0 ✓

---
*Requirements defined: 2026-08-07*
*Last updated: 2026-08-11 — Phase 5 confirmed complete on origin; Phase 6 TAMA-01/TAMA-02 status reconciled against `git log` after STATE.md was found stale*
