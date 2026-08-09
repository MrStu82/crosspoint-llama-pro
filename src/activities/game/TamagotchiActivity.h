#pragma once

#include <cstdint>

#include "activities/Activity.h"

class MappedInputManager;

// Bandai-style virtual pet: hatches from an egg, grows through life stages gated on
// elapsed real time + care quality, and dies from sustained neglect. Time is real RTC
// time (not millis()), so an 8-hour power-off produces a hungry/possibly-dead creature
// on wake -- see loadOrTick(). State persists to flash via HalStorage, same versioned
// blob idiom as StatsManager. Touch-only (tap icon buttons), no swipe surface. No
// engine/shared abstraction with the other games -- deliberately kept flat (YAGNI).
class TamagotchiActivity final : public Activity {
 public:
  explicit TamagotchiActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Tamagotchi", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool preventAutoSleep() override { return true; }

 private:
  enum class Stage : uint8_t { Egg = 0, Baby = 1, Child = 2, Adult = 3, Dead = 4 };

  // Byte-exact on-disk layout, version-prefixed (see save()/load()), same idiom as
  // StatsManager::GlobalStats.
  struct State {
    uint8_t stage = static_cast<uint8_t>(Stage::Egg);
    uint8_t hunger = 100;
    uint8_t happiness = 100;
    uint8_t energy = 100;
    uint32_t careGoodSeconds = 0;   // cumulative seconds all meters were healthy (>50); gates evolution
    int32_t stageStartEpoch = 0;    // epoch the current stage began (egg creation for Stage::Egg)
    int32_t lastUpdateEpoch = 0;    // epoch decay/evolution/death was last computed against
    int32_t neglectStartEpoch = 0;  // epoch a meter first hit 0, continuously; 0 = currently fine
  };

  State state;
  bool dirty = false;

  // Icon button hit-rects, recomputed each render() and read back by loop()'s touch
  // handling -- same pattern as TetrisActivity's bankRect.
  static constexpr int kActionCount = 3;  // Feed, Play, Sleep
  int actionRectX[kActionCount] = {};
  int actionRectY[kActionCount] = {};
  int actionRectSize = 0;

  void load();
  void save();

  // Current RTC-backed epoch seconds, or 0 if the clock is unset/unavailable (same
  // "unknown" convention as StatsManager::getCurrentDate()).
  static int32_t nowEpoch();

  // Applies real-elapsed-time decay/evolution/death since state.lastUpdateEpoch, then
  // updates it to `now`. No-ops if the clock is unavailable.
  void tick(int32_t now);

  void feed();
  void play();
  void sleep();
  void hatchIfReady(int32_t now);
  void evolveIfReady(int32_t now);
  void checkDeath(int32_t now);
  void restartFromEgg(int32_t now);

  void drawCreature(int cx, int cy, int size) const;
  void drawMeterBar(int x, int y, int width, int height, uint8_t value) const;
};
