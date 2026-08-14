#pragma once

#include "GameTypes.h"
#include "Sprite2bpp.h"

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

// Second registry entry, proving the mechanism is two-deep already. Also
// all-nullptr for now -- real sprite art lands in a future change.
inline constexpr TileTheme kThemeCarl = {
    "Dungeon Crawler Carl",
    {},
    nullptr,
    {},
    {},
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
