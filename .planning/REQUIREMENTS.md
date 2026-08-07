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

- **DEF-01**: Quick-settings swipe sheet (§5.3)
- **DEF-02**: Stats screen redesign (§4.5)

## Out of Scope

| Feature | Reason |
|---------|--------|
| Any [device]-marked spec item | Needs Stuart's hardware to build/verify, not actionable here |
| Quick-settings swipe sheet | Explicitly deferred by parent's dispatch |
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
