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

## Technical notes (discovered building the itemType-fix test harness, `77d5bfa`)

- **The per-run draw guard applies to every achievement id, including the
  legacy hand-coded ones.** `AchievementBus::resetRun()` does a partial
  Fisher-Yates shuffle over the *entire* `AchievementId` space (id 0 through
  `ACHIEVEMENT_DEF_COUNT`), drawing `DRAWN_POOL_SIZE` (50) ids into
  `drawnThisRun_[]`. `unlock(id)` checks `isDrawnThisRun(id)` for any
  `id >= FIRST_DRAWABLE_ID` (`FIRST_DRAWABLE_ID = 0`) and silently no-ops if
  the id wasn't drawn this run — a fully-satisfied condition still won't
  fire unless its id happened to land in that run's 50-slot draw. This is
  **not limited to the newer data-driven `CONDITIONS[]` entries (id 48+)** —
  it applies identically to the original hand-coded achievements (id 0-47ish)
  emitted from `AchievementBus::emit()`'s switch. Any test or manual repro
  against a low/legacy id must loop across simulated `resetRun()` draws (or
  force the id into the pool) or it will falsely read as "never unlocks."
  This was never written down anywhere before it cost a test-harness debug
  cycle to rediscover — don't let it go undocumented again.

## Buckets

| # | Milestone | Bucket | Entries | Status |
|---|-----------|--------|---------|--------|
| 1 | 1 | Depth | 30 + 12 demo | **DONE** — `7253047` |
| 2 | 2 | Combat | 35 | **DONE** — `6eab6be` |
| 3 | 2 | Survival | 25 | **DONE** — `62057f4` |
| 4 | 2 | Exploration | 25 | **DONE** — `79e4f30` |
| 5 | 3 | Loot & Economy | 35 | **DONE** — `95b883f` |
| 6 | 3 | Curiosities & Secrets | 25 | **DONE** — `9c5fdb7` |
| 7 | 3 | Pet & Companion | 15 | not started (inert until pet system exists) |

**Running total: 235 / 250.**

## Bucket scope notes

- **Combat (35)** — kill counts, crits, overkill, multi-kills, being
  surrounded, out-levelled kills, pacifist descents.
- **Survival (25)** — HP thresholds, near-death recoveries, turn-count
  endurance, character-level milestones, deaths.
- **Exploration (25)** — full-floor clears, tiles walked, stairs-rush vs.
  scenic-route, dead ends, revisits.
- **Loot & Economy (35)** — gold-accumulation tiers (7), item-pickup-count
  tiers by type: ring/amulet/armor/shield/food/weapon/lootbox (7),
  item-usage tiers restricted to scroll/food (4, avoids the saturated
  legacy potion thresholds), loot-box-opened tiers (4), depth+gold
  "efficient looter" compounds (5), ascetic no-item-pickup achievements —
  per-floor, per-run, and a depth-gated compound (3), high-tier
  ring/amulet pickup + full-loadout/well-equipped/grand-bazaar compounds
  (5). Compound rows needed `compoundA`/`compoundB` widened from `int8_t`
  to `int16_t` (pool now exceeds 127 rows).
- **Curiosities & Secrets (25)** — no dedicated "secret found" mechanic exists
  anywhere in the codebase (confirmed twice by grep), so this bucket is built
  entirely from existing CounterField/GameEventType/EventField/ItemType
  primitives confirmed unused elsewhere: beefy-monster kill tiers via
  MonsterMaxHp (4), live-HP curiosity tiers via CounterField::Hp (3),
  thrown-item tiers for potion/scroll/food — ItemThrown previously only
  covered Weapon (6), pickup tiers for potion/gold — ItemPickedUp previously
  only covered scroll/ring/amulet/armor/shield/weapon/lootbox (4),
  floor-hopping tiers via FloorChanged as an EventCount row — previously
  only a struct flag (3), no-item-use pacifism via NoEventInWindow/ItemUsed
  plus a depth-gated compound (2), and three cross-bucket "coincidence"
  compounds mashing up unrelated conditions from other buckets for a
  secrets flavor (3).
- **Pet & Companion (15)** — TBD detail, Milestone 3. Authored inert — draws
  as locked, no mechanical dependency on the pet system existing yet.

## Milestone map

- **Milestone 1** — Depth bucket. 90/250. DONE.
- **Milestone 2** — Combat + Survival + Exploration. 90 → 175/250. Combat DONE (125/250), Survival DONE (150/250), Exploration DONE (175/250). **Milestone 2 complete.**
- **Milestone 3** — Loot & Economy + Curiosities & Secrets + Pet & Companion.
  175 → 250/250. Loot & Economy DONE (210/250), Curiosities & Secrets DONE
  (235/250). Full pool green pending the final Pet & Companion bucket.
