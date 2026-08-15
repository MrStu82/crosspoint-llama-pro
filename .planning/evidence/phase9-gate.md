# Phase 9 host-gate evidence — "Voice — make it Dungeon Crawler Carl"

Host-only gate (no glass gate this phase). Build: Trantor `x4pro`, commit `259c5566fcc048c73e44e470856e5d3c8b051df7`.

## Requirement 1 — zero Tolkien nouns (re-confirmed)

```
grep -rniE "\b(orc|orcs|elf|elves|dwarf|dwarves|hobbit|hobbits|gandalf|mordor|sauron|frodo|gollum|middle-earth|tolkien|goblin|goblins)\b" src/ lib/ --include="*.cpp" --include="*.h"
```
Zero matches.

## Requirement 2 — flavour variant coverage + no-consecutive-repeat

`src/game/FlavorText.h` defines 12 `FlavorCategory` bands (HitGraze, HitSolid, HitHeavy, HitOverkill,
MonsterKilled, DamagedLight, DamagedModerate, DamagedHeavy, PlayerDeath, LevelUp, FloorEntry, BossArrival),
each with exactly 3 `inline constexpr const char*` variants — satisfies the ">=3 per band" floor.

`FlavorTextTracker::pick()` (`src/game/FlavorText.cpp`) draws an index via `GameState::rollRange`, and if the
draw equals the previous index for that category, advances by one (`(idx+1) % count`) before returning —
guarantees no immediate repeat.

Proven by a standalone host simulation (`g++ -std=c++17`, not linked into firmware) mirroring this exact
algorithm against a PRNG stream, 10,000 draws per category:

```
category  0: count=3 coverage=3/3 consecutiveRepeat=no -> PASS
category  1: count=3 coverage=3/3 consecutiveRepeat=no -> PASS
category  2: count=3 coverage=3/3 consecutiveRepeat=no -> PASS
category  3: count=3 coverage=3/3 consecutiveRepeat=no -> PASS
category  4: count=3 coverage=3/3 consecutiveRepeat=no -> PASS
category  5: count=3 coverage=3/3 consecutiveRepeat=no -> PASS
category  6: count=3 coverage=3/3 consecutiveRepeat=no -> PASS
category  7: count=3 coverage=3/3 consecutiveRepeat=no -> PASS
category  8: count=3 coverage=3/3 consecutiveRepeat=no -> PASS
category  9: count=3 coverage=3/3 consecutiveRepeat=no -> PASS
category 10: count=3 coverage=3/3 consecutiveRepeat=no -> PASS
category 11: count=3 coverage=3/3 consecutiveRepeat=no -> PASS

OVERALL: PASS
```

## Requirement 3 — no per-turn heap allocation

Grep for `new`/`malloc`/`std::string`/`std::vector`/`std::to_string` across `FlavorText.{h,cpp}`,
`AchievementBus.cpp`, `Achievements.h`, and every `showNotification()` call site in `GameActivity.cpp`:
zero real matches (the only hits were the substrings "**new** floor generated" and "**new**Level", both
plain English/identifier text, not allocations).

- Flavour selection is pure array indexing — `pick()` returns a direct flash-resident `const char*`, no copy.
- Every notification body is copied via `snprintf(notificationBody_, NOTIFICATION_BODY_LEN, "%s", body)` into
  a fixed 96-byte `GameRenderer` member buffer (`GameRenderer.h:216,220`) — one static buffer, reused every call,
  never resized or heap-backed.

## Requirement 4 — notification box stays inside partial-refresh budget

`FrameDirtyPlanner::FramePlan::windows[4]` (`FrameDirtyPlanner.h:32`) is a fixed 4-slot stack array (viewport,
status bar, message log, notification box) — never heap-growing. `GameRenderer.cpp:140-141` appends the
notification's `DirtyWindow` (from `notificationRect()`, a single bounded rect) only when
`!plan.fullClear && plan.windowCount < 4` — so showing a notification always rides the existing partial-refresh
path (`displayWindow()` on that one rect) and never forces a full-screen clear.

## Requirement 5 — achievements screen completeness / reload-survival / redaction

- **Completeness**: `GameMenuActivity::renderAchievements()` loops `for (i = 0; i < AchievementId::Count; i++)`
  — `Count` is the enum's own sentinel (=8), so the loop always covers every achievement with no drift risk
  between the enum and the render loop.
- **Reload survival**: `AchievementBus::unlock()` calls `save()` on every new unlock, persisting the `unlocked[]`
  bool array to `/.crosspoint/game/achievements.bin`. `ACHIEVEMENTS.load()` is called from
  `GameActivity.cpp:100` (on activity entry), reading that file back — unlock state is durable across app
  restarts, not run-scoped (run-scoped state is the separate `unlockedThisRun[]`, used only by the
  end-of-run screen).
- **Redaction**: the locked branch of `renderAchievements()` only ever calls `tr(STR_DM_ACHIEVEMENT_LOCKED)`
  and `achievementHint(id)` — `achievementShortName(id)` (the real name) is structurally unreachable on that
  branch, not just conditionally hidden.

## Requirement 6 — new achievement fire/no-fire matrix

| Achievement | Event | Fire condition | No-fire condition |
|---|---|---|---|
| `PackRat` | `ItemPickedUp` | `GAME_STATE.inventoryCount >= MAX_INVENTORY` (20) at time of pickup | pickup with `inventoryCount < 20` |
| `SpeedRunner` | `FloorChanged` | `dungeonDepth >= 5 && turnCount < 150` | floor change with `dungeonDepth < 5`, or `dungeonDepth >= 5` but `turnCount >= 150` |
| `DeepDiver` | `FloorChanged` | `dungeonDepth >= 10` | floor change with `dungeonDepth < 10` |
| `MaxedOut` | `LevelUp` | `event.newLevel >= 20` | level-up with `newLevel < 20` |

All four route through the shared `AchievementBus::unlock()`, which no-ops on a second call for an
already-unlocked id (`if (unlocked[idx]) return;`, `AchievementBus.cpp:76`) — confirmed idempotent, matching
the Phase 7 fire/no-fire pattern for the original 4 achievements (re-checked, unchanged this phase).
