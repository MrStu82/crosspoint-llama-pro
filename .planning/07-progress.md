# Phase 7 (World Dungeon: Correctness) — working notes

Written as work happens, per parent's 2026-08-15 feedback (don't let state only live in-head).

## Requirements (frozen, confirmed by parent msg 3618)
1. 10,000 generated floors, zero victory items in the loot pool.
2. Blocking death screen (cause, floor, turns, kills, level, achievements), tap-dismiss only.
3. Blocking victory screen, same data shape, no eject to launcher.
4. Depth-weighted monster selection: depth 26 mean tier > depth 5 mean tier, over 1,000 floors.
5. One persistent RNG stream on GameState, seeded once/run, serialised with save. Same seed replays identically; no repeated roll same tile/turn.
6. turnCount -> uint32_t. Opened-door tile state persists across save/reload.
7. Turn 70,000 reached, regen still firing at correct cadence.

Constraint (parent msg 3622): death/victory screens must not be a `clearScreen()` full repaint — Phase 8 dirty-rect work lands on top immediately after.

## Status (2026-08-15): reqs 1, 2, 3, 4, 5, 6 implemented and build-green on Trantor
(x4pro env, `pio run` SUCCESS in 92s, full tree + uncommitted overlay). Not yet committed/pushed --
still local-only, pending harness verification below. Not yet Gauge/parent reviewed.

