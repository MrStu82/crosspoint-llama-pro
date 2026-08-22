# World Dungeon Pickup Refresh — GSD Plan

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

- [ ] Focused proof pins Action at one panel transaction and Menu feedback unchanged.
- [ ] Inventory/actions/deferred-save regression harnesses pass.
- [ ] Complete host suite passes.
- [ ] Exact committed source builds locally for `x4pro` on Trantor.
- [ ] Bare app image verified with SHA-256 and esptool image-info.
