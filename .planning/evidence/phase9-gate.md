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

Executed host harness (`/workspace/agent/ach_test/`, real unmodified `AchievementBus.cpp`/`.h`/
`Achievements.h` compiled against host mocks — same harness class as Requirement 2's sim), covering per
parent's explicit ask: each predicate driven over threshold (fires once), each driven just under threshold
(does not fire), `achievements.bin` written/dumped/reloaded into a fresh instance with flags read back set,
and an already-unlocked id re-triggered (no second unlock event, no re-write).

Compile: `g++ -std=c++17 -Imocks -Isrc -I../crosspoint-llama-pro/lib/Serialization test_main.cpp src/AchievementBus.cpp -o ach_test_bin`

```
PASS: PackRat does NOT unlock at inventoryCount=19 (just under threshold 20)
PASS: PackRat unlocks at inventoryCount=20 (at threshold)
PASS: PackRat unlock message reaches GAME_STATE.addMessage()
PASS: PackRat does not re-fire on a second over-threshold pickup
PASS: SpeedRunner does NOT unlock at turnCount=150 (boundary, condition requires <150)
PASS: SpeedRunner does NOT unlock at dungeonDepth=4 (just under threshold 5) even with low turnCount
PASS: SpeedRunner unlocks at dungeonDepth=5, turnCount=149 (both thresholds satisfied)
PASS: SpeedRunner unlock message reaches GAME_STATE.addMessage()
PASS: SpeedRunner does not re-fire on a second qualifying FloorChanged event
PASS: DeepDiver does NOT unlock at dungeonDepth=9 (just under threshold 10)
PASS: DeepDiver unlocks at dungeonDepth=10 (at threshold)
PASS: DeepDiver unlock message reaches GAME_STATE.addMessage()
PASS: DeepDiver does not re-fire on a second, deeper qualifying FloorChanged event
PASS: MaxedOut does NOT unlock at newLevel=19 (just under threshold 20)
PASS: MaxedOut unlocks at newLevel=20 (at threshold)
PASS: MaxedOut unlock message reaches GAME_STATE.addMessage()
PASS: MaxedOut does not re-fire on a second, higher-level qualifying LevelUp event
PASS: all four new achievements unlocked in-memory before dump/reload
PASS: achievements.bin non-empty on disk before reload
achievements.bin bytes (10): 01 08 01 00 00 00 01 01 01 01
PASS: byte count matches version(1)+count(1)+8 bool flags
PASS: version byte == 1
PASS: count byte == 8 (AchievementId::Count)
PASS: PackRat flag reads back set after reload
PASS: SpeedRunner flag reads back set after reload
PASS: DeepDiver flag reads back set after reload
PASS: MaxedOut flag reads back set after reload
PASS: re-triggering already-unlocked PackRat/MaxedOut adds no new messages (no second unlock event)
PASS: openFileForWrite() call count unchanged after re-trigger (unlock() returns before save() on an already-unlocked id, no re-write to disk)

=== ALL PASS (0 failure(s)) ===
```

Full run including the pre-existing Phase 7 scenarios: 49/49 PASS. Byte layout is
`[version=01][count=08][unlocked[0..7]]`, index order `Ding, ThatllBuffOut, AudienceParticipation,
EscalationOfForce, PackRat, SpeedRunner, DeepDiver, MaxedOut`. In this run's byte dump (`01 00 00 00 01 01
01 01`), byte 0 (`Ding`) is set because Scenario 1 unlocks it earlier in the same run; bytes 1-3
(`ThatllBuffOut`, `AudienceParticipation`, `EscalationOfForce`) stay `00` (unexercised by this scenario);
bytes 4-7 (`PackRat, SpeedRunner, DeepDiver, MaxedOut`) are the four Phase 9 achievements this requirement
targets, all `01`.

"No re-write" is proven via `HalStorage::openFileForWrite()` call-count instrumentation
(`mocks/HalStorage.h`), not filesystem mtime — mtime is only 1-second granular and can't distinguish "never
wrote" from "wrote identical bytes again within the same second."
