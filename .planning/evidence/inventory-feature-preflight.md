# Inventory Feature Preflight — frozen base `f852309`

- Source transferred from `/workspace/agent/crosspoint-f852309-release` to a fresh Trantor tree.
- Focused inventory harness: `PASS (0 failures)`.
- Deferred achievement persistence harness: `PASS (combat writes=0, safe flush=1, failed flush retries)`.
- Full repository host suite: `150/150` passed.
- X4 Pro PlatformIO build: `SUCCESS` (`RAM 26.2%`, `Flash 86.0%`).
- Terminal marker: `INVENTORY_PREFLIGHT_PASS`; SSH exit `0`.
- Raw log: `/workspace/agent/build-scripts/crosspoint_inventory_preflight.log`.

## Corpse-loot pause trace

The corpse/drop and pickup loops are strictly bounded and do not request a forced full refresh. The synchronous work on that input path was `AchievementBus::unlock() -> save()`, which rewrote `achievements.bin` immediately and could repeat for multiple unlocks on one kill/pickup. The implementation now marks lifetime state dirty during `emit()` and performs one retryable `flush()` at existing menu/save/exit boundaries. The focused harness proves zero storage writes during emit and exactly one write at the safe boundary.
