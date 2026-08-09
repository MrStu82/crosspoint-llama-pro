#include "StatsActivity.h"

#include <Bitmap.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>

#include "FsHelpers.h"
#include "MappedInputManager.h"
#include "RecentBooksStore.h"
#include "StatsManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
// -1 means "not yet computed this boot session".
int s_cachedBookCount = -1;
}  // namespace

void StatsActivity::invalidateBookCountCache() { s_cachedBookCount = -1; }

int StatsActivity::countEpubsRecursively(const char* path) {
  int count = 0;
  auto root = Storage.open(path);
  if (!root || !root.isDirectory()) {
    if (root) root.close();
    return 0;
  }

  root.rewindDirectory();
  char name[500];
  for (auto file = root.openNextFile(); file; file = root.openNextFile()) {
    file.getName(name, sizeof(name));
    if (name[0] == '.' || strcmp(name, "System Volume Information") == 0) {
      file.close();
      continue;
    }

    if (file.isDirectory()) {
      std::string nextPath = std::string(path);
      if (nextPath.back() != '/') nextPath += "/";
      nextPath += name;
      count += countEpubsRecursively(nextPath.c_str());
    } else {
      std::string_view filename(name);
      if (FsHelpers::hasEpubExtension(filename) || FsHelpers::hasXtcExtension(filename) ||
          FsHelpers::hasTxtExtension(filename) || FsHelpers::hasMarkdownExtension(filename)) {
        count++;
      }
    }
    file.close();
  }
  root.close();
  return count;
}

void StatsActivity::onEnter() {
  Activity::onEnter();
  if (s_cachedBookCount < 0) {
    s_cachedBookCount = countEpubsRecursively("/");
  }
  totalBooksOnDevice = s_cachedBookCount;
  requestUpdate();
}

void StatsActivity::onExit() { Activity::onExit(); }

void StatsActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back) ||
      mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    finish();
    return;
  }
}

void StatsActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const GlobalStats& stats = READING_STATS.getStats();

  const int screenWidth = renderer.getScreenWidth();
  const auto& metrics = UITheme::getInstance().getMetrics();

  // Title
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, screenWidth, metrics.headerHeight}, tr(STR_READING_STATS));

  int yPos = metrics.topPadding + metrics.headerHeight + 14;
  const int sidePad = metrics.contentSidePadding;

  // --- TOP SECTION ---
  std::string currentBookTitle = tr(STR_STATS_NO_BOOK_OPEN);
  std::string currentBookAuthor = "";
  std::string coverBmpPath = "";
  std::string currentBookPath = "";
  int currentProgress = -1;

  const auto& recentBooks = RECENT_BOOKS.getBooks();
  if (!recentBooks.empty()) {
    const auto& book = recentBooks.front();
    currentBookTitle = book.title;
    currentBookAuthor = book.author;
    currentProgress = book.progressPercent;
    coverBmpPath = book.coverBmpPath;
    currentBookPath = book.path;
  }

  int coverW = 120;
  int coverH = 135;
  bool hasCover = false;

  if (!coverBmpPath.empty()) {
    const std::string thumbPath = UITheme::getCoverThumbPath(coverBmpPath, metrics.homeCoverHeight);
    HalFile file;
    if (Storage.openFileForRead("STATS", thumbPath, file)) {
      Bitmap bitmap(file);
      if (bitmap.parseHeaders() == BmpReaderError::Ok) {
        hasCover = true;
        coverW = (coverH * bitmap.getWidth()) / bitmap.getHeight();
        if (coverW > 160) coverW = 160;
        renderer.drawBitmap(bitmap, sidePad, yPos, coverW, coverH);
      }
    }
  }

  if (!hasCover) {
    renderer.drawRect(sidePad, yPos, coverW, coverH);
    renderer.drawText(UI_10_FONT_ID, sidePad + 10, yPos + coverH / 2, tr(STR_STATS_NO_COVER));
  }

  int rightTextX = sidePad + coverW + 20;
  int rightTextW = screenWidth - rightTextX - sidePad;

  renderer.drawText(UI_12_FONT_ID, rightTextX, yPos,
                    renderer.truncatedText(UI_12_FONT_ID, currentBookTitle.c_str(), rightTextW).c_str(), true,
                    EpdFontFamily::ITALIC);
  renderer.drawText(UI_10_FONT_ID, rightTextX, yPos + 25,
                    renderer.truncatedText(UI_10_FONT_ID, currentBookAuthor.c_str(), rightTextW).c_str());

  int currentChapterProgress = -1;

  // Note: currentProgress (whole-book %) comes only from BookProgressBadge via
  // RecentBooksStore above — progress.bin has no book-level percent field to fall back to.
  // data[6] here is byte 0 of the optional visibleTextOffset, not a percent; reading it as
  // one produced the "181%" bug. Only chapter progress (page/pageCount, data[2-5]) is
  // legitimately derivable from this file.
  if (FsHelpers::hasEpubExtension(currentBookPath)) {
    std::string cachePath = "/.crosspoint/epub_" + std::to_string(std::hash<std::string>{}(currentBookPath));
    HalFile f;
    if (Storage.openFileForRead("STATS", cachePath + "/progress.bin", f)) {
      uint8_t data[6];
      if (f.read(data, 6) >= 6) {
        int currentPage = data[2] | (data[3] << 8);
        int pageCount = data[4] | (data[5] << 8);
        if (pageCount > 0) {
          currentChapterProgress = (currentPage * 100) / pageCount;
        } else {
          currentChapterProgress = 0;
        }
      }
    }
  }

  // Badge cache miss: a book already in progress when BookProgressBadge shipped (or one
  // whose reader hasn't re-saved since) has no book_progress.bin yet. Approximate the
  // whole-book % from chapter progress rather than showing STR_STATS_UNKNOWN -- bounded
  // and tilde-marked as an estimate, since it ignores relative chapter sizes.
  bool bookProgressIsApprox = false;
  if (currentProgress < 0 && currentChapterProgress >= 0) {
    currentProgress = currentChapterProgress;
    bookProgressIsApprox = true;
  }

  int barY = yPos + 80;
  renderer.drawText(UI_10_FONT_ID, rightTextX, barY - 22, tr(STR_STATS_BOOK_PROGRESS));
  if (currentProgress >= 0) {
    char progStr[16];
    snprintf(progStr, sizeof(progStr), bookProgressIsApprox ? "~%d%%" : "%d%%", currentProgress);
    int progW = renderer.getTextWidth(UI_10_FONT_ID, progStr);
    renderer.drawText(UI_10_FONT_ID, rightTextX + rightTextW - progW, barY - 22, progStr);
    renderer.drawRect(rightTextX, barY, rightTextW, 12);
    if (currentProgress > 0) {
      renderer.fillRect(rightTextX, barY, (rightTextW * currentProgress) / 100, 12);
    }
  } else {
    renderer.drawText(UI_10_FONT_ID, rightTextX + rightTextW - 50, barY - 22, tr(STR_STATS_UNKNOWN));
  }

  barY += 45;
  renderer.drawText(UI_10_FONT_ID, rightTextX, barY - 22, tr(STR_STATS_CHAPTER_PROGRESS));
  if (currentChapterProgress >= 0) {
    char progStr[16];
    snprintf(progStr, sizeof(progStr), "%d%%", currentChapterProgress);
    int progW = renderer.getTextWidth(UI_10_FONT_ID, progStr);
    renderer.drawText(UI_10_FONT_ID, rightTextX + rightTextW - progW, barY - 22, progStr);
    renderer.drawRect(rightTextX, barY, rightTextW, 12);
    if (currentChapterProgress > 0) {
      renderer.fillRect(rightTextX, barY, (rightTextW * currentChapterProgress) / 100, 12);
    }
  } else {
    renderer.drawText(UI_10_FONT_ID, rightTextX + rightTextW - 50, barY - 20, tr(STR_STATS_UNKNOWN));
  }

  yPos += coverH + 12;
  renderer.drawLine(sidePad, yPos, screenWidth - sidePad, yPos, 2, true);
  yPos += 12;

  auto drawCentered = [&](int fontId, const char* text, int cx, int y,
                          EpdFontFamily::Style style = EpdFontFamily::REGULAR) {
    if (text[0] == '\0') return;
    int w = renderer.getTextWidth(fontId, text, style);
    renderer.drawText(fontId, cx - w / 2, y, text, true, style);
  };

  int colWidth = (screenWidth - (sidePad * 2)) / 3;

  // Redesigned per X4Pro-Firmware-Design-Proposal.md §4.5: the Today and All Time
  // blocks no longer need the old 3-column trio height, and the activity grid gains
  // a legend but no new rows — the page fits in fixed spacing without the old
  // scale-factor compression hack (see git history for that mechanism if a future
  // screen size ever needs it back).
  constexpr int kBlockValueOffset = 34;   // value baseline -> first label line
  constexpr int kBlockLabel2Offset = 52;  // value baseline -> second label line
  constexpr int kBlockDividerHeight = 65;
  const int sectionHeaderGap = 28;  // header strip -> block start
  const int blockGap = 60;          // block start -> next section header (unchanged blocks only)
  const int sectionGap = 8;         // extra breathing room after each block

  // Activity history grid: DAILY_HISTORY_SLOTS days as a 12-week contribution grid
  // (7 rows = days of the week, 12 columns = weeks), column-major so index 0 (oldest)
  // is the top of the leftmost column and index DAILY_HISTORY_SLOTS-1 (today) is the
  // bottom of the rightmost column. Per-cell day-number labels from the old single-row
  // strip are dropped — unreadable at this density; the shading alone (same 4 e-ink
  // levels as before) carries the information. §4.5 calls this grid already correct
  // and asks only for a legend explaining the 4 shading levels, added below it.
  static_assert(DAILY_HISTORY_SLOTS % 7 == 0, "activity grid assumes a whole number of 7-day columns");
  constexpr int kGridRows = 7;
  constexpr int kGridCols = DAILY_HISTORY_SLOTS / kGridRows;
  const int cellSize = 13;
  const int cellGap = 3;
  const int blockValueOffset = kBlockValueOffset;
  const int blockLabel2Offset = kBlockLabel2Offset;
  const int blockDividerHeight = kBlockDividerHeight;

  auto drawTrio = [&](int startY, const char* val1, const char* label1a, const char* label1b, const char* val2,
                      const char* label2a, const char* label2b, const char* val3, const char* label3a,
                      const char* label3b) {
    int cx1 = sidePad + colWidth / 2;
    int cx2 = sidePad + colWidth + colWidth / 2;
    int cx3 = sidePad + colWidth * 2 + colWidth / 2;

    drawCentered(UI_12_FONT_ID, val1, cx1, startY, EpdFontFamily::BOLD);
    drawCentered(UI_10_FONT_ID, label1a, cx1, startY + blockValueOffset);
    drawCentered(UI_10_FONT_ID, label1b, cx1, startY + blockLabel2Offset);

    renderer.drawLine(sidePad + colWidth, startY, sidePad + colWidth, startY + blockDividerHeight, 1, true);

    drawCentered(UI_12_FONT_ID, val2, cx2, startY, EpdFontFamily::BOLD);
    drawCentered(UI_10_FONT_ID, label2a, cx2, startY + blockValueOffset);
    drawCentered(UI_10_FONT_ID, label2b, cx2, startY + blockLabel2Offset);

    renderer.drawLine(sidePad + colWidth * 2, startY, sidePad + colWidth * 2, startY + blockDividerHeight, 1, true);

    drawCentered(UI_12_FONT_ID, val3, cx3, startY, EpdFontFamily::BOLD);
    drawCentered(UI_10_FONT_ID, label3a, cx3, startY + blockValueOffset);
    drawCentered(UI_10_FONT_ID, label3b, cx3, startY + blockLabel2Offset);
  };

  auto drawDuo = [&](int startY, const char* val1, const char* label1a, const char* label1b, const char* val2,
                     const char* label2a, const char* label2b) {
    int cx1 = sidePad + (screenWidth - sidePad * 2) / 4;
    int cx2 = sidePad + 3 * (screenWidth - sidePad * 2) / 4;

    drawCentered(UI_12_FONT_ID, val1, cx1, startY, EpdFontFamily::BOLD);
    drawCentered(UI_10_FONT_ID, label1a, cx1, startY + blockValueOffset);
    drawCentered(UI_10_FONT_ID, label1b, cx1, startY + blockLabel2Offset);

    renderer.drawLine(sidePad + (screenWidth - sidePad * 2) / 2, startY, sidePad + (screenWidth - sidePad * 2) / 2,
                      startY + blockDividerHeight, 1, true);

    drawCentered(UI_12_FONT_ID, val2, cx2, startY, EpdFontFamily::BOLD);
    drawCentered(UI_10_FONT_ID, label2a, cx2, startY + blockValueOffset);
    drawCentered(UI_10_FONT_ID, label2b, cx2, startY + blockLabel2Offset);
  };

  // Fetched once here so the week-column chart, the streak count, and the 12-week
  // grid (further down) all read from the same snapshot. getLast7DaysMinutes()
  // fills all DAILY_HISTORY_SLOTS entries oldest-first; today is always the last
  // slot, and its value is live (stats.readingTimeTodaySeconds/60), not archived.
  uint16_t history[DAILY_HISTORY_SLOTS];
  int historyDates[DAILY_HISTORY_SLOTS];
  READING_STATS.getLast7DaysMinutes(history, historyDates);

  // --- TODAY SECTION (redesigned per §4.5) ---
  renderer.fillRectDither(sidePad, yPos, screenWidth - sidePad * 2, 24, Color::LightGray);
  renderer.drawText(UI_10_FONT_ID, sidePad + 10, yPos + 3, tr(STR_STATS_TODAY));
  yPos += sectionHeaderGap;

  // Week columns: a 7-bar chart of the last 7 days (today last). Bar length encodes
  // minutes; today's bar is solid black, the other 6 are 50% dither (the same
  // Color::DarkGray level the 12-week grid already uses for a 15-44 minute day —
  // no new dither logic). A zero-minute day draws a thin baseline rule for that
  // column instead of leaving a blank gap, so "no data drawn" is never mistaken for
  // "no data collected". A dashed rule marks the week's own mean across all 7 days.
  constexpr int kWeekChartHeight = 40;
  constexpr int kWeekBarGapPx = 10;
  const int weekChartTop = yPos;
  const int weekBaselineY = weekChartTop + kWeekChartHeight;
  const int weekChartW = screenWidth - sidePad * 2;
  const int weekBarSlot = weekChartW / 7;
  const int weekBarWidth = weekBarSlot - kWeekBarGapPx;

  uint16_t week[7];
  for (int i = 0; i < 7; i++) week[i] = history[DAILY_HISTORY_SLOTS - 7 + i];

  int weekMax = 30;  // floor so a quiet week doesn't make every bar look maxed out
  int weekSum = 0;
  for (int i = 0; i < 7; i++) {
    if (week[i] > weekMax) weekMax = week[i];
    weekSum += week[i];
  }
  const int weekMean = weekSum / 7;

  for (int i = 0; i < 7; i++) {
    const int barX = sidePad + i * weekBarSlot + kWeekBarGapPx / 2;
    const bool isToday = (i == 6);
    if (week[i] == 0) {
      renderer.drawLine(barX, weekBaselineY, barX + weekBarWidth, weekBaselineY, 2, true);
    } else {
      int barH = (week[i] * kWeekChartHeight) / weekMax;
      if (barH < 2) barH = 2;
      renderer.fillRectDither(barX, weekBaselineY - barH, weekBarWidth, barH, isToday ? Color::Black : Color::DarkGray);
    }
  }

  if (weekMean > 0) {
    const int meanY = weekBaselineY - (weekMean * kWeekChartHeight) / weekMax;
    constexpr int kDashLen = 6;
    constexpr int kDashGap = 4;
    for (int dx = sidePad; dx < sidePad + weekChartW; dx += kDashLen + kDashGap) {
      const int segEnd = std::min(dx + kDashLen, sidePad + weekChartW);
      renderer.drawLine(dx, meanY, segEnd, meanY, 1, true);
    }
  }

  yPos = weekBaselineY + 8;

  // One number, large: minutes read today, against a fixed daily goal bar. There is
  // no existing reading-goal setting in CrossPointSettings to read from, so this
  // uses a fixed constant — flagged as a judgment call the doc doesn't specify.
  // Pages-per-minute is dropped (least glanceable per §4.5) in favor of a streak
  // count, computed by walking history[] backward from today with no new
  // persistence. The codebase has no 52px/scalable-text primitive (drawText has no
  // scale param, no large font asset exists) — NOTOSANS_18_FONT_ID bold is the
  // largest available font and stands in for the doc's "52px" figure.
  constexpr int kGoalMinutesPerDay = 30;
  const int minutesToday = static_cast<int>(stats.readingTimeTodaySeconds / 60);

  int streak = 0;
  for (int i = DAILY_HISTORY_SLOTS - 1; i >= 0; i--) {
    if (historyDates[i] == 0 || history[i] == 0) break;
    streak++;
  }

  const int leftColW = (screenWidth - sidePad * 2) * 3 / 5;
  const int leftColCx = sidePad + leftColW / 2;
  const int rightColX = sidePad + leftColW;
  const int rightColCx = rightColX + (screenWidth - sidePad * 2 - leftColW) / 2;

  const int bigNumY = yPos + 5;
  char bigNumStr[16];
  snprintf(bigNumStr, sizeof(bigNumStr), "%d", minutesToday);
  drawCentered(NOTOSANS_18_FONT_ID, bigNumStr, leftColCx, bigNumY, EpdFontFamily::BOLD);
  const int bigNumBottom = bigNumY + 26;
  drawCentered(UI_10_FONT_ID, tr(STR_STATS_MINUTES), leftColCx, bigNumBottom);

  const int goalBarY = bigNumBottom + 14;
  const int goalBarX = sidePad + 10;
  const int goalBarW = leftColW - 20;
  int goalPercent = kGoalMinutesPerDay > 0 ? (minutesToday * 100) / kGoalMinutesPerDay : 0;
  if (goalPercent > 100) goalPercent = 100;
  renderer.drawRect(goalBarX, goalBarY, goalBarW, 12);
  if (goalPercent > 0) renderer.fillRect(goalBarX, goalBarY, (goalBarW * goalPercent) / 100, 12);
  char goalStr[48];
  snprintf(goalStr, sizeof(goalStr), "%s: %d/%d %s", tr(STR_STATS_GOAL), minutesToday, kGoalMinutesPerDay,
           tr(STR_STATS_MIN));
  drawCentered(UI_10_FONT_ID, goalStr, leftColCx, goalBarY + 18);

  const int todayBlockBottom = goalBarY + 18 + 4;
  renderer.drawLine(rightColX, bigNumY - 20, rightColX, todayBlockBottom, 1, true);

  char streakStr[16];
  snprintf(streakStr, sizeof(streakStr), "%d", streak);
  drawCentered(UI_12_FONT_ID, streakStr, rightColCx, bigNumY, EpdFontFamily::BOLD);
  drawCentered(UI_10_FONT_ID, tr(STR_STATS_STREAK), rightColCx, bigNumY + blockValueOffset);

  yPos = todayBlockBottom + sectionGap;

  // --- ALL TIME SECTION (compressed to one strip per §4.5 — "reference data, nobody
  // opens this page for it") ---
  renderer.fillRectDither(sidePad, yPos, screenWidth - sidePad * 2, 24, Color::LightGray);
  renderer.drawText(UI_10_FONT_ID, sidePad + 10, yPos + 3, tr(STR_STATS_ALL_TIME));
  yPos += sectionHeaderGap;

  char a_val1[16];
  snprintf(a_val1, sizeof(a_val1), "%.1f", stats.totalReadingTimeSeconds / 3600.0f);
  char a_val2[16];
  snprintf(a_val2, sizeof(a_val2), "%lu", static_cast<unsigned long>(stats.totalPagesRead));
  float ppmAll = 0;
  if (stats.totalReadingTimeSeconds > 60)
    ppmAll = static_cast<float>(stats.totalPagesRead) / (stats.totalReadingTimeSeconds / 60.0f);
  char a_val3[16];
  snprintf(a_val3, sizeof(a_val3), "%.1f", ppmAll);

  char allTimeLine[128];
  snprintf(allTimeLine, sizeof(allTimeLine), "%s %s    %s %s    %s %s", a_val1, tr(STR_STATS_HOURS), a_val2,
           tr(STR_STATS_PAGES), a_val3, tr(STR_STATS_PAGES_PER_MIN));
  drawCentered(UI_12_FONT_ID, allTimeLine, screenWidth / 2, yPos, EpdFontFamily::BOLD);
  yPos += 24;

  yPos += sectionGap;

  // --- ALL ITEMS SECTION ---
  renderer.fillRectDither(sidePad, yPos, screenWidth - sidePad * 2, 24, Color::LightGray);
  renderer.drawText(UI_10_FONT_ID, sidePad + 10, yPos + 3, tr(STR_STATS_ALL_ITEMS));
  yPos += sectionHeaderGap;

  char i_val1[16];
  snprintf(i_val1, sizeof(i_val1), "%lu", static_cast<unsigned long>(stats.booksFinished));
  char i_val2[16];
  snprintf(i_val2, sizeof(i_val2), "%d", totalBooksOnDevice);

  drawDuo(yPos, i_val1, tr(STR_STATS_BOOKS), tr(STR_STATS_FINISHED), i_val2, tr(STR_STATS_TOTAL), tr(STR_STATS_BOOKS));

  yPos += blockGap;

  yPos += sectionGap;

  // --- ACTIVITY HISTORY SECTION ---
  // 12-week contribution grid: one cell per day, column-major (oldest at top-left,
  // today at bottom-right), shaded to one of the four e-ink levels the display can
  // actually produce (solid white / 25% dither LightGray / 50% checkerboard DarkGray /
  // solid black) — see fillRectDither()'s Color cases. A day with an unknown date (RTC
  // was unset that far back) and a day with zero minutes read are visually
  // indistinguishable (both plain white) — acceptable at this density, unlike the old
  // single-row strip which could afford a per-cell "-" label for the unknown case.
  renderer.fillRectDither(sidePad, yPos, screenWidth - sidePad * 2, 24, Color::LightGray);
  renderer.drawText(UI_10_FONT_ID, sidePad + 10, yPos + 3, tr(STR_STATS_ACTIVITY_HISTORY));
  yPos += sectionHeaderGap;

  // history/historyDates already fetched once above, shared with the week chart
  // and streak count.
  const int gridW = kGridCols * cellSize + (kGridCols - 1) * cellGap;
  const int gridX = sidePad + (screenWidth - sidePad * 2 - gridW) / 2;

  for (int i = 0; i < DAILY_HISTORY_SLOTS; i++) {
    const int col = i / kGridRows;
    const int row = i % kGridRows;
    const int cellX = gridX + col * (cellSize + cellGap);
    const int cellY = yPos + row * (cellSize + cellGap);

    Color level;
    if (historyDates[i] == 0 || history[i] == 0) {
      level = Color::White;
    } else if (history[i] < 15) {
      level = Color::LightGray;
    } else if (history[i] < 45) {
      level = Color::DarkGray;
    } else {
      level = Color::Black;
    }

    renderer.fillRectDither(cellX, cellY, cellSize, cellSize, level);
    renderer.drawRect(cellX, cellY, cellSize, cellSize);
  }

  yPos += kGridRows * cellSize + (kGridRows - 1) * cellGap;

  // Legend for the grid's four shading levels — the doc's only required addition to
  // this section, since the grid itself already draws the same 4 e-ink levels used
  // above (no new dither logic).
  yPos += 8;
  struct LegendItem {
    Color color;
    StrId label;
  };
  const LegendItem legendItems[4] = {
      {Color::White, StrId::STR_STATS_LEGEND_NONE},
      {Color::LightGray, StrId::STR_STATS_LEGEND_LOW},
      {Color::DarkGray, StrId::STR_STATS_LEGEND_MED},
      {Color::Black, StrId::STR_STATS_LEGEND_HIGH},
  };
  constexpr int kLegendSwatch = 10;
  int legendX = gridX;
  for (const auto& item : legendItems) {
    renderer.fillRectDither(legendX, yPos, kLegendSwatch, kLegendSwatch, item.color);
    renderer.drawRect(legendX, yPos, kLegendSwatch, kLegendSwatch);
    legendX += kLegendSwatch + 5;
    const char* label = I18n::getInstance().get(item.label);
    renderer.drawText(UI_10_FONT_ID, legendX, yPos - 2, label);
    legendX += renderer.getTextWidth(UI_10_FONT_ID, label) + 18;
  }
  yPos += kLegendSwatch + 6;

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
