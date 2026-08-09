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
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr const char* kStateFilePath = "/.crosspoint/tamagotchi.bin";
// Bumped for this rebuild's new State layout (poop/sickness/care-mistakes/attention-call
// fields replace careGoodSeconds) -- load() already resets to a fresh State{} on any
// version mismatch, so old saves are simply discarded rather than migrated.
constexpr uint8_t kStateFileVersion = 2;

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

constexpr uint8_t kMealGain = 40;
constexpr uint8_t kSnackGain = 15;
constexpr uint8_t kPlayGain = 25;
constexpr uint8_t kLightGain = 40;  // energy, via the Light icon (put pet to sleep/wake)

uint8_t clampToByte(int32_t value) {
  if (value < 0) return 0;
  if (value > 100) return 100;
  return static_cast<uint8_t>(value);
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
  Stage next = stage;
  if (stage == Stage::Baby && ageInStage >= kBabyToChildSeconds && state.careMistakes <= kMaxMistakesToEvolve) {
    next = Stage::Child;
  } else if (stage == Stage::Child && ageInStage >= kChildToAdultSeconds &&
             state.careMistakes <= kMaxMistakesToEvolve) {
    next = Stage::Adult;
  }
  if (next != stage) {
    state.stage = static_cast<uint8_t>(next);
    state.stageStartEpoch = now;
    state.careMistakes = 0;
    dirty = true;
  }
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
  dirty = true;
}

