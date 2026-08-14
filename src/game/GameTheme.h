#pragma once

#include "GameTypes.h"
#include "Sprite2bpp.h"
#include "../../sprites/world_dungeon_default_sprites.h"

namespace game {

// Theme-swap mechanism for Deep Mines rendering. A TileTheme is a table of
// optional sprite pointers, one slot per tile/player/monster/item glyph. Any
// slot may be nullptr (or point at a Sprite2bpp with a null `data`) to mean
// "no art for this theme yet" -- GameRenderer falls back to the existing
// glyph-based drawText() rendering in that case, so a theme with zero real
// sprites (like both themes defined below, today) renders byte-for-byte
// identically to the pre-theme glyph-only renderer.
struct TileTheme {
  const char* name;
  const Sprite2bpp* tiles[static_cast<size_t>(Tile::TileCount)];
  const Sprite2bpp* player;
  const Sprite2bpp* monsters[MONSTER_DEF_COUNT];
  const Sprite2bpp* items[ITEM_DEF_COUNT];
};

// Registry of available themes. Values are persisted (see
// CrossPointSettings::gameTheme), so existing entries must never be
// reordered or removed -- only append.
enum class GameThemeId : uint8_t {
  Default = 0,
  DungeonCrawlerCarl = 1,
  Count
};

// All-nullptr: no sprite art exists yet, every lookup falls through to the
// existing glyph rendering.
inline constexpr TileTheme kThemeDefault = {
    "Default",
    {},  // tiles[Tile::TileCount]
    nullptr,
    {},  // monsters[MONSTER_DEF_COUNT]
    {},  // items[ITEM_DEF_COUNT]
};

// Proof-of-pipeline theme: 8 tiles + player + Fire Drake are real recovered
// art (2026-08-14); Ancient Dragon and Young Dragon slots stay nullptr until
// Pixel draws them fresh, post polarity-verdict -- they fall back to glyph
// rendering like any other unpopulated slot, per the TileTheme contract
// above. Polarity (black = both bit-planes 0) is UNVERIFIED pending Gauge's
// on-device photo check.
inline constexpr TileTheme kThemeCarl = {
    "Dungeon Crawler Carl",
    {
        &WallDefault,        // Tile::Wall
        &FloorDefault,       // Tile::Floor
        &DoorClosedDefault,  // Tile::DoorClosed
        &DoorOpenDefault,    // Tile::DoorOpen
        &StairsUpDefault,    // Tile::StairsUp
        &StairsDownDefault,  // Tile::StairsDown
        &RubbleDefault,      // Tile::Rubble
        &WaterDefault,       // Tile::Water
    },
    &PlayerDefault,
    {
        nullptr, nullptr, nullptr, nullptr, nullptr,  // Giant Rat..Goblin
        nullptr, nullptr, nullptr, nullptr, nullptr,  // Orc..Uruk-hai
        nullptr, nullptr, nullptr, nullptr, nullptr,  // Cave Troll..Olog-hai
        &FireDrakeCarl,  // Fire Drake (index 15)
        nullptr,         // Nazgul
        nullptr,         // Young Dragon -- not recoverable, drawn fresh post-verdict
        nullptr,         // Balrog
        nullptr,         // Ancient Dragon -- not recoverable, drawn fresh post-verdict
        nullptr,         // The Necromancer (boss)
    },
    {},  // items -- unpopulated, falls back to glyph rendering
};

// Returns the theme table for the given id, falling back to kThemeDefault
// for any out-of-range value (defensive against a corrupt/legacy persisted
// setting).
inline const TileTheme* getTheme(GameThemeId id) {
  switch (id) {
    case GameThemeId::DungeonCrawlerCarl:
      return &kThemeCarl;
    case GameThemeId::Default:
    default:
      return &kThemeDefault;
  }
}

}  // namespace game
