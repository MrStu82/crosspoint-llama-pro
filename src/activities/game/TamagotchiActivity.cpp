#include "TamagotchiActivity.h"

#include <Arduino.h>
#include <GfxRenderer.h>
#include <HalClock.h>
#include <HalStorage.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>

#include "CrossPointSettings.h"
#include "I18nKeys.h"
#include "MappedInputManager.h"
#include "TamagotchiSpriteData.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "game/GameSprites.h"

namespace {
constexpr const char* kStateFilePath = "/.crosspoint/tamagotchi.bin";
// Bumped for this build's new State layout (disciplineLevel added, see below) -- load()
// already resets to a fresh State{} on any version mismatch, so old saves are simply
// discarded rather than migrated.
constexpr uint8_t kStateFileVersion = 4;

// Real-time decay/growth tuning. Approved as-is from the original build -- deliberately
// generous vs. the original 90s hardware (which ran meters down over ~hours) so the
// behaviour is provable on a build/QA timescale, while still being "hours matter" per
// Stuart's 8-hours-off-is-hungry requirement.
constexpr int32_t kHungerDecaySeconds = 5 * 60;      // -1 hunger per 5 min
constexpr int32_t kHappinessDecaySeconds = 8 * 60;   // -1 happiness per 8 min
constexpr int32_t kEnergyDecaySeconds = 10 * 60;     // -1 energy per 10 min
constexpr int32_t kIncubationSeconds = 60;            // egg must sit this long before it can hatch
constexpr int32_t kBabyToChildSeconds = 30 * 60;      // min age in Baby stage
constexpr int32_t kChildToAdultSeconds = 60 * 60;     // min age in Child stage
constexpr int32_t kNeglectDeathSeconds = 6 * 60 * 60;  // any meter pinned at 0 this long -> death

// New mechanics per parent's Bandai-idiom redirect.
constexpr int32_t kPoopIntervalSeconds = 12 * 60;      // +1 mess per 12 min, uncleaned
constexpr uint8_t kMaxPoop = 4;
constexpr int32_t kSicknessGraceSeconds = 20 * 60;     // poop pinned at max this long -> sick
constexpr uint8_t kCallThreshold = 30;                 // meter <= this can trigger an attention call
constexpr int32_t kMinCallGapSeconds = 3 * 60;          // throttle between calls
constexpr int32_t kCallTimeoutSeconds = 5 * 60;         // unanswered call -> a care mistake
constexpr uint8_t kMaxMistakesToEvolve = 3;             // care mistakes this stage must stay at/below to evolve

// Discipline: a persisted stat tracking care quality over time, distinct from the
// Discipline icon/button (which scolds the pet to catch-all-resolve a call, see
// resolveAnyCall). Genuinely-resolved calls (feed/play/light/medicine/clean answering
// the correct call) build it up; ignored calls that time out erode it -- same signal
// careMistakes tracks, but persistent across stages rather than reset on every evolve,
// so it also gates evolution alongside careMistakes.
constexpr uint8_t kDisciplineGainOnResolve = 5;          // + for a genuinely resolved call
constexpr uint8_t kDisciplineLossOnExpire = 8;           // - for an ignored/timed-out call
constexpr uint8_t kMinDisciplineToEvolve = 40;           // discipline must be at/above this to evolve

// Sleep cycle. Real RTC day/night schedule -- the pet is asleep from kNightStartHour
// through kNightEndHour every day, decays slower while asleep, and recovers energy
// instead of losing it. Light toggles isAsleep directly rather than being a stat top-up;
// waking it early (still inside the night window) is a care mistake, same as an ignored
// attention call, plus an energy cost for the disturbed rest.
constexpr uint8_t kNightStartHour = 21;  // 9pm
constexpr uint8_t kNightEndHour = 7;     // 7am
constexpr int32_t kSleepEnergyRecoverSeconds = 4 * 60;  // +1 energy per 4 min asleep
constexpr int32_t kAwakeSleepDecayDivisor = 3;          // hunger/happiness decay this many times slower asleep
constexpr uint8_t kWakeEarlyEnergyPenalty = 15;         // energy lost for being woken before the window ends
constexpr int32_t kManualNapDurationSeconds = 30 * 60;  // a daytime nap (started outside the night window) self-wakes after this long

constexpr uint8_t kMealGain = 40;
constexpr uint8_t kSnackGain = 15;
constexpr uint8_t kPlayGain = 25;

// All pet stage sprites are authored at a uniform 64x64 (see TamagotchiSpriteData.h) --
// half-height used to lay out text below the sprite without reading a Sprite2bpp field
// from render()'s free-standing layout code.
constexpr int kPetSpriteHalfH = 32;

uint8_t clampToByte(int32_t value) {
  if (value < 0) return 0;
  if (value > 100) return 100;
  return static_cast<uint8_t>(value);
}

// Night-window test against an arbitrary epoch (not just "now") -- lets tick() detect the
// wasNight->nightNow edge across a single elapsed interval, rather than only ever seeing
// the current instant. gmtime_r is safe here even though nowEpoch()/mktime() built the
// epoch from a local-clock tm with no real timezone applied -- it's used purely as
// calendar arithmetic, so the field pack/unpack round-trips exactly.
bool isNightAtEpoch(int32_t epoch) {
  if (epoch == 0) return false;
  const time_t t = static_cast<time_t>(epoch);
  struct tm result {};
  gmtime_r(&t, &result);
  return result.tm_hour >= kNightStartHour || result.tm_hour < kNightEndHour;
}
}  // namespace

