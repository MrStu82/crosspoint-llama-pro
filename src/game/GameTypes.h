#pragma once

#include <cstdint>

namespace game {

// --- Map Constants ---

constexpr int MAP_WIDTH = 80;
constexpr int MAP_HEIGHT = 50;
constexpr int MAP_SIZE = MAP_WIDTH * MAP_HEIGHT;            // 4000 bytes
constexpr int FOG_SIZE = (MAP_SIZE + 7) / 8;                // 500 bytes (bitfield)
constexpr int MAX_MONSTERS = 30;
constexpr int MAX_ITEMS_PER_LEVEL = 40;
constexpr int MAX_INVENTORY = 20;
constexpr int MAX_MESSAGES = 10;
constexpr int MAX_DEPTH = 26;

// --- Hunger clock (Phase 11) ---
// Increments by 1 every player turn (see processMonsterTurns()). Fully relieved
// (reset to 0) by eating any Food item -- item use never advances turnCount, so
// eating is always instant and safe regardless of current hp/hunger, guaranteeing
// escapability from the worst reachable state as long as one food item exists.
constexpr uint16_t HUNGER_MAX = 500;
constexpr uint16_t HUNGER_HUNGRY_THRESHOLD = 300;
constexpr uint16_t HUNGER_STARVING_THRESHOLD = 450;
// Applied every turn once hunger has capped at HUNGER_MAX -- exceeds the fastest
// possible natural regen (1 hp per >=5 turns), so starvation is always a strictly
// net loss and death is bounded at HUNGER_MAX + ceil(maxHp / this) turns with zero food.
constexpr uint16_t HUNGER_STARVE_DAMAGE = 2;

// --- Enums ---

enum class Tile : uint8_t {
  Wall,
  Floor,
  DoorClosed,
  DoorOpen,
  StairsUp,
  StairsDown,
  Rubble,
  Water,
  TileCount
};

enum class MonsterState : uint8_t { Asleep, Wandering, Hostile };

enum class ItemType : uint8_t {
  Weapon,
  Armor,
  Shield,
  Potion,
  Scroll,
  Food,
  Gold,
  Ring,
  Amulet,
  LootBox,
  ItemTypeCount
};

enum class ItemFlag : uint8_t {
  None = 0,
  Identified = 1 << 0,
  Cursed = 1 << 1,
  Equipped = 1 << 2,
};

enum class Direction : uint8_t { North, South, East, West };

// --- Structs ---

struct Player {
  int16_t x = 0;
  int16_t y = 0;
  uint16_t hp = 20;
  uint16_t maxHp = 20;
  uint16_t mp = 5;
  uint16_t maxMp = 5;
  uint16_t strength = 10;
  uint16_t dexterity = 10;
  uint16_t constitution = 10;
  uint16_t intelligence = 10;
  uint16_t charLevel = 1;
  uint32_t experience = 0;
  uint16_t gold = 0;
  uint8_t dungeonDepth = 1;
  uint32_t gameSeed = 0;
  uint32_t turnCount = 0;
  uint16_t kills = 0;
  // Persistent combat RNG stream state (see struct Rng below). Seeded once from gameSeed at
  // run start, advanced (never reseeded) by every combat/AI roll, serialised with the save so a
  // reloaded run continues the exact same stream instead of restarting it.
  uint32_t combatRngState = 1;
  // Hunger clock (Phase 11). 0 = fully fed, HUNGER_MAX = starving (takes damage
  // every turn). See processMonsterTurns() for the tick and GameMenuActivity's
  // Food case for the reset-on-eat.
  uint16_t hunger = 0;
  // Active per-floor sponsor (Phase 11), index into SPONSOR_DEFS. Rolled fresh
  // on every floor load (see GameActivity::loadOrGenerateLevel()) via a true
  // RNG stream -- never derived from the level seed, so reloading a floor can
  // roll a different sponsor. Base stats (strength/maxHp/etc.) are never
  // mutated by a sponsor; its modifier is applied only at the point of use
  // (equippedAttackBonus(), equippedDefenseBonus(), effectiveMaxHp()), so
  // there is nothing to clear, leak, or clamp on FloorChanged.
  uint8_t activeSponsorId = 0;
};

struct Monster {
  int16_t x = 0;
  int16_t y = 0;
  uint8_t type = 0;
  uint16_t hp = 0;
  uint8_t state = static_cast<uint8_t>(MonsterState::Asleep);
};

struct Item {
  int16_t x = -1;
  int16_t y = -1;
  uint8_t type = 0;
  uint8_t subtype = 0;
  uint8_t count = 1;
  uint8_t enchantment = 0;
  uint8_t flags = 0;
};

// --- Definition Tables (const, stored in flash) ---

struct MonsterDef {
  const char* name;
  char glyph;
  uint8_t minDepth;
  uint16_t baseHp;
  uint8_t attack;
  uint8_t defense;
  uint16_t expValue;
};

// Dungeon Crawler Carl register — System-dungeon bestiary, not fantasy pastiche.
// Glyphs and stat blocks are unchanged from the prior table (Phase 9 work item 1:
// string swap only, not a balance pass).
inline constexpr MonsterDef MONSTER_DEFS[] = {
    {"Giant Rat", 'r', 1, 4, 2, 0, 5},
    {"Bat", 'b', 1, 3, 1, 0, 3},
    {"Sewer Ratling", 'k', 1, 6, 3, 1, 8},
    {"Grid Bug", 'x', 1, 2, 1, 0, 2},
    {"Mutant Scav", 'g', 2, 8, 4, 2, 12},
    {"Brood Grunt", 'o', 3, 12, 6, 3, 20},
    {"Hound-Beast", 'w', 3, 10, 7, 2, 18},
    {"Large Spider", 'S', 4, 14, 5, 2, 22},
    {"Skeleton", 's', 4, 10, 5, 4, 15},
    {"Broodkin Berserker", 'U', 5, 18, 8, 5, 30},
    {"Cave Brute", 'T', 6, 30, 10, 6, 50},
    {"Husk Wraith", 'W', 7, 20, 9, 5, 40},
    {"Shade", 'G', 8, 16, 11, 3, 45},
    {"Siege Beast", 'O', 9, 50, 12, 8, 80},
    {"Ogre Warlord", 'P', 10, 40, 14, 9, 70},
    {"Fire Drake", 'd', 12, 35, 13, 7, 90},
    {"Reaper Unit", 'N', 15, 60, 18, 12, 150},
    {"Young Dragon", 'D', 18, 80, 20, 14, 200},
    {"Inferno Titan", 'B', 22, 120, 25, 16, 500},
    {"Ancient Dragon", 'D', 25, 150, 30, 20, 1000},
    // Boss — The Adjudicator, the System's floor-26 arbiter, always on the deepest level
    {"The Adjudicator", 'p', 26, 250, 35, 22, 5000},
};
inline constexpr int MONSTER_DEF_COUNT = sizeof(MONSTER_DEFS) / sizeof(MONSTER_DEFS[0]);
inline constexpr int BOSS_MONSTER_TYPE = MONSTER_DEF_COUNT - 1;  // Index of The Adjudicator

struct ItemDef {
  const char* name;
  char glyph;
  uint8_t type;    // ItemType
  uint8_t subtype;
  uint16_t value;  // Base gold value
  int8_t attack;   // Bonus if weapon/armor
  int8_t defense;  // Bonus if armor/shield
  bool throwable;  // Can be thrown at a monster from the inventory screen
};

inline constexpr ItemDef ITEM_DEFS[] = {
    // Weapons
    {"Dagger", '/', static_cast<uint8_t>(ItemType::Weapon), 0, 5, 2, 0, true},
    {"Short Sword", '/', static_cast<uint8_t>(ItemType::Weapon), 1, 15, 4, 0, true},
    {"Long Sword", '/', static_cast<uint8_t>(ItemType::Weapon), 2, 30, 6, 0, true},
    {"Battle Axe", '/', static_cast<uint8_t>(ItemType::Weapon), 3, 50, 8, 0, true},
    {"Nanoweave Blade", '/', static_cast<uint8_t>(ItemType::Weapon), 4, 200, 12, 0, true},
    // Armor
    {"Leather Armor", '[', static_cast<uint8_t>(ItemType::Armor), 0, 10, 0, 2, false},
    {"Chain Mail", '[', static_cast<uint8_t>(ItemType::Armor), 1, 30, 0, 4, false},
    {"Plate Mail", '[', static_cast<uint8_t>(ItemType::Armor), 2, 60, 0, 6, false},
    {"Nanoweave Coat", '[', static_cast<uint8_t>(ItemType::Armor), 3, 300, 0, 10, false},
    // Shields
    {"Wooden Shield", ')', static_cast<uint8_t>(ItemType::Shield), 0, 8, 0, 1, false},
    {"Iron Shield", ')', static_cast<uint8_t>(ItemType::Shield), 1, 25, 0, 3, false},
    // Potions
    {"Potion of Healing", '!', static_cast<uint8_t>(ItemType::Potion), 0, 20, 0, 0, true},
    {"Potion of Mana", '!', static_cast<uint8_t>(ItemType::Potion), 1, 25, 0, 0, true},
    {"Potion of Strength", '!', static_cast<uint8_t>(ItemType::Potion), 2, 50, 0, 0, true},
    // Scrolls
    {"Scroll of Identify", '?', static_cast<uint8_t>(ItemType::Scroll), 0, 15, 0, 0, false},
    {"Scroll of Teleport", '?', static_cast<uint8_t>(ItemType::Scroll), 1, 30, 0, 0, false},
    {"Scroll of Mapping", '?', static_cast<uint8_t>(ItemType::Scroll), 2, 40, 0, 0, false},
    // Food
    {"Rations", '%', static_cast<uint8_t>(ItemType::Food), 0, 5, 0, 0, false},
    {"Nutrient Bar", '%', static_cast<uint8_t>(ItemType::Food), 1, 30, 0, 0, false},
    // Gold
    {"Gold Coins", '$', static_cast<uint8_t>(ItemType::Gold), 0, 1, 0, 0, false},
    // Sponsor Crate (Phase 11 loot boxes) -- spawns via the same unmodified random floor-item
    // roll as everything above it; opening one (GameMenuActivity::useInventoryItem) rolls its
    // prize from this same table, uniformly (no depth bias -- that's the joke, see parent's
    // ruling), excluding quest items and itself. Inserted here (after Gold, before Master Key)
    // so it stays outside RING_OF_POWER_DEF/MASTER_KEY_DEF's roll exclusion without disturbing
    // RATIONS_DEF's hardcoded index above.
    {"Sponsor Crate", '&', static_cast<uint8_t>(ItemType::LootBox), 0, 0, 0, 0, false},
    // Quest item -- dropped by The Adjudicator on floor 26 (Phase 11 work item 4)
    {"Master Key", '"', static_cast<uint8_t>(ItemType::Amulet), 0, 500, 0, 0, false},
    // Quest item -- dropped by The Adjudicator
    {"Ring of Power", '=', static_cast<uint8_t>(ItemType::Ring), 0, 999, 0, 0, false},
};
inline constexpr int ITEM_DEF_COUNT = sizeof(ITEM_DEFS) / sizeof(ITEM_DEFS[0]);
inline constexpr int RING_OF_POWER_DEF = ITEM_DEF_COUNT - 1;  // Index of Ring of Power
inline constexpr int MASTER_KEY_DEF = RING_OF_POWER_DEF - 1;  // Index of Master Key
// Index of Rations -- forced onto every floor by placeItems() (DungeonGenerator.cpp)
// outside the random roll, guaranteeing hunger is always escapable. See the
// static_assert next to placeItems() that keeps this index pinned to a Food entry.
inline constexpr int RATIONS_DEF = 17;
// Index of Sponsor Crate -- excluded from its own reward roll in
// GameMenuActivity::useInventoryItem() so opening one can't hand back another
// unopened crate. See the static_assert below pinning this to a LootBox entry.
inline constexpr int LOOT_BOX_DEF = 20;
static_assert(static_cast<ItemType>(ITEM_DEFS[LOOT_BOX_DEF].type) == ItemType::LootBox,
              "LOOT_BOX_DEF must point at the Sponsor Crate entry -- if ITEM_DEFS is ever "
              "reordered, update this index or the reward roll's self-exclusion breaks");

// --- Sponsors (Phase 11) ---
// Per-floor, personal cosmetic-with-teeth modifier. Rolled fresh every floor
// load (GameActivity::loadOrGenerateLevel(), true RNG stream via
// GAME_STATE.rollRange -- not derived from the level seed, so revisiting a
// floor can roll a different sponsor). Stored as an id on Player
// (activeSponsorId above) and applied ONLY at the point a stat is used
// (equippedAttackBonus(), equippedDefenseBonus(), effectiveMaxHp()) -- base
// stats are never mutated, so there is nothing to clear on FloorChanged, no
// drift risk across save/load, and no leak between floors.

enum class SponsorStat : uint8_t { None, Attack, Defense, MaxHp, GoldPercent };

struct SponsorDef {
  const char* name;
  SponsorStat stat;
  int8_t amount;  // May be negative (e.g. a defense-reducing sponsor)
};

// Index 0 is the "no sponsor" sentinel (SPONSOR_NONE). All strings ASCII-only
// -- the builtin bitmap font is not guaranteed to have glyphs above 0x7F, so
// no trademark symbol, no em dash, no smart quotes.
inline constexpr SponsorDef SPONSOR_DEFS[] = {
    {"No Sponsor", SponsorStat::None, 0},
    {"Big Hollow Insurance", SponsorStat::MaxHp, 2},
    {"Vantage Extraction Partners", SponsorStat::GoldPercent, 10},
    {"Quiet Room Wellness", SponsorStat::Attack, 1},
    {"Loyalty Plus (Terms Apply)", SponsorStat::Defense, 1},
    {"The Adjudicator's Legal Team", SponsorStat::Defense, -1},
    {"System Uptime Guarantee (tm)", SponsorStat::GoldPercent, -5},
};
inline constexpr int SPONSOR_DEF_COUNT = sizeof(SPONSOR_DEFS) / sizeof(SPONSOR_DEFS[0]);
inline constexpr uint8_t SPONSOR_NONE = 0;

inline int sponsorAttackModifier(uint8_t sponsorId) {
  if (sponsorId >= SPONSOR_DEF_COUNT) return 0;
  const SponsorDef& s = SPONSOR_DEFS[sponsorId];
  return s.stat == SponsorStat::Attack ? s.amount : 0;
}

inline int sponsorDefenseModifier(uint8_t sponsorId) {
  if (sponsorId >= SPONSOR_DEF_COUNT) return 0;
  const SponsorDef& s = SPONSOR_DEFS[sponsorId];
  return s.stat == SponsorStat::Defense ? s.amount : 0;
}

// Percent modifier applied to gold awards (loot box wins, floor pickups),
// read at the point gold is credited -- never mutates Player::gold itself.
inline int sponsorGoldPercentModifier(uint8_t sponsorId) {
  if (sponsorId >= SPONSOR_DEF_COUNT) return 0;
  const SponsorDef& s = SPONSOR_DEFS[sponsorId];
  return s.stat == SponsorStat::GoldPercent ? s.amount : 0;
}

// maxHp as modified by the active sponsor, read at the point of use (regen
// tick, damage-band selection, damage-event reporting, healing/food clamps)
// instead of ever writing back into Player::maxHp. Clamped to a minimum of 1
// so a hypothetical negative-maxHp sponsor could never produce a 0/negative
// cap.
inline uint16_t effectiveMaxHp(const Player& p) {
  if (p.activeSponsorId >= SPONSOR_DEF_COUNT) return p.maxHp;
  const SponsorDef& s = SPONSOR_DEFS[p.activeSponsorId];
  if (s.stat != SponsorStat::MaxHp) return p.maxHp;
  int v = static_cast<int>(p.maxHp) + s.amount;
  return v < 1 ? 1 : static_cast<uint16_t>(v);
}

// --- Themed floors (Phase 11) ---
// Decorate, never gate: a theme may change the floor's name/flavor text and
// nudge monster selection within the SAME eligible range placeMonsters()
// already computes. It never changes reachability, item availability, or the
// food guarantee (RATIONS_DEF is still forced onto every floor unconditionally
// -- see DungeonGenerator.cpp).

struct ThemeDef {
  const char* name;
  int8_t monsterBias;  // Added to placeMonsters()'s bestIdx roll, then
                        // clamped into [0, eligibleCount-1] -- never changes
                        // eligibleCount/count/which monsters are eligible.
};

// All strings ASCII-only, plain hyphens only (no em dash). No "no theme"
// sentinel -- every floor gets one of the six named themes.
inline constexpr ThemeDef THEME_DEFS[] = {
    {"The Break Room", -1},
    {"Compliance Corridor", 0},
    {"The Quarterly Pit", 1},
    {"Server Room (Cold Aisle)", 0},
    {"The Onboarding Wing", 0},
    {"The Executive Suite", 1},
};
inline constexpr int THEME_DEF_COUNT = sizeof(THEME_DEFS) / sizeof(THEME_DEFS[0]);
// themeForDepth() is defined further below, after Rng and levelSeed().

// --- Glyph lookup helpers ---

inline char tileGlyph(Tile tile) {
  switch (tile) {
    case Tile::Wall: return '#';
    case Tile::Floor: return '.';
    case Tile::DoorClosed: return '+';
    case Tile::DoorOpen: return '\'';
    case Tile::StairsUp: return '<';
    case Tile::StairsDown: return '>';
    case Tile::Rubble: return ':';
    case Tile::Water: return '~';
    default: return '?';
  }
}

inline char itemGlyph(uint8_t type) {
  switch (static_cast<ItemType>(type)) {
    case ItemType::Weapon: return '/';
    case ItemType::Armor: return '[';
    case ItemType::Shield: return ')';
    case ItemType::Potion: return '!';
    case ItemType::Scroll: return '?';
    case ItemType::Food: return '%';
    case ItemType::Gold: return '$';
    case ItemType::Ring: return '=';
    case ItemType::Amulet: return '"';
    default: return '*';
  }
}

// --- Fog of War helpers ---

inline bool fogIsExplored(const uint8_t* fog, int x, int y) {
  int idx = y * MAP_WIDTH + x;
  return (fog[idx / 8] >> (idx % 8)) & 1;
}

inline void fogSetExplored(uint8_t* fog, int x, int y) {
  int idx = y * MAP_WIDTH + x;
  fog[idx / 8] |= (1 << (idx % 8));
}

// --- XorShift32 RNG (deterministic, 4 bytes state) ---

struct Rng {
  uint32_t state;

