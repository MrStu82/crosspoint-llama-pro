# Roadmap: X4 Pro UI Layout Fixes

## Overview

Four phases, each a separate branch/commit, each verified in a built PlatformIO app image
and reported to parent before the next begins: fix the three cheapest/lowest-risk layout
defects first, then add dual top/bottom status bars, then give the home menu's last row
real breathing room from the bezel, then flip on USB mass storage.

## Phases

**Phase Numbering:** integer phases are the four planned milestones from parent's dispatch, in the exact order given. No decimal/inserted phases.

- [x] **Phase 1: Three defects (§2)** - button-hints reservation, home-menu clamp, screen margin default
- [ ] **Phase 2: Two status bars (§5.2)** - independent top + bottom status bars
- [ ] **Phase 3: Home bottom buffer (§2.2)** - real clearance for the home menu's last row
- [ ] **Phase 4: USB mass storage (§3.1)** - device enumerates as USB MSC

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
**Plans**: TBD

Plans:
- [ ] 02-01: TBD

### Phase 3: Home bottom buffer (§2.2)
**Goal**: Home menu's last row no longer sits flush against the bezel/physical home button.
**Depends on**: Phase 2
**Requirements**: [BUF-01]
**Success Criteria**:
  1. `homeBottomInset=24` is reserved below the last menu row, untouchable by touch input
  2. Six default menu rows still fit above it after the row-pitch reduction (`menuRowHeight` 64→60, `menuSpacing` 8→4)
  3. Seven-item overflow (OPDS enabled) is handled by Phase 1's clamp/scroll, not a special case here
**Plans**: TBD

Plans:
- [ ] 03-01: TBD

### Phase 4: USB mass storage (§3.1)
**Goal**: X4 Pro enumerates as a USB mass storage device when connected to a host.
**Depends on**: Phase 3
**Requirements**: [USB-01]
**Success Criteria**:
  1. Build flags `FREEINK_CAP_USB_MSC=1`, `ARDUINO_USB_MODE=0`, `ARDUINO_USB_CDC_ON_BOOT=1` are set alongside the existing `USE_BLOCK_DEVICE_INTERFACE=1`
  2. Build succeeds with the new flags
  3. Any serial-logging impact from CDC-on-boot is flagged explicitly in the report, not silently absorbed
**Plans**: TBD

Plans:
- [ ] 04-01: TBD

## Progress

**Execution Order:** Phases execute in numeric order: 1 → 2 → 3 → 4

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. Three defects (§2) | 1/1 | Complete | 2026-08-07 |
| 2. Two status bars (§5.2) | 0/? | Not started | - |
| 3. Home bottom buffer (§2.2) | 0/? | Not started | - |
| 4. USB mass storage (§3.1) | 0/? | Not started | - |