int32_t TamagotchiActivity::nowEpoch() {
  int yyyymmdd = 0;
  uint8_t hour = 0, minute = 0;
  if (!halClock.getDate(yyyymmdd, SETTINGS.clockUtcOffsetQ) || !halClock.getTime(hour, minute)) {
    return 0;
  }
  struct tm t {};
  t.tm_year = yyyymmdd / 10000 - 1900;
  t.tm_mon = (yyyymmdd / 100) % 100 - 1;
  t.tm_mday = yyyymmdd % 100;
  t.tm_hour = hour;
  t.tm_min = minute;
  t.tm_sec = 0;
  const time_t epoch = mktime(&t);
  return static_cast<int32_t>(epoch);
}

bool TamagotchiActivity::isNightNow() { return isNightAtEpoch(nowEpoch()); }

void TamagotchiActivity::load() {
  HalFile file;
  if (!Storage.openFileForRead("TAMA", kStateFilePath, file)) {
    state = State{};
    return;
  }
  uint8_t buf[sizeof(uint8_t) + sizeof(State)];
  const int readLen = file.read(buf, sizeof(buf));
  if (readLen != static_cast<int>(sizeof(buf)) || buf[0] != kStateFileVersion) {
    state = State{};
    return;
  }
  memcpy(&state, buf + 1, sizeof(State));
}

void TamagotchiActivity::save() {
  if (!dirty) return;
  HalFile file;
  if (Storage.openFileForWrite("TAMA", kStateFilePath, file)) {
    uint8_t buf[sizeof(uint8_t) + sizeof(State)];
    buf[0] = kStateFileVersion;
    memcpy(buf + 1, &state, sizeof(State));
    file.write(buf, sizeof(buf));
    dirty = false;
  }
}

void TamagotchiActivity::restartFromEgg(int32_t now) {
  state = State{};
  state.stageStartEpoch = now;
  state.lastUpdateEpoch = now;
  dirty = true;
  screen = Screen::Main;
  cursorIndex = 0;
}

void TamagotchiActivity::hatchIfReady(int32_t now) {
  if (static_cast<Stage>(state.stage) != Stage::Egg) return;
  // now==0 means the RTC is unset (fresh device, never synced/set) -- with no elapsed
  // time to measure, don't hold the egg hostage on a timer that can never expire; let
  // the tap through immediately rather than leaving the game permanently unhatchable.
  if (now != 0 && now - state.stageStartEpoch < kIncubationSeconds) return;
  int tx = 0, ty = 0;
  if (mappedInput.wasScreenTapped(tx, ty)) {
    state.stage = static_cast<uint8_t>(Stage::Baby);
    state.stageStartEpoch = now;
    state.hunger = state.happiness = state.energy = 100;
    state.careMistakes = 0;
    dirty = true;
  }
}

void TamagotchiActivity::evolveIfReady(int32_t now) {
  const Stage stage = static_cast<Stage>(state.stage);
  const int32_t ageInStage = now - state.stageStartEpoch;
  const bool ageReady = (stage == Stage::Baby && ageInStage >= kBabyToChildSeconds) ||
                        (stage == Stage::Child && ageInStage >= kChildToAdultSeconds);
  if (!ageReady) return;

  if (state.careMistakes > kMaxMistakesToEvolve || state.disciplineLevel < kMinDisciplineToEvolve) {
    // Age condition is met but care quality isn't -- this used to leave the pet frozen
    // in this stage forever, since careMistakes only ever reset inside the branch it
    // also gated. Restart the stage clock as well as the count: a failed evolve check
    // costs a full stage window served with good care, not just a one-tick pardon that
    // would let ageReady stay true next tick and evolve anyway despite the neglect.
    // disciplineLevel is NOT reset here (unlike careMistakes) -- it's a running care-quality
    // account across the pet's whole life, not a per-stage counter, so a bad discipline
    // score has to be genuinely earned back up through resolved calls, not wiped free by
    // the stage-clock restart.
    state.stageStartEpoch = now;
    state.careMistakes = 0;
    dirty = true;
    return;
  }

  const Stage next = (stage == Stage::Baby) ? Stage::Child : Stage::Adult;
  state.stage = static_cast<uint8_t>(next);
  state.stageStartEpoch = now;
  state.careMistakes = 0;
  dirty = true;
}