void TamagotchiActivity::resolveAnyCall() {
  if (!state.callActive) return;
  state.callActive = 0;
  state.lastCallEndEpoch = nowEpoch();
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

  const int32_t elapsed = now - state.lastUpdateEpoch;
  if (elapsed <= 0) {
    state.lastUpdateEpoch = now;
    return;
  }

  const int32_t hungerLoss = elapsed / kHungerDecaySeconds;
  const int32_t happinessLoss = elapsed / kHappinessDecaySeconds;
  const int32_t energyLoss = elapsed / kEnergyDecaySeconds;
  if (hungerLoss > 0) state.hunger = clampToByte(static_cast<int32_t>(state.hunger) - hungerLoss);
  if (happinessLoss > 0) state.happiness = clampToByte(static_cast<int32_t>(state.happiness) - happinessLoss);
  if (energyLoss > 0) state.energy = clampToByte(static_cast<int32_t>(state.energy) - energyLoss);

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
    maybeStartCall(now);
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
  state.energy = clampToByte(static_cast<int32_t>(state.energy) + kLightGain);
  resolveCall(CallKind::Energy);
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
  // without granting the meter gain the "correct" icon would have -- so it's a real
  // fallback, not a free way to dodge every attention call cheaply.
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
  using Button = MappedInputManager::Button;

  navigator.onNextRelease([this] { cursorIndex = ButtonNavigator::nextIndex(cursorIndex, kIconCount); });
  navigator.onPreviousRelease([this] { cursorIndex = ButtonNavigator::previousIndex(cursorIndex, kIconCount); });

  if (mappedInput.wasReleased(Button::Confirm)) {
    switch (static_cast<Icon>(cursorIndex)) {
      case Icon::Food:
        screen = Screen::FoodSubmenu;
        foodCursorIndex = 0;
        break;
      case Icon::Light: toggleLight(); break;
      case Icon::Play: play(); break;
      case Icon::Medicine: giveMedicine(); break;
      case Icon::Clean: clean(); break;
      case Icon::Status: screen = Screen::Status; break;
      case Icon::Discipline: discipline(); break;
      default: break;
    }
    requestUpdate();
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

  // Direct icon tap on Screen::Main moves the cursor onto it (does not execute).
  if (screen == Screen::Main) {
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
        // A: cycle forward.
        if (screen == Screen::Main) {
          cursorIndex = ButtonNavigator::nextIndex(cursorIndex, kIconCount);
        } else if (screen == Screen::FoodSubmenu) {
          foodCursorIndex = ButtonNavigator::nextIndex(foodCursorIndex, 2);
        }
      } else if (i == 1) {
        // B: confirm -- reuse the same dispatch as a physical Confirm release.
        if (screen == Screen::Main) {
          switch (static_cast<Icon>(cursorIndex)) {
            case Icon::Food:
              screen = Screen::FoodSubmenu;
              foodCursorIndex = 0;
              break;
            case Icon::Light: toggleLight(); break;
            case Icon::Play: play(); break;
            case Icon::Medicine: giveMedicine(); break;
            case Icon::Clean: clean(); break;
            case Icon::Status: screen = Screen::Status; break;
            case Icon::Discipline: discipline(); break;
            default: break;
          }
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
    case Screen::FoodSubmenu: handleFoodSubmenuInput(); break;
    case Screen::Status: handleStatusInput(); break;
  }

  save();
}

void TamagotchiActivity::drawCreature(int cx, int cy, int size) const {
  const Stage stage = static_cast<Stage>(state.stage);
  switch (stage) {
    case Stage::Egg: {
      // Egg: tall rounded rect.
      renderer.fillRoundedRect(cx - size / 3, cy - size / 2, size * 2 / 3, size, size / 3, Color::Black);
      break;
    }
    case Stage::Dead: {
      // Small headstone-ish rounded rect with a crossbar.
      renderer.drawRoundedRect(cx - size / 3, cy - size / 2, size * 2 / 3, size, 1, size / 6, true);
      renderer.drawLine(cx - size / 4, cy, cx + size / 4, cy, true);
      break;
    }
    default: {
      // Baby/Child/Adult: body scales up with stage, simple round-ish head via
      // fillRoundedRect (no circle primitive available in GfxRenderer).
      const int scale = 1 + static_cast<int>(stage);  // Baby=2, Child=3, Adult=4 (Stage enum values)
      const int bodySize = size * scale / 4;
      renderer.fillRoundedRect(cx - bodySize / 2, cy - bodySize / 2, bodySize, bodySize, bodySize / 3, Color::Black);
      // Eyes.
      const int eyeOffset = bodySize / 4;
      renderer.fillRect(cx - eyeOffset - 2, cy - bodySize / 6, 3, 3, true);
      renderer.fillRect(cx + eyeOffset - 1, cy - bodySize / 6, 3, 3, true);
      break;
    }
  }
  // Mess sitting next to the pet -- what Clean acts on.
  if (state.poopCount > 0) {
    const int poopSize = std::max(4, size / 10);
    for (uint8_t i = 0; i < state.poopCount; ++i) {
      renderer.fillRoundedRect(cx + size / 2 + 4, cy + size / 2 - i * (poopSize + 3) - poopSize, poopSize, poopSize,
                                poopSize / 3, Color::Black);
    }
  }
  // Sickness indicator: a small cross over the sprite.
  if (state.sick) {
    const int m = size / 6;
    renderer.drawLine(cx - m, cy - size / 2 - 6, cx + m, cy - size / 2 - 6, true);
    renderer.drawLine(cx, cy - size / 2 - 6 - m, cx, cy - size / 2 - 6 + m, true);
  }
  // Active attention call -- a blinking "!" would need frame timing this codebase's
  // e-ink refresh cadence doesn't cheaply support, so it's drawn steady instead.
  if (state.callActive) {
    renderer.drawText(UI_12_FONT_ID, cx - size / 2 - 14, cy - size / 2 - 10, "!", true, EpdFontFamily::BOLD);
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

void TamagotchiActivity::drawIconStrip(int top, int width) {
  const int left = (renderer.getScreenWidth() - width) / 2;
  iconRectSize = std::min(36, width / kIconCount - 4);
  const int gap = kIconCount > 1 ? (width - iconRectSize * kIconCount) / (kIconCount - 1) : 0;

  for (int i = 0; i < kIconCount; ++i) {
    iconRectX[i] = left + i * (iconRectSize + gap);
    iconRectY[i] = top;
    const bool selected = i == cursorIndex;
    if (selected) {
      renderer.fillRoundedRect(iconRectX[i] - 2, iconRectY[i] - 2, iconRectSize + 4, iconRectSize + 4, 6,
                                Color::Black);
      renderer.drawCenteredText(UI_12_FONT_ID, iconRectY[i] + iconRectSize + 4, iconLabel(static_cast<Icon>(i)),
                                 true, EpdFontFamily::BOLD);
    } else {
      renderer.drawRoundedRect(iconRectX[i], iconRectY[i], iconRectSize, iconRectSize, 1, 6, true);
    }
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
    const int frameSize = std::min(contentHeight * 3 / 5, pageWidth * 3 / 5);
    const int frameLeft = (pageWidth - frameSize) / 2;
    const int frameTop = contentTop + (contentHeight - frameSize) / 2;
    renderer.drawRoundedRect(frameLeft, frameTop, frameSize, frameSize, 1, 12, true);
    drawCreature(frameLeft + frameSize / 2, frameTop + frameSize / 2, frameSize * 2 / 3);
    renderer.drawCenteredText(UI_12_FONT_ID, frameTop + frameSize + 12, tr(STR_TAMA_TAP_TO_HATCH), true);
    renderer.displayBuffer(HalDisplay::FAST_REFRESH);
    return;
  }

  if (stage == Stage::Dead) {
    const int frameSize = std::min(contentHeight * 3 / 5, pageWidth * 3 / 5);
    const int frameLeft = (pageWidth - frameSize) / 2;
    const int frameTop = contentTop + (contentHeight - frameSize) / 2;
    renderer.drawRoundedRect(frameLeft, frameTop, frameSize, frameSize, 1, 12, true);
    drawCreature(frameLeft + frameSize / 2, frameTop + frameSize / 2, frameSize * 2 / 3);
    renderer.drawCenteredText(UI_12_FONT_ID, frameTop + frameSize + 12, tr(STR_TAMA_DIED), true);
    renderer.drawCenteredText(UI_12_FONT_ID, frameTop + frameSize + 32, tr(STR_TAMA_NEW_EGG), true);
    renderer.displayBuffer(HalDisplay::FAST_REFRESH);
    return;
  }

  if (screen == Screen::Status) {
    int y = contentTop + 8;
    const char* stageLabel = tr(STR_TAMA_STAGE_BABY);
    if (stage == Stage::Child) stageLabel = tr(STR_TAMA_STAGE_CHILD);
    else if (stage == Stage::Adult) stageLabel = tr(STR_TAMA_STAGE_ADULT);
    renderer.drawCenteredText(UI_12_FONT_ID, y, stageLabel, true, EpdFontFamily::BOLD);
    y += 30;

    const int labelX = (pageWidth - std::min(220, pageWidth - 40)) / 2;
    const int pipsX = labelX + 90;

    renderer.drawText(UI_12_FONT_ID, labelX, y + 2, tr(STR_TAMA_HUNGER), true);
    drawHeartPips(pipsX, y, state.hunger);
    y += 30;

    renderer.drawText(UI_12_FONT_ID, labelX, y + 2, tr(STR_TAMA_HAPPINESS), true);
    drawHeartPips(pipsX, y, state.happiness);
    y += 30;

    renderer.drawText(UI_12_FONT_ID, labelX, y + 2, tr(STR_TAMA_ENERGY), true);
    drawHeartPips(pipsX, y, state.energy);
    y += 40;

    renderer.drawCenteredText(UI_12_FONT_ID, y, state.sick ? tr(STR_TAMA_SICK) : tr(STR_TAMA_HEALTHY), true);

    drawAbcTargets(pageHeight - metrics.buttonHintsHeight - metrics.verticalSpacing - 48, pageWidth - 40, 40);
    renderer.displayBuffer(HalDisplay::FAST_REFRESH);
    return;
  }

  // Screen::Main / Screen::FoodSubmenu share the same pet-frame + icon-strip layout.
  const int frameSize = std::min(contentHeight * 2 / 5, pageWidth * 2 / 5);
  const int frameLeft = (pageWidth - frameSize) / 2;
  const int frameTop = contentTop;
  renderer.drawRoundedRect(frameLeft, frameTop, frameSize, frameSize, 1, 12, true);
  drawCreature(frameLeft + frameSize / 2, frameTop + frameSize / 2, frameSize * 2 / 3);

  const int stripTop = frameTop + frameSize + 26;
  const int stripWidth = std::min(pageWidth - 32, 260);
  drawIconStrip(stripTop, stripWidth);

  const int abcTop = pageHeight - metrics.buttonHintsHeight - metrics.verticalSpacing - 48;
  drawAbcTargets(abcTop, pageWidth - 40, 40);

  if (screen == Screen::FoodSubmenu) {
    // Small popup over the lower half of the play area: Meal / Snack.
    const int boxWidth = std::min(pageWidth - 40, 220);
    const int boxHeight = 90;
    const int boxLeft = (pageWidth - boxWidth) / 2;
    const int boxTop = abcTop - boxHeight - 12;
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
  }

  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}