Implemented this pass:
- Req 1: `RING_OF_POWER_DEF` excluded from `DungeonGenerator::placeItems()`'s uniform roll.
- Req 2/3: `GameScreenMode{Playing,Death,Victory}` on `GameActivity`; `EndScreenData` struct
  (cause/floor/turns/kills/level/achievements-this-run) on `GameRenderer.h`; `handlePlayerDeath()`/
  `handleVictory()` populate it via new `populateEndScreenData()` (idempotent, guarded against
  double-entry) then flip `screenMode` instead of an immediate `onGoHome()`; `render()` branches to
  `GameRenderer::drawEndScreen()` (no `clearScreen()`, one-shot `FULL_REFRESH`, per parent's msg
  3622 constraint) while `screenMode != Playing`; `loop()` gates all input to a single tap-dismiss
  -> `onGoHome()` path. 10 new `STR_DM_*`/`STR_SOLITAIRE_TITLE` i18n keys added + regenerated.
  Kill counter and run-scoped achievement tracking (`AchievementBus::unlockedThisRun[]`,
  `resetRun()` called from `GameState::newGame()`) both landed as part of this — see AchievementBus
  diff. `handleAction()`'s old `if (p.hp==0) handlePlayerDeath()` guard kept as a defensive no-op
  fallback (loop()'s new gating makes it normally unreachable; commented as such).
- Req 4: depth-weighted monster selection in `DungeonGenerator.cpp` (not yet re-verified via
  harness this pass -- carried from prior session's implementation).
- Req 5: persistent `GameState`-level RNG stream (`combatRngState`), `rollRange`/
  `rollRangeInclusive` mutate it; `SAVE_FILE_VERSION` bumped to 2.
- Req 6: `Player::turnCount` -> `uint32_t`; door-open bitmap (`doorOpen[game::FOG_SIZE]`, reusing
  `fogIsExplored`/`fogSetExplored` helpers) added to `GameActivity`, threaded through
  `GameSave::saveLevel/loadLevel`; `LEVEL_FILE_VERSION` bumped to 2.

## Update (2026-08-15, later same day): reqs 1 & 4 now machine-verified; 5 & 6 code-reviewed

**Environment gap hit first**: neither the container (no `cmake` at all) nor Trantor (`cmake`
present but no native x86_64 C++ compiler -- only ESP32 cross-toolchains under
`~/.platformio/packages`) currently has a working cmake+gtest+native-compiler combo. The gtest
harness (`test/dungeon_generator/DungeonGeneratorTest.cpp` + its `CMakeLists.txt`, wired into
`test/CMakeLists.txt`) is written and committed as source but has never actually been built/run
anywhere. Worked around for this pass by writing a dependency-free standalone harness
(`test/dungeon_generator/DungeonGeneratorHarness.cpp`, no gtest/cmake, just `<cstdio>` + a
`CHECK()` macro) compiled directly with the container's native `g++` -- same test logic, ported
1:1. Flag to Rivet/parent as an infra gap if gtest-based host tests are wanted going forward.

**Req 1 -- PASS (machine-verified).** 10,000-floor sweep (varying seed via
`i*2654435761u+1` and depth via `1+(i%MAX_DEPTH)`), scanning every generated item for
`type==Ring && subtype==0` (the Ring of Power signature). Result: **154,920 items generated, 0
Ring-of-Power hits.**

**Req 4 -- PASS (machine-verified).** 1,000-floor sweep each at depth 5 and depth 26 (=MAX_DEPTH),
mean `monster.type` (MONSTER_DEFS index, tier proxy) per floor. Result: **depth 5 mean tier=4.495,
depth 26 mean tier=16.330** -- deep floors skew clearly and heavily toward tougher monsters.

**Req 5 -- PASS (machine-verified, real save/reload round trip).** Parent explicitly rejected
code-review as sufficient for a save-format requirement (2026-08-15, msg 3626: "a save-format
change is only true if something wrote bytes, closed the file, reopened it and got the same state
back") and redirected to reuse `/workspace/agent/ach_test/`'s existing `HalStorage` host mock
(real `fopen`/`fwrite`/`fread`/`fclose` against a temp dir, not simulated -- already proven getting
the achievement bus to 21/21). New harness
(`test/game_save/GameSaveRoundTripHarness.cpp`, same no-cmake/no-gtest g++ pattern, extended stub
at `test/game_save/stubs/HalStorage.h` with `close()`/`exists()`/`remove()` added on top of the
`ach_test` version) compiles the REAL `src/game/GameState.cpp`, `GameSave.cpp` and
`AchievementBus.cpp` unmodified against the stub via include-path substitution. Method: 137 rolls
via `GAME_STATE.rollRange()`, `saveToFile()`, deliberately corrupt the live in-memory
`combatRngState`/hp/turnCount to garbage, `loadFromFile()`, 89 more rolls, then compare those 89
rolls against roll indices [137,226) of an independent reference `game::Rng` replayed from the same
seed with no save/reload involved at all -- this specifically catches a reload that silently
restarts the stream from the seed instead of continuing it. Result: **0 of 89 post-reload rolls
diverged from the continued reference stream; hp/turnCount both correctly restored from disk, not
left at the corrupted in-memory values.**

**Req 6 -- PASS (machine-verified, real save/reload round trip, including old-save compat path).**
Same harness file, two more tests. (a) `doorOpen`/`fogOfWar` round trip: built 500-byte bitmaps
with a known non-trivial byte pattern, `GameSave::saveLevel()`, fresh sentinel-filled buffers,
`GameSave::loadLevel()` -- **doorOpen and fogOfWar both byte-for-byte identical after the round
trip, monster/item payloads intact.** (b) old-save nullptr-compat path: hand-crafted a byte-exact
v1-format file (version=1, depth, fog, *no* door bytes, monsters, items -- the literal pre-Phase-7
layout, not a version-2 file with door bytes stripped) and loaded it through the current
`loadLevel()` twice -- once with `doorOpen=nullptr` (**succeeded, fog/monster/item data intact and
correctly aligned, proving the version-gated discard-read consumes the right byte count instead of
misreading monster bytes as door bytes**), once with a real sentinel-filled (`0x5A`) doorOpen buffer
(**left completely untouched, confirming the "buffer unmodified on old saves" contract in
GameSave.h is actually honored, not just documented**).
Self-check performed before trusting the green result (per the general standard for a new harness,
not parent-requested this time): temporarily commented out the `doorOpen` write in the real
`GameSave.cpp`, rebuilt, reran -- harness correctly failed 3 checks (bitmap mismatch, monster/item
corruption from the resulting byte-misalignment), confirming the harness has real teeth rather than
being vacuously green. Reverted immediately, confirmed the restored source is back to the exact
pre-edit content, rebuilt clean.
Build: `g++ -std=c++20 -O2 -Wall -Wextra -I test/game_save/stubs -I src/game -I lib/Serialization
test/game_save/GameSaveRoundTripHarness.cpp src/game/GameState.cpp src/game/GameSave.cpp
src/game/AchievementBus.cpp -o /tmp/game_save_harness`. Runs in well under a second, all file I/O
against `/tmp/game_save_harness_sd` (wiped and recreated at the start of every run).

**Req 7 -- PASS (machine-verified).** New standalone harness
(`test/dungeon_generator/RegenCadenceHarness.cpp`, same no-cmake/no-gtest pattern) re-implements
the exact cadence formula from `GameActivity.cpp:586-592` (`regenRate = max(5, 20-con/2)`, HP ticks
`turnCount % regenRate == 0`, MP ticks `turnCount % (regenRate+5) == 0`) as a turn-by-turn
simulation (not a closed-form shortcut, so it would actually catch drift if the real code used an
accumulator) with `turnCount` as `uint32_t`, matching `Player::turnCount`'s real type. Two checks:
(1) aggregate tick count to turn 70,000 matches closed-form `floor(target/rate)` across 7
constitution values (0/5/10/20/30/50/100, spanning both the clamped-at-5 and unclamped branches);
(2) tick rate in turns [1,35000] matches [35001,70000] within 1 tick (rules out mid-run drift an
aggregate-only check could mask). Result: **all 7 constitution values passed exact aggregate
match; steady-rate-across-halves passed (HP 2692 vs 2692, MP 1944 vs 1944 at con=14)**. Runs in
3ms.

**Reqs 2/3** -- unchanged, review/hardware-only (render path is 👁️).

### Summary: 6 of 7 reqs now machine-verified (1, 4, 5, 6, 7), 2 review/hardware-only (2, 3)

All reqs testable in a container are now genuinely machine-verified — no code-review-only
verification remains anywhere in Phase 7. Reqs 2/3 are the only ones left unproven-in-container,
and that's correct by design: they're a render-path claim (blocking screen actually paints and
blocks input) that no host harness can honor, confirmed explicitly by parent ("I'm not pretending a
render path can be proven in a container").

### Still open / next
- Update `STATE.md` to reflect Phase 7 status.
- Send parent a progress update summarizing final status of all 7 reqs.
- Decide commit/push timing for the new test assets: `test/game_save/stubs/Logging.h`,
  `test/game_save/stubs/HalStorage.h`, `test/game_save/GameSaveRoundTripHarness.cpp`, plus this
  progress-file update. No APK build is involved (host-only test infra), but per parent's own
  framing this harness is a durable, reusable asset — expected to be needed again for the final
  handover checklist and for Phase 12's pre-companion-save load test — so it's worth committing for
  durability rather than leaving it only in this container's workspace.
- Phase 7 is otherwise complete pending parent's own hardware verification of reqs 2/3.
- Update `STATE.md`, send parent a progress update, then decide on commit/push timing.

## Status (superseded, kept for history): investigation complete, findings below.

Read in full: GameTypes.h, GameActivity.h/cpp, GameSave.h/cpp, DungeonGenerator.cpp (prior session),
GameRenderer.h/cpp (draw() only), Achievements.h, AchievementBus.h/cpp.

### Req 1 — no victory item in random loot pool
Narrower than it first looked. Boss-drop already correct: killing `monsters[i].type ==
BOSS_MONSTER_TYPE` already spawns a Ring of Power at the death tile (GameActivity.cpp:274-286,
untouched). Bug is solely in `DungeonGenerator::placeItems()` — its uniform
`rng.nextRange(ITEM_DEF_COUNT)` roll includes `RING_OF_POWER_DEF`, so it can also spawn as
ordinary floor loot (double-spawn risk). **Fix: exclude `RING_OF_POWER_DEF` from that roll only.**
No other req 1 changes needed.

### Req 2/3 — death/victory screens
No screen exists at all today. `handlePlayerDeath()` (GameActivity.cpp:629-638) and
`handleVictory()` (GameActivity.cpp:642-655) each just: add one line to the message log,
`requestUpdate()`, delete save data (already correct, keep as-is), then `onGoHome()` — immediate
eject, no blocking, no tap-dismiss, no data display.

Required data shape (cause, floor, turns, kills, level, achievements this run) — **two fields
don't exist yet and need adding**:
- **Kill count**: no counter exists anywhere in `Player`/`GameState`. Must add a field (e.g.
  `Player::kills` uint16_t, or GameState-level) incremented on `MonsterKilled` events.
- **Achievements this run**: `AchievementBus::unlocked[]` is an all-time flag file
  (achievements.bin) — `unlock()` is a no-op once already-unlocked
  (`if (unlocked[idx]) return;`), so it never reports "newly earned this run." Need a second,
  run-scoped `unlockedThisRun[]` array (reset at run start, e.g. in `GameActivity::onEnter()` when
  starting fresh) alongside the existing persistent one, plus a getter for the death/victory
  screen to read. Persistent unlock semantics (no re-fire, no double-save) stay untouched.
- **Cause of death**: not currently captured as a displayable string — `PlayerDied` event carries
  `monsterAttack`/`monsterMaxHp` but no cause/name. Need to capture the killer (monster name or
  "starvation"/other cause if any exist) into the death-screen data at the point of death.
- floor (`p.dungeonDepth`), turns (`p.turnCount`), level (`p.charLevel`) already exist and are
  readable as-is.

**Rendering constraint (parent msg 3622, confirmed by reading source):** `GameRenderer::draw()`
(GameRenderer.cpp:32-52) unconditionally calls `renderer.clearScreen()` at line 39 — this is THE
normal per-frame full-repaint path. Death/victory screens must NOT reuse `draw()` as-is. Plan:
give `GameActivity`/`GameRenderer` a new render mode (e.g. `drawEndScreen(...)`) that paints once
without going through the standard `clearScreen()`+viewport+status+controls pipeline, called from
`GameActivity::render()` when a `screenMode == Death|Victory` flag is set, gated on explicit tap
input in `loop()` before returning to `onGoHome()`/normal flow. Needs to coexist cleanly with
Phase 8's dirty-rect work landing right after — keep the new path self-contained/minimal so it's
easy to adapt.

### Req 4 — depth-weighted monster selection
Unchanged from prior session's finding: `DungeonGenerator::placeMonsters()` builds an eligible
list (`minDepth <= depth`) then picks uniformly via `rng.nextRange(eligibleCount)` — no depth
weighting at all. Lives in DungeonGenerator.cpp, not GameActivity.cpp. Fix: weight the roll toward
higher-tier (higher-index) eligible monsters as depth increases, e.g. bias distribution or
restrict the low end of the eligible range at high depth.

### Req 5 — persistent RNG stream
Confirmed 4 ad-hoc call sites, each constructs a fresh `game::Rng` seeded from
`turnCount ^ position` on every single roll — cheap/predictable, and reseeding per-call means no
real persistent stream:
- GameActivity.cpp:255 — player melee attack: `game::Rng rng(p.turnCount ^ (p.x*31 + p.y*37));`
- GameActivity.cpp:457 — monster wake-chance (Asleep state), inside `processMonsterTurns()`:
  `game::Rng rng(p.turnCount ^ (m.x*17 + m.y*13 + i));`
- GameActivity.cpp:474 — monster wander direction: `game::Rng rng(p.turnCount ^ (m.x*23 + m.y*29 + i*7));`
- GameActivity.cpp:569 — monster attacks player, inside `monsterAttackPlayer()`:
  `game::Rng rng(p.turnCount ^ (m.x*41 + m.y*43));`

Fix: add a persistent `game::Rng combatRng` (or just its `uint32_t state`) as a `GameState`
member, seeded once at run start from `p.gameSeed` (or a derived constant), and have all 4 sites
call `GAME_STATE.combatRng.next()`/`nextRange()` instead of constructing a local `Rng`. Must be
serialised in `GameState::saveToFile()`/`loadFromFile()` — **bump `SAVE_FILE_VERSION`
(GameState.cpp) before changing the struct layout**, per CLAUDE.md's binary-format-versioning
rule.

### Req 6 — turnCount width + door persistence
Two independent sub-fixes:
- **turnCount width**: `Player::turnCount` is `uint16_t` (GameTypes.h:75) — overflows at 65,535,
  short of req 7's 70,000 target. Widen to `uint32_t`. This changes `Player`'s raw POD layout
  (written/read via `writePod`/`readPod` in GameState.cpp) — **bump `SAVE_FILE_VERSION`** (can
  fold into the same bump as req 5's RNG-stream addition, one version bump covering both changes).
- **Door persistence**: confirmed at the *file-format* level, not just in-memory. `GameSave.h`'s
  `saveLevel`/`loadLevel` signatures only take `fogOfWar`, `monsters`, `items` — no tile/door
  parameter exists at all. `GameSave.cpp`'s binary format
  (`LEVEL_FILE_VERSION=1`, `/.crosspoint/game/level_%02u.bin`) is
  `[version][depth][fogOfWar bytes][monsterCount][monsters...][itemCount][items...]` — no tile
  data. `GameActivity::loadOrGenerateLevel()` (GameActivity.cpp:659-680) always calls
  `DungeonGenerator::generate(...)` fresh from the seed every level entry, which resets every door
  to `DoorClosed`, then overlays only fog/monsters/items from the save file — door state is never
  part of the save/restore cycle at all today.
  Fix: extend `GameSave::saveLevel()`/`loadLevel()` with a door-state parameter (compact bitmap
  over `MAP_SIZE`, or a short diff-list of opened door tile indices — bitmap is simpler and
  MAP_SIZE/8 = 500 bytes, same cost as the existing fog bitmap, so just reuse that pattern) and
  apply it as an extra overlay step in `loadOrGenerateLevel()` after the DungeonGenerator call.
  **Bump `LEVEL_FILE_VERSION`** in GameSave.cpp before changing this format.

### Req 7 — turn 70,000 + regen cadence
Regen logic already exists and looks correct: `processMonsterTurns()` (GameActivity.cpp:551-559)
does natural HP/MP regen keyed off `p.turnCount % regenRate == 0`
(`regenRate = max(5, 20 - CON/2)` for HP, `regenRate+5` for MP). Once turnCount is widened to
`uint32_t` (req 6), this arithmetic should keep working unchanged (mod arithmetic is agnostic to
the integer width as long as no overflow occurs before the widening). Req 7 is primarily a
verification exercise: build a HOST harness that fast-forwards a run to turn 70,000+ and confirms
regen still fires at the expected cadence, no crash/overflow artifacts.

## Additional gaps identified beyond the frozen 7 requirements (informational, do only what's needed to satisfy the 7)
- No kill counter exists anywhere pre-Phase-7 — net-new field, required for req 2/3's data shape.
- No "cause of death" string capture exists — net-new, required for req 2/3.
- `AchievementBus` has no concept of "this run" vs "all time" — net-new run-scoped array required
  for req 2/3's "achievements this run" field.

## Implementation order (proposed)
1. Req 1 (smallest, isolated — DungeonGenerator.cpp placeItems() exclusion).
2. Req 6 turnCount widen + req 5 RNG stream (bundle into one SAVE_FILE_VERSION bump since both
   touch GameState's saved Player/stream layout).
3. Req 6 door persistence (separate LEVEL_FILE_VERSION bump, independent of the above).
4. Req 4 depth-weighted monster selection (DungeonGenerator.cpp, isolated).
5. Req 2/3 death/victory screens (largest — needs kill counter + cause-of-death capture +
   run-scoped achievement tracking + new non-clearScreen render path, built last since it depends
   on turnCount/kills/RNG-adjacent state being final).
6. Req 7 — HOST harness soak test validating regen cadence to turn 70,000+, run after all above
   land (turnCount width must already be uint32_t).
7. Build/extend HOST machine-gate harness covering: 10,000-floor loot sweep (req 1), 1,000-floor
   monster-tier sweep (req 4), RNG determinism replay check (req 5), save/reload door-state check
   (req 6), 70,000-turn soak (req 7).