void TamagotchiActivity::checkDeath(int32_t now) {
  const bool anyMeterZero = state.hunger == 0 || state.happiness == 0 || state.energy == 0;
  if (!anyMeterZero) {
    if (state.neglectStartEpoch != 0) {
      state.neglectStartEpoch = 0;
      dirty = true;
    }
    return;
  }
  if (state.neglectStartEpoch == 0) {
    state.neglectStartEpoch = now;
    dirty = true;
    return;
  }
  if (now - state.neglectStartEpoch >= kNeglectDeathSeconds) {
    state.stage = static_cast<uint8_t>(Stage::Dead);
    dirty = true;
  }
}

void TamagotchiActivity::maybeGetSick(int32_t now) {
  if (state.sick) return;
  if (state.poopCount < kMaxPoop) {
    if (state.sicknessGraceStartEpoch != 0) {
      state.sicknessGraceStartEpoch = 0;
      dirty = true;
    }
    return;
  }
  if (state.sicknessGraceStartEpoch == 0) {
    state.sicknessGraceStartEpoch = now;
    dirty = true;
    return;
  }
  if (now - state.sicknessGraceStartEpoch >= kSicknessGraceSeconds) {
    state.sick = 1;
    state.sicknessGraceStartEpoch = 0;
    dirty = true;
  }
}

void TamagotchiActivity::resolveCall(CallKind kind) {
  if (!state.callActive) return;
  if (static_cast<CallKind>(state.callKind) != kind) return;
  state.callActive = 0;
  state.lastCallEndEpoch = nowEpoch();
  state.disciplineLevel = clampToByte(static_cast<int32_t>(state.disciplineLevel) + kDisciplineGainOnResolve);
  dirty = true;
}

void TamagotchiActivity::resolveAnyCall() {
  if (!state.callActive) return;
  state.callActive = 0;
  state.lastCallEndEpoch = nowEpoch();
  // Catch-all resolution is a real care mistake, same as letting a call time out
  // (see maybeExpireCall) -- otherwise spamming Discipline is a zero-cost way to
  // silence every attention call and defeats the evolution care-mistake gate entirely.
  if (state.careMistakes < 255) state.careMistakes++;
  dirty = true;
}

void TamagotchiActivity::maybeStartCall(int32_t now) {
  if (state.callActive) return;
  if (now - state.lastCallEndEpoch < kMinCallGapSeconds) return;

  CallKind kind = CallKind::None;
  if (state.sick) {
    kind = CallKind::Sick;
  } else if (state.poopCount >= kMaxPoop) {
    kind = CallKind::Poop;
  } else if (state.hunger <= kCallThreshold) {
    kind = CallKind::Hunger;
  } else if (state.happiness <= kCallThreshold) {
    kind = CallKind::Happiness;
  } else if (state.energy <= kCallThreshold) {
    kind = CallKind::Energy;
  }
  if (kind == CallKind::None) return;

  state.callActive = 1;
  state.callKind = static_cast<uint8_t>(kind);
  state.callStartEpoch = now;
  dirty = true;
}

void TamagotchiActivity::maybeExpireCall(int32_t now) {
  if (!state.callActive) return;
  if (now - state.callStartEpoch < kCallTimeoutSeconds) return;
  state.callActive = 0;
  state.lastCallEndEpoch = now;
  if (state.careMistakes < 255) state.careMistakes++;
  state.disciplineLevel = clampToByte(static_cast<int32_t>(state.disciplineLevel) - kDisciplineLossOnExpire);
  dirty = true;
}

