#include "SolitaireActivity.h"

#include <Arduino.h>
#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>

#include "I18nKeys.h"
#include "MappedInputManager.h"
#include "SolitaireStore.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
bool rectContains(const Rect& r, int x, int y) {
  return x >= r.x && x < r.x + r.width && y >= r.y && y < r.y + r.height;
}

// Filled disc via fillRoundedRect: a square with cornerRadius == half its side
// rounds away every straight edge, leaving a circle. Reused below instead of
// reaching for a dedicated circle primitive that doesn't exist publicly.
void fillDisc(GfxRenderer& renderer, int cx, int cy, int r, Color color) {
  renderer.fillRoundedRect(cx - r, cy - r, 2 * r, 2 * r, r, color);
}

// Draws one suit's pip shape, solid black if `black` else solid white,
// inside a size x size box with top-left corner at (x, y). Factored out of
// drawSuitPip so the same shape can be stamped twice -- once black, once
// shrunk in white on top -- to produce a hollow outline for the red suits.
void drawSuitPipShape(GfxRenderer& renderer, int x, int y, int size, uint8_t suit, bool black) {
  const Color discColor = black ? Color::Black : Color::White;
  const int cx = x + size / 2;
  const int lobeR = size / 4;
  switch (suit) {
    case 1: {  // Hearts: two top lobes + downward-pointing triangle base.
      fillDisc(renderer, x + lobeR, y + lobeR, lobeR, discColor);
      fillDisc(renderer, x + size - lobeR, y + lobeR, lobeR, discColor);
      int xs[3] = {x, x + size, cx};
      int ys[3] = {y + lobeR, y + lobeR, y + size};
      renderer.fillPolygon(xs, ys, 3, black);
      break;
    }
    case 0: {  // Spades: upward-pointing triangle + two bottom lobes + stem.
      int xs[3] = {cx, x, x + size};
      int ys[3] = {y, y + size - lobeR, y + size - lobeR};
      renderer.fillPolygon(xs, ys, 3, black);
      fillDisc(renderer, x + lobeR, y + size - lobeR, lobeR, discColor);
      fillDisc(renderer, x + size - lobeR, y + size - lobeR, lobeR, discColor);
      renderer.fillRect(cx - 1, y + size - lobeR, 2, lobeR, black);
      break;
    }
    case 2: {  // Diamonds: rotated square (rhombus).
      int xs[4] = {cx, x + size, cx, x};
      int ys[4] = {y, y + size / 2, y + size, y + size / 2};
      renderer.fillPolygon(xs, ys, 4, black);
      break;
    }
    case 3:
    default: {  // Clubs: trefoil (three discs) + stem.
      fillDisc(renderer, cx, y + lobeR, lobeR, discColor);
      fillDisc(renderer, x + lobeR, y + size - lobeR, lobeR, discColor);
      fillDisc(renderer, x + size - lobeR, y + size - lobeR, lobeR, discColor);
      renderer.fillRect(cx - 1, y + size / 2, 2, size / 2, black);
      break;
    }
  }
}

// Basic monochrome suit pip, "even if basic" per the ask -- built from the
// existing primitive set (fillDisc, fillPolygon, fillRect), no new assets.
// Drawn inside a size x size box with top-left corner at (x, y). Hearts and
// Diamonds (the red suits) render hollow -- a solid black stamp of the shape
// with a shrunk white copy stamped on top, leaving a black outline ring --
// since there's no dedicated outline-polygon primitive to draw one directly.
// Spades and Clubs (black suits) render solid, matching a real deck.
void drawSuitPip(GfxRenderer& renderer, int x, int y, int size, uint8_t suit) {
  bool hollow = (suit == 1 || suit == 2);
  drawSuitPipShape(renderer, x, y, size, suit, true);
  if (hollow) {
    int inset = std::max(1, size / 6);
    drawSuitPipShape(renderer, x + inset, y + inset, size - 2 * inset, suit, false);
  }
}
}  // namespace

const char* SolitaireActivity::rankLabel(uint8_t rank, char* buf, size_t bufSize) {
  switch (rank) {
    case 1:
      return "A";
    case 11:
      return "J";
    case 12:
      return "Q";
    case 13:
      return "K";
    default:
      snprintf(buf, bufSize, "%d", rank);
      return buf;
  }
}

