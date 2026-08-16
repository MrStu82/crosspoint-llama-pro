# Phase 10 host-gate evidence — "Decisions" (throw resolution)

Host-only gate (no glass gate this phase). Build: Trantor `x4pro`, SUCCESS, 59.91s.
Commit under gate: `b1b4f1e215fa171b2877a576184bae93f73ab931` (`GameActivity.cpp::handleThrow()` —
damage calc, item consumption, and `emit(ItemThrown)` landed together in one commit-set, per
parent's explicit instruction).

Parent's ruling (msg 3756, already settled, no code change needed): Percussive Maintenance stays
gated on `killedMonster` — "merely lobbing a potion at a wall isn't" the achievement, killing with
a thrown object is.

## Requirement 1 — throwable/non-throwable item correctness

`src/game/GameTypes.h`, `ITEM_DEFS[]` — full table inspected via grep:

```
{"Dagger",              '/', Weapon, 0,   5,  2, 0, true }
{"Short Sword",         '/', Weapon, 1,  15,  4, 0, true }
{"Long Sword",          '/', Weapon, 2,  30,  6, 0, true }
{"Battle Axe",          '/', Weapon, 3,  50,  8, 0, true }
{"Nanoweave Blade",     '/', Weapon, 4, 200, 12, 0, true }
{"Leather Armor",       '[', Armor,  0,  10,  0, 2, false}
{"Chain Mail",          '[', Armor,  1,  30,  0, 4, false}
{"Plate Mail",          '[', Armor,  2,  60,  0, 6, false}
{"Nanoweave Coat",      '[', Armor,  3, 300,  0,10, false}
{"Wooden Shield",       ')', Shield, 0,   8,  0, 1, false}
{"Iron Shield",         ')', Shield, 1,  25,  0, 3, false}
{"Potion of Healing",   '!', Potion, 0,  20,  0, 0, true }
{"Potion of Mana",      '!', Potion, 1,  25,  0, 0, true }
{"Potion of Strength",  '!', Potion, 2,  50,  0, 0, true }
{"Scroll of Identify",  '?', Scroll, 0,  15,  0, 0, false}
{"Scroll of Teleport",  '?', Scroll, 1,  30,  0, 0, false}
{"Scroll of Mapping",   '?', Scroll, 2,  40,  0, 0, false}
{"Rations",             '%', Food,   0,   5,  0, 0, false}
{"Nutrient Bar",        '%', Food,   1,  30,  0, 0, false}
{"Gold Coins",          '$', Gold,   0,   1,  0, 0, false}
{"Ring of Power",       '=', Ring,   0, 999,  0, 0, false}
```

All 5 weapons and all 3 potions (offensive/consumable items) are `throwable=true`. All armor,
shields, scrolls, food, gold, and the quest ring (worn/passive/utility/quest items) are
`throwable=false`. `handleThrow()` (`GameActivity.cpp:531`) enforces this with an early return —
`if (def == nullptr || !def->throwable) return;` — PASS.

## Requirement 2 — throw damage curve structurally distinct from melee

Melee (`GameActivity.cpp:284-287`):
```
atkPower = strength + equippedAttackBonus()
damage = max(1, atkPower - defense)
damage = max(1, damage +- damage/4)   [rollRangeInclusive]
```

Throw (`GameActivity.cpp:579-581`):
```
atkPower = dexterity/2 + item.attack + item.enchantment
damage = max(1, atkPower - defense)
damage = max(1, damage +- damage/3)   [rollRangeInclusive]
```

Structurally distinct on two axes: melee uses the player's full strength stat, throw uses half
the dexterity stat plus the item's own attack/enchantment (a different stat entirely, halved, plus
a per-item modifier melee doesn't have); melee's variance denominator is 4 (+-25%), throw's is 3
(+-33%) — a thrown item is less consistent than a wielded one, deliberately.

Proven via a standalone host simulation (`g++ -std=c++17 -O2`, not linked into firmware) mirroring
both formulas verbatim, swept across 462 stat/bonus/defense combinations at zero-roll (no RNG
noise, isolating the deterministic part of each curve):

```
cases=462 mismatches=442
structural_check: melee uses full 'stat', throw uses 'stat/2' -> divergence expected
example: stat=20 bonus=6 defense=4 -> melee=22 throw=12 (DISTINCT)
variance_width: melee=6 throw=8 (DISTINCT)
RESULT: PASS
```

The 20 non-mismatching cases were verified (separately, via a Python cross-check) to be
`max(1, ...)` floor-clamp coincidences only — both formulas independently bottoming out at
damage=1 under low-stat/high-defense inputs, never the underlying `atkPower - defense` arithmetic
actually agreeing. The concrete example and variance-width check confirm the curves diverge in the
normal (non-floor-clamped) case. PASS.

## Requirement 3 — stack-aware item consumption on throw

`GameActivity.cpp:620-628`:
```cpp
if (item.count > 1) {
  GAME_STATE.inventory[inventoryIndex].count--;
} else {
  for (int i = inventoryIndex; i < GAME_STATE.inventoryCount - 1; i++) {
    GAME_STATE.inventory[i] = GAME_STATE.inventory[i + 1];
  }
  GAME_STATE.inventoryCount--;
}
```

Proven via a standalone host simulation mirroring this block verbatim, exercising: a stacked item
(count=3) decrementing on throw with `inventoryCount` and neighboring slots untouched; a stack
drained to count=1 then thrown a second time, correctly shift-removing and decrementing
`inventoryCount` only on that final throw; a single-count item thrown from a middle inventory
index, preserving trailing order and leaving preceding slots untouched; the last-slot edge case
(no-op shift loop, still decrements); and a full N=5 sequential drain of a count=5 stack, confirmed
to decrement exactly 4 times then shift-remove on the 5th, never leaving a count=0 slot in place.

```
PASS: stacked item (count=3) decrements to 2 on throw
PASS: inventoryCount unchanged when a stacked item is only decremented
PASS: other slots undisturbed by a decrement-only consume
PASS: second-to-last throw of a stack decrements to 1, slot remains
PASS: final throw of a stack (count==1) removes the slot, inventoryCount decremented
PASS: shift-remove moves the following item into the emptied slot
PASS: shift-remove preserves relative order of remaining items
PASS: single-count item throw from a middle index decrements inventoryCount
PASS: shift-remove from a middle index preserves trailing order
PASS: shift-remove from a middle index leaves preceding slots untouched
PASS: throwing the last slot's single-count item removes it (no shift needed, loop body no-ops)
PASS: the remaining earlier slot is untouched when the last slot is thrown
PASS: N=5 sequential throws of a count=5 stack decrement 4 times then remove on the 5th, never hitting count=0 in place

RESULT: PASS
```

## Requirement 4 — PercussiveMaintenance fire/no-fire matrix

Executed host harness (`/workspace/agent/ach_test/`, real unmodified `AchievementBus.cpp`/`.h`/
`Achievements.h` compiled against host mocks — same harness used for Phase 9's Requirement 6),
extended with a new scenario covering: locked at start; a thrown non-kill hit (`ItemThrown`,
`killedMonster=false`) does not unlock and emits no message; a melee kill (`MonsterKilled` event)
does not unlock it either, proving the gate keys on event *type* not just any kill signal; a thrown
kill (`ItemThrown`, `killedMonster=true`) unlocks it and the flavor message reaches
`GAME_STATE.addMessage()`; and a second thrown kill does not re-fire a message.

Compile: `g++ -std=c++17 -I src -I mocks -I ../crosspoint-llama-pro/lib/Serialization -c src/AchievementBus.cpp -o AchievementBus.o && g++ -std=c++17 -I src -I mocks test_main.cpp AchievementBus.o -o ach_test_bin`

```
PASS: PercussiveMaintenance starts locked
PASS: ItemThrown with killedMonster=false does NOT unlock PercussiveMaintenance
PASS: ItemThrown non-kill emits no achievement message
PASS: MonsterKilled (melee kill) event does NOT unlock PercussiveMaintenance
PASS: ItemThrown with killedMonster=true unlocks PercussiveMaintenance
PASS: PercussiveMaintenance unlock message reaches GAME_STATE.addMessage()
PASS: PercussiveMaintenance does not re-fire a message on a second thrown kill
```

Full harness run including all pre-existing Phase 7/9 scenarios: 58/58 PASS, 0 failures
(`=== ALL PASS (0 failure(s)) ===`). Byte layout re-verified at the new `AchievementId::Count == 9`
(PercussiveMaintenance added to the enum): `01 09 01 00 00 00 01 01 01 01 00` — 11 bytes =
version(1) + count(1) + 9 flags. Two pre-existing Phase-9-era hardcoded assertions
(`test_main.cpp:338/340`, asserting a stale `Count == 8`) were corrected to `9` as part of this
proof — a harness staleness bug from before this phase, not a Phase 10 regression; the real
on-disk serialization was already correct.

## Requirement 5 — no dirty-rect/redraw regression

`handleThrow()` (`GameActivity.cpp:517-645`) calls `requestUpdate()` exactly once, at the end of
the function (`GameActivity.cpp:644`) — the identical single-call-per-handler pattern used by every
other action handler (`handleMove()`, `handleAction()`, etc.). `requestUpdate()`
(`Activity.cpp:23`) delegates to `activityManager.requestUpdate()`, which drives the same
`FrameDirtyPlanner::FramePlan::windows[4]` fixed 4-slot bounded-window mechanism proven in Phase
9's Requirement 4 evidence — no new redraw path is introduced.

The achievement-unlock notification on a thrown kill (`GameActivity.cpp:635`,
`gameRenderer.showNotification(NotificationKind::Achievement, ...)`) is the exact same call site
used by every other achievement unlock, level-up, floor-entry, and boss-arrival notification
(`GameActivity.cpp:305,387,395,407,493,635,799,829`) — no throw-specific notification code exists.
`GameRenderer::showNotification()` (`GameRenderer.cpp:531-535`) only sets `notificationDirty_ =
true` and copies the body into the existing fixed 96-byte buffer; `notificationRect()`
(`GameRenderer.cpp:543-554`) returns a fixed-position rect depending only on screen layout, never
on content — the same bounded `DirtyWindow` the frame planner already accounts for. PASS.

## Summary

| Req | Description | Result |
|-----|--------------|--------|
| 1 | Throwable/non-throwable item correctness | PASS (source inspection) |
| 2 | Damage curve structurally distinct from melee | PASS (host sim, 462 cases) |
| 3 | Stack-aware item consumption | PASS (host sim, 13 assertions) |
| 4 | PercussiveMaintenance fire/no-fire matrix | PASS (host harness, 58/58 total) |
| 5 | No dirty-rect/redraw regression | PASS (source inspection) |

All 5 requirements PASS. Phase 10 ("Decisions" — throw resolution) is closed.

Open judgment call flagged for parent, not a defect: `handleThrow()`'s kill branch emits only
`GameEventType::ItemThrown` (per parent's literal instruction), not also `MonsterKilled` — so a
thrown kill can never trigger the pre-existing `EscalationOfForce` overkill achievement (gated on
`MonsterKilled`). Deliberate reading of the instruction as given, surfaced for parent to weigh in
on if that's not the intended scope.