void TamagotchiActivity::tick(int32_t now) {
  if (now == 0) return;  // clock unavailable -- skip decay rather than guess
  if (state.lastUpdateEpoch == 0) {
    state.lastUpdateEpoch = now;
    dirty = true;
    return;
  }

  const Stage stage = static_cast<Stage>(state.stage);
  if (stage == Stage::Dead || stage == Stage::Egg) {
    state.lastUpdateEpoch = now;
    return;
  }

  // Auto sleep/wake at the schedule boundary (an edge, not a level) -- this is what lets
  // a manual early wake via toggleLight() hold for the rest of the night instead of being
  // immediately re-forced back to sleep on the very next tick.
  const bool wasNight = isNightAtEpoch(state.lastUpdateEpoch);
  const bool nightNow = isNightAtEpoch(now);
  if (!wasNight && nightNow && !state.isAsleep) {
    state.isAsleep = 1;
    state.sleepStartEpoch = now;
  } else if (wasNight && !nightNow && state.isAsleep) {
    state.isAsleep = 0;
    state.sleepStartEpoch = 0;
  }

  // Bound a manual daytime nap. The edge check above only ever wakes the pet at the
  // night-window's end, so a nap started outside that window (toggleLight() at noon) has
  // no edge to ever fire and would otherwise sleep indefinitely at slowed decay with free
  // energy recovery -- the same species of exploit as the earlier Discipline spam-fix. A
  // nap has a natural length; only applies while genuinely outside the night window, so
  // real night sleep (woken by the edge above) is untouched.
  if (state.isAsleep && !nightNow && (now - state.sleepStartEpoch) >= kManualNapDurationSeconds) {
    state.isAsleep = 0;
    state.sleepStartEpoch = 0;
  }

  const int32_t elapsed = now - state.lastUpdateEpoch;
  if (elapsed <= 0) {
    state.lastUpdateEpoch = now;
    return;
  }

  // Hunger/happiness decay slower while asleep; energy recovers instead of draining.
  const int32_t hungerDecaySeconds = state.isAsleep ? kHungerDecaySeconds * kAwakeSleepDecayDivisor : kHungerDecaySeconds;
  const int32_t happinessDecaySeconds =
      state.isAsleep ? kHappinessDecaySeconds * kAwakeSleepDecayDivisor : kHappinessDecaySeconds;
  const int32_t hungerLoss = elapsed / hungerDecaySeconds;
  const int32_t happinessLoss = elapsed / happinessDecaySeconds;
  if (hungerLoss > 0) state.hunger = clampToByte(static_cast<int32_t>(state.hunger) - hungerLoss);
  if (happinessLoss > 0) state.happiness = clampToByte(static_cast<int32_t>(state.happiness) - happinessLoss);

  if (state.isAsleep) {
    const int32_t energyGain = elapsed / kSleepEnergyRecoverSeconds;
    if (energyGain > 0) state.energy = clampToByte(static_cast<int32_t>(state.energy) + energyGain);
  } else {
    const int32_t energyLoss = elapsed / kEnergyDecaySeconds;
    if (energyLoss > 0) state.energy = clampToByte(static_cast<int32_t>(state.energy) - energyLoss);
  }

  const int32_t poopGain = elapsed / kPoopIntervalSeconds;
  if (poopGain > 0) {
    const int32_t next = static_cast<int32_t>(state.poopCount) + poopGain;
    state.poopCount = static_cast<uint8_t>(std::min<int32_t>(next, kMaxPoop));
  }

  state.lastUpdateEpoch = now;
  dirty = true;

  maybeGetSick(now);
  checkDeath(now);
  if (static_cast<Stage>(state.stage) != Stage::Dead) {
    // Real Tamagotchis don't call for attention while asleep -- an already-active call
    // (started before sleep began) still runs its normal expiry clock, unchanged.
    if (!state.isAsleep) maybeStartCall(now);
    maybeExpireCall(now);
    evolveIfReady(now);
  }
}

void TamagotchiActivity::feedMeal() {
  if (static_cast<Stage>(state.stage) == Stage::Egg || static_cast<Stage>(state.stage) == Stage::Dead) return;
  state.hunger = clampToByte(static_cast<int32_t>(state.hunger) + kMealGain);
  resolveCall(CallKind::Hunger);
  dirty = true;
}

void TamagotchiActivity::feedSnack() {
  if (static_cast<Stage>(state.stage) == Stage::Egg || static_cast<Stage>(state.stage) == Stage::Dead) return;
  state.hunger = clampToByte(static_cast<int32_t>(state.hunger) + kSnackGain);
  resolveCall(CallKind::Hunger);
  dirty = true;
}

void TamagotchiActivity::toggleLight() {
  if (static_cast<Stage>(state.stage) == Stage::Egg || static_cast<Stage>(state.stage) == Stage::Dead) return;
  if (state.isAsleep) {
    // Waking it up. Only a care mistake (plus an energy hit) if the night window hasn't
    // naturally ended yet -- matches the real Tamagotchi "don't disturb its rest"
    // mechanic. Waking it after the window has already ended (tick() would have
    // auto-woken it anyway) is free.
    if (isNightNow()) {
      if (state.careMistakes < 255) state.careMistakes++;
      state.energy = clampToByte(static_cast<int32_t>(state.energy) - kWakeEarlyEnergyPenalty);
    }
    state.isAsleep = 0;
    state.sleepStartEpoch = 0;
  } else {
    state.isAsleep = 1;
    state.sleepStartEpoch = nowEpoch();
    resolveCall(CallKind::Energy);
  }
  dirty = true;
}

void TamagotchiActivity::play() {
  if (static_cast<Stage>(state.stage) == Stage::Egg || static_cast<Stage>(state.stage) == Stage::Dead) return;
  state.happiness = clampToByte(static_cast<int32_t>(state.happiness) + kPlayGain);
  resolveCall(CallKind::Happiness);
  dirty = true;
}

void TamagotchiActivity::giveMedicine() {
  if (static_cast<Stage>(state.stage) == Stage::Egg || static_cast<Stage>(state.stage) == Stage::Dead) return;
  if (!state.sick) return;
  state.sick = 0;
  resolveCall(CallKind::Sick);
  dirty = true;
}

void TamagotchiActivity::clean() {
  if (static_cast<Stage>(state.stage) == Stage::Egg || static_cast<Stage>(state.stage) == Stage::Dead) return;
  if (state.poopCount == 0) return;
  state.poopCount = 0;
  state.sicknessGraceStartEpoch = 0;
  resolveCall(CallKind::Poop);
  dirty = true;
}

