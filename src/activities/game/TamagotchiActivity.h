#pragma once

#include <cstdint>

#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

class MappedInputManager;

// Bandai-style virtual pet: hatches from an egg, grows through life stages gated on
// elapsed real time + care quality, and dies from sustained neglect. Time is real RTC
// time (not millis()), so an 8-hour power-off produces a hungry/possibly-dead creature
// on wake -- see tick(). State persists to flash via HalStorage, same versioned blob
// idiom as StatsManager. No engine/shared abstraction with the other games --
// deliberately kept flat (YAGNI).
//
// Controls follow the Bandai Uni idiom: Main shows only the pet -- A (or a tap on the
// pet) summons the Care Menu overlay with a cursor over pictogram tiles, B confirms the
// highlighted one, C backs out to Main. Main's own C exits the activity. Touch is an
// additive input path -- the same three roles are also exposed as three large on-screen
// tap targets at the bottom of every screen -- never the only path.
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

  // Top-level icon strip, in on-screen (and cursor-cycle) order.
  enum class Icon : uint8_t { Food = 0, Light, Play, Medicine, Clean, Status, Discipline, Count };
  static constexpr int kIconCount = static_cast<int>(Icon::Count);

  // What an unanswered attention call is asking for -- determines which icon (or
  // Discipline as a catch-all) resolves it without counting as a care mistake.
  enum class CallKind : uint8_t { None = 0, Hunger, Happiness, Energy, Poop, Sick };

  // Which sub-screen is on-screen; A/B/C mean different things depending on this. Main is
  // the unified Home screen (pet + all stats + stage/health caption, formerly a separate
  // Status screen -- merged, see render()) -- Care Menu stays a distinct overlay.
  enum class Screen : uint8_t { Main = 0, CareMenu, FoodSubmenu };

  // Byte-exact on-disk layout, version-prefixed (see save()/load()), same idiom as
  // StatsManager::GlobalStats. UI-only fields (cursor, screen) are deliberately NOT in
  // here -- only pet-facts persist.
  struct State {
    uint8_t stage = static_cast<uint8_t>(Stage::Egg);
    uint8_t hunger = 100;
    uint8_t happiness = 100;
    uint8_t energy = 100;
    uint8_t poopCount = 0;               // 0..kMaxPoop; Clean resets to 0
    uint8_t sick = 0;                    // 0/1; Medicine cures
    uint8_t careMistakes = 0;            // unanswered attention calls this stage; gates evolution
    int32_t stageStartEpoch = 0;         // epoch the current stage began (egg creation for Stage::Egg)
    int32_t lastUpdateEpoch = 0;         // epoch decay/evolution/death was last computed against
    int32_t neglectStartEpoch = 0;       // epoch a meter first hit 0, continuously; 0 = currently fine
    int32_t sicknessGraceStartEpoch = 0; // epoch poop first pinned at max, continuously; 0 = currently fine
    uint8_t callActive = 0;              // 0/1
    uint8_t callKind = 0;                // CallKind
    int32_t callStartEpoch = 0;          // epoch the active call began
    int32_t lastCallEndEpoch = 0;        // epoch the last call ended (resolved or timed out); throttles new calls
    uint8_t isAsleep = 0;                // 0/1; auto-set by the night schedule, toggled early by Light
    int32_t sleepStartEpoch = 0;         // epoch sleep began (auto or manual); 0 while awake
    uint8_t disciplineLevel = 50;        // 0..100 care-quality stat; persists across stages, gates evolution
  };

  State state;
  bool dirty = false;

  // UI-only, not persisted.
  Screen screen = Screen::Main;
  int cursorIndex = 0;      // selected Icon on Screen::CareMenu
  int foodCursorIndex = 0;  // 0 = Meal, 1 = Snack, within Screen::FoodSubmenu
  ButtonNavigator navigator;
  // True when the pet is age-ready to evolve but disciplineLevel specifically is what's
  // blocking it (as opposed to careMistakes, or simply not being old enough yet).
  // Recomputed every real tick in evolveIfReady() -- not persisted, purely a UI signal.
  bool disciplineBlockedEvolve = false;

  // On-screen A/B/C tap-target hit-rects, recomputed each render() and read back by
  // loop()'s touch handling -- same pattern as BrightnessSheet/TetrisActivity.
  static constexpr int kAbcCount = 3;  // A(select), B(confirm), C(cancel)
  int abcRectX[kAbcCount] = {};
  int abcRectY[kAbcCount] = {};
  int abcRectW[kAbcCount] = {};
  int abcRectH[kAbcCount] = {};

  // Care Menu tile hit-rects (touch can also tap a tile directly, which both moves the
  // cursor onto it and is equivalent to pressing A that many times -- it does not
  // execute the icon; B/tap-B still confirms).
  int iconRectX[kIconCount] = {};
  int iconRectY[kIconCount] = {};
  int iconRectSize = 0;

  // Pet hit-rect on Screen::Main, recomputed each render() -- tapping the pet is
  // equivalent to pressing A.
  int petRectX = 0;
  int petRectY = 0;
  int petRectSize = 0;

  // Widest of the Home screen's 4 stat labels (Hunger/Energy/Joy/Discipline), measured
  // once in onEnter() so pip meters start a fixed gap after whichever label is longest
  // instead of a hardcoded offset that only fit the shortest one.
  int statLabelWidth = 0;

  void load();
  void save();

  // Current RTC-backed epoch seconds, or 0 if the clock is unset/unavailable (same
  // "unknown" convention as StatsManager::getCurrentDate()).
  static int32_t nowEpoch();
  // True if the current RTC time-of-day falls inside the night sleep window
  // [kNightStartHour, kNightEndHour). Returns false if the clock is unavailable (no
  // schedule to apply without a real clock).
  static bool isNightNow();

  // Applies real-elapsed-time decay/poop/evolution/death/attention-calls since
  // state.lastUpdateEpoch, then updates it to `now`. No-ops if the clock is unavailable.
  void tick(int32_t now);

  // Resolves the active attention call (if its kind matches) without a care mistake.
  void resolveCall(CallKind kind);
  // Unconditionally resolves any active call -- Discipline's catch-all.
  void resolveAnyCall();
  void maybeStartCall(int32_t now);
  void maybeExpireCall(int32_t now);
  void maybeGetSick(int32_t now);

  void feedMeal();
  void feedSnack();
  void toggleLight();
  void play();
  void giveMedicine();
  void clean();
  void discipline();

  void hatchIfReady(int32_t now);
  void evolveIfReady(int32_t now);
  void checkDeath(int32_t now);
  void restartFromEgg(int32_t now);

  // A/B/C handling, split by current Screen.
  void handleMainInput();
  void handleCareMenuInput();
  void handleFoodSubmenuInput();
  // Touch equivalents of the physical A/B/C roles + direct icon taps. Returns true if a
  // tap was consumed this frame.
  bool handleTouch();

  // Runs the effect of confirming `icon` from the Care Menu (feed/toggle/play/etc.),
  // including any screen transition it causes. Shared by physical B and touch-B so the
  // dispatch table exists exactly once.
  void dispatchIconAction(Icon icon);

  // `size` is accepted for call-site symmetry but ignored -- pet sprites are fixed-size
  // placeholder art (see TamagotchiSpriteData.h); a real converter-generated header can
  // swap the art in later without touching this signature.
  void drawCreature(int cx, int cy, int size) const;
  void drawHeartPips(int x, int y, uint8_t value) const;
  // Lays out the 7 care icon tiles (sprite pictograms, not vector glyphs) in a grid
  // between `top` and `bottom`, `width` wide, centered on screen.
  void drawCareMenu(int top, int bottom, int width);
  // `labels`, if non-null, overrides the default A/B/C hint text (Home screen shows verb
  // labels instead -- see render()); all other Tamagotchi screens pass nullptr.
  void drawAbcTargets(int top, int width, int height, const char* const* labels = nullptr);
  const char* iconLabel(Icon icon) const;
};
