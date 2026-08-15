#pragma once

// System flavour text — Phase 9 work item 2 ("Voice — make it Dungeon Crawler
// Carl"). Every category below is a flash-resident set of >=3 const char*
// literals; FlavorTextTracker::pick() draws an index from the run's
// persistent combat RNG stream (GameState::rollRange, never a local RNG —
// see GameState.h) and refuses to repeat the same variant twice in a row
// within that category. No string is ever built at runtime here — callers
// combine a picked literal with snprintf into a stack buffer exactly like
// the rest of GameActivity.cpp already does for numeric/name interpolation.

#include <cstdint>

#include "GameState.h"

namespace game {

enum class FlavorCategory : uint8_t {
  HitGraze,
  HitSolid,
  HitHeavy,
  HitOverkill,
  MonsterKilled,
  DamagedLight,
  DamagedModerate,
  DamagedHeavy,
  PlayerDeath,
  LevelUp,
  FloorEntry,
  BossArrival,
  Count,
};

namespace flavor_detail {

inline constexpr const char* kHitGraze[] = {
    "System log: glancing blow registered.",
    "Barely a scratch. The System notes it anyway.",
    "Contact, minimal damage. Try harder, contestant.",
};
inline constexpr const char* kHitSolid[] = {
    "Solid hit. The System awards partial credit.",
    "That connected. Nice work, contestant.",
    "A clean strike. The crowd murmurs approval.",
};
inline constexpr const char* kHitHeavy[] = {
    "Heavy damage! The System is impressed.",
    "That one hurt. Ratings just spiked.",
    "A brutal hit. Somewhere, a sponsor is smiling.",
};
inline constexpr const char* kHitOverkill[] = {
    "Overkill. The System flags this for the highlight reel.",
    "Completely obliterated. The audience goes wild.",
    "That was excessive. The System approves anyway.",
};
inline constexpr const char* kMonsterKilled[] = {
    "System: kill confirmed. XP awarded.",
    "Another one down. The crowd cheers.",
    "Elimination logged. Keep the ratings up, contestant.",
};
inline constexpr const char* kDamagedLight[] = {
    "System: minor damage taken. Try to look competent.",
    "That stung a little. Nothing serious.",
    "A light hit. The System barely notices.",
};
inline constexpr const char* kDamagedModerate[] = {
    "System: moderate damage. The crowd gasps.",
    "That one landed hard. Watch yourself, contestant.",
    "A solid hit taken. Ratings climbing.",
};
inline constexpr const char* kDamagedHeavy[] = {
    "System: critical damage taken! This is must-see TV.",
    "That nearly ended the show. Get it together.",
    "Severe damage. The System flags a medical drone, just in case.",
};
inline constexpr const char* kPlayerDeath[] = {
    "System: contestant terminated. Ratings hold steady.",
    "The show goes on without you. Better luck next dungeon.",
    "Elimination confirmed. Thanks for the entertainment.",
};
inline constexpr const char* kLevelUp[] = {
    "System: level threshold cleared. Stats updated.",
    "Ding! The audience loves a good glow-up.",
    "You've leveled up. The System adjusts the odds accordingly.",
};
inline constexpr const char* kFloorEntry[] = {
    "System: new floor generated. Try not to die immediately.",
    "Floor transition complete. The crowd settles in.",
    "A fresh floor unfolds. The System resets the scoreboard.",
};
inline constexpr const char* kBossArrival[] = {
    "System: boss encounter detected. This is the main event.",
    "This floor's Adjudicator has taken notice of you.",
    "Boss fight incoming. The System cues dramatic music.",
};

struct FlavorSet {
  const char* const* variants;
  uint8_t count;
};

inline constexpr FlavorSet kFlavorSets[] = {
    {kHitGraze, sizeof(kHitGraze) / sizeof(kHitGraze[0])},
    {kHitSolid, sizeof(kHitSolid) / sizeof(kHitSolid[0])},
    {kHitHeavy, sizeof(kHitHeavy) / sizeof(kHitHeavy[0])},
    {kHitOverkill, sizeof(kHitOverkill) / sizeof(kHitOverkill[0])},
    {kMonsterKilled, sizeof(kMonsterKilled) / sizeof(kMonsterKilled[0])},
    {kDamagedLight, sizeof(kDamagedLight) / sizeof(kDamagedLight[0])},
    {kDamagedModerate, sizeof(kDamagedModerate) / sizeof(kDamagedModerate[0])},
    {kDamagedHeavy, sizeof(kDamagedHeavy) / sizeof(kDamagedHeavy[0])},
    {kPlayerDeath, sizeof(kPlayerDeath) / sizeof(kPlayerDeath[0])},
    {kLevelUp, sizeof(kLevelUp) / sizeof(kLevelUp[0])},
    {kFloorEntry, sizeof(kFloorEntry) / sizeof(kFloorEntry[0])},
    {kBossArrival, sizeof(kBossArrival) / sizeof(kBossArrival[0])},
};

}  // namespace flavor_detail

// Outcome-band thresholds live here once, shared by every call site so the
// combat-hit and damage-taken bands can't drift out of sync with each other.
inline FlavorCategory hitBandForDamage(uint16_t damage, uint16_t targetMaxHp) {
  if (targetMaxHp == 0) return FlavorCategory::HitSolid;
  uint32_t pct = (static_cast<uint32_t>(damage) * 100u) / targetMaxHp;
  if (pct < 15) return FlavorCategory::HitGraze;
  if (pct < 40) return FlavorCategory::HitSolid;
  if (pct < 80) return FlavorCategory::HitHeavy;
  return FlavorCategory::HitOverkill;
}

inline FlavorCategory damagedBandForDamage(uint16_t damage, uint16_t maxHp) {
  if (maxHp == 0) return FlavorCategory::DamagedLight;
  uint32_t pct = (static_cast<uint32_t>(damage) * 100u) / maxHp;
  if (pct < 15) return FlavorCategory::DamagedLight;
  if (pct < 40) return FlavorCategory::DamagedModerate;
  return FlavorCategory::DamagedHeavy;
}

// Selects a flavour-text variant for `category`, drawing from the run's
// persistent combat RNG stream (GameState::rollRange) and refusing to repeat
// the same variant twice in a row within that category. Returned pointers
// are flash-resident string literals -- never heap/stack copies.
class FlavorTextTracker {
  static FlavorTextTracker instance;
  uint8_t lastIndex[static_cast<uint8_t>(FlavorCategory::Count)];

 public:
  FlavorTextTracker() {
    for (auto& v : lastIndex) v = 0xFF;
  }
  static FlavorTextTracker& getInstance() { return instance; }
  // Called on newGame()/loadFromFile() -- a fresh run/reload has no "last
  // shown" history to avoid repeating against.
  void resetRun() {
    for (auto& v : lastIndex) v = 0xFF;
  }
  const char* pick(FlavorCategory category);
};

#define FLAVOR_TEXT game::FlavorTextTracker::getInstance()

}  // namespace game