void TamagotchiActivity::discipline() {
  if (static_cast<Stage>(state.stage) == Stage::Egg || static_cast<Stage>(state.stage) == Stage::Dead) return;
  // Catch-all: scolding the pet settles it regardless of what it was calling about,
  // without granting the meter gain the "correct" icon would have, and it costs a
  // care mistake just like an ignored/expired call (see resolveAnyCall) -- so it's
  // a real fallback, not a free way to dodge every attention call cheaply.
  resolveAnyCall();
}

void TamagotchiActivity::onEnter() {
  Activity::onEnter();
  load();
  const int32_t now = nowEpoch();
  if (state.stageStartEpoch == 0) {
    state.stageStartEpoch = now;
    state.lastUpdateEpoch = now;
    dirty = true;
  }
  screen = Screen::Main;
  cursorIndex = 0;
  foodCursorIndex = 0;
  tick(now);
  requestUpdate();
}

void TamagotchiActivity::handleMainInput() {
  // Main holds no persistent selection -- Confirm has nothing to act on. Either nav
  // direction summons the Care Menu (Uni's press-to-reveal idiom); C still exits the
  // activity, handled centrally in loop() via Button::Back.
  navigator.onNextRelease([this] {
    screen = Screen::CareMenu;
    cursorIndex = 0;
    requestUpdate();
  });
  navigator.onPreviousRelease([this] {
    screen = Screen::CareMenu;
    cursorIndex = 0;
    requestUpdate();
  });
}

void TamagotchiActivity::handleCareMenuInput() {
  using Button = MappedInputManager::Button;

  navigator.onNextRelease([this] { cursorIndex = ButtonNavigator::nextIndex(cursorIndex, kIconCount); });
  navigator.onPreviousRelease([this] { cursorIndex = ButtonNavigator::previousIndex(cursorIndex, kIconCount); });

  if (mappedInput.wasReleased(Button::Confirm)) {
    dispatchIconAction(static_cast<Icon>(cursorIndex));
    requestUpdate();
  }
}

void TamagotchiActivity::dispatchIconAction(Icon icon) {
  switch (icon) {
    case Icon::Food:
      screen = Screen::FoodSubmenu;
      foodCursorIndex = 0;
      break;
    case Icon::Light: toggleLight(); screen = Screen::Main; break;
    case Icon::Play: play(); screen = Screen::Main; break;
    case Icon::Medicine: giveMedicine(); screen = Screen::Main; break;
    case Icon::Clean: clean(); screen = Screen::Main; break;
    case Icon::Status: screen = Screen::Status; break;
    case Icon::Discipline: discipline(); screen = Screen::Main; break;
    default: break;
  }
}

void TamagotchiActivity::handleFoodSubmenuInput() {
  using Button = MappedInputManager::Button;

  navigator.onNextRelease([this] { foodCursorIndex = ButtonNavigator::nextIndex(foodCursorIndex, 2); });
  navigator.onPreviousRelease([this] { foodCursorIndex = ButtonNavigator::previousIndex(foodCursorIndex, 2); });

  if (mappedInput.wasReleased(Button::Confirm)) {
    if (foodCursorIndex == 0) {
      feedMeal();
    } else {
      feedSnack();
    }
    screen = Screen::Main;
    requestUpdate();
  }
}

void TamagotchiActivity::handleStatusInput() {
  // Read-only screen: any C press backs out. Handled centrally in loop() via Button::Back.
}

bool TamagotchiActivity::handleTouch() {
  int tx = 0, ty = 0;
  if (!mappedInput.wasScreenTapped(tx, ty)) return false;

  // Tapping the pet itself on Screen::Main is equivalent to pressing A: it summons the
  // Care Menu, same as the reference article's press-to-reveal idiom.
  if (screen == Screen::Main && mappedInput.wasTapInRect(petRectX, petRectY, petRectSize, petRectSize)) {
    screen = Screen::CareMenu;
    cursorIndex = 0;
    requestUpdate();
    return true;
  }

  // Direct tile tap on Screen::CareMenu moves the cursor onto it (does not execute).
  if (screen == Screen::CareMenu) {
    for (int i = 0; i < kIconCount; ++i) {
      if (mappedInput.wasTapInRect(iconRectX[i], iconRectY[i], iconRectSize, iconRectSize)) {
        cursorIndex = i;
        requestUpdate();
        return true;
      }
    }
  }

  // The three on-screen A/B/C tap targets mirror the physical buttons exactly.
  for (int i = 0; i < kAbcCount; ++i) {
    if (mappedInput.wasTapInRect(abcRectX[i], abcRectY[i], abcRectW[i], abcRectH[i])) {
      if (i == 0) {
        // A: summon the Care Menu from Main, else cycle within it.
        if (screen == Screen::Main) {
          screen = Screen::CareMenu;
          cursorIndex = 0;
        } else if (screen == Screen::CareMenu) {
          cursorIndex = ButtonNavigator::nextIndex(cursorIndex, kIconCount);
        } else if (screen == Screen::FoodSubmenu) {
          foodCursorIndex = ButtonNavigator::nextIndex(foodCursorIndex, 2);
        }
      } else if (i == 1) {
        // B: confirm -- reuse the same dispatch as a physical Confirm release.
        if (screen == Screen::CareMenu) {
          dispatchIconAction(static_cast<Icon>(cursorIndex));
        } else if (screen == Screen::FoodSubmenu) {
          if (foodCursorIndex == 0) {
            feedMeal();
          } else {
            feedSnack();
          }
          screen = Screen::Main;
        }
      } else {
        // C: cancel -- back out of a submenu, or exit the activity from Main.
        if (screen == Screen::Main) {
          save();
          finish();
          return true;
        }
        screen = Screen::Main;
      }
      requestUpdate();
      return true;
    }
  }
  return false;
}

