# World Dungeon — 250-Entry Achievement Pool Plan

Branch: `phase-7-world-dungeon-correctness`. Source of truth for the achievement
pool build-out — reconstructed 2026-08-18 after Forge lost this thread mid-run
(it had existed only in Skippy's head + one session's context). Read this file
first before touching `ACHIEVEMENT_DEFS` again.

## Rules (append-only, no exceptions)

- `ACHIEVEMENT_DEFS[]` (`src/game/GameTypes.h`) is **append-only**. The array
  index is the persisted achievement id. Never reorder, never delete, never
  insert in the middle.
- `ACHIEVEMENT_DEF_COUNT` is derived (`sizeof(ACHIEVEMENT_DEFS) / sizeof(...)`)
  — never hand-maintained.
- Buffs/skills granted as rewards are **run-scoped** — wiped at `newGame()`.
  Only the unlock record (`unlocked[]`) persists across runs. See `a36aba9`
  for why this matters (reward gated on the wrong flag once already).
- Save is v5 and must migrate forward, never wipe. Stuart is playing this
  build live.
- Reward ladder: common entries → small run-scoped `Buff`; rare entries →
  `Skill`. Some entries pay `None`, `Title`, `SponsorUnlock`, or `LoreUnlock`
  per the existing enum — mix per bucket, not every entry needs a mechanical
  reward.
- Pet/Companion bucket entries are authored and appended now even though the
  pet system doesn't exist yet — they draw as locked/unreachable and cost
  nothing until the pet lands.
- YAGNI/DRY/KISS. One bucket = one commit, compiled green on Trantor
  (local gradle/PlatformIO — never GitHub Actions) before moving to the next.
- Every status ping to Skippy carries: bucket name, entry count, running
  total against 250.

## Buckets

| # | Milestone | Bucket | Entries | Status |
|---|-----------|--------|---------|--------|
| 1 | 1 | Depth | 30 + 12 demo | **DONE** — `7253047` |
| 2 | 2 | Combat | 35 | **DONE** — `6eab6be` |
| 3 | 2 | Survival | 25 | **DONE** — `62057f4` |
| 4 | 2 | Exploration | 25 | **DONE** — `79e4f30` |
| 5 | 3 | Loot & Economy | 35 | not started |
| 6 | 3 | Curiosities & Secrets | 25 | not started |
| 7 | 3 | Pet & Companion | 15 | not started (inert until pet system exists) |

**Running total: 175 / 250.**

## Bucket scope notes

- **Combat (35)** — kill counts, crits, overkill, multi-kills, being
  surrounded, out-levelled kills, pacifist descents.
- **Survival (25)** — HP thresholds, near-death recoveries, turn-count
  endurance, character-level milestones, deaths.
- **Exploration (25)** — full-floor clears, tiles walked, stairs-rush vs.
  scenic-route, dead ends, revisits.
- **Loot & Economy (35)** — TBD detail, Milestone 3.
- **Curiosities & Secrets (25)** — TBD detail, Milestone 3.
- **Pet & Companion (15)** — TBD detail, Milestone 3. Authored inert — draws
  as locked, no mechanical dependency on the pet system existing yet.

## Milestone map

- **Milestone 1** — Depth bucket. 90/250. DONE.
- **Milestone 2** — Combat + Survival + Exploration. 90 → 175/250. Combat DONE (125/250), Survival DONE (150/250), Exploration DONE (175/250). **Milestone 2 complete.**
- **Milestone 3** — Loot & Economy + Curiosities & Secrets + Pet & Companion.
  175 → 250/250. Full pool green.
