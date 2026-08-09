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

## v2 Requirements

Deferred, not in this dispatch.

### Deferred UI

- ~~**DEF-01**: Quick-settings swipe sheet (§5.3)~~ — back in scope, Stuart 2026-08-09 (see below)
- **DEF-02**: Stats screen redesign (§4.5)

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
| BUF-01 | Phase 3 | Pending |
| USB-01 | Phase 4 | Pending |

**Coverage:**
- v1 requirements: 9 total
- Mapped to phases: 9
- Unmapped: 0 ✓

---
*Requirements defined: 2026-08-07*
*Last updated: 2026-08-07 after Phase 1 investigation*