void TamagotchiActivity::loop() {
  using Button = MappedInputManager::Button;

  const int32_t now = nowEpoch();
  tick(now);

  const Stage stage = static_cast<Stage>(state.stage);

  if (stage == Stage::Dead) {
    int tx = 0, ty = 0;
    if (mappedInput.wasScreenTapped(tx, ty)) {
      restartFromEgg(now);
      requestUpdate();
    } else if (mappedInput.wasReleased(Button::Back)) {
      save();
      finish();
      return;
    }
    save();
    return;
  }

  if (stage == Stage::Egg) {
    hatchIfReady(now);
    if (mappedInput.wasReleased(Button::Back)) {
      save();
      finish();
      return;
    }
    save();
    requestUpdate();
    return;
  }

  if (handleTouch()) {
    save();
    return;
  }

  // C: cancel -- back out of a submenu, or exit the whole activity from Main. Top-level
  // Back always means "leave Tamagotchi", same as every other Activity in this codebase.
  if (mappedInput.wasReleased(Button::Back)) {
    if (screen == Screen::Main) {
      save();
      finish();
      return;
    }
    screen = Screen::Main;
    requestUpdate();
    save();
    return;
  }

  switch (screen) {
    case Screen::Main: handleMainInput(); break;
    case Screen::CareMenu: handleCareMenuInput(); break;
    case Screen::FoodSubmenu: handleFoodSubmenuInput(); break;
    case Screen::Status: handleStatusInput(); break;
  }

  save();
}

void TamagotchiActivity::drawCreature(int cx, int cy, int size) const {
  (void)size;  // ignored -- pet sprites are fixed-size placeholder art, see header note.
  const Stage stage = static_cast<Stage>(state.stage);
  const Sprite2bpp* sprite = &kPetBaby;
  switch (stage) {
    case Stage::Egg: sprite = &kPetEgg; break;
    case Stage::Baby: sprite = &kPetBaby; break;
    case Stage::Child: sprite = &kPetChild; break;
    case Stage::Adult: sprite = &kPetAdult; break;
    case Stage::Dead: sprite = &kPetDead; break;
  }
  const int spriteLeft = cx - sprite->w / 2;
  const int spriteTop = cy - sprite->h / 2;
  drawSprite(renderer, spriteLeft, spriteTop, *sprite);

  const int halfW = sprite->w / 2;
  const int halfH = sprite->h / 2;

  // Mess sitting next to the pet -- what Clean acts on.
  if (state.poopCount > 0) {
    constexpr int poopSize = 8;
    for (uint8_t i = 0; i < state.poopCount; ++i) {
      renderer.fillRoundedRect(cx + halfW + 4, cy + halfH - i * (poopSize + 3) - poopSize, poopSize, poopSize,
                                poopSize / 3, Color::Black);
    }
  }
  // Sickness indicator: a small cross over the sprite.
  if (state.sick) {
    constexpr int m = 6;
    renderer.drawLine(cx - m, cy - halfH - 6, cx + m, cy - halfH - 6, true);
    renderer.drawLine(cx, cy - halfH - 6 - m, cx, cy - halfH - 6 + m, true);
  }
  // Active attention call -- a blinking "!" would need frame timing this codebase's
  // e-ink refresh cadence doesn't cheaply support, so it's drawn steady instead.
  if (state.callActive) {
    renderer.drawText(UI_12_FONT_ID, cx - halfW - 14, cy - halfH - 10, "!", true, EpdFontFamily::BOLD);
  }
  // Asleep indicator -- same no-new-sprite overlay approach as the other icons above.
  if (state.isAsleep) {
    renderer.drawText(UI_12_FONT_ID, cx + halfW - 10, cy - halfH - 10, "Z", true, EpdFontFamily::BOLD);
  }
}

