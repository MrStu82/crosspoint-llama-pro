# Milestone 3 final gate — Phases 11 and 12

Date: 2026-08-21
Final source commit built: `f852309adc59b404acf827d4d0fcecee8ecc8810`
Build provenance: `v1.5.0-115-gf852309`
Product version: `1.5.0-llmp`

## Phase 11 — reward economy / The Show

- The complete append-only achievement pool contains 250 definitions; 50 are dealt per run from the full id space.
- Mechanical rewards reuse the existing run-scoped buff/skill model.
- Loot boxes ship as four visible Common/Uncommon/Rare/Legendary tiers with an exact 50/30/15/5 selector and compact existing-definition reward tables.
- The four box records occupy one logical shared-loot slot; quest items and nested boxes are excluded.
- Direct opening and achievement/menu presentation reuse the existing inventory and notification surfaces.

Key commits: `f38799c`, `7253047` through `9a419ee`, `f307adc`.
Detailed loot-box proof: `.planning/evidence/job-phase4-loot-box-gate.md`.

## Phase 12 — corpse loot / The Co-star

- Every eligible kill produces independent player-visible and companion-only loot streams from the same depth-scaled table.
- Companion loot never receives a map position, never renders, never enters player inventory and is foraged autonomously.
- Companion species, name and attributes are independently rolled at run creation; the companion levels with the player and auto-equips its best gear.
- The companion has its own profile/progress/gear surface and a Random-name control reusing the single canonical name pool.
- Companion occupancy and save boundaries reject invalid indexes, terminate stored names and migrate v6 companions with a valid map position.

Key commits: `bc549e9`, `d00b446`, `4e52954`, `613d6e9` through `e7ac46a`.

## Exact final host gate

The final source commit was checked out clean on Trantor. Deterministic checks:

```
LootBoxEligibilityHarness: ALL CHECKS PASSED
PetSaveHardeningHarness (ASan+UBSan, no recovery): ALL CHECKS PASSED
Full CMake/gtest: 149/149 passed
CorpseLootTest included in the 149-test suite
X4 Pro production build: SUCCESS
RAM: 26.2% (85,900 / 327,680 bytes)
Flash: 86.0% (5,635,638 / 6,553,600 bytes)
```

The first final-artifact attempt exposed a stale PlatformIO object-cache provenance string after a documentation-only commit. The gate was rerun after removing both `.pio/build/x4pro` and the repository `.cache`; the genuine clean build produced different bytes and is the only retained final artifact.

Final bare updater/app image (flash offset `0x10000`):

```
/workspace/agent/crosspoint-x4pro-world-dungeon-final-v1.5.0-115-gf852309-app-0x10000.bin
size: 5,636,144 bytes
SHA-256: bf942b64c9fd57ca181029359a501999065968ea852cd84c2438bef97df1790e
ESP32-S3 image magic: e9
embedded product version: 1.5.0-llmp
```

Persistent logs:
- `/workspace/agent/build-scripts/crosspoint_milestone3_final.log` — exact-source harnesses, 149/149 suite, initial production build.
- `/workspace/agent/build-scripts/crosspoint_milestone3_clean_final.log` — cache-purged final production rebuild and final artifact hash.
