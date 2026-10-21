# Roadmap — x4pro-slim

## Phase 1 — Archive and remove games
Archive branch/tag at 68be8d33; delete World Dungeon (src/game, Game*/sprites) and
Tamagotchi source, assets, menu entries, registrations, gameTheme setting and
game-only i18n keys (STR_DM_HINT_MENU kept: Solitaire uses it). Affected host tests,
one compile, ONE x4pro bare-app release build.
Done when: artifact verified (ESP32-S3, descriptor = head, checksum/hash valid, slot fit).

## Phase 2 — Refactor review (review only)
Rank dead code, duplication, flash/RAM hogs, hot paths by value vs risk with measured bytes/ms.

## Phase 3 — Upstream review (review only)
Upstream CrossPoint develop + releases since port base; exclude already ported;
priority/size/risk/UI flag. Stuart chooses before porting.
