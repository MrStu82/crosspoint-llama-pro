#include "StatsActivity.h"

#include <Bitmap.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>

#include <cstdio>
#include <cstring>
#include <string>

#include "FsHelpers.h"
#include "MappedInputManager.h"
#include "RecentBooksStore.h"
#include "StatsManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

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
  totalBooksOnDevice = countEpubsRecursively("/");
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

  int yPos = metrics.topPadding + metrics.headerHeight + 20;
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
  int coverH = 160;
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

  if (currentProgress < 0 && FsHelpers::hasEpubExtension(currentBookPath)) {
    std::string cachePath = "/.crosspoint/epub_" + std::to_string(std::hash<std::string>{}(currentBookPath));
    HalFile f;
    if (Storage.openFileForRead("STATS", cachePath + "/progress.bin", f)) {
      uint8_t data[7];
      if (f.read(data, 7) >= 7) {
        currentProgress = data[6];
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

  int barY = yPos + 80;
  renderer.drawText(UI_10_FONT_ID, rightTextX, barY - 22, tr(STR_STATS_BOOK_PROGRESS));
  if (currentProgress >= 0) {
    char progStr[16];
    snprintf(progStr, sizeof(progStr), "%d%%", currentProgress);
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

  yPos += coverH + 20;
  renderer.drawLine(sidePad, yPos, screenWidth - sidePad, yPos, 2, true);
  yPos += 30;

  auto drawCentered = [&](int fontId, const char* text, int cx, int y,
                          EpdFontFamily::Style style = EpdFontFamily::REGULAR) {
    if (text[0] == '\0') return;
    int w = renderer.getTextWidth(fontId, text, style);
    renderer.drawText(fontId, cx - w / 2, y, text, true, style);
  };

  int colWidth = (screenWidth - (sidePad * 2)) / 3;

  auto drawTrio = [&](int startY, const char* val1, const char* label1a, const char* label1b, const char* val2,
                      const char* label2a, const char* label2b, const char* val3, const char* label3a,
                      const char* label3b) {
    int cx1 = sidePad + colWidth / 2;
    int cx2 = sidePad + colWidth + colWidth / 2;
    int cx3 = sidePad + colWidth * 2 + colWidth / 2;

    drawCentered(UI_12_FONT_ID, val1, cx1, startY, EpdFontFamily::BOLD);
    drawCentered(UI_10_FONT_ID, label1a, cx1, startY + 34);
    drawCentered(UI_10_FONT_ID, label1b, cx1, startY + 52);

    renderer.drawLine(sidePad + colWidth, startY, sidePad + colWidth, startY + 65, 1, true);

    drawCentered(UI_12_FONT_ID, val2, cx2, startY, EpdFontFamily::BOLD);
    drawCentered(UI_10_FONT_ID, label2a, cx2, startY + 34);
    drawCentered(UI_10_FONT_ID, label2b, cx2, startY + 52);

    renderer.drawLine(sidePad + colWidth * 2, startY, sidePad + colWidth * 2, startY + 65, 1, true);

    drawCentered(UI_12_FONT_ID, val3, cx3, startY, EpdFontFamily::BOLD);
    drawCentered(UI_10_FONT_ID, label3a, cx3, startY + 34);
    drawCentered(UI_10_FONT_ID, label3b, cx3, startY + 52);
  };

  auto drawDuo = [&](int startY, const char* val1, const char* label1a, const char* label1b, const char* val2,
                     const char* label2a, const char* label2b) {
    int cx1 = sidePad + (screenWidth - sidePad * 2) / 4;
    int cx2 = sidePad + 3 * (screenWidth - sidePad * 2) / 4;

    drawCentered(UI_12_FONT_ID, val1, cx1, startY, EpdFontFamily::BOLD);
    drawCentered(UI_10_FONT_ID, label1a, cx1, startY + 34);
    drawCentered(UI_10_FONT_ID, label1b, cx1, startY + 52);

    renderer.drawLine(sidePad + (screenWidth - sidePad * 2) / 2, startY, sidePad + (screenWidth - sidePad * 2) / 2,
                      startY + 65, 1, true);

    drawCentered(UI_12_FONT_ID, val2, cx2, startY, EpdFontFamily::BOLD);
    drawCentered(UI_10_FONT_ID, label2a, cx2, startY + 34);
    drawCentered(UI_10_FONT_ID, label2b, cx2, startY + 52);
  };

  // --- TODAY SECTION ---
  renderer.fillRectDither(sidePad, yPos, screenWidth - sidePad * 2, 24, Color::LightGray);
  renderer.drawText(UI_10_FONT_ID, sidePad + 10, yPos + 3, tr(STR_STATS_TODAY));
  yPos += 45;

  char t_val1[16];
  snprintf(t_val1, sizeof(t_val1), "%lu", static_cast<unsigned long>(stats.readingTimeTodaySeconds / 60));
  char t_val2[16];
  snprintf(t_val2, sizeof(t_val2), "%lu", static_cast<unsigned long>(stats.pagesReadToday));
  float ppmToday = 0;
  if (stats.readingTimeTodaySeconds > 60)
    ppmToday = static_cast<float>(stats.pagesReadToday) / (stats.readingTimeTodaySeconds / 60.0f);
  char t_val3[16];
  snprintf(t_val3, sizeof(t_val3), "%.1f", ppmToday);

  drawTrio(yPos, t_val1, tr(STR_STATS_MINUTES), "", t_val2, tr(STR_STATS_PAGES), "", t_val3,
           tr(STR_STATS_PAGES_PER_MIN), "");
  yPos += 85;

  yPos += 15;

  // --- ALL TIME SECTION ---
  renderer.fillRectDither(sidePad, yPos, screenWidth - sidePad * 2, 24, Color::LightGray);
  renderer.drawText(UI_10_FONT_ID, sidePad + 10, yPos + 3, tr(STR_STATS_ALL_TIME));
  yPos += 45;

  char a_val1[16];
  snprintf(a_val1, sizeof(a_val1), "%.1f", stats.totalReadingTimeSeconds / 3600.0f);
  char a_val2[16];
  snprintf(a_val2, sizeof(a_val2), "%lu", static_cast<unsigned long>(stats.totalPagesRead));
  float ppmAll = 0;
  if (stats.totalReadingTimeSeconds > 60)
    ppmAll = static_cast<float>(stats.totalPagesRead) / (stats.totalReadingTimeSeconds / 60.0f);
  char a_val3[16];
  snprintf(a_val3, sizeof(a_val3), "%.1f", ppmAll);

  drawTrio(yPos, a_val1, tr(STR_STATS_HOURS), "", a_val2, tr(STR_STATS_PAGES), "", a_val3, tr(STR_STATS_PAGES_PER_MIN),
           "");
  yPos += 85;

  yPos += 15;

  // --- ALL ITEMS SECTION ---
  renderer.fillRectDither(sidePad, yPos, screenWidth - sidePad * 2, 24, Color::LightGray);
  renderer.drawText(UI_10_FONT_ID, sidePad + 10, yPos + 3, tr(STR_STATS_ALL_ITEMS));
  yPos += 45;

  char i_val1[16];
  snprintf(i_val1, sizeof(i_val1), "%lu", static_cast<unsigned long>(stats.booksFinished));
  char i_val2[16];
  snprintf(i_val2, sizeof(i_val2), "%d", totalBooksOnDevice);

  drawDuo(yPos, i_val1, tr(STR_STATS_BOOKS), tr(STR_STATS_FINISHED), i_val2, tr(STR_STATS_TOTAL), tr(STR_STATS_BOOKS));

  yPos += 85;

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