  explicit Rng(uint32_t seed) : state(seed ? seed : 1) {}

  uint32_t next() {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
  }

  // Returns value in [0, max)
  uint32_t nextRange(uint32_t max) { return next() % max; }

  // Returns value in [min, max]
  int nextRangeInclusive(int min, int max) { return min + static_cast<int>(next() % (max - min + 1)); }
};

// --- Level-up XP thresholds ---

// XP required for each character level (index = level, value = cumulative XP needed)
inline uint32_t xpForLevel(uint16_t level) {
  if (level <= 1) return 0;
  // Roughly: 20, 60, 140, 300, 600, 1100, 1900, 3200, 5200, 8200...
  uint32_t xp = 0;
  for (uint16_t i = 2; i <= level; i++) {
    xp += 10u * i * i;
  }
  return xp;
}

// --- Level seed derivation ---

inline uint32_t levelSeed(uint32_t gameSeed, uint8_t depth) {
  return gameSeed ^ (depth * 2654435761u);
}

// Deterministic per-depth theme pick, independent of the dungeon generator's
// own Rng(levelSeed(gameSeed, depth)) stream (XORed with a distinct constant)
// so revisiting a floor always shows the same theme without correlating with
// -- or perturbing -- room/monster/item generation.
inline uint8_t themeForDepth(uint32_t gameSeed, uint8_t depth) {
  Rng rng(levelSeed(gameSeed, depth) ^ 0x5AC38A2Du);
  return static_cast<uint8_t>(rng.nextRange(THEME_DEF_COUNT));
}

}  // namespace game
