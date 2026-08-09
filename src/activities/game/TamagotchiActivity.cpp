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
constexpr uint8_t kStateFileVersion = 1;

// Real-time decay/growth tuning. Deliberately generous vs. the original 90s
// hardware (which ran meters down over ~hours) so the behaviour is provable
// on a build/QA timescale, while still being "hours matter" per Stuart's
// 8-hours-off-is-hungry requirement.
constexpr int32_t kHungerDecaySeconds = 5 * 60;      // -1 hunger per 5 min
constexpr int32_t kHappinessDecaySeconds = 8 * 60;   // -1 happiness per 8 min
constexpr int32_t kEnergyDecaySeconds = 10 * 60;     // -1 energy per 10 min
constexpr int32_t kIncubationSeconds = 60;            // egg must sit this long before it can hatch
constexpr int32_t kBabyToChildSeconds = 30 * 60;      // min age in Baby stage
constexpr int32_t kChildToAdultSeconds = 60 * 60;     // min age in Child stage
constexpr uint32_t kCareGoodSecondsForEvolution = 15 * 60;  // cumulative "well cared for" time required
constexpr int32_t kNeglectDeathSeconds = 6 * 60 * 60;  // any meter pinned at 0 this long -> death

constexpr uint8_t kFeedGain = 30;
constexpr uint8_t kPlayGain = 25;
constexpr uint8_t kSleepGain = 40;

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
}

void TamagotchiActivity::hatchIfReady(int32_t now) {
  if (static_cast<Stage>(state.stage) != Stage::Egg) return;
  if (now - state.stageStartEpoch < kIncubationSeconds) return;
  int tx = 0, ty = 0;
  if (mappedInput.wasScreenTapped(tx, ty)) {
    state.stage = static_cast<uint8_t>(Stage::Baby);
    state.stageStartEpoch = now;
    state.hunger = state.happiness = state.energy = 100;
    state.careGoodSeconds = 0;
    dirty = true;
  }
}