void SolitaireActivity::onEnter() {
  Activity::onEnter();
  if (!loadGame()) {
    newGame();
  }
  requestUpdate();
}

void SolitaireActivity::newGame() {
  stock.clear();
  waste.clear();
  for (int col = 0; col < kColumns; col++) {
    tableau[col].clear();
  }
  for (int f = 0; f < kFoundations; f++) {
    foundationTop[f] = 0;
  }
  clearSelection();
  won = false;

  std::vector<Card> deck;
  deck.reserve(52);
  for (uint8_t suit = 0; suit < 4; suit++) {
    for (uint8_t rank = 1; rank <= 13; rank++) {
      deck.push_back(Card{rank, suit, false});
    }
  }
  for (int i = static_cast<int>(deck.size()) - 1; i > 0; i--) {
    int j = static_cast<int>(random(i + 1));
    std::swap(deck[i], deck[j]);
  }

  int idx = 0;
  for (int col = 0; col < kColumns; col++) {
    tableau[col].reserve(col + 1);
    for (int row = 0; row <= col; row++) {
      Card c = deck[idx++];
      c.faceUp = (row == col);
      tableau[col].push_back(c);
    }
  }
  stock.reserve(deck.size() - idx);
  for (; idx < static_cast<int>(deck.size()); idx++) {
    Card c = deck[idx];
    c.faceUp = false;
    stock.push_back(c);
  }
  saveGame();
}

namespace {
template <typename Dst, typename Src>
Dst convertCard(const Src& c) {
  return Dst{c.rank, c.suit, c.faceUp};
}

template <typename DstPile, typename SrcPile>
void convertPile(DstPile& dst, const SrcPile& src) {
  dst.clear();
  dst.reserve(src.size());
  for (const auto& c : src) {
    dst.push_back(convertCard<typename DstPile::value_type>(c));
  }
}
}  // namespace

void SolitaireActivity::saveGame() const {
  SolitaireStore& store = SOLITAIRE_STORE;
  convertPile(store.stock, stock);
  convertPile(store.waste, waste);
  for (int col = 0; col < kColumns; col++) {
    convertPile(store.tableau[col], tableau[col]);
  }
  for (int f = 0; f < kFoundations; f++) {
    store.foundationTop[f] = foundationTop[f];
  }
  store.saveToFile();
}

bool SolitaireActivity::loadGame() {
  SolitaireStore& store = SOLITAIRE_STORE;
  if (!store.loadFromFile()) {
    return false;
  }
  convertPile(stock, store.stock);
  convertPile(waste, store.waste);
  for (int col = 0; col < kColumns; col++) {
    convertPile(tableau[col], store.tableau[col]);
  }
  for (int f = 0; f < kFoundations; f++) {
    foundationTop[f] = store.foundationTop[f];
  }
  clearSelection();
  won = false;
  checkWin();
  return true;
}

void SolitaireActivity::drawFromStock() {
  if (!stock.empty()) {
    int n = std::min(3, static_cast<int>(stock.size()));
    for (int i = 0; i < n; i++) {
      Card c = stock.back();
      stock.pop_back();
      c.faceUp = true;
      waste.push_back(c);
    }
  } else if (!waste.empty()) {
    stock.assign(waste.rbegin(), waste.rend());
    for (auto& c : stock) {
      c.faceUp = false;
    }
    waste.clear();
  }
  clearSelection();
  saveGame();
}

bool SolitaireActivity::canPlaceOnTableau(int col, const Card& card) const {
  if (col < 0 || col >= kColumns) {
    return false;
  }
  if (tableau[col].empty()) {
    return card.rank == 13;
  }
  const Card& top = tableau[col].back();
  if (!top.faceUp) {
    return false;
  }
  return isRed(card.suit) != isRed(top.suit) && card.rank == top.rank - 1;
}

bool SolitaireActivity::canPlaceOnFoundation(int foundationIdx, const Card& card) const {
  if (foundationIdx < 0 || foundationIdx >= kFoundations) {
    return false;
  }
  if (card.suit != foundationIdx) {
    return false;
  }
  if (foundationTop[foundationIdx] == 0) {
    return card.rank == 1;
  }
  return card.rank == foundationTop[foundationIdx] + 1;
}

