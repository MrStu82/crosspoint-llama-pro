#pragma once

#include <cstdint>
#include <vector>

#include "activities/Activity.h"
#include "components/themes/BaseTheme.h"

class MappedInputManager;

// Klondike Solitaire, mimicking Windows Solitaire per Stuart's spec: draw-3
// stock/waste, 7 tableau columns, 4 suit foundations, red/black alternation,
// kings into empty columns. Touch is tap-only (no drag, matching the
// Tetris/Tamagotchi idiom -- dragging is a bad fit for e-ink): tap a card to
// select it, tap a destination pile to move it, double-tap a card to
// auto-send it to its foundation if legal. Tapping a face-up card partway
// down a tableau column selects it and the whole ordered run beneath it
// (always a valid alternating-colour descending sequence by construction --
// only legal placements ever build it), moving the entire run to the
// destination column in one tap-source/tap-destination pair. Only a
// single-card selection (the column's top card) can be sent to a foundation.
// Cards render as rank+suit-letter corner text only, no face art (1bpp
// panel). No engine/shared abstraction with the other games -- deliberately
// kept flat (YAGNI).
class SolitaireActivity final : public Activity {
 public:
  explicit SolitaireActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Solitaire", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool preventAutoSleep() override { return true; }

 private:
  struct Card {
    uint8_t rank = 0;   // 1=Ace .. 13=King, 0 = invalid/unused
    uint8_t suit = 0;   // 0=Spades, 1=Hearts, 2=Diamonds, 3=Clubs
    bool faceUp = false;
  };

  enum class SelSource : uint8_t { None, Waste, Tableau };

  static constexpr int kColumns = 7;
  static constexpr int kFoundations = 4;
  static constexpr uint32_t kDoubleTapWindowMs = 350;
  static constexpr int kDoubleTapMaxDist = 24;

  std::vector<Card> stock;
  std::vector<Card> waste;
  std::vector<Card> tableau[kColumns];
  uint8_t foundationTop[kFoundations] = {};  // 0 = empty, else highest rank placed

  SelSource selSource = SelSource::None;
  int selCol = -1;
  int selRunIndex = -1;  // index into tableau[selCol] where the selected run starts
  bool won = false;

  // Set on every interaction and consumed once by render(), which then asks
  // for a HALF_REFRESH instead of the default FAST_REFRESH -- mirrors
  // MinesweeperActivity's existing fix for the same e-ink ghosting complaint.
  bool forceFullRefresh = false;

  // Menu overlay (Resume/New Game/Exit) replacing the old direct "New Game"
  // button. Rendered in place, not a separate Activity -- avoids the
  // heap-alloc/lifecycle cost of pushing a new screen for three buttons.
  bool menuOpen = false;

  uint32_t lastTapMs = 0;
  int lastTapX = -1000;
  int lastTapY = -1000;

  // Hit-rects and card geometry, recomputed each render() and read back by loop()'s touch handling.
  Rect stockRect;
  Rect wasteRect;
  Rect menuRect;
  Rect menuResumeRect;
  Rect menuNewGameRect;
  Rect menuExitRect;
  Rect foundationRect[kFoundations];
  Rect columnRect[kColumns];
  int cardW = 0;
  int cardH = 0;
  int cardDownOffset = 0;  // vertical step for a face-down card in a cascade
  int cardUpOffset = 0;    // vertical step for a face-up card in a cascade

  static bool isRed(uint8_t suit) { return suit == 1 || suit == 2; }
  static const char* rankLabel(uint8_t rank, char* buf, size_t bufSize);

  void newGame();
  void saveGame() const;
  bool loadGame();
  void drawFromStock();
  bool canPlaceOnTableau(int col, const Card& card) const;
  bool canPlaceOnFoundation(int foundationIdx, const Card& card) const;
  void flipTopIfNeeded(int col);
  void clearSelection();
  bool isDoubleTap(int x, int y);
  bool checkWin();
  int hitTestColumnCard(int col, int tx, int ty) const;

  void drawCardBack(int x, int y, int w, int h) const;
  void drawCardFace(int x, int y, int w, int h, const Card& card, bool highlighted) const;
  void drawEmptySlot(int x, int y, int w, int h, const char* hint) const;
};
