# World Dungeon loot boxes — Phase 4 design checkpoint

## Locked rules

- Six rarity tiers, in order: **Bronze, Silver, Gold, Platinum, Legendary, Celestial**.
- A box is awarded by achievements, bosses, quests, or sponsor/viewer gifts. Boss tier rises with dungeon depth.
- The player may open a queued box at any time. There is **no safe-room gate**.
- Rewards are deliberately race-critical rather than class-safe. An item may be unstable or damaged; its description must say so before it is used.
- Pets are never box rewards: each run starts with its independently rolled companion.
- Sponsor/Benefactor boxes are deliberately off-ladder: they may contain exceptional or off-theme rewards.

## Minimal shipping model

Keep the current `Sponsor Crate` as the first concrete box implementation. When box presentation is expanded, use a small type × tier grid rather than a separate one-off implementation per reward source:

| Type | Purpose |
|---|---|
| Adventurer | General player progression rewards |
| Boss | Depth-scaled combat/key-to-race rewards |
| Sponsor/Benefactor | Sponsored, exceptional, potentially off-theme rewards |
| Quest | Objective-specific rewards |

A box record therefore needs only `type`, `tier`, and an optional `unstable/damaged` modifier. Opening selects from its matching table, consumes the box, and displays the item description before equip/use. This avoids a box-generation framework and preserves room for future content.

## Explicit Stuart decisions before implementation

1. **How many types ship in the first playable release?** Recommendation: start with Adventurer, Boss, and Sponsor/Benefactor; add Quest only when quest rewards exist.
2. **What are the per-tier drop tables and weights?** This includes which key-to-race items, damage/instability odds, currency range, and any sponsor-only exclusions belong at each tier.

Until those two content decisions are made, no further loot-box code is warranted. The existing crate remains valid and independently testable; this note is the Phase 4 design checkpoint, not a new feature implementation.
