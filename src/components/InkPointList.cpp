#include "InkPointList.h"

#include <GfxRenderer.h>
#include <HalClock.h>

#include <algorithm>
#include <cstdio>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "activities/Activity.h"
#include "activities/ActivityManager.h"
#include "fontIds.h"
#include "util/ButtonNavigator.h"

namespace InkPointList {
namespace {
constexpr int kHomeX = 432;
constexpr int kHomeY = 58;
constexpr int kHomeR = 24;
constexpr int kIconX = 38;
constexpr int kIconLabelX = 72;
constexpr int kPlainLabelX = 20;
constexpr int kRightEdge = 455;
constexpr int kDividerEnd = 460;
constexpr int kPagerY = 752;
constexpr int kPagerUpX = 120;
constexpr int kPagerDownX = 360;
constexpr int kPagerHitW = 64;
constexpr int kPagerHitH = 48;

void drawThickLine(const GfxRenderer& r, const int x1, const int y1, const int x2, const int y2) {
  r.drawLine(x1, y1, x2, y2, 2, true);
}

void drawRowIcon(const GfxRenderer& r, const Icon icon, const int cx, const int cy) {
  switch (icon) {
    case Icon::Display:  // panel
      r.drawRect(cx - 10, cy - 12, 21, 25, 2, true);
      drawThickLine(r, cx - 5, cy + 7, cx + 5, cy + 7);
      break;
    case Icon::Reader:  // open book
      drawThickLine(r, cx, cy - 8, cx, cy + 11);
      for (const int s : {-1, 1}) {
        drawThickLine(r, cx, cy - 8, cx + 13 * s, cy - 11);
        drawThickLine(r, cx + 13 * s, cy - 11, cx + 13 * s, cy + 8);
        drawThickLine(r, cx + 13 * s, cy + 8, cx, cy + 11);
      }
      break;
    case Icon::Controls:  // side rocker
      r.drawRoundedRect(cx - 6, cy - 13, 13, 27, 2, 5, true);
      drawThickLine(r, cx - 3, cy - 6, cx + 3, cy - 6);
      drawThickLine(r, cx - 3, cy + 6, cx + 3, cy + 6);
      break;
    case Icon::System:  // the footer Settings cog
      InkPointShell::drawDestinationIcon(r, InkPointShell::Destination::Settings, cx, cy);
      break;
    case Icon::None:
      break;
  }
}

void drawTriangle(const GfxRenderer& r, const int cx, const bool up) {
  const int s = up ? -1 : 1;
  const int baseY = kPagerY - 6 * s;
  const int tipY = kPagerY + 8 * s;
  drawThickLine(r, cx - 12, baseY, cx + 12, baseY);
  drawThickLine(r, cx - 12, baseY, cx, tipY);
  drawThickLine(r, cx + 12, baseY, cx, tipY);
}

int textTopCentred(const GfxRenderer& r, const int fontId, const int cy) { return cy - r.getLineHeight(fontId) / 2; }

bool inRect(const int x, const int y, const int left, const int top, const int w, const int h) {
  return x >= left && x < left + w && y >= top && y < top + h;
}
}  // namespace

bool active(const GfxRenderer& renderer) { return InkPointShell::enabled(renderer); }

int pageItemsOr(const GfxRenderer& renderer, const int fallback, const int pageItems) {
  return active(renderer) ? pageItems : fallback;
}

void draw(const GfxRenderer& renderer, const char* title, const Rows& rows, const char* statusCentre) {
  InkPointShell::drawHeader(renderer, title);
  char clock[9];
  if (halClock.isAvailable() &&
      halClock.formatTime(clock, sizeof(clock), SETTINGS.clockUtcOffsetQ, SETTINGS.clockFormat == 1)) {
    renderer.drawText(SMALL_FONT_ID, 20, 5, clock);
  }
  if (statusCentre != nullptr && statusCentre[0] != '\0') {
    const std::string text = renderer.truncatedText(SMALL_FONT_ID, statusCentre, 300);
    renderer.drawText(SMALL_FONT_ID, 240 - renderer.getTextWidth(SMALL_FONT_ID, text.c_str()) / 2, 5, text.c_str());
  }

  renderer.drawRoundedRect(kHomeX - kHomeR, kHomeY - kHomeR, kHomeR * 2, kHomeR * 2, 2, kHomeR, true);
  InkPointShell::drawDestinationIcon(renderer, InkPointShell::Destination::Home, kHomeX, kHomeY);

  const int perPage = std::max(1, rows.pageItems);
  const int pages = std::max(1, (rows.count + perPage - 1) / perPage);
  const int page = std::clamp(std::max(0, rows.selected) / perPage, 0, pages - 1);
  const int labelX = rows.icon ? kIconLabelX : kPlainLabelX;
  const int lh12 = renderer.getLineHeight(UI_12_FONT_ID);
  const int lhSmall = renderer.getLineHeight(SMALL_FONT_ID);

  for (int slot = 0; slot < perPage; ++slot) {
    const int index = page * perPage + slot;
    if (index >= rows.count) break;
    const int y = kTop + slot * kRowHeight;
    const int cy = y + kRowHeight / 2;

    if (rows.icon) drawRowIcon(renderer, rows.icon(index), kIconX, cy);

    const std::string value = rows.value ? rows.value(index) : std::string();
    int labelRight = kRightEdge - 22;
    if (value.empty()) {
      drawThickLine(renderer, kRightEdge - 9, cy - 9, kRightEdge, cy);
      drawThickLine(renderer, kRightEdge, cy, kRightEdge - 9, cy + 9);
    } else {
      const std::string shown = renderer.truncatedText(UI_12_FONT_ID, value.c_str(), 180);
      const int w = renderer.getTextWidth(UI_12_FONT_ID, shown.c_str());
      renderer.drawText(UI_12_FONT_ID, kRightEdge - w, textTopCentred(renderer, UI_12_FONT_ID, cy), shown.c_str());
      labelRight = kRightEdge - w - 16;
    }

    const std::string label = renderer.truncatedText(UI_12_FONT_ID, rows.label(index).c_str(), labelRight - labelX);
    const std::string subtitle = rows.subtitle ? rows.subtitle(index) : std::string();
    if (subtitle.empty()) {
      renderer.drawText(UI_12_FONT_ID, labelX, textTopCentred(renderer, UI_12_FONT_ID, cy), label.c_str());
    } else {
      const int top = cy - (lh12 + lhSmall) / 2;
      renderer.drawText(UI_12_FONT_ID, labelX, top, label.c_str());
      const std::string sub = renderer.truncatedText(SMALL_FONT_ID, subtitle.c_str(), labelRight - labelX);
      renderer.drawText(SMALL_FONT_ID, labelX, top + lh12, sub.c_str());
    }

    for (int x = labelX; x <= kDividerEnd; x += 4) renderer.drawPixel(x, y + kRowHeight - 1);
    if (index == rows.selected) renderer.drawRoundedRect(8, y + 6, 464, kRowHeight - 11, 2, 7, true);
  }

  char pager[16];
  std::snprintf(pager, sizeof(pager), "%d/%d", page + 1, pages);
  renderer.drawText(UI_12_FONT_ID, 240 - renderer.getTextWidth(UI_12_FONT_ID, pager) / 2,
                    textTopCentred(renderer, UI_12_FONT_ID, kPagerY), pager);
  drawTriangle(renderer, kPagerUpX, true);
  drawTriangle(renderer, kPagerDownX, false);
}

Touch touch(const MappedInputManager& input, int& selected, const int count, const int pageItems) {
  const int perPage = std::max(1, pageItems);
  int x = 0;
  int y = 0;
  const bool down = input.wasScreenTouchDown(x, y);
  if (!down && !input.wasScreenTapped(x, y)) return Touch::None;

  const int page = std::max(0, selected) / perPage;
  const int row = y >= kTop ? (y - kTop) / kRowHeight : -1;
  const int index = page * perPage + row;
  if (row >= 0 && row < perPage && index < count && x >= 0 && x < 480) {
    selected = index;
    return down ? Touch::Consumed : Touch::Activated;
  }
  if (down) return Touch::None;

  if (inRect(x, y, kHomeX - kHomeR, kHomeY - kHomeR, kHomeR * 2, kHomeR * 2)) {
    activityManager.goHome();
    return Touch::Consumed;
  }
  const int pages = std::max(1, (count + perPage - 1) / perPage);
  if (pages > 1 && inRect(x, y, kPagerUpX - kPagerHitW / 2, kPagerY - kPagerHitH / 2, kPagerHitW, kPagerHitH)) {
    selected = ButtonNavigator::previousPageIndex(std::max(0, selected), count, perPage);
    return Touch::Consumed;
  }
  if (pages > 1 && inRect(x, y, kPagerDownX - kPagerHitW / 2, kPagerY - kPagerHitH / 2, kPagerHitW, kPagerHitH)) {
    selected = ButtonNavigator::nextPageIndex(std::max(0, selected), count, perPage);
    return Touch::Consumed;
  }
  return Touch::None;
}

}  // namespace InkPointList
