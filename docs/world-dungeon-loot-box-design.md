# World Dungeon loot boxes — Job Phase 4 shipping design

## Locked shipping model

- Four tiers: **Common, Uncommon, Rare, Legendary**.
- Tier roll is fixed and explicit: **50 / 30 / 15 / 5 percent**.
- Four `ItemDef` records are one logical slot in the shared floor/corpse loot table, so adding
  visible tiers does not multiply box frequency.
- The player opens a box directly from inventory at any time. There is no safe-room gate.
- One compact reward table per tier; no box-type registry and no second reward framework.
- Tables reference existing `ITEM_DEFS`, `BUFF_DEFS`, and `SKILL_DEFS`. Quest items and nested
  boxes are excluded.
- A box transforms in place for item rewards (works with a full pack) and is consumed for gold,
  buff, or skill rewards.
- `SPONSOR_DEFS` supplies the signed presentation/courtesy identity only. It is deliberately not
  a Legendary reward pool because `activeSponsorId` is transient per-floor state; Legendary
  rewards use the existing run-persistent Skill pool instead.
- Pets remain independent of boxes: each run starts with its separately rolled companion.

## Tier intent

| Tier | Existing pools used |
|---|---|
| Common | baseline items, consumables, food, gold |
| Uncommon | mid-grade items and entry buffs |
| Rare | upper-grade items and the wider buff pool |
| Legendary | top gear and existing skills |

This is intentionally the smallest playable implementation: four fixed tables and two tiny
selectors (`rollLootBoxTier`, `selectLootBoxReward`). Future quest/achievement-specific sources
may award these same four item subtypes without changing the opening model.
