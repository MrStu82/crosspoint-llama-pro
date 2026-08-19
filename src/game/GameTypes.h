#pragma once

#include <cstdint>
#include <cstdio>

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
// Headroom for stacked run-scoped achievement rewards (Buff/Skill, see the
// "Buffs and Skills" section below) -- sized well above what the first bucket
// actually grants so later buckets don't need a Player layout change.
constexpr int MAX_ACTIVE_BUFFS = 24;
constexpr int MAX_ACTIVE_SKILLS = 12;

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
  // Buff/Skill rewards (achievement reward work, save v5). Same point-of-use
  // pattern as Sponsors above: these are index lists into BUFF_DEFS/SKILL_DEFS
  // (below), never a mutation of maxHp/strength/etc. Lifetime-persistent
  // (survive across runs, unlike the per-floor sponsor) since they represent
  // a permanent account-level reward, not a roll. Append-only, deduped by
  // grantBuff()/grantSkill() -- an already-held reward is never granted twice.
  uint8_t activeBuffIds[MAX_ACTIVE_BUFFS] = {};
  uint8_t activeBuffCount = 0;
  uint8_t activeSkillIds[MAX_ACTIVE_SKILLS] = {};
  uint8_t activeSkillCount = 0;
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

// Loot addressed to the player's pet rather than the player. Items here are deliberately
// never written into a level's Item[] array -- GameRenderer's drawViewportCell() and every
// pickup path only ever iterate that array, so a pet-stream item is invisible on the map and
// unpickable by construction, not by an extra guard check. Every entry keeps x=-1,y=-1
// (never positioned -- the pet "forages" it directly, see Job Phase 3). Cleared on every
// floor load, same lifetime as levelItems.
struct PetLootStream {
  Item items[MAX_ITEMS_PER_LEVEL];
  uint8_t itemCount = 0;
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

// Sponsor Crate reward selection (Phase 11 loot boxes). Uniform draw over
// ITEM_DEFS, excluding Ring of Power/Master Key (last two entries, positional
// quest-item placement only) and the crate itself (LOOT_BOX_DEF) so opening
// one can never hand back another unopened crate. rollFn takes max (exclusive)
// and returns a value in [0, max) -- the caller supplies the real combat RNG
// (GAME_STATE.rollRange) or a deterministic stand-in for host-harness testing.
// Free of GameActivity/GameState so it can be linked and exercised outside the
// firmware build. Behaviour identical to the inline loop it replaces; no new
// state.
inline uint8_t selectLootBoxReward(uint32_t (*rollFn)(uint32_t)) {
  uint8_t eligible[ITEM_DEF_COUNT];
  uint8_t eligibleCount = 0;
  for (uint8_t d = 0; d < ITEM_DEF_COUNT - 2; d++) {
    if (d == LOOT_BOX_DEF) continue;
    eligible[eligibleCount++] = d;
  }
  return eligible[rollFn(eligibleCount)];
}

// --- Pet Companion (Job Phase 3) ---
// One companion, rolled once at character creation and carried for the run.
// Species and name are cosmetic-only (no stat differences between species by
// design -- the pool exists for flavor/identity, not build variety); attack/
// defense/hp bases ARE rolled per-instance (small random spread) so no two
// pets of the same species are stat-identical either. Same point-of-use
// pattern as buffs/sponsors above: hpBase/attackBase/defenseBase are stored,
// but the level-scaled totals a caller actually needs (petMaxHp/petAttack/
// petDefense in Pet.h) are computed at read time, never folded back into
// these stored bases.
struct PetSpeciesDef {
  const char* name;
  char glyph;
};

inline constexpr PetSpeciesDef PET_SPECIES_DEFS[] = {
    {"Rat Terrier", 'd'}, {"Cave Lizard", 'l'}, {"Baby Griffin", 'g'},
    {"Shadow Sprite", 's'}, {"Iron Beetle", 'i'}, {"Sewer Pup", 'p'},
    {"Scrap Drone", 'r'}, {"Grid Sprite", 'x'},
};
inline constexpr int PET_SPECIES_COUNT = sizeof(PET_SPECIES_DEFS) / sizeof(PET_SPECIES_DEFS[0]);

inline constexpr const char* PET_NAME_POOL[] = {
    "Bolt", "Ash", "Pip", "Rusty", "Ember", "Sable", "Nova", "Fizz",
    "Grit", "Pixel", "Cinder", "Dusk", "Wisp", "Copper", "Sprocket", "Marble",
};
inline constexpr int PET_NAME_COUNT = sizeof(PET_NAME_POOL) / sizeof(PET_NAME_POOL[0]);

struct Pet {
  bool active = false;
  uint8_t speciesId = 0;
  char name[16] = {};
  uint8_t hpBase = 0;
  uint8_t attackBase = 0;
  uint8_t defenseBase = 0;
  Item gear;
  bool hasGear = false;
};

// --- Buffs and Skills (achievement reward work) ---
// Run-scoped, stacking achievement rewards. Same point-of-use pattern as
// Sponsors below (never mutate base stats directly -- sum modifiers where a
// stat is actually consumed) but additive/stacking within a run rather than
// a single active slot. Earned and spent within one run only -- wiped by
// GameState::newGame() (player = game::Player{}) same as everything else on
// Player, and re-earnable next run if drawn and achieved again. What
// persists across runs is the lifetime AchievementBus::unlocked[] record
// (progress screen), never these numbers -- see AchievementBus::unlock().
// Skills are the same mechanism at bigger magnitude, gated to
// deeper/harder achievements -- there is no separate active-use/trigger
// subsystem; "Skill" here means "big passive", not "ability you invoke".

enum class BuffStat : uint8_t { None, Attack, Defense, MaxHp, GoldPercent };

struct BuffDef {
  const char* name;
  BuffStat stat;
  int8_t amount;
};

inline constexpr BuffDef BUFF_DEFS[] = {
    {"None", BuffStat::None, 0},
    {"Steady Grip", BuffStat::Attack, 1},
    {"Worn Leather Wraps", BuffStat::Defense, 1},
    {"Deep Breather", BuffStat::MaxHp, 2},
    {"Prospector's Eye", BuffStat::GoldPercent, 5},
    {"Sharpened Edge", BuffStat::Attack, 2},
    {"Reinforced Plating", BuffStat::Defense, 2},
    {"Hardy Constitution", BuffStat::MaxHp, 4},
    {"Silver Tongue", BuffStat::GoldPercent, 10},
    {"Killer Instinct", BuffStat::Attack, 3},
    {"Iron Skin", BuffStat::Defense, 3},
    {"Vital Surge", BuffStat::MaxHp, 6},
    {"Treasure Sense", BuffStat::GoldPercent, 15},
    {"Practiced Strike", BuffStat::Attack, 4},
    {"Bulwark Stance", BuffStat::Defense, 4},
};
inline constexpr int BUFF_DEF_COUNT = sizeof(BUFF_DEFS) / sizeof(BUFF_DEFS[0]);
inline constexpr uint8_t BUFF_NONE = 0;

inline constexpr BuffDef SKILL_DEFS[] = {
    {"None", BuffStat::None, 0},
    {"Adrenaline Mastery", BuffStat::Attack, 6},
    {"Veteran's Guard", BuffStat::Defense, 6},
    {"Deep Lungs", BuffStat::MaxHp, 8},
    {"Gilded Instinct", BuffStat::GoldPercent, 25},
    {"Executioner's Form", BuffStat::Attack, 8},
    {"Fortress Body", BuffStat::Defense, 8},
    {"Bottomless Reserve", BuffStat::MaxHp, 10},
};
inline constexpr int SKILL_DEF_COUNT = sizeof(SKILL_DEFS) / sizeof(SKILL_DEFS[0]);
inline constexpr uint8_t SKILL_NONE = 0;

inline void grantBuff(Player& p, uint8_t buffId) {
  if (buffId == BUFF_NONE || buffId >= BUFF_DEF_COUNT) return;
  for (uint8_t i = 0; i < p.activeBuffCount; i++) {
    if (p.activeBuffIds[i] == buffId) return;
  }
  if (p.activeBuffCount >= MAX_ACTIVE_BUFFS) return;
  p.activeBuffIds[p.activeBuffCount++] = buffId;
}

inline void grantSkill(Player& p, uint8_t skillId) {
  if (skillId == SKILL_NONE || skillId >= SKILL_DEF_COUNT) return;
  for (uint8_t i = 0; i < p.activeSkillCount; i++) {
    if (p.activeSkillIds[i] == skillId) return;
  }
  if (p.activeSkillCount >= MAX_ACTIVE_SKILLS) return;
  p.activeSkillIds[p.activeSkillCount++] = skillId;
}

inline int buffAttackModifier(const Player& p) {
  int total = 0;
  for (uint8_t i = 0; i < p.activeBuffCount; i++) {
    const BuffDef& b = BUFF_DEFS[p.activeBuffIds[i]];
    if (b.stat == BuffStat::Attack) total += b.amount;
  }
  for (uint8_t i = 0; i < p.activeSkillCount; i++) {
    const BuffDef& s = SKILL_DEFS[p.activeSkillIds[i]];
    if (s.stat == BuffStat::Attack) total += s.amount;
  }
  return total;
}

inline int buffDefenseModifier(const Player& p) {
  int total = 0;
  for (uint8_t i = 0; i < p.activeBuffCount; i++) {
    const BuffDef& b = BUFF_DEFS[p.activeBuffIds[i]];
    if (b.stat == BuffStat::Defense) total += b.amount;
  }
  for (uint8_t i = 0; i < p.activeSkillCount; i++) {
    const BuffDef& s = SKILL_DEFS[p.activeSkillIds[i]];
    if (s.stat == BuffStat::Defense) total += s.amount;
  }
  return total;
}

inline int buffMaxHpModifier(const Player& p) {
  int total = 0;
  for (uint8_t i = 0; i < p.activeBuffCount; i++) {
    const BuffDef& b = BUFF_DEFS[p.activeBuffIds[i]];
    if (b.stat == BuffStat::MaxHp) total += b.amount;
  }
  for (uint8_t i = 0; i < p.activeSkillCount; i++) {
    const BuffDef& s = SKILL_DEFS[p.activeSkillIds[i]];
    if (s.stat == BuffStat::MaxHp) total += s.amount;
  }
  return total;
}

inline int buffGoldPercentModifier(const Player& p) {
  int total = 0;
  for (uint8_t i = 0; i < p.activeBuffCount; i++) {
    const BuffDef& b = BUFF_DEFS[p.activeBuffIds[i]];
    if (b.stat == BuffStat::GoldPercent) total += b.amount;
  }
  for (uint8_t i = 0; i < p.activeSkillCount; i++) {
    const BuffDef& s = SKILL_DEFS[p.activeSkillIds[i]];
    if (s.stat == BuffStat::GoldPercent) total += s.amount;
  }
  return total;
}

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
  int v = static_cast<int>(p.maxHp);
  if (p.activeSponsorId < SPONSOR_DEF_COUNT) {
    const SponsorDef& s = SPONSOR_DEFS[p.activeSponsorId];
    if (s.stat == SponsorStat::MaxHp) v += s.amount;
  }
  v += buffMaxHpModifier(p);
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

// --- Shared loot table (floor placement + corpse drops) ---

// A random item meeting or exceeding this base value is eligible for the rare tail below.
// Currently matches Nanoweave Blade (200) and Nanoweave Coat (300) -- the two top-tier
// gear pieces -- deliberately excludes Sponsor Crate (value 0) and quest items (never
// rolled here at all, see the exclusion below).
inline constexpr uint16_t RARE_TAIL_VALUE_THRESHOLD = 150;
// 1-in-20 chance per roll of drawing from the rare pool instead of the full table.
inline constexpr uint32_t RARE_TAIL_CHANCE_DENOM = 20;

// Rolls a single random item from the shared depth-scaled loot table: DungeonGenerator's
// per-floor item placement and GameActivity's per-corpse drops both call this, so the two
// never drift into separate tables. Always excludes the two quest items (Ring of Power,
// Master Key -- only ever placed as the boss's death drop, see GameActivity.cpp). Returns
// the item at (x=-1, y=-1); the caller positions or queues it.
inline Item rollLootItem(uint8_t depth, Rng& rng) {
  uint8_t defIdx;
  if (rng.nextRange(RARE_TAIL_CHANCE_DENOM) == 0) {
    uint8_t rarePool[ITEM_DEF_COUNT];
    uint8_t rareCount = 0;
    for (uint8_t i = 0; i < ITEM_DEF_COUNT - 2; i++) {
      if (ITEM_DEFS[i].value >= RARE_TAIL_VALUE_THRESHOLD) {
        rarePool[rareCount++] = i;
      }
    }
    defIdx = rareCount > 0 ? rarePool[rng.nextRange(rareCount)]
                           : static_cast<uint8_t>(rng.nextRange(ITEM_DEF_COUNT - 2));
  } else {
    defIdx = static_cast<uint8_t>(rng.nextRange(ITEM_DEF_COUNT - 2));
  }
  const auto& def = ITEM_DEFS[defIdx];

  Item item{};
  item.type = def.type;
  item.subtype = def.subtype;
  item.count = (def.type == static_cast<uint8_t>(ItemType::Gold))
                   ? static_cast<uint8_t>(rng.nextRangeInclusive(1, 10 + depth * 5))
                   : 1;
  item.enchantment = 0;
  if ((def.type == static_cast<uint8_t>(ItemType::Weapon) || def.type == static_cast<uint8_t>(ItemType::Armor)) &&
      rng.nextRange(4) == 0) {
    item.enchantment = static_cast<uint8_t>(rng.nextRangeInclusive(1, 3));
  }
  item.flags = 0;
  item.x = -1;
  item.y = -1;
  return item;
}

// --- Achievement definitions ---

// Buff/Skill rewardValue indexes into BUFF_DEFS/SKILL_DEFS respectively (the
// same array-index convention Title/SponsorUnlock already use against
// TITLE_STRINGS/SPONSOR_DEFS).
enum class AchievementReward : uint8_t { None, Title, SponsorUnlock, LoreUnlock, Buff, Skill };

struct AchievementDef {
  const char* name;
  const char* description;
  AchievementReward reward;
  uint8_t rewardValue;
};

// Titles unlocked via AchievementReward::Title, indexed by AchievementDef::rewardValue.
inline constexpr const char* TITLE_STRINGS[] = {
    "the Unproven",       // 0
    "Ratcatcher",         // 1
    "Delver",             // 2
    "Deep Delver",        // 3
    "Abyss-Walker",       // 4
    "the Unkilled",       // 5
    "the Thorough",       // 6
    "the Wealthy",        // 7
    "the Frugal",         // 8
    "Blademaster",        // 9
    "the Reckless",       // 10
    "the Cautious",       // 11
    "Beastbane",          // 12
    "the Curious",        // 13
    "the Lucky",          // 14
    "Archivist",          // 15
    "the Patient",        // 16
    "Giant-Killer",       // 17
    "the Untouchable",    // 18
    "Dungeon Sovereign",  // 19
    "the Prosperous",     // 20
    "the Opportunist",    // 21
    "the Uncanny",        // 22
    "the Abstinent",      // 23
    "the Unfathomable",   // 24
    "Beastfriend",        // 25
    "the Bonded",         // 26
};

inline constexpr AchievementDef ACHIEVEMENT_DEFS[] = {
    {"Ding!", "Leveled up. Groundbreaking.", AchievementReward::None, 0},
    {"That'll Buff Out", "Survived a floor at critical health.", AchievementReward::None, 0},
    {"Audience Participation", "Died to something weak.", AchievementReward::None, 0},
    {"Escalation of Force", "Overkilled a monster.", AchievementReward::None, 0},
    {"Pack Rat", "Filled the inventory.", AchievementReward::None, 0},
    {"Speed Runner", "Reached floor 5 in under 150 turns.", AchievementReward::None, 0},
    {"Deep Diver", "Reached floor 10.", AchievementReward::None, 0},
    {"Maxed Out", "Reached character level 20.", AchievementReward::None, 0},
    {"Percussive Maintenance", "Killed a monster with a thrown item.", AchievementReward::None, 0},
    {"Sponsored Content", "Opened a loot box.", AchievementReward::None, 0},
    // -- Depth --
    {"First Steps", "Entered the dungeon at all. The bar was on the floor.", AchievementReward::None, 0},
    {"Down We Go", "Reached dungeon level 2.", AchievementReward::None, 0},
    {"Getting Comfortable", "Reached dungeon level 5.", AchievementReward::Title, 2},
    {"Deep Delver", "Reached dungeon level 15.", AchievementReward::Title, 3},
    {"Pressure Tolerance", "Reached dungeon level 20.", AchievementReward::LoreUnlock, 0},
    {"Abyss-Walker", "Reached dungeon level 25.", AchievementReward::Title, 4},
    {"Structurally Unsound", "Reached dungeon level 22.", AchievementReward::SponsorUnlock, 1},
    {"Express Descent", "Descended three levels in under 200 turns.", AchievementReward::None, 0},
    {"Dungeon Sovereign", "Reached the deepest level the dungeon has.", AchievementReward::Title, 19},
    // -- Combat --
    {"First Blood", "Killed something. It started it.", AchievementReward::None, 0},
    {"Ratcatcher", "Killed 10 monsters.", AchievementReward::Title, 1},
    {"Exterminator", "Killed 50 monsters.", AchievementReward::None, 0},
    {"Beastbane", "Killed 250 monsters.", AchievementReward::Title, 12},
    {"Overkill", "Dealt more damage in one blow than the target had left.", AchievementReward::None, 0},
    {"Giant-Killer", "Killed something with more life in it than you.", AchievementReward::Title, 17},
    {"Clean Sweep", "Cleared an entire floor of monsters.", AchievementReward::None, 0},
    {"Untouched", "Cleared a floor without taking a single point of damage.", AchievementReward::Title, 18},
    {"Pacifist Run", "Descended a full floor without killing anything.", AchievementReward::Title, 11},
    // -- Survival --
    {"One Hit Point", "Survived a turn at exactly 1 HP.", AchievementReward::None, 0},
    {"The Unkilled", "Reached character level 10 without dying.", AchievementReward::Title, 5},
    {"Veteran", "Reached character level 5.", AchievementReward::None, 0},
    {"Seasoned", "Reached character level 10.", AchievementReward::None, 0},
    {"Long Haul", "Survived 1000 turns in a single run.", AchievementReward::Title, 16},
    {"Attrition", "Survived 5000 turns in a single run.", AchievementReward::None, 0},
    {"Back From The Brink", "Healed from below 10% to full in one run.", AchievementReward::None, 0},
    {"Died Anyway", "Died. The System is not surprised.", AchievementReward::None, 0},
    // -- Exploration --
    {"Cartographer", "Fully explored a floor.", AchievementReward::Title, 6},
    {"Thorough", "Fully explored five floors.", AchievementReward::None, 0},
    {"Obsessive", "Fully explored twenty floors.", AchievementReward::LoreUnlock, 0},
    {"Shortcut", "Found the stairs within 30 turns of arriving.", AchievementReward::None, 0},
    {"Scenic Route", "Took over 500 turns on a single floor.", AchievementReward::None, 0},
    {"Wanderer", "Walked 2000 tiles across all runs.", AchievementReward::None, 0},
    {"Pathfinder", "Walked 10000 tiles across all runs.", AchievementReward::None, 0},
    // -- Loot --
    {"Finders Keepers", "Picked up your first item.", AchievementReward::None, 0},
    {"Magpie", "Picked up 50 items.", AchievementReward::None, 0},
    {"The Wealthy", "Accumulated 1000 gold.", AchievementReward::Title, 7},
    {"Obscene Wealth", "Accumulated 10000 gold.", AchievementReward::SponsorUnlock, 2},
    // -- Secrets --
    {"Completionist", "Unlocked forty other achievements. This one was inevitable.", AchievementReward::LoreUnlock, 0},
    // -- Demo set (48-59): condition-table type proof, ids IronStomach..DeepAndDeadly --
    {"Iron Stomach", "Let hunger climb absurdly high and lived to complain about it.", AchievementReward::None, 0},
    {"Step Counter", "Walked 500 tiles in a single run.", AchievementReward::None, 0},
    {"One Shot", "Landed a single hit hard enough to erase most anything.", AchievementReward::None, 0},
    {"Nick of Time", "Survived a hit that left you at exactly 1 HP.", AchievementReward::None, 0},
    {"Serial Killer", "Killed 25 monsters in a single run.", AchievementReward::None, 0},
    {"Klepto", "Picked up 100 items in a single run. There's a hoarding problem here.", AchievementReward::None, 0},
    {"Ghost", "Finished a run without ever being hit.", AchievementReward::Title, 18},
    {"Untroubled", "Cleared a single floor without ever being hit.", AchievementReward::None, 0},
    {"Potion Chugger", "Drank 10 potions in a single run.", AchievementReward::None, 0},
    {"Scroll Hoarder", "Picked up 5 scrolls in a single run.", AchievementReward::None, 0},
    {"Greedy and Fast", "Walked far and killed fast in the same run.", AchievementReward::None, 0},
    {"Deep and Deadly", "Ate constantly and looted relentlessly in the same run.", AchievementReward::None, 0},
    // -- Depth bucket (60-89): every remaining single-floor threshold, plus
    // depth-gated compounds against the demo pool's normal (non-silly)
    // condition rows. Reward tier climbs with depth: shallow = None/small
    // Buff, deep singles = Skill, compounds mix Buff/Skill depending on how
    // hard the paired condition is to also satisfy at that depth. --
    {"Footing", "Reached dungeon level 3.", AchievementReward::None, 0},
    {"Four Floors Down", "Reached dungeon level 4.", AchievementReward::Buff, 1},
    {"Sixth Sense", "Reached dungeon level 6.", AchievementReward::None, 0},
    {"Lucky Seven", "Reached dungeon level 7.", AchievementReward::Buff, 2},
    {"Eight Below", "Reached dungeon level 8.", AchievementReward::Buff, 3},
    {"Ninth Circle", "Reached dungeon level 9.", AchievementReward::None, 0},
    {"Eleven and Counting", "Reached dungeon level 11.", AchievementReward::Buff, 4},
    {"Dozen Deep", "Reached dungeon level 12.", AchievementReward::Buff, 5},
    {"Unlucky Thirteen", "Reached dungeon level 13.", AchievementReward::None, 0},
    {"Fourteen Fathoms", "Reached dungeon level 14.", AchievementReward::Buff, 6},
    {"Sixteen Strides", "Reached dungeon level 16.", AchievementReward::Skill, 1},
    {"Seventeen Sunken", "Reached dungeon level 17.", AchievementReward::Skill, 2},
    {"Eighteen Echoes", "Reached dungeon level 18.", AchievementReward::Buff, 7},
    {"Nineteen Nadir", "Reached dungeon level 19.", AchievementReward::Skill, 3},
    {"Twenty-One Undertow", "Reached dungeon level 21.", AchievementReward::Skill, 4},
    {"Twenty-Three Threshold", "Reached dungeon level 23.", AchievementReward::Skill, 5},
    {"Twenty-Four Frontier", "Reached dungeon level 24.", AchievementReward::Skill, 6},
    {"Treader of Shallows", "Reached level 3 while covering serious ground on the way.", AchievementReward::None, 0},
    {"Butcher of Six", "Reached level 6 having already killed 25 things this run.", AchievementReward::Buff, 8},
    {"Unscathed Eighth", "Reached level 8 without taking a single hit all run.", AchievementReward::Buff, 9},
    {"Serene Ninth", "Reached level 9 without a scratch on the current floor.", AchievementReward::None, 0},
    {"Alchemist of Eleven", "Reached level 11 after chugging 10 potions along the way.", AchievementReward::Buff, 10},
    {"Scribe of Twelve", "Reached level 12 having hoarded 5 scrolls.", AchievementReward::Buff, 11},
    {"Hoarder of Thirteen", "Reached level 13 with 100 items picked up this run.", AchievementReward::None, 0},
    {"Wanderer of Fourteen", "Reached level 14 having walked 500 tiles this run.", AchievementReward::Buff, 12},
    {"Reaper of Sixteen", "Reached level 16 with 25 kills already banked this run.", AchievementReward::Skill, 7},
    {"Phantom of Seventeen", "Reached level 17 without ever being hit.", AchievementReward::Buff, 13},
    {"Unbroken Nineteen", "Reached level 19 unscathed on the current floor.", AchievementReward::Buff, 14},
    {"Brewmaster of Twenty-One", "Reached level 21 having chugged 10 potions along the way.", AchievementReward::Skill, 6},
    {"Loremaster of Twenty-Three", "Reached level 23 having hoarded 5 scrolls.", AchievementReward::Skill, 4},
    // -- Combat bucket (Milestone 2, ids 90-124) --
    {"Bloodletter", "Killed 5 monsters in a single run.", AchievementReward::Buff, 1},
    {"Cutthroat", "Killed 15 monsters in a single run.", AchievementReward::Buff, 2},
    {"Hunter's Tally", "Killed 35 monsters in a single run.", AchievementReward::Buff, 5},
    {"Body Count", "Killed 60 monsters in a single run.", AchievementReward::Buff, 9},
    {"Warpath", "Killed 90 monsters in a single run.", AchievementReward::Skill, 1},
    {"Slaughterhouse", "Killed 130 monsters in a single run.", AchievementReward::Skill, 5},
    {"Kill Streak", "Killed 180 monsters in a single run.", AchievementReward::Skill, 1},
    {"Merciless", "Killed 250 monsters in a single run.", AchievementReward::Skill, 5},
    {"Genocide Run", "Killed 350 monsters in a single run.", AchievementReward::Skill, 7},
    {"Apex Predator", "Killed 500 monsters in a single run.", AchievementReward::Title, 10},
    {"First Cut", "Landed a hit for 12 or more damage.", AchievementReward::None, 0},
    {"Heavy Hand", "Landed a hit for 20 or more damage.", AchievementReward::Buff, 3},
    {"Bone Breaker", "Landed a hit for 30 or more damage.", AchievementReward::Buff, 6},
    {"Crushing Blow", "Landed a hit for 45 or more damage.", AchievementReward::Buff, 10},
    {"Devastator", "Landed a hit for 65 or more damage.", AchievementReward::Skill, 2},
    {"Annihilating Strike", "Landed a hit for 90 or more damage.", AchievementReward::Skill, 6},
    {"World Ender", "Landed a hit for 120 or more damage.", AchievementReward::Skill, 3},
    {"Cataclysm", "Landed a hit for 160 or more damage.", AchievementReward::Skill, 4},
    {"Bruised Not Broken", "Survived 10 hits in a single run.", AchievementReward::None, 0},
    {"Punching Bag", "Survived 20 hits in a single run.", AchievementReward::Buff, 2},
    {"Glutton for Punishment", "Survived 35 hits in a single run.", AchievementReward::Buff, 7},
    {"Besieged", "Survived 50 hits in a single run.", AchievementReward::Skill, 2},
    {"Last One Standing", "Survived 75 hits in a single run.", AchievementReward::Skill, 7},
    {"Overmatched", "Killed a monster from 10 or more floors below your own depth.", AchievementReward::Buff, 9},
    {"Outgunned", "Killed a monster from 14 or more floors below your own depth.", AchievementReward::Buff, 13},
    {"In Over Your Head", "Killed a monster from 18 or more floors below your own depth.", AchievementReward::Skill, 5},
    {"David and Goliath", "Killed a monster from 22 or more floors below your own depth.", AchievementReward::Skill, 1},
    {"Quick Draw", "Threw 5 weapons in a single run.", AchievementReward::None, 0},
    {"Ranged Specialist", "Threw 15 weapons in a single run.", AchievementReward::Buff, 1},
    {"Master Marksman", "Threw 30 weapons in a single run.", AchievementReward::Skill, 1},
    {"Bloodless", "Died without ever landing a kill.", AchievementReward::Buff, 4},
    {"Pacifist to Six", "Reached depth 6 without landing a kill.", AchievementReward::Buff, 8},
    {"Pacifist to Eleven", "Reached depth 11 without landing a kill.", AchievementReward::Buff, 12},
    {"Pacifist to Sixteen", "Reached depth 16 without landing a kill.", AchievementReward::Skill, 4},
    {"Peaceful Sovereign", "Reached depth 21 without landing a kill.", AchievementReward::Title, 14},
    // -- Survival bucket (Milestone 2, ids 125-149) --
    {"Steady Pace", "Survived 100 turns in a single run.", AchievementReward::None, 0},
    {"Long Stretch", "Survived 250 turns in a single run.", AchievementReward::Buff, 2},
    {"Grinding It Out", "Survived 600 turns in a single run.", AchievementReward::Buff, 6},
    {"Deep Focus", "Survived 1500 turns in a single run.", AchievementReward::Skill, 3},
    {"Relentless", "Survived 2500 turns in a single run.", AchievementReward::Skill, 6},
    {"Timeless Delve", "Survived 4000 turns in a single run.", AchievementReward::Title, 16},
    {"Rising Star", "Reached character level 3.", AchievementReward::None, 0},
    {"Battle-Hardened", "Reached character level 7.", AchievementReward::Buff, 3},
    {"Combat Adept", "Reached character level 13.", AchievementReward::Buff, 9},
    {"War-Forged", "Reached character level 16.", AchievementReward::Skill, 2},
    {"Legendary Might", "Reached character level 19.", AchievementReward::Skill, 5},
    {"Living Legend", "Reached character level 24.", AchievementReward::Title, 9},
    {"Close Call", "Survived a hit that left you at 8 HP or less.", AchievementReward::None, 0},
    {"Razor's Edge", "Survived a hit that left you at 5 HP or less.", AchievementReward::Buff, 5},
    {"Scraping By", "Survived a hit that left you at 3 HP or less.", AchievementReward::Buff, 11},
    {"Whisper From The Brink", "Survived a hit that left you at 2 HP or less.", AchievementReward::Skill, 4},
    {"Tightening the Belt", "Let hunger drop to 25 or below and kept going.", AchievementReward::None, 0},
    {"Running on Empty", "Let hunger drop to 10 or below and kept going.", AchievementReward::Buff, 8},
    {"Starving Survivor", "Let hunger drop to 3 or below and kept going.", AchievementReward::Skill, 1},
    {"Iron Hide", "Survived a single hit for 15 or more damage.", AchievementReward::Buff, 4},
    {"Shrug It Off", "Survived a single hit for 25 or more damage.", AchievementReward::Buff, 12},
    {"Juggernaut's Endurance", "Survived a single hit for 35 or more damage.", AchievementReward::Skill, 7},
    {"Sip and See", "Drank 3 potions in a single run.", AchievementReward::None, 0},
    {"Steady Dosage", "Drank 6 potions in a single run.", AchievementReward::Buff, 7},
    {"Alchemical Overkill", "Drank 20 potions in a single run.", AchievementReward::Skill, 1},

    // -- Exploration bucket (Milestone 2, ids 150-174) --
    {"First Footprints", "Walked 100 tiles in a single run.", AchievementReward::None, 0},
    {"Worn Path", "Walked 250 tiles in a single run.", AchievementReward::Buff, 1},
    {"Beaten Track", "Walked 450 tiles in a single run.", AchievementReward::Buff, 5},
    {"Long Walk", "Walked 800 tiles in a single run.", AchievementReward::Buff, 9},
    {"Grand Tour", "Walked 1200 tiles in a single run.", AchievementReward::Skill, 2},
    {"Wayfarer", "Walked 1800 tiles in a single run.", AchievementReward::Skill, 5},
    {"Endless Trek", "Walked 2500 tiles in a single run.", AchievementReward::Skill, 7},
    {"Horizon Chaser", "Walked 3500 tiles in a single run.", AchievementReward::Title, 13},
    {"Second Sweep", "Fully explored 2 floors in a single run.", AchievementReward::None, 0},
    {"Triple Check", "Fully explored 3 floors in a single run.", AchievementReward::Buff, 2},
    {"Quad Cleared", "Fully explored 4 floors in a single run.", AchievementReward::Buff, 6},
    {"Eight-Floor Sweep", "Fully explored 8 floors in a single run.", AchievementReward::Buff, 10},
    {"Dozen Cleared", "Fully explored 12 floors in a single run.", AchievementReward::Skill, 3},
    {"Sixteen Swept", "Fully explored 16 floors in a single run.", AchievementReward::Skill, 4},
    {"Eighteen Exhausted", "Fully explored 18 floors in a single run.", AchievementReward::Skill, 6},
    {"Grounded at Four", "Reached depth 4 having already walked 100 tiles this run.", AchievementReward::Buff, 1},
    {"Mapped at Seven", "Reached depth 7 having fully explored 2 floors this run.", AchievementReward::Buff, 4},
    {"Well-Trodden at Nine", "Reached depth 9 having walked 450 tiles this run.", AchievementReward::Buff, 8},
    {"Charted at Twelve", "Reached depth 12 having fully explored 3 floors this run.", AchievementReward::Skill, 2},
    {"Long Road at Fourteen", "Reached depth 14 having walked 800 tiles this run.", AchievementReward::Buff, 11},
    {"Surveyed at Sixteen", "Reached depth 16 having fully explored 4 floors this run.", AchievementReward::Skill, 5},
    {"Grand Tourist at Eighteen", "Reached depth 18 having walked 1200 tiles this run.", AchievementReward::Skill, 1},
    {"Meticulous at Nineteen", "Reached depth 19 having fully explored 8 floors this run.", AchievementReward::Skill, 7},
    {"Wayfarer of Twenty-One", "Reached depth 21 having walked 1800 tiles this run.", AchievementReward::Title, 15},
    {"Cartographer's Peak at Twenty-Four", "Reached depth 24 having fully explored 12 floors this run.", AchievementReward::Title, 6},
    // -- Loot & Economy bucket (Milestone 3, ids 175-209): see
    // Achievements.h and AchievementConditions.h for the enum ids and
    // condition rows (114-148) backing these entries. --
    {"Copper Count", "Amassed 100 gold in a single run.", AchievementReward::None, 0},
    {"Coin Purse", "Amassed 500 gold in a single run.", AchievementReward::Buff, 1},
    {"Silver Lining", "Amassed 2,500 gold in a single run.", AchievementReward::Buff, 5},
    {"Deep Pockets", "Amassed 5,000 gold in a single run.", AchievementReward::Buff, 9},
    {"Vault Dweller", "Amassed 25,000 gold in a single run.", AchievementReward::Skill, 2},
    {"Coffers Overflowing", "Amassed 50,000 gold in a single run.", AchievementReward::Skill, 6},
    {"Beyond Counting", "Amassed 100,000 gold in a single run.", AchievementReward::Skill, 7},
    {"Ring Finder", "Picked up 3 rings in a single run.", AchievementReward::None, 0},
    {"Amulet Collector", "Picked up 3 amulets in a single run.", AchievementReward::Buff, 2},
    {"Suited Up", "Picked up 10 pieces of armor in a single run.", AchievementReward::Buff, 6},
    {"Shield Wall", "Picked up 8 shields in a single run.", AchievementReward::Buff, 10},
    {"Well Stocked", "Picked up 15 food items in a single run.", AchievementReward::Skill, 3},
    {"Armory", "Picked up 15 weapons in a single run.", AchievementReward::Skill, 7},
    {"Box Collector", "Picked up 5 loot boxes in a single run.", AchievementReward::Buff, 3},
    {"Well Read", "Used 5 scrolls in a single run.", AchievementReward::None, 0},
    {"Archive Diver", "Used 15 scrolls in a single run.", AchievementReward::Buff, 4},
    {"Snack Attack", "Ate 10 food items in a single run.", AchievementReward::Buff, 7},
    {"Bottomless Stomach", "Ate 25 food items in a single run.", AchievementReward::Skill, 4},
    {"Box Office Hit", "Opened 3 loot boxes in a single run.", AchievementReward::None, 0},
    {"Unwrapping Spree", "Opened 8 loot boxes in a single run.", AchievementReward::Buff, 8},
    {"Jackpot Streak", "Opened 15 loot boxes in a single run.", AchievementReward::Skill, 1},
    {"Sponsor's Favorite", "Opened 25 loot boxes in a single run.", AchievementReward::Skill, 5},
    {"Rich at Three", "Reached depth 3 having amassed 100 gold this run.", AchievementReward::Buff, 11},
    {"Flush at Eight", "Reached depth 8 having amassed 2,500 gold this run.", AchievementReward::Buff, 12},
    {"Gilded at Thirteen", "Reached depth 13 having amassed 5,000 gold this run.", AchievementReward::Skill, 5},
    {"Mogul at Eighteen", "Reached depth 18 having amassed 25,000 gold this run.", AchievementReward::Skill, 1},
    {"Tycoon at Twenty-Three", "Reached depth 23 having amassed 50,000 gold this run.", AchievementReward::Title, 20},
    {"Ascetic", "Went a full floor without picking up an item.", AchievementReward::Buff, 13},
    {"Self-Sufficient", "Went an entire run without picking up a single item.", AchievementReward::Title, 8},
    {"Minimalist Delver", "Reached depth 9 having gone a full floor without picking up an item.", AchievementReward::Buff, 14},
    {"Ring Hoarder", "Picked up 8 rings in a single run.", AchievementReward::Skill, 2},
    {"Amulet Vault", "Picked up 8 amulets in a single run.", AchievementReward::Skill, 3},
    {"Full Loadout", "Picked up 3 rings and 3 amulets in a single run.", AchievementReward::Buff, 1},
    {"Well Equipped", "Picked up 10 pieces of armor and 8 shields in a single run.", AchievementReward::Skill, 6},
    {"Grand Bazaar", "Picked up 5 loot boxes having amassed 5,000 gold this run.", AchievementReward::Title, 21},
    // -- Curiosities & Secrets bucket (Milestone 3, ids 210-234): see
    // Achievements.h and AchievementConditions.h for the enum ids and
    // condition rows (162-186) backing these entries. --
    {"Curious Specimen", "Killed a monster with unusually high vitality.", AchievementReward::Buff, 2},
    {"Oddly Robust", "Killed a monster that refused to die easily.", AchievementReward::Buff, 5},
    {"Freak of Nature", "Killed a monster built like a tank.", AchievementReward::Skill, 3},
    {"What Was That Thing", "Killed something that really shouldn't have had that much health.", AchievementReward::Title, 22},
    {"In Rude Health", "Ended a turn in surprisingly good shape.", AchievementReward::None, 0},
    {"Bursting with Vitality", "Ended a turn near full strength.", AchievementReward::Buff, 4},
    {"Unnaturally Hale", "Ended a turn at an almost suspicious level of health.", AchievementReward::Skill, 2},
    {"Bottle Rocket", "Threw a potion instead of drinking it. Three times.", AchievementReward::None, 0},
    {"Alchemical Waste", "Threw ten potions. Waste not?", AchievementReward::Buff, 3},
    {"Paper Airplane", "Threw a scroll instead of reading it.", AchievementReward::None, 0},
    {"Literary Litterer", "Threw ten scrolls without reading a word.", AchievementReward::Buff, 3},
    {"Food Fight", "Threw food at a monster instead of eating it.", AchievementReward::None, 0},
    {"Waste Not, Want Most", "Threw ten meals at monsters. Bold strategy.", AchievementReward::Buff, 6},
    {"Potion Cabinet", "Picked up fifteen potions in a single run.", AchievementReward::None, 0},
    {"Hoard of Vials", "Picked up forty potions in a single run.", AchievementReward::Skill, 5},
    {"Nest Egg", "Picked up twenty piles of gold in a single run.", AchievementReward::None, 0},
    {"Windfall", "Picked up sixty piles of gold in a single run.", AchievementReward::Buff, 7},
    {"Wanderlust", "Changed floors fifteen times in one run.", AchievementReward::None, 0},
    {"Restless", "Changed floors thirty times in one run.", AchievementReward::Buff, 4},
    {"Never Settled", "Changed floors fifty times in one run. Can't sit still.", AchievementReward::Skill, 6},
    {"Teetotaler", "Completed a run without using a single item.", AchievementReward::LoreUnlock, 0},
    {"Teetotaler at Twelve", "Reached depth 12 without using a single item this run.", AchievementReward::Title, 23},
    {"Strange Convergence", "Killed a monstrously huge creature at a suspiciously shallow depth.", AchievementReward::LoreUnlock, 0},
    {"Eerie Coincidence", "Finished a run having neither killed anything nor used a single item. Did you even play?", AchievementReward::Skill, 7},
    {"Hidden Depths", "Reached depth 18 in almost suspiciously good health.", AchievementReward::Title, 24},

    // -- Pet & Companion bucket (Milestone 3, ids 235-249): authored inert.
    // No AchievementConditions.h rows exist for these ids and nothing in the
    // legacy AchievementBus::emit() switch references them either, so they
    // are permanently un-unlockable until a real pet system exists and calls
    // unlock() for them directly -- zero mechanical cost, zero runtime risk.
    // See Achievements.h and .planning/ACHIEVEMENT_POOL.md. --
    {"First Friend", "Tamed your first companion.", AchievementReward::None, 0},
    {"Inseparable Bond", "Kept the same companion for an entire run.", AchievementReward::Buff, 2},
    {"Menagerie Keeper", "Tamed five different companions across your travels.", AchievementReward::Skill, 3},
    {"Full Stable", "Had three companions active at once.", AchievementReward::Buff, 5},
    {"Well Fed", "Fed your companion ten times.", AchievementReward::None, 0},
    {"Never Hungry", "Kept your companion fed for an entire run.", AchievementReward::Buff, 3},
    {"Loyal to the End", "Your companion stood by you until the very last floor.", AchievementReward::Title, 25},
    {"Old Friend", "Kept the same companion across ten runs.", AchievementReward::Skill, 6},
    {"Raised from an Egg", "Hatched a companion from an egg.", AchievementReward::None, 0},
    {"Tamer of Beasts", "Tamed a monster twice your own depth.", AchievementReward::Skill, 4},
    {"Whisperer of the Deep", "Tamed a creature from the deepest floors.", AchievementReward::LoreUnlock, 0},
    {"Two Against the Dungeon", "Cleared a floor with your companion at your side.", AchievementReward::None, 0},
    {"Guardian at My Side", "Your companion saved you from a killing blow.", AchievementReward::Buff, 6},
    {"Share the Spoils", "Let your companion carry loot for you.", AchievementReward::None, 0},
    {"Beast and Delver", "Reached the bottom of the dungeon with a companion still alive.", AchievementReward::Title, 26},
};

inline constexpr int ACHIEVEMENT_DEF_COUNT = sizeof(ACHIEVEMENT_DEFS) / sizeof(ACHIEVEMENT_DEFS[0]);
static_assert(ACHIEVEMENT_DEF_COUNT == 250, "ACHIEVEMENT_DEFS must have exactly 250 entries");

// Bounds-safe achievementDef(AchievementId) lookup lives in Achievements.h,
// not here -- AchievementId is declared there (which already includes this
// header), and GameTypes.h can't reference it without a circular include.

// Resolves an achievement's reward through TITLE_STRINGS/SPONSOR_DEFS into a
// short display phrase, e.g. "Unlocks title: the Reckless". Writes an empty
// string for AchievementReward::None. Shared by the achievement list's
// subtitle line (GameMenuActivity) and the unlock banner's reward line
// (GameRenderer) so the resolution logic exists in exactly one place.
inline void achievementRewardText(const AchievementDef& def, char* out, size_t outSize) {
  switch (def.reward) {
    case AchievementReward::None:
      if (outSize > 0) out[0] = '\0';
      break;
    case AchievementReward::Title:
      snprintf(out, outSize, "Unlocks title: %s", TITLE_STRINGS[def.rewardValue]);
      break;
    case AchievementReward::SponsorUnlock:
      snprintf(out, outSize, "Unlocks sponsor: %s", SPONSOR_DEFS[def.rewardValue].name);
      break;
    case AchievementReward::LoreUnlock:
      snprintf(out, outSize, "Unlocks a lore entry");
      break;
    case AchievementReward::Buff:
      snprintf(out, outSize, "Grants buff: %s", BUFF_DEFS[def.rewardValue].name);
      break;
    case AchievementReward::Skill:
      snprintf(out, outSize, "Grants skill: %s", SKILL_DEFS[def.rewardValue].name);
      break;
  }
}

}  // namespace game