void SolitaireActivity::flipTopIfNeeded(int col) {
  if (!tableau[col].empty() && !tableau[col].back().faceUp) {
    tableau[col].back().faceUp = true;
  }
}

void SolitaireActivity::clearSelection() {
  selSource = SelSource::None;
  selCol = -1;
  selRunIndex = -1;
}

int SolitaireActivity::hitTestColumnCard(int col, int tx, int ty) const {
  const auto& pile = tableau[col];
  const int n = static_cast<int>(pile.size());
  if (n == 0) {
    return -1;
  }
  if (tx < columnRect[col].x || tx >= columnRect[col].x + cardW) {
    return -1;
  }
  static constexpr int kMaxPile = 32;
  int ys[kMaxPile];
  int count = std::min(n, kMaxPile);
  int y = columnRect[col].y;
  for (int i = 0; i < count; i++) {
    ys[i] = y;
    y += pile[i].faceUp ? cardUpOffset : cardDownOffset;
  }
  for (int i = count - 1; i >= 0; i--) {
    int top = ys[i];
    int bottom = (i + 1 < count) ? ys[i + 1] : ys[i] + cardH;
    if (ty >= top && ty < bottom) {
      return i;
    }
  }
  return -1;
}

bool SolitaireActivity::isDoubleTap(int x, int y) {
  uint32_t now = millis();
  bool dbl = false;
  if (lastTapMs != 0 && (now - lastTapMs) <= kDoubleTapWindowMs) {
    int dx = x - lastTapX;
    int dy = y - lastTapY;
    if (dx * dx + dy * dy <= kDoubleTapMaxDist * kDoubleTapMaxDist) {
      dbl = true;
    }
  }
  if (dbl) {
    lastTapMs = 0;  // consume, so a triple-tap doesn't chain into another double
  } else {
    lastTapMs = now;
  }
  lastTapX = x;
  lastTapY = y;
  return dbl;
}

bool SolitaireActivity::checkWin() {
  for (int f = 0; f < kFoundations; f++) {
    if (foundationTop[f] != 13) {
      return false;
    }
  }
  won = true;
  return true;
}