void TamagotchiActivity::evolveIfReady(int32_t now) {
  const Stage stage = static_cast<Stage>(state.stage);
  const int32_t ageInStage = now - state.stageStartEpoch;
  Stage next = stage;
  if (stage == Stage::Baby && ageInStage >= kBabyToChildSeconds && state.careGoodSeconds >= kCareGoodSecondsForEvolution) {
    next = Stage::Child;
  } else if (stage == Stage::Child && ageInStage >= kChildToAdultSeconds &&
             state.careGoodSeconds >= kCareGoodSecondsForEvolution) {
    next = Stage::Adult;
  }
  if (next != stage) {
    state.stage = static_cast<uint8_t>(next);
    state.stageStartEpoch = now;
    state.careGoodSeconds = 0;
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

  if (state.hunger > 50 && state.happiness > 50 && state.energy > 50) {
    state.careGoodSeconds += static_cast<uint32_t>(elapsed);
  }

  state.lastUpdateEpoch = now;
  dirty = true;

  checkDeath(now);
  if (static_cast<Stage>(state.stage) != Stage::Dead) evolveIfReady(now);
}

void TamagotchiActivity::feed() {
  if (static_cast<Stage>(state.stage) == Stage::Egg || static_cast<Stage>(state.stage) == Stage::Dead) return;
  state.hunger = clampToByte(static_cast<int32_t>(state.hunger) + kFeedGain);
  dirty = true;
}

void TamagotchiActivity::play() {
  if (static_cast<Stage>(state.stage) == Stage::Egg || static_cast<Stage>(state.stage) == Stage::Dead) return;
  state.happiness = clampToByte(static_cast<int32_t>(state.happiness) + kPlayGain);
  dirty = true;
}

void TamagotchiActivity::sleep() {
  if (static_cast<Stage>(state.stage) == Stage::Egg || static_cast<Stage>(state.stage) == Stage::Dead) return;
  state.energy = clampToByte(static_cast<int32_t>(state.energy) + kSleepGain);
  dirty = true;
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
  tick(now);
  requestUpdate();
}

void TamagotchiActivity::loop() {
  using Button = MappedInputManager::Button;

  if (mappedInput.wasReleased(Button::Back)) {
    save();
    finish();
    return;
  }

  const int32_t now = nowEpoch();
  tick(now);

  const Stage stage = static_cast<Stage>(state.stage);

  if (stage == Stage::Dead) {
    int tx = 0, ty = 0;
    if (mappedInput.wasScreenTapped(tx, ty)) {
      restartFromEgg(now);
      requestUpdate();
    }
    save();
    return;
  }

  if (stage == Stage::Egg) {
    hatchIfReady(now);
    save();
    requestUpdate();
    return;
  }

  bool acted = false;
  for (int i = 0; i < kActionCount; ++i) {
    if (mappedInput.wasTapInRect(actionRectX[i], actionRectY[i], actionRectSize, actionRectSize)) {
      switch (i) {
        case 0: feed(); break;
        case 1: play(); break;
        case 2: sleep(); break;
        default: break;
      }
      acted = true;
      break;
    }
  }

  if (acted) requestUpdate();
  save();
}

void TamagotchiActivity::drawMeterBar(int x, int y, int width, int height, uint8_t value) const {
  renderer.drawRect(x, y, width, height, true);
  const int fillWidth = (width - 2) * value / 100;
  if (fillWidth > 0) renderer.fillRect(x + 1, y + 1, fillWidth, height - 2, true);
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
}

void TamagotchiActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const auto& metrics = UITheme::getInstance().getMetrics();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_TAMA_TITLE));

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing;

  // Bandai idiom: small sprite in a framed window, centered in the upper
  // portion of the content area.
  const int frameSize = std::min(contentHeight * 3 / 5, pageWidth * 3 / 5);
  const int frameLeft = (pageWidth - frameSize) / 2;
  const int frameTop = contentTop;
  renderer.drawRoundedRect(frameLeft, frameTop, frameSize, frameSize, 1, 12, true);
  drawCreature(frameLeft + frameSize / 2, frameTop + frameSize / 2, frameSize * 2 / 3);

  const Stage stage = static_cast<Stage>(state.stage);
  int textY = frameTop + frameSize + 8;

  if (stage == Stage::Egg) {
    renderer.drawCenteredText(UI_12_FONT_ID, textY, tr(STR_TAMA_TAP_TO_HATCH), true);
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer(HalDisplay::FAST_REFRESH);
    return;
  }

  if (stage == Stage::Dead) {
    renderer.drawCenteredText(UI_12_FONT_ID, textY, tr(STR_TAMA_DIED), true);
    renderer.drawCenteredText(UI_12_FONT_ID, textY + 20, tr(STR_TAMA_NEW_EGG), true);
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer(HalDisplay::FAST_REFRESH);
    return;
  }

  const char* stageLabel = tr(STR_TAMA_STAGE_BABY);
  if (stage == Stage::Child) stageLabel = tr(STR_TAMA_STAGE_CHILD);
  else if (stage == Stage::Adult) stageLabel = tr(STR_TAMA_STAGE_ADULT);
  renderer.drawCenteredText(UI_12_FONT_ID, textY, stageLabel, true);
  textY += 22;

  const int barWidth = frameSize;
  const int barLeft = frameLeft;
  const int barHeight = 10;
  const int barSpacing = 16;

  char buf[24];
  std::snprintf(buf, sizeof(buf), "%s", tr(STR_TAMA_HUNGER));
  renderer.drawText(UI_12_FONT_ID, barLeft, textY, buf, true);
  drawMeterBar(barLeft, textY + 14, barWidth, barHeight, state.hunger);
  textY += barSpacing + 14;

  std::snprintf(buf, sizeof(buf), "%s", tr(STR_TAMA_HAPPINESS));
  renderer.drawText(UI_12_FONT_ID, barLeft, textY, buf, true);
  drawMeterBar(barLeft, textY + 14, barWidth, barHeight, state.happiness);
  textY += barSpacing + 14;

  std::snprintf(buf, sizeof(buf), "%s", tr(STR_TAMA_ENERGY));
  renderer.drawText(UI_12_FONT_ID, barLeft, textY, buf, true);
  drawMeterBar(barLeft, textY + 14, barWidth, barHeight, state.energy);
  textY += barSpacing + 14 + 6;

  // Icon action buttons -- tap hit-rects, recomputed here and read back in loop().
  const char* actionLabels[kActionCount] = {tr(STR_TAMA_FEED), tr(STR_TAMA_PLAY), tr(STR_TAMA_SLEEP)};
  actionRectSize = std::min(48, barWidth / kActionCount - 8);
  const int totalActionsWidth = actionRectSize * kActionCount + 16 * (kActionCount - 1);
  const int actionsLeft = barLeft + (barWidth - totalActionsWidth) / 2;
  for (int i = 0; i < kActionCount; ++i) {
    actionRectX[i] = actionsLeft + i * (actionRectSize + 16);
    actionRectY[i] = textY;
    renderer.drawRoundedRect(actionRectX[i], actionRectY[i], actionRectSize, actionRectSize, 1, 8, true);
    renderer.drawCenteredText(UI_12_FONT_ID, actionRectY[i] + actionRectSize + 4, actionLabels[i], true);
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}
