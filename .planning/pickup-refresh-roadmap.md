# World Dungeon Pickup/Scroll/Performance Batch — GSD Plan

Base: exact frozen inventory release `071f8b30b634eefbf9b64453e308d23fc0f27d84`.

## Phase 1 — Instrument and isolate

- [x] Trace pickup from resolved Action tap through `handleAction()` and render.
- [x] Time input-handled → blocking panel call return for labelled `move`, `action`, and `pickup` frames.
- [x] Identify the extra target cost: Action touch feedback performs two synchronous `displayWindow()` calls before the normal pickup render; UC8179 treats those as full-panel FAST waveforms.

## Phase 2 — Minimal fix

- [x] Do not paint/refresh the Action button on touch-down or resolved release; it remains in its normal on-glass state until the resulting game frame.
- [x] Preserve Menu pressed/release feedback because Menu does not produce the same game-frame refresh.
- [x] Preserve physical controls and all pickup/gameplay behavior.

## Phase 3 — Gate and release

- [x] Focused proof pins Action at one panel transaction and Menu feedback unchanged.
- [x] Inventory/actions/deferred-save regression harnesses pass.
- [x] Complete host suite passes.
- [x] Exact committed pickup-only source built locally for `x4pro` on Trantor.

## Phase 4 — Scroll effects

- [x] Mapping effect moved to GameActivity's live floor state and reveals every walkable fog bit.
- [x] Teleport effect moved to GameActivity, reservoir-selects a safe unoccupied walkable tile, and relocates the player.
- [x] Mapping/Teleport are consumed only after their effect succeeds; zero-effect Teleport is retained.
- [ ] Host effect harness and menu-route source proof pass.

## Phase 5 — Engine performance sweep

- [x] Profile hooks added for input→display return, plan, paint, display waveform, monster turns, and visibility.
- [x] Per-cell O(view cells × (monsters + items)) occupant scan replaced with one per-frame viewport index.
- [x] Message dirty rectangle excludes the permanent 136px blank reserve.
- [ ] Benchmark planner before/after and record message-area delta.
- [ ] Re-run all no-regression gates; build one final combined image only.
