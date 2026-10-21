# x4pro-slim (Stuart 1085300, parent #9740, 2026-10-21)

Base: shipped X4 Pro release head `68be8d33ba0b5da7684298bd535e4ead6725e5ae`
(descriptor `68be8d3`, platformio version 1.5.0; v236 11e57f90 + b94ce319 GPIO1 sleep + quote refresh).
Last parent-confirmed delivery remains v236 `11e57f90`.

Goal: reclaim flash by removing World Dungeon and Tamagotchi, then review-only
refactor (P2) and upstream (P3) reports. Never flash. Partitions, sdkconfig and
SDK (freeink-sdk `8c960bc2`) unchanged. YAGNI/DRY/KISS.

Archive: branch `archive/games-x4pro` and tag `archive/games-x4pro-68be8d3` hold full game source.