void SolitaireActivity::loop() {
  using Button = MappedInputManager::Button;

  int tx = 0;
  int ty = 0;
  bool tapped = mappedInput.wasScreenTapped(tx, ty);

  // Menu overlay: while open, it owns Back (resumes instead of exiting the
  // activity) and every tap; nothing below this block runs.
  if (menuOpen) {
    if (mappedInput.wasReleased(Button::Back)) {
      menuOpen = false;
      forceFullRefresh = true;
      requestUpdate();
      return;
    }
    if (tapped) {
      if (rectContains(menuResumeRect, tx, ty)) {
        menuOpen = false;
      } else if (rectContains(menuNewGameRect, tx, ty)) {
        newGame();
        menuOpen = false;
      } else if (rectContains(menuExitRect, tx, ty)) {
        finish();
        return;
      }
      forceFullRefresh = true;
      requestUpdate();
    }
    return;
  }

  if (mappedInput.wasReleased(Button::Back)) {
    finish();
    return;
  }

  if ((tapped && rectContains(menuRect, tx, ty)) || mappedInput.wasPressed(Button::Confirm)) {
    menuOpen = true;
    forceFullRefresh = true;
    requestUpdate();
    return;
  }

  if (won || !tapped) {
    return;
  }

  bool dbl = isDoubleTap(tx, ty);

  if (rectContains(stockRect, tx, ty)) {
    drawFromStock();
    forceFullRefresh = true;
    requestUpdate();
    return;
  }

  if (rectContains(wasteRect, tx, ty)) {
    if (waste.empty()) {
      clearSelection();
    } else if (selSource == SelSource::Waste) {
      clearSelection();
    } else {
      if (selSource == SelSource::Tableau) {
        clearSelection();
      }
      if (dbl) {
        Card c = waste.back();
        if (canPlaceOnFoundation(c.suit, c)) {
          foundationTop[c.suit] = c.rank;
          waste.pop_back();
          clearSelection();
          checkWin();
          saveGame();
          forceFullRefresh = true;
          requestUpdate();
          return;
        }
      }
      selSource = SelSource::Waste;
    }
    forceFullRefresh = true;
    requestUpdate();
    return;
  }

  for (int f = 0; f < kFoundations; f++) {
    if (rectContains(foundationRect[f], tx, ty)) {
      bool moved = false;
      if (selSource == SelSource::Waste && !waste.empty()) {
        Card c = waste.back();
        if (canPlaceOnFoundation(f, c)) {
          foundationTop[f] = c.rank;
          waste.pop_back();
          checkWin();
          moved = true;
        }
      } else if (selSource == SelSource::Tableau && selCol >= 0 && !tableau[selCol].empty() &&
                 selRunIndex == static_cast<int>(tableau[selCol].size()) - 1) {
        // Only a single-card selection (the run start is the top card) can go to a foundation.
        Card c = tableau[selCol].back();
        if (canPlaceOnFoundation(f, c)) {
          foundationTop[f] = c.rank;
          tableau[selCol].pop_back();
          flipTopIfNeeded(selCol);
          checkWin();
          moved = true;
        }
      }
      clearSelection();
      if (moved) {
        saveGame();
      }
      forceFullRefresh = true;
      requestUpdate();
      return;
    }
  }

  for (int col = 0; col < kColumns; col++) {
    if (!rectContains(columnRect[col], tx, ty)) {
      continue;
    }

    bool moved = false;
    if (selSource == SelSource::None) {
      int hitIndex = hitTestColumnCard(col, tx, ty);
      if (hitIndex >= 0 && tableau[col][hitIndex].faceUp) {
        bool isTopCard = (hitIndex == static_cast<int>(tableau[col].size()) - 1);
        const Card& tapped = tableau[col][hitIndex];
        if (dbl && isTopCard && canPlaceOnFoundation(tapped.suit, tapped)) {
          foundationTop[tapped.suit] = tapped.rank;
          tableau[col].pop_back();
          flipTopIfNeeded(col);
          checkWin();
          moved = true;
        } else {
          selSource = SelSource::Tableau;
          selCol = col;
          selRunIndex = hitIndex;
        }
      }
    } else if (selSource == SelSource::Waste) {
      if (!waste.empty() && canPlaceOnTableau(col, waste.back())) {
        Card c = waste.back();
        waste.pop_back();
        tableau[col].push_back(c);
        checkWin();
        moved = true;
      }
      clearSelection();
    } else {  // SelSource::Tableau -- move the whole run from selRunIndex down
      if (selCol == col) {
        clearSelection();
      } else if (selCol >= 0 && selRunIndex >= 0 && selRunIndex < static_cast<int>(tableau[selCol].size()) &&
                 canPlaceOnTableau(col, tableau[selCol][selRunIndex])) {
        auto& src = tableau[selCol];
        auto& dst = tableau[col];
        dst.insert(dst.end(), src.begin() + selRunIndex, src.end());
        src.erase(src.begin() + selRunIndex, src.end());
        flipTopIfNeeded(selCol);
        checkWin();
        clearSelection();
        moved = true;
      } else {
        clearSelection();
      }
    }
    if (moved) {
      saveGame();
    }
    forceFullRefresh = true;
    requestUpdate();
    return;
  }
}

void SolitaireActivity::drawCardBack(int x, int y, int w, int h) const {
  renderer.fillRoundedRect(x, y, w, h, 4, Color::DarkGray);
  renderer.drawRoundedRect(x, y, w, h, 1, 4, true);
}

void SolitaireActivity::drawCardFace(int x, int y, int w, int h, const Card& card, bool highlighted) const {
  renderer.fillRoundedRect(x, y, w, h, 4, Color::White);
  renderer.drawRoundedRect(x, y, w, h, highlighted ? 2 : 1, 4, true);
  // Corner label is rank text + a small inline suit pip glyph (replaces the
  // old rank+letter text, e.g. "3H") so the suit reads as a real pip at a
  // glance instead of a letter abbreviation.
  char rbuf[4];
  const char* rl = rankLabel(card.rank, rbuf, sizeof(rbuf));
  renderer.drawText(UI_10_FONT_ID, x + 4, y + 4, rl, true);
  int labelPipSize = 10;
  int labelPipX = x + 4 + renderer.getTextWidth(UI_10_FONT_ID, rl) + 3;
  drawSuitPip(renderer, labelPipX, y + 5, labelPipSize, card.suit);

  int pipSize = std::min(w, h) / 3;
  drawSuitPip(renderer, x + w - pipSize - 4, y + h - pipSize - 4, pipSize, card.suit);
}

