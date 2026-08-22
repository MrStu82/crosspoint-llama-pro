# Inventory Feature Completion — pinned `f852309`

Goal: close Stuart's inventory interaction gaps without disturbing the pinned
World Dungeon feature line or its render-stack correction.

## Phase 1 — Sorted inventory — COMPLETE

- Stable, allocation-free grouping by `ItemType` when Inventory is entered.
- Preserve item flags, stacks, enchantments, and same-type order.
- Host harness proves ordering and record preservation.

## Phase 2 — Item action menu — COMPLETE

- Inventory activation opens a four-row action screen: Use,
  Equip/Unequip, Drop, Sell (with exact gold amount).
- Use and Equip are type-appropriate; Drop returns the item to the current
  floor; Sell removes it and credits its base value × stack count.
- Physical and touch paths drive the same action dispatcher.
- Gameplay Action/Menu controls remain touch-safe but shrink to a 220px right-aligned column; draw and hit-test share `actionMenuButtonRect()`.
- All visible copy is translated through `tr()`.

## Phase 3 — New-item lifecycle — COMPLETE

- Ground pickups gain `ItemFlag::New`.
- Inventory rows surface the flag.
- Leaving Inventory for the pause menu or game clears all New flags once.
- Entering/cancelling the action submenu does not clear them prematurely.

## Phase 4 — Corpse-loot pause — COMPLETE

- Remove synchronous achievement-file writes from kill/pickup hot paths.
- Mark achievement state dirty on unlock and flush at existing save/exit
  boundaries, preserving persistence without an SD write inside combat input.
- Host harness proves no save call occurs during kill/pickup-style emit and
  that explicit flush persists once.

## Phase 5 — Gate and release — IN PROGRESS

- Run focused host harnesses and the repository host suite.
- Build `x4pro` locally on Trantor from the committed frozen-base branch.
- Produce and verify the bare app image for offset `0x10000`.

