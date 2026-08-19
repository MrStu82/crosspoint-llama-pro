#pragma once

#include "GameTypes.h"
#include "Sprite2bpp.h"
#include "../../sprites/world_dungeon_system_carl_sprites.h"
#include "../../sprites/world_dungeon_companion_species.h"

namespace game {

// Theme-swap mechanism for Deep Mines rendering. A TileTheme is a table of
// optional sprite pointers, one slot per tile/player/companion/monster/item
// glyph. Any slot may be nullptr (or point at a Sprite2bpp with a null
// `data`) to mean "no art for this theme yet" -- GameRenderer falls back to
// the existing glyph-based drawText() rendering in that case.
struct TileTheme {
  const char* name;
  const Sprite2bpp* tiles[static_cast<size_t>(Tile::TileCount)];
  const Sprite2bpp* player;
  const Sprite2bpp* companion[PET_SPECIES_COUNT];
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
    {},  // companion[PET_SPECIES_COUNT]
    {},  // monsters[MONSTER_DEF_COUNT]
    {},  // items[ITEM_DEF_COUNT]
};

// Full "System Carl" art pass (Pixel, 2026-08-18) -- one coherent set, one
// hand, one convention. Supersedes the earlier partial recovered-art theme
// wholesale (not merged). Polarity verified from source by both Pixel and
// Gauge independently (matching decoders, matching verdict: correct) before
// this landed -- see parent msg 4168.
inline constexpr TileTheme kThemeCarl = {
    "Dungeon Crawler Carl",
    {
        &kTileWall,
        &kTileFloor,
        &kTileDoorClosed,
        &kTileDoorOpen,
        &kTileStairsUp,
        &kTileStairsDown,
        &kTileRubble,
        &kTileWater,
    },
    &kPlayerSystemCarl,
    {
        // Order matches PET_SPECIES_DEFS (GameTypes.h): Rat Terrier, Cave
        // Lizard, Baby Griffin, Shadow Sprite, Iron Beetle, Sewer Pup, Scrap
        // Drone, Grid Sprite.
        &kSpeciesRatTerrier, &kSpeciesCaveLizard, &kSpeciesBabyGriffin, &kSpeciesShadowSprite,
        &kSpeciesIronBeetle, &kSpeciesSewerPup, &kSpeciesScrapDrone, &kSpeciesGridSprite,
    },
    {
        &kMonsterGiantRat, &kMonsterBat, &kMonsterSewerRatling, &kMonsterGridBug,
        &kMonsterMutantScav, &kMonsterBroodGrunt, &kMonsterHoundBeast, &kMonsterLargeSpider,
        &kMonsterSkeleton, &kMonsterBroodkinBerserker, &kMonsterCaveBrute, &kMonsterHuskWraith,
        &kMonsterShade, &kMonsterSiegeBeast, &kMonsterOgreWarlord, &kMonsterFireDrake,
        &kMonsterReaperUnit, &kMonsterYoungDragon, &kMonsterInfernoTitan, &kMonsterAncientDragon,
        &kMonsterTheAdjudicator,
    },
    {
        &kItemDagger, &kItemShortSword, &kItemLongSword, &kItemBattleAxe, &kItemNanoweaveBlade,
        &kItemLeatherArmor, &kItemChainMail, &kItemPlateMail, &kItemNanoweaveCoat,
        &kItemWoodenShield, &kItemIronShield, &kItemPotionOfHealing, &kItemPotionOfMana,
        &kItemPotionOfStrength, &kItemScrollOfIdentify, &kItemScrollOfTeleport,
        &kItemScrollOfMapping, &kItemRations, &kItemNutrientBar, &kItemGoldCoins,
        &kItemSponsorCrate, &kItemMasterKey, &kItemRingOfPower,
    },
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
