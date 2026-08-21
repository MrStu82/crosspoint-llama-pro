# Phase 13 — corrupt-save touch options

## Baseline
Live release baseline `7598e30` rendered Purge/Leave rows but `GameActivity::loop()` explicitly had no screen-tap path.

## Implementation
- `GameRenderer::corruptNoticeOptionRect()` is the single geometry source for draw and hit-test.
- `hitTestCorruptSaveNoticeOption()` returns Purge=0, Leave=1, or -1 outside.
- `GameActivity::loop()` maps a resolved tap to the existing selection and commits the same pre-existing Purge/Leave branches.
- Up/Down, Confirm, and Back paths are unchanged.

## Host proof
`test/corrupt_notice_hittest/CorruptNoticeHitTestHarness.cpp` compiles real `GameRenderer.cpp` and reports:

`PASS: Purge=[70..409,469..502], Leave=[70..409,511..544], exact 340x34 rows + 8px gap`

The +13px Y position is the previously Pixel-signed Phase-12 title/body spacing; the shared helper preserves that rendered geometry exactly while making it tappable.

Full X4 Pro build and independent Pixel/Gauge verdicts are pending.
