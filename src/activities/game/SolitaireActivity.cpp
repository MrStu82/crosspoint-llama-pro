#include "SolitaireActivity.h"

#include <Arduino.h>
#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>

#include "I18nKeys.h"
#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
bool rectContains(const Rect& r, int x, int y) {
  return x >= r.x && x < r.x + r.width && y >= r.y && y < r.y + r.height;
}
}  // namespace

const char* SolitaireActivity::suitLetter(uint8_t suit) {
  switch (suit) {
    case 0:
      return "S";
    case 1:
      return "H";
    case 2:
      return "D";
    case 3:
      return "C";
    default:
      return "?";
  }
}

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
  newGame();
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

  if (mappedInput.wasReleased(Button::Back)) {
    finish();
    return;
  }

  int tx = 0;
  int ty = 0;
  bool tapped = mappedInput.wasScreenTapped(tx, ty);

  if ((tapped && rectContains(newGameRect, tx, ty)) || mappedInput.wasPressed(Button::Confirm)) {
    newGame();
    requestUpdate();
    return;
  }

  if (won || !tapped) {
    return;
  }

  bool dbl = isDoubleTap(tx, ty);

  if (rectContains(stockRect, tx, ty)) {
    drawFromStock();
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
          requestUpdate();
          return;
        }
      }
      selSource = SelSource::Waste;
    }
    requestUpdate();
    return;
  }

  for (int f = 0; f < kFoundations; f++) {
    if (rectContains(foundationRect[f], tx, ty)) {
      if (selSource == SelSource::Waste && !waste.empty()) {
        Card c = waste.back();
        if (canPlaceOnFoundation(f, c)) {
          foundationTop[f] = c.rank;
          waste.pop_back();
          checkWin();
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
        }
      }
      clearSelection();
      requestUpdate();
      return;
    }
  }

  for (int col = 0; col < kColumns; col++) {
    if (!rectContains(columnRect[col], tx, ty)) {
      continue;
    }

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
      } else {
        clearSelection();
      }
    }
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
  char rbuf[4];
  const char* rl = rankLabel(card.rank, rbuf, sizeof(rbuf));
  char label[8];
  snprintf(label, sizeof(label), "%s%s", rl, suitLetter(card.suit));
  renderer.drawText(UI_10_FONT_ID, x + 4, y + 4, label, true);
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
  newGameRect = Rect{marginX + 2 * (cardW + gap), topRowY, cardW, cardH};

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

  // New game button
  renderer.drawRoundedRect(newGameRect.x, newGameRect.y, newGameRect.width, newGameRect.height, 1, 4, true);
  {
    const char* label = tr(STR_SOLITAIRE_NEW_GAME);
    int tw = renderer.getTextWidth(UI_10_FONT_ID, label);
    renderer.drawText(UI_10_FONT_ID, newGameRect.x + (newGameRect.width - tw) / 2, newGameRect.y + newGameRect.height / 2 - 6,
                      label, true);
  }

  // Foundations
  for (int f = 0; f < kFoundations; f++) {
    if (foundationTop[f] == 0) {
      drawEmptySlot(foundationRect[f].x, foundationRect[f].y, foundationRect[f].width, foundationRect[f].height,
                    suitLetter(static_cast<uint8_t>(f)));
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

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SOLITAIRE_NEW_GAME), "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