void TamagotchiActivity::drawHeartPips(int x, int y, uint8_t value) const {
  constexpr int kPips = 4;
  constexpr int kPipSize = 14;
  constexpr int kPipGap = 6;
  const int filled = (static_cast<int>(value) * kPips + 50) / 100;  // rounded, 0..4
  for (int i = 0; i < kPips; ++i) {
    const int px = x + i * (kPipSize + kPipGap);
    if (i < filled) {
      renderer.fillRoundedRect(px, y, kPipSize, kPipSize, 3, Color::Black);
    } else {
      renderer.drawRoundedRect(px, y, kPipSize, kPipSize, 1, 3, true);
    }
  }
}

const char* TamagotchiActivity::iconLabel(Icon icon) const {
  switch (icon) {
    case Icon::Food: return tr(STR_TAMA_ICON_FOOD);
    case Icon::Light: return tr(STR_TAMA_ICON_LIGHT);
    case Icon::Play: return tr(STR_TAMA_PLAY);
    case Icon::Medicine: return tr(STR_TAMA_ICON_MEDICINE);
    case Icon::Clean: return tr(STR_TAMA_ICON_CLEAN);
    case Icon::Status: return tr(STR_TAMA_ICON_STATUS);
    case Icon::Discipline: return tr(STR_TAMA_ICON_DISCIPLINE);
    default: return "";
  }
}

void TamagotchiActivity::drawCareMenu(int top, int bottom, int width) {
  static const Sprite2bpp* const kIcons[kIconCount] = {
      &kIconFood, &kIconLight, &kIconPlay, &kIconMedicine, &kIconClean, &kIconStatus, &kIconDiscipline,
  };

  const int left = (renderer.getScreenWidth() - width) / 2;
  constexpr int kCols = 4;
  const int rows = (kIconCount + kCols - 1) / kCols;
  const int cellW = width / kCols;
  const int cellH = std::max(1, (bottom - top) / rows);
  iconRectSize = std::min(cellW, cellH) - 16;

  for (int i = 0; i < kIconCount; ++i) {
    const int col = i % kCols;
    const int row = i / kCols;
    const int cellLeft = left + col * cellW;
    const int cellTop = top + row * cellH;
    iconRectX[i] = cellLeft + (cellW - iconRectSize) / 2;
    iconRectY[i] = cellTop + (cellH - iconRectSize) / 2;

    const bool selected = i == cursorIndex;
    const Sprite2bpp& icon = *kIcons[i];
    const int spriteX = iconRectX[i] + (iconRectSize - icon.w) / 2;
    const int spriteY = iconRectY[i] + (iconRectSize - icon.h) / 2;

    if (selected) {
      renderer.fillRoundedRect(iconRectX[i] - 4, iconRectY[i] - 4, iconRectSize + 8, iconRectSize + 8, 6,
                                Color::Black);
      drawSprite(renderer, spriteX, spriteY, icon, /*invert=*/true);
    } else {
      renderer.drawRoundedRect(iconRectX[i] - 4, iconRectY[i] - 4, iconRectSize + 8, iconRectSize + 8, 1, 6, true);
      drawSprite(renderer, spriteX, spriteY, icon);
    }

    const char* label = iconLabel(static_cast<Icon>(i));
    const int labelX = cellLeft + (cellW - renderer.getTextWidth(UI_12_FONT_ID, label)) / 2;
    renderer.drawText(UI_12_FONT_ID, labelX, iconRectY[i] + iconRectSize + 12, label, true);
  }
}

void TamagotchiActivity::drawAbcTargets(int top, int width, int height) {
  const int left = (renderer.getScreenWidth() - width) / 2;
  const int gap = 12;
  const int btnWidth = (width - gap * (kAbcCount - 1)) / kAbcCount;
  const char* labels[kAbcCount] = {tr(STR_TAMA_BTN_A), tr(STR_TAMA_BTN_B), tr(STR_TAMA_BTN_C)};

  for (int i = 0; i < kAbcCount; ++i) {
    abcRectX[i] = left + i * (btnWidth + gap);
    abcRectY[i] = top;
    abcRectW[i] = btnWidth;
    abcRectH[i] = height;
    renderer.drawRoundedRect(abcRectX[i], abcRectY[i], btnWidth, height, 1, 8, true);
    renderer.drawCenteredText(UI_12_FONT_ID, abcRectY[i] + height / 2 - 8, labels[i], true, EpdFontFamily::BOLD);
  }
}

void TamagotchiActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const auto& metrics = UITheme::getInstance().getMetrics();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_TAMA_TITLE));

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing;

  const Stage stage = static_cast<Stage>(state.stage);

  if (stage == Stage::Egg) {
    const int cx = pageWidth / 2;
    const int cy = contentTop + contentHeight / 2 - 16;
    drawCreature(cx, cy, 0);
    renderer.drawCenteredText(UI_12_FONT_ID, cy + kPetSpriteHalfH + 24, tr(STR_TAMA_TAP_TO_HATCH), true);
    renderer.displayBuffer(HalDisplay::FAST_REFRESH);
    return;
  }

  if (stage == Stage::Dead) {
    const int cx = pageWidth / 2;
    const int cy = contentTop + contentHeight / 2 - 16;
    drawCreature(cx, cy, 0);
    renderer.drawCenteredText(UI_12_FONT_ID, cy + kPetSpriteHalfH + 24, tr(STR_TAMA_DIED), true);
    renderer.drawCenteredText(UI_12_FONT_ID, cy + kPetSpriteHalfH + 44, tr(STR_TAMA_NEW_EGG), true);
    renderer.displayBuffer(HalDisplay::FAST_REFRESH);
    return;
  }

  if (screen == Screen::Status) {
    const int abcTop = pageHeight - metrics.buttonHintsHeight - metrics.verticalSpacing - 48;
    // Five rows (stage, 3 stats, health) spread evenly across the whole content area instead
    // of stacking near the top with fixed +=30/+=40 steps -- that left roughly 180px of empty
    // white below the health line with nothing else on the screen to fill it.
    constexpr int kStatusRowCount = 5;
    const int rowSpacing = (abcTop - contentTop) / (kStatusRowCount + 1);
    int y = contentTop + rowSpacing;

    const char* stageLabel = tr(STR_TAMA_STAGE_BABY);
    if (stage == Stage::Child) stageLabel = tr(STR_TAMA_STAGE_CHILD);
    else if (stage == Stage::Adult) stageLabel = tr(STR_TAMA_STAGE_ADULT);
    renderer.drawCenteredText(UI_12_FONT_ID, y, stageLabel, true, EpdFontFamily::BOLD);
    y += rowSpacing;

    const int labelX = (pageWidth - std::min(220, pageWidth - 40)) / 2;
    const int pipsX = labelX + 90;

    renderer.drawText(UI_12_FONT_ID, labelX, y + 2, tr(STR_TAMA_HUNGER), true);
    drawHeartPips(pipsX, y, state.hunger);
    y += rowSpacing;

    renderer.drawText(UI_12_FONT_ID, labelX, y + 2, tr(STR_TAMA_HAPPINESS), true);
    drawHeartPips(pipsX, y, state.happiness);
    y += rowSpacing;

    renderer.drawText(UI_12_FONT_ID, labelX, y + 2, tr(STR_TAMA_ENERGY), true);
    drawHeartPips(pipsX, y, state.energy);
    y += rowSpacing;

    renderer.drawCenteredText(UI_12_FONT_ID, y, state.sick ? tr(STR_TAMA_SICK) : tr(STR_TAMA_HEALTHY), true);

    drawAbcTargets(abcTop, pageWidth - 40, 40);
    renderer.displayBuffer(HalDisplay::FAST_REFRESH);
    return;
  }

  // Screen::Main / Screen::FoodSubmenu share the ABC row, but FoodSubmenu replaces the pet
  // frame + icon strip with a single full popup rather than trying to fit a small popup
  // alongside them -- the two layouts can then never collide with each other.
  const int abcTop = pageHeight - metrics.buttonHintsHeight - metrics.verticalSpacing - 48;
  drawAbcTargets(abcTop, pageWidth - 40, 40);
  const int playAreaHeight = abcTop - contentTop;

  if (screen == Screen::FoodSubmenu) {
    const int boxWidth = std::min(pageWidth - 40, 220);
    const int boxHeight = 90;
    const int boxLeft = (pageWidth - boxWidth) / 2;
    const int boxTop = contentTop + std::max(0, (playAreaHeight - boxHeight) / 2);
    renderer.fillRect(boxLeft, boxTop, boxWidth, boxHeight, false);
    renderer.drawRoundedRect(boxLeft, boxTop, boxWidth, boxHeight, 1, 8, true);

    const char* labels[2] = {tr(STR_TAMA_MEAL), tr(STR_TAMA_SNACK)};
    for (int i = 0; i < 2; ++i) {
      const int rowY = boxTop + 10 + i * 36;
      if (i == foodCursorIndex) {
        renderer.fillRoundedRect(boxLeft + 8, rowY - 4, boxWidth - 16, 30, 4, Color::Black);
        renderer.drawText(UI_12_FONT_ID, boxLeft + 16, rowY + 3, labels[i], false);
      } else {
        renderer.drawText(UI_12_FONT_ID, boxLeft + 16, rowY + 3, labels[i], true);
      }
    }
  } else if (screen == Screen::CareMenu) {
    drawCareMenu(contentTop, abcTop - metrics.verticalSpacing, pageWidth - 40);
  } else {
    // Screen::Main -- pet only, no permanent icon strip or frame. Tapping the pet is
    // equivalent to pressing A (see handleTouch()), so its hit-rect is recorded here.
    const int cx = pageWidth / 2;
    const int cy = contentTop + playAreaHeight / 2;
    drawCreature(cx, cy, 0);
    petRectX = cx - kPetSpriteHalfH;
    petRectY = cy - kPetSpriteHalfH;
    petRectSize = kPetSpriteHalfH * 2;
  }

  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}
