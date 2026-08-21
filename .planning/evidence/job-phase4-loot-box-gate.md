# Job Phase 4 — World Dungeon loot-box gate

Date: 2026-08-21
Implementation commit: `f307adc0d4256815d5ca7168522f2e80adb0d58e`

## Scope

- Four visible tiers: Common, Uncommon, Rare, Legendary.
- Exact tier weighting: 50/30/15/5.
- One compact reward table per tier, reusing existing item, buff and skill definitions.
- Four box definitions occupy one logical shared-loot slot, so adding tiers does not multiply overall box frequency.
- Direct inventory opening; item rewards replace the box in place, while gold/buff/skill rewards consume it.
- Quest items and nested boxes are excluded.
- `SPONSOR_DEFS` remains presentation/per-floor identity; it is not used as durable Legendary loot.

## Deterministic proof

Clean clone on Trantor at the exact implementation commit:

```
HEAD f307adc0d4256815d5ca7168522f2e80adb0d58e
VERSION v1.5.0-114-gf307adc
```

Standalone real-path harness:

```
exact 50/30/15/5 tier boundaries PASS
four real tables valid; no quest items, nested boxes, or parallel reward definitions
shared loot boxes: 8965 / 200000
tier counts: 4513 / 2678 / 1344 / 430
ALL CHECKS PASSED
```

Full host suite:

```
100% tests passed, 0 tests failed out of 149
Total Test time (real) = 0.23 sec
```

X4 Pro production build:

```
x4pro SUCCESS
RAM 26.2% (85,900 / 327,680 bytes)
Flash 86.0% (5,635,638 / 6,553,600 bytes)
firmware.bin 5,636,144 bytes
SHA-256 22f41dcb39dd255b0857aa341682309e5e6aa7b7ac4f575fc65616ba09c0e29a
PHASE4_GATE_PASS
```

Persistent gate log: `/workspace/agent/build-scripts/crosspoint_job_phase4_gate.log`.

No intermediate firmware is shipped; the standing delivery contract remains one final image after Phase 12 closes.
