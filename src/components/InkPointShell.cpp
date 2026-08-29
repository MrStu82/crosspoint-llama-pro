#include "InkPointShell.h"

#include <GfxRenderer.h>
#include <HalPowerManager.h>

#include <cstdio>
#include <array>

#include "MappedInputManager.h"
#include "activities/Activity.h"
#include "activities/ActivityManager.h"
#include "fontIds.h"

namespace {
constexpr int kFooterX = 14;
constexpr int kTabWidth = 72;
constexpr int kTabGap = 4;

// Filled 8-tooth silhouette with exact horizontal/vertical symmetry. Keeping
// the vertices relative to the supplied centre prevents the Settings glyph
// from drifting within its existing footer button.
constexpr std::array<int, 32> kSettingsCogX = {
    -3, 3, 3, 6, 8, 12, 10, 11, 14, 14, 11, 10, 12, 8, 6, 3,
     3,-3,-3,-6,-8,-12,-10,-11,-14,-14,-11,-10,-12,-8,-6,-3};
constexpr std::array<int, 32> kSettingsCogY = {
    -12,-12,-9,-8,-10,-6,-4,-2,-2, 2, 2, 4, 6,10, 8, 9,
     12, 12, 9, 8, 10, 6, 4, 2, 2,-2,-2,-4,-6,-10,-8,-9};

int textTop(const GfxRenderer& renderer, const int fontId, const int baseline) {
  return baseline - renderer.getFontAscenderSize(fontId);
}

void drawCentered(const GfxRenderer& renderer, const int fontId, const int cx, const int baseline, const char* text,
                  const bool black = true) {
  renderer.drawText(fontId, cx - renderer.getTextWidth(fontId, text) / 2, textTop(renderer, fontId, baseline), text,
                    black);
}

void drawIcon(const GfxRenderer& r, const int index, const int cx, const int cy, const bool black) {
  if (index == 0) {
    r.drawLine(cx - 9, cy, cx, cy - 8, black); r.drawLine(cx, cy - 8, cx + 9, cy, black);
    r.drawRect(cx - 7, cy, 14, 10, black);
  } else if (index == 1) {
    r.drawRect(cx - 10, cy - 8, 5, 15, black); r.drawRect(cx - 3, cy - 8, 5, 15, black);
    r.drawRect(cx + 4, cy - 6, 5, 13, black); r.drawLine(cx - 11, cy + 9, cx + 11, cy + 9, black);
  } else if (index == 2) {
    r.drawRect(cx - 9, cy - 9, 18, 18, black); r.drawLine(cx - 9, cy, cx + 9, cy, black);
    r.drawLine(cx - 3, cy - 5, cx + 3, cy - 5, black); r.drawLine(cx - 3, cy + 4, cx + 3, cy + 4, black);
  } else if (index == 3) {
    r.drawRoundedRect(cx - 11, cy - 7, 22, 14, 1, 4, black);
    r.drawLine(cx - 7, cy, cx - 1, cy, black); r.drawLine(cx - 4, cy - 3, cx - 4, cy + 3, black);
    r.fillRect(cx + 4, cy - 2, 2, 2, black); r.fillRect(cx + 7, cy + 1, 2, 2, black);
  } else if (index == 4) {
    r.drawLine(cx - 10, cy - 4, cx + 7, cy - 4, black); r.drawLine(cx + 7, cy - 4, cx + 3, cy - 8, black);
    r.drawLine(cx + 10, cy + 4, cx - 7, cy + 4, black); r.drawLine(cx - 7, cy + 4, cx - 3, cy + 8, black);
  } else {
    int x[kSettingsCogX.size()] = {}, y[kSettingsCogY.size()] = {};
    for (size_t i = 0; i < kSettingsCogX.size(); ++i) {
      x[i] = cx + kSettingsCogX[i];
      y[i] = cy + kSettingsCogY[i];
    }
    r.fillPolygon(x, y, static_cast<int>(kSettingsCogX.size()), black);
    r.fillRoundedRect(cx - 4, cy - 4, 8, 8, 4,
                      black ? Color::White : Color::Black);
  }
}
}  // namespace

namespace InkPointShell {

bool enabled(const GfxRenderer& renderer) {
  return renderer.getScreenWidth() == 480 && renderer.getScreenHeight() == 800;
}

void drawHeader(const GfxRenderer& renderer, const char* title) {
  char battery[12];
  std::snprintf(battery, sizeof(battery), "%u%%", static_cast<unsigned>(powerManager.getBatteryPercentage()));
  renderer.drawText(SMALL_FONT_ID, 460 - renderer.getTextWidth(SMALL_FONT_ID, battery), 5, battery);
  // Caveat 30 has the same measured on-panel ink height as the approved 42px prototype token.
  // Drawn from y=23, its descenders reach y=94 (kHeaderBottom); kContentTop sits
  // clear of that, so no screen's content can collide with the heading.
  renderer.drawText(CAVEAT_30_FONT_ID, 20, 23, title);
}

void drawFooter(const GfxRenderer& renderer, const Destination active, const int focus) {
  static constexpr const char* labels[6] = {"Home", "Library", "Files", "Games", "Transfer", "Settings"};
  for (int i = 0; i < 6; ++i) {
    const int left = kFooterX + i * (kTabWidth + kTabGap);
    const bool selected = i == static_cast<int>(active);
    if (selected) renderer.fillRoundedRect(left, kFooterTop, kTabWidth, kFooterHeight, 7, Color::Black);
    else renderer.drawRoundedRect(left, kFooterTop, kTabWidth, kFooterHeight, 2, 7, true);
    const int cx = left + kTabWidth / 2;
    drawIcon(renderer, i, cx, kFooterTop + 19, !selected);
    drawCentered(renderer, SMALL_FONT_ID, cx, kFooterTop + 49, labels[i], !selected);
    if (focus == i && !selected)
      renderer.drawRoundedRect(left + 3, kFooterTop + 3, kTabWidth - 6, kFooterHeight - 6, 1, 5, true);
  }
}

std::optional<Destination> tappedDestination(const MappedInputManager& input) {
  int x = 0;
  int y = 0;
  if (!input.wasScreenTapped(x, y) || y < kFooterTop || y >= kFooterTop + kFooterHeight) return std::nullopt;
  for (int i = 0; i < 6; ++i) {
    const int left = kFooterX + i * (kTabWidth + kTabGap);
    if (x >= left && x < left + kTabWidth) return static_cast<Destination>(i);
  }
  return std::nullopt;
}

void navigate(const Destination destination) {
  switch (destination) {
    case Destination::Home: activityManager.goHome(); break;
    case Destination::Library: activityManager.goToRecentBooks(); break;
    case Destination::Files: activityManager.goToFileBrowser(); break;
    case Destination::Games: activityManager.goToGames(); break;
    case Destination::Transfer: activityManager.goToFileTransfer(); break;
    case Destination::Settings: activityManager.goToSettings(); break;
  }
}

}  // namespace InkPointShell