void SolitaireActivity::drawEmptySlot(int x, int y, int w, int h, const char* hint) const {
  renderer.drawRoundedRect(x, y, w, h, 1, 4, true);
  if (hint && hint[0]) {
    int tw = renderer.getTextWidth(UI_10_FONT_ID, hint);
    renderer.drawText(UI_10_FONT_ID, x + (w - tw) / 2, y + h / 2 - 6, hint, true);
  }
}

void SolitaireActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const auto& metrics = UITheme::getInstance().getMetrics();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_SOLITAIRE_TITLE));

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing;

  const int marginX = 6;
  const int gap = 4;
  cardW = (pageWidth - 2 * marginX - 6 * gap) / kColumns;
  cardH = cardW * 7 / 5;

  const int topRowY = contentTop;

  stockRect = Rect{marginX, topRowY, cardW, cardH};
  wasteRect = Rect{marginX + cardW + gap, topRowY, cardW, cardH};
  menuRect = Rect{marginX + 2 * (cardW + gap), topRowY, cardW, cardH};

  int rightEdge = pageWidth - marginX;
  for (int f = kFoundations - 1; f >= 0; f--) {
    int x = rightEdge - cardW;
    foundationRect[f] = Rect{x, topRowY, cardW, cardH};
    rightEdge = x - gap;
  }

  const int tableauY = topRowY + cardH + gap * 3;
  const int tableauH = std::max(0, contentHeight - (tableauY - contentTop));
  for (int col = 0; col < kColumns; col++) {
    columnRect[col] = Rect{marginX + col * (cardW + gap), tableauY, cardW, tableauH};
  }

  // Stock
  if (!stock.empty()) {
    drawCardBack(stockRect.x, stockRect.y, stockRect.width, stockRect.height);
  } else {
    drawEmptySlot(stockRect.x, stockRect.y, stockRect.width, stockRect.height, waste.empty() ? "" : "Redeal");
  }

  // Waste (draw-3 fan hint: up to 2 faint outlines behind the top card)
  if (waste.empty()) {
    drawEmptySlot(wasteRect.x, wasteRect.y, wasteRect.width, wasteRect.height, "");
  } else {
    int fanCount = std::min(3, static_cast<int>(waste.size()));
    for (int i = fanCount - 1; i >= 1; i--) {
      renderer.drawRoundedRect(wasteRect.x - i * 4, wasteRect.y, wasteRect.width, wasteRect.height, 1, 4, true);
    }
    drawCardFace(wasteRect.x, wasteRect.y, wasteRect.width, wasteRect.height, waste.back(),
                 selSource == SelSource::Waste);
  }

  // Menu button (was a direct "New Game" button) -- opens the Resume/New
  // Game/Exit overlay instead of acting immediately. Drawn as a simple
  // hamburger icon (three bars) since it's a corner-sized card slot, too
  // narrow for the old label text to sit comfortably.
  renderer.drawRoundedRect(menuRect.x, menuRect.y, menuRect.width, menuRect.height, 1, 4, true);
  {
    int barW = menuRect.width / 2;
    int barX = menuRect.x + (menuRect.width - barW) / 2;
    int barSpacing = std::max(6, menuRect.height / 6);
    int barY = menuRect.y + menuRect.height / 2 - barSpacing;
    for (int i = 0; i < 3; i++) {
      renderer.fillRect(barX, barY + i * barSpacing, barW, 2, true);
    }
  }

  // Foundations
  for (int f = 0; f < kFoundations; f++) {
    if (foundationTop[f] == 0) {
      drawEmptySlot(foundationRect[f].x, foundationRect[f].y, foundationRect[f].width, foundationRect[f].height, "");
      int pipSize = std::min(foundationRect[f].width, foundationRect[f].height) / 3;
      drawSuitPip(renderer, foundationRect[f].x + (foundationRect[f].width - pipSize) / 2,
                  foundationRect[f].y + (foundationRect[f].height - pipSize) / 2, pipSize,
                  static_cast<uint8_t>(f));
    } else {
      Card synthetic{foundationTop[f], static_cast<uint8_t>(f), true};
      drawCardFace(foundationRect[f].x, foundationRect[f].y, foundationRect[f].width, foundationRect[f].height,
                   synthetic, false);
    }
  }

  // Tableau
  cardDownOffset = std::max(6, cardH / 9);
  cardUpOffset = std::max(18, cardH / 3);
  for (int col = 0; col < kColumns; col++) {
    int y = columnRect[col].y;
    const auto& pile = tableau[col];
    for (size_t i = 0; i < pile.size(); i++) {
      bool inSelectedRun = selSource == SelSource::Tableau && selCol == col &&
                            static_cast<int>(i) >= selRunIndex;
      const Card& c = pile[i];
      if (c.faceUp) {
        drawCardFace(columnRect[col].x, y, cardW, cardH, c, inSelectedRun);
        y += cardUpOffset;
      } else {
        drawCardBack(columnRect[col].x, y, cardW, cardH);
        y += cardDownOffset;
      }
    }
    if (pile.empty()) {
      drawEmptySlot(columnRect[col].x, columnRect[col].y, cardW, cardH, "K");
    }
  }

  if (won) {
    const int boxW = pageWidth - 80;
    const int boxH = 100;
    const int boxX = (pageWidth - boxW) / 2;
    const int boxY = contentTop + (contentHeight - boxH) / 2;
    renderer.fillRoundedRect(boxX, boxY, boxW, boxH, 8, Color::White);
    renderer.drawRoundedRect(boxX, boxY, boxW, boxH, 2, 8, true);
    renderer.drawCenteredText(UI_12_FONT_ID, boxY + 24, tr(STR_SOLITAIRE_WON), true);
    renderer.drawCenteredText(UI_10_FONT_ID, boxY + 56, tr(STR_SOLITAIRE_TAP_NEW_GAME), true);
  }

  if (menuOpen) {
    const int boxW = pageWidth - 100;
    const int rowH = 44;
    const int boxH = rowH * 3 + 20;
    const int boxX = (pageWidth - boxW) / 2;
    const int boxY = contentTop + (contentHeight - boxH) / 2;
    renderer.fillRoundedRect(boxX, boxY, boxW, boxH, 8, Color::White);
    renderer.drawRoundedRect(boxX, boxY, boxW, boxH, 2, 8, true);

    menuResumeRect = Rect(boxX + 10, boxY + 10, boxW - 20, rowH - 8);
    menuNewGameRect = Rect(boxX + 10, boxY + 10 + rowH, boxW - 20, rowH - 8);
    menuExitRect = Rect(boxX + 10, boxY + 10 + rowH * 2, boxW - 20, rowH - 8);

    renderer.drawCenteredText(UI_12_FONT_ID, menuResumeRect.y + (menuResumeRect.height / 2) - 8,
                               tr(STR_RESUME), true);
    renderer.drawCenteredText(UI_12_FONT_ID, menuNewGameRect.y + (menuNewGameRect.height / 2) - 8,
                               tr(STR_SOLITAIRE_NEW_GAME), true);
    renderer.drawCenteredText(UI_12_FONT_ID, menuExitRect.y + (menuExitRect.height / 2) - 8,
                               tr(STR_EXIT), true);
    renderer.drawRect(menuResumeRect.x, menuResumeRect.y, menuResumeRect.width, menuResumeRect.height, true);
    renderer.drawRect(menuNewGameRect.x, menuNewGameRect.y, menuNewGameRect.width, menuNewGameRect.height, true);
    renderer.drawRect(menuExitRect.x, menuExitRect.y, menuExitRect.width, menuExitRect.height, true);
  }

  const auto labels =
      mappedInput.mapLabels(tr(STR_BACK), tr(STR_DM_HINT_MENU), "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  if (forceFullRefresh) {
    renderer.displayBuffer(HalDisplay::HALF_REFRESH);
    forceFullRefresh = false;
  } else {
    renderer.displayBuffer();
  }
}
