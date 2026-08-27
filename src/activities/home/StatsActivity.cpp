#include "StatsActivity.h"

#include <Bitmap.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>

#include "CrossPointSettings.h"
#include "FsHelpers.h"
#include "MappedInputManager.h"
#include "RecentBooksStore.h"
#include "StatsManager.h"
#include "util/BookProgressBadge.h"
#include "util/ChapterProgress.h"
#include "components/InkPointShell.h"
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

  // Title. On the InkPoint shell every other screen draws the shared header, so
  // drawing the generic one here was the only place the frame changed shape
  // between screens.
  if (InkPointShell::enabled(renderer)) InkPointShell::drawHeader(renderer, tr(STR_READING_STATS));
  else GUI.drawHeader(renderer, Rect{0, metrics.topPadding, screenWidth, metrics.headerHeight}, tr(STR_READING_STATS));

  // Every other coordinate on this screen comes from a pixel spec expressed as text
  // baselines, but drawText()/getFontAscenderSize() work in top-edge y. Convert once
  // here rather than hand-computing an offset at each call site.
  auto topOf = [&](int fontId, int baselineY) { return baselineY - renderer.getFontAscenderSize(fontId); };
  auto drawBaseline = [&](int fontId, int x, int baselineY, const char* text, bool black = true,
                          EpdFontFamily::Style style = EpdFontFamily::REGULAR) {
    renderer.drawText(fontId, x, topOf(fontId, baselineY), text, black, style);
  };
  // maxWidth/fallbackFontId let a call site guard against a value that's grown wider than
  // its column (e.g. a stat that crossed into 4+ digits): if the primary font overflows,
  // retry once with a narrower fallback font rather than drawing past the column edge.
  auto drawCentered = [&](int fontId, const char* text, int cx, int baselineY,
                          EpdFontFamily::Style style = EpdFontFamily::REGULAR, int maxWidth = 0,
                          int fallbackFontId = 0) {
    if (text[0] == '\0') return;
    int w = renderer.getTextWidth(fontId, text, style);
    int drawFontId = fontId;
    if (maxWidth > 0 && w > maxWidth && fallbackFontId != 0) {
      drawFontId = fallbackFontId;
      w = renderer.getTextWidth(drawFontId, text, style);
    }
    renderer.drawText(drawFontId, cx - w / 2, topOf(drawFontId, baselineY), text, true, style);
  };
  // STAT-01 (Stuart, real-hardware report): the BOOK/CHAPTER progress % was drawn
  // left-anchored at a fixed x with no width check, so on a real 4" panel (narrower
  // than whatever the sim implied) a 3-digit value plus its "%" glyph ran past the
  // panel edge and got clipped. Measure the FULL string (digits + "%") up front, fall
  // back to a narrower font if it doesn't fit, and always anchor from the right edge
  // so nothing can ever land within the reserved margin, let alone past it.
  auto drawRightAligned = [&](int fontId, const char* text, int rightEdge, int baselineY,
                              EpdFontFamily::Style style = EpdFontFamily::REGULAR, int maxWidth = 0,
                              int fallbackFontId = 0) {
    if (text[0] == '\0') return;
    int w = renderer.getTextWidth(fontId, text, style);
    int drawFontId = fontId;
    if (maxWidth > 0 && w > maxWidth && fallbackFontId != 0) {
      drawFontId = fallbackFontId;
      w = renderer.getTextWidth(drawFontId, text, style);
    }
    renderer.drawText(drawFontId, rightEdge - w, topOf(drawFontId, baselineY), text, true, style);
  };

  // --- BOOK HERO ---
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
    // RecentBook::progressPercent on the RECENT_BOOKS singleton is never populated --
    // only HomeActivity's own locally-scoped copy of each entry gets enriched from the
    // badge cache (see HomeActivity::loadRecentBooks). Read the badge directly here too,
    // same as HomeActivity does, instead of trusting a field that's permanently -1.
    currentProgress = BookProgressBadge::read(book.path).value_or(-1);
    coverBmpPath = book.coverBmpPath;
    currentBookPath = book.path;
  }

  constexpr int kCoverX = 24;
  constexpr int kCoverY = 90;
  int coverW = 140;
  int coverH = 186;
  bool hasCover = false;

  if (!coverBmpPath.empty()) {
    const std::string thumbPath = UITheme::getCoverThumbPath(coverBmpPath, metrics.homeCoverHeight);
    HalFile file;
    if (Storage.openFileForRead("STATS", thumbPath, file)) {
      Bitmap bitmap(file);
      if (bitmap.parseHeaders() == BmpReaderError::Ok) {
        hasCover = true;
        coverW = (coverH * bitmap.getWidth()) / bitmap.getHeight();
        if (coverW > 140) coverW = 140;
        renderer.drawBitmap(bitmap, kCoverX, kCoverY, coverW, coverH);
      }
    }
  }

  if (!hasCover) {
    renderer.drawRect(kCoverX, kCoverY, coverW, coverH);
    renderer.drawText(UI_10_FONT_ID, kCoverX + 10, topOf(UI_10_FONT_ID, kCoverY + coverH / 2), tr(STR_STATS_NO_COVER));
  }

  constexpr int kTextX = 180;
  const int textRightEdge = screenWidth - 16;
  const int textW = textRightEdge - kTextX;

  renderer.drawText(UI_12_FONT_ID, kTextX, topOf(UI_12_FONT_ID, 118),
                    renderer.truncatedText(UI_12_FONT_ID, currentBookTitle.c_str(), textW).c_str(), true,
                    EpdFontFamily::ITALIC);
  renderer.drawText(UI_10_FONT_ID, kTextX, topOf(UI_10_FONT_ID, 158),
                    renderer.truncatedText(UI_10_FONT_ID, currentBookAuthor.c_str(), textW).c_str());

  int currentChapterProgress = -1;

  // Note: currentProgress (whole-book %) comes only from BookProgressBadge::read() above —
  // progress.bin has no book-level percent field to fall back to. data[6] here is byte 0 of
  // the optional visibleTextOffset, not a percent; reading it as one produced the "181%" bug.
  // Only chapter progress (page/pageCount, data[2-5]) is legitimately derivable from this file.
  const ChapterProgressValue chapter = ChapterProgress::read(currentBookPath);
  if (chapter.available) currentChapterProgress = (chapter.currentPage * 100) / chapter.pageCount;

  // Badge cache miss: a book already in progress when BookProgressBadge shipped (or one
  // whose reader hasn't re-saved since) has no book_progress.bin yet. Approximate the
  // whole-book % from chapter progress rather than showing STR_STATS_UNKNOWN -- bounded
  // and tilde-marked as an estimate, since it ignores relative chapter sizes.
  bool bookProgressIsApprox = false;
  if (currentProgress < 0 && currentChapterProgress >= 0) {
    currentProgress = currentChapterProgress;
    bookProgressIsApprox = true;
  }

  constexpr int kBarX = 180;
  constexpr int kBarW = 240;
  constexpr int kBarH = 10;
  // STAT-01: percent text now anchors off textRightEdge (the same reserved-margin edge the
  // book title/author truncate against, screenWidth - 16) instead of a fixed left x — the
  // column between the bar and that edge is what's actually available, and the fallback
  // font keeps a 3-digit "~100%" from ever being asked to fit in less space than it needs.
  constexpr int kPctGap = 10;
  const int kPctMaxTextW = textRightEdge - (kBarX + kBarW) - kPctGap;

  drawBaseline(UI_10_FONT_ID, kTextX, 188, tr(STR_STATS_BOOK_PROGRESS));
  renderer.drawRect(kBarX, 194, kBarW, kBarH);
  if (currentProgress >= 0) {
    if (currentProgress > 0) renderer.fillRect(kBarX, 194, (kBarW * currentProgress) / 100, kBarH);
    char progStr[16];
    snprintf(progStr, sizeof(progStr), bookProgressIsApprox ? "~%d%%" : "%d%%", currentProgress);
    drawRightAligned(UI_12_FONT_ID, progStr, textRightEdge, 203, EpdFontFamily::REGULAR, kPctMaxTextW, UI_10_FONT_ID);
  } else {
    drawRightAligned(UI_12_FONT_ID, tr(STR_STATS_UNKNOWN), textRightEdge, 203, EpdFontFamily::REGULAR, kPctMaxTextW,
                      UI_10_FONT_ID);
  }

  drawBaseline(UI_10_FONT_ID, kTextX, 222, tr(STR_STATS_CHAPTER_PROGRESS));
  renderer.drawRect(kBarX, 228, kBarW, kBarH);
  if (currentChapterProgress >= 0) {
    if (currentChapterProgress > 0) renderer.fillRect(kBarX, 228, (kBarW * currentChapterProgress) / 100, kBarH);
    char progStr[16];
    snprintf(progStr, sizeof(progStr), "%d%%", currentChapterProgress);
    drawRightAligned(UI_12_FONT_ID, progStr, textRightEdge, 237, EpdFontFamily::REGULAR, kPctMaxTextW, UI_10_FONT_ID);
  } else {
    drawRightAligned(UI_12_FONT_ID, tr(STR_STATS_UNKNOWN), textRightEdge, 237, EpdFontFamily::REGULAR, kPctMaxTextW,
                      UI_10_FONT_ID);
  }

  // --- MIDDLE BAND (new work — everything else on this screen reproduces the
  // frozen spec exactly; this section is the only part actually being designed) ---
  // pagesReadToday is already tracked live by StatsManager (GlobalStats::pagesReadToday),
  // so no new forward-only counter was needed to source it.
  const int minutesToday = static_cast<int>(stats.readingTimeTodaySeconds / 60);
  const uint32_t pagesToday = stats.pagesReadToday;

  // Middle-band columns are 224px wide (16..240, 240..464); cap primary-font text to what
  // fits with a margin either side of center, falling back to the smaller digit font for
  // days with an unusually large minute/page count rather than clipping into the divider.
  constexpr int kMiddleBandMaxTextW = 200;

  char minTodayStr[16];
  snprintf(minTodayStr, sizeof(minTodayStr), "%d", minutesToday);
  drawCentered(NOTOSANS_40_BOLD_DIGITS_FONT_ID, minTodayStr, 128, 356, EpdFontFamily::REGULAR,
               kMiddleBandMaxTextW, NOTOSANS_20_BOLD_DIGITS_FONT_ID);
  drawCentered(UI_10_FONT_ID, tr(STR_STATS_MIN_TODAY), 128, 378);

  char pagesTodayStr[16];
  snprintf(pagesTodayStr, sizeof(pagesTodayStr), "%lu", static_cast<unsigned long>(pagesToday));
  drawCentered(NOTOSANS_40_BOLD_DIGITS_FONT_ID, pagesTodayStr, 352, 356, EpdFontFamily::REGULAR,
               kMiddleBandMaxTextW, NOTOSANS_20_BOLD_DIGITS_FONT_ID);
  drawCentered(UI_10_FONT_ID, tr(STR_STATS_PAGES_TODAY), 352, 378);

  renderer.drawLine(240, 318, 240, 318 + 66, 1, true);

  // Guarded per spec: never divide by zero, never fabricate a rate from near-zero
  // minutes. Below 1 full minute today, show an em dash instead of a number.
  char ppmLine[48];
  if (minutesToday < 1) {
    snprintf(ppmLine, sizeof(ppmLine), "\xE2\x80\x94 %s", tr(STR_STATS_PAGES_PER_MINUTE_LONG));
  } else {
    const float ppmToday = static_cast<float>(pagesToday) / static_cast<float>(minutesToday);
    snprintf(ppmLine, sizeof(ppmLine), "%.1f %s", ppmToday, tr(STR_STATS_PAGES_PER_MINUTE_LONG));
  }
  // STAT-01: was baseline 402, only 24px below the "min today"/"pages today" label row
  // (baseline 378) — too tight on real hardware, read as crushed between the two numeric
  // rows. Pushed to 410 (32px gap) for real breathing room; still well clear of the stat
  // column dividers, which start at y=424.
  drawCentered(NOTOSANS_14_FONT_ID, ppmLine, 240, 410);

  // Fetched once here so both the 30-day grid slice and the streak walk read from
  // the same snapshot. getLast7DaysMinutes() (name predates this screen) fills all
  // DAILY_HISTORY_SLOTS entries oldest-first; slot 0 is 83 days ago, the last slot
  // is today, sourced live from stats.readingTimeTodaySeconds rather than the ring.
  uint16_t history[DAILY_HISTORY_SLOTS];
  int historyDates[DAILY_HISTORY_SLOTS];
  READING_STATS.getLast7DaysMinutes(history, historyDates);

  int streak = 0;
  for (int i = DAILY_HISTORY_SLOTS - 1; i >= 0; i--) {
    if (historyDates[i] == 0 || history[i] == 0) break;
    streak++;
  }

  // BEST DAY is the max over the archived 84-day ring (minutes), per confirmed
  // spec semantics — this deliberately does not include today's live minutes,
  // which are never written into dailyHistory[] until the day rolls over.
  uint16_t bestDayMinutes = 0;
  for (const DailyMinutesEntry& entry : stats.dailyHistory) {
    if (entry.minutes > bestDayMinutes) bestDayMinutes = entry.minutes;
  }

  const float allTimeHours = stats.totalReadingTimeSeconds / 3600.0f;

  char streakStr[16];
  snprintf(streakStr, sizeof(streakStr), "%d", streak);
  char hrsStr[16];
  snprintf(hrsStr, sizeof(hrsStr), "%.1f", allTimeHours);
  char pagesStr[16];
  snprintf(pagesStr, sizeof(pagesStr), "%lu", static_cast<unsigned long>(stats.totalPagesRead));
  char bestDayStr[16];
  snprintf(bestDayStr, sizeof(bestDayStr), "%dm", static_cast<int>(bestDayMinutes));

  const int colCx[4] = {88, 200, 312, 424};
  const char* colVal[4] = {streakStr, hrsStr, pagesStr, bestDayStr};
  const StrId colLabel[4] = {StrId::STR_STATS_STREAK, StrId::STR_STATS_HRS, StrId::STR_STATS_PAGES,
                             StrId::STR_STATS_BEST_DAY};
  // Columns are 112px wide; cap the primary bold-digit font's text and fall back to the
  // smaller non-bold digit font (NotoSans, still has digit glyphs) if all-time hours/pages
  // grow past what the bold face fits without touching the neighboring divider.
  constexpr int kStatColMaxTextW = 100;
  for (int i = 0; i < 4; i++) {
    drawCentered(NOTOSANS_20_BOLD_DIGITS_FONT_ID, colVal[i], colCx[i], 448, EpdFontFamily::BOLD, kStatColMaxTextW,
                 NOTOSANS_14_FONT_ID);
    drawCentered(UI_10_FONT_ID, I18n::getInstance().get(colLabel[i]), colCx[i], 468);
  }
  for (int i = 1; i <= 3; i++) {
    const int dividerX = 16 + i * 112;
    renderer.drawLine(dividerX, 424, dividerX, 424 + 54, 1, true);
  }

  drawBaseline(UI_12_FONT_ID, 24, 506, tr(STR_STATS_ACTIVITY_HISTORY), true, EpdFontFamily::BOLD);

  // --- 30-DAY ACTIVITY GRID ---
  // Re-laid-out to spec geometry (10 cols x 3 rows), sliced from the last 30 of the
  // 84 archived slots — not a reuse of the old 7x12/84-cell grid. The 45px/58px
  // pitch against 39x52 cells leaves a real 6px white gutter between cells.
  constexpr int kGridCols = 10;
  constexpr int kGridRows = 3;
  constexpr int kCellW = 39;
  constexpr int kCellH = 52;
  constexpr int kCellStepX = 45;
  constexpr int kCellStepY = 58;
  constexpr int kGridX = 18;
  constexpr int kGridY = 520;
  constexpr int kGridSliceDays = kGridCols * kGridRows;
  const int gridStart = DAILY_HISTORY_SLOTS - kGridSliceDays;

  for (int i = 0; i < kGridSliceDays; i++) {
    const int col = i % kGridCols;
    const int row = i / kGridCols;
    const int cellX = kGridX + col * kCellStepX;
    const int cellY = kGridY + row * kCellStepY;
    const int hi = gridStart + i;

    Color level;
    if (historyDates[hi] == 0 || history[hi] == 0) {
      level = Color::White;
    } else if (history[hi] < 15) {
      level = Color::LightGray;
    } else if (history[hi] < 45) {
      level = Color::DarkGray;
    } else {
      level = Color::Black;
    }

    renderer.fillRectDither(cellX, cellY, kCellW, kCellH, level);
    renderer.drawRect(cellX, cellY, kCellW, kCellH);
  }

  // --- LEGEND ---
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
  constexpr int kLegendSwatch = 12;
  constexpr int kLegendY = 704;
  for (int i = 0; i < 4; i++) {
    const int swatchX = 62 + i * 100;
    renderer.fillRectDither(swatchX, kLegendY, kLegendSwatch, kLegendSwatch, legendItems[i].color);
    renderer.drawRect(swatchX, kLegendY, kLegendSwatch, kLegendSwatch);
    drawBaseline(UI_12_FONT_ID, swatchX + 19, 715, I18n::getInstance().get(legendItems[i].label));
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBufferGhostGuard(ghostGuardCounter_, SETTINGS.getRefreshFrequency());
}
