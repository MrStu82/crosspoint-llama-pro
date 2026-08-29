#include "HomeActivity.h"
#include "HomeInkPointGeometry.h"

#include <Bitmap.h>
#include <Epub.h>
#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <HalDisplay.h>
#include <HalPowerManager.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Utf8.h>
#include <Xtc.h>

#include <algorithm>
#include <cstring>
#include <ctime>
#include <cstdio>
#include <cctype>
#include <vector>

#include "CrossPointSettings.h"
#include "CrossPointState.h"
#include "MappedInputManager.h"
#include "OpdsServerStore.h"
#include "RecentBooksStore.h"
#include "StatsManager.h"
#include "components/UITheme.h"
#include "components/InkPointShell.h"
#include "fontIds.h"
#include "network/HardcoverRating.h"
#include "util/BookProgressBadge.h"
#include "util/BookReadingStats.h"
#include "util/ChapterProgress.h"
#include "util/DailyQuote.h"

namespace {
// The cover's right edge sits on the screen centre; everything to the right of
// it is the metadata gutter. The lane is the maximum the cover may occupy, not
// a frame drawn around it -- see renderInkPointHome.
constexpr int kInkCoverRight = 240;
const Rect kInkCoverLane{20, InkPointShell::kContentTop, 220, 434};
// Stats block and its ">>>" chevron form a single target for the Stats screen.
// Kept clear of the cover lane to its left and the footer tabs below.
const Rect kInkStats{252, 300, 208, 250};
constexpr int kInkStatsX = 260;
constexpr int kInkStatsWidth = 200;
constexpr int kInkStatsStep = 58;
const Rect kInkFooter{14, 728, 452, 60};
constexpr int kInkTabWidth = 72;
constexpr int kInkTabGap = 4;

// Three right-pointing triangles, the same glyph the menu scroll indicator
// uses, right-aligned under the stat block and underlined so the group reads as
// a link rather than as decoration. No new asset.
constexpr int kInkChevronHalf = 6;   // half-height/half-width of one triangle
constexpr int kInkChevronStep = 15;  // pitch between triangle centres
constexpr int kInkChevronRight = 458;
constexpr int kInkChevronLeft = kInkChevronRight - 2 * kInkChevronStep - 2 * kInkChevronHalf;
constexpr int kInkChevronRuleGap = 5;
constexpr int kInkChevronRuleHeight = 2;

void drawStatsChevron(const GfxRenderer& renderer, const int cy) {
  const int rightCx = kInkChevronRight - kInkChevronHalf;
  for (int i = 0; i < 3; ++i) {
    const int cx = rightCx - (2 - i) * kInkChevronStep;
    const int xPoints[3] = {cx - kInkChevronHalf, cx - kInkChevronHalf, cx + kInkChevronHalf};
    const int yPoints[3] = {cy - kInkChevronHalf, cy + kInkChevronHalf, cy};
    renderer.fillPolygon(xPoints, yPoints, 3, true);
  }
  renderer.fillRect(kInkChevronLeft, cy + kInkChevronHalf + kInkChevronRuleGap,
                    kInkChevronRight - kInkChevronLeft, kInkChevronRuleHeight);
}

int textTop(const GfxRenderer& renderer, int fontId, int baseline) {
  return baseline - renderer.getFontAscenderSize(fontId);
}

void drawCentered(const GfxRenderer& renderer, int fontId, int cx, int baseline, const char* text,
                  bool black = true, EpdFontFamily::Style style = EpdFontFamily::REGULAR) {
  renderer.drawText(fontId, cx - renderer.getTextWidth(fontId, text, style) / 2,
                    textTop(renderer, fontId, baseline), text, black, style);
}

void drawStar(const GfxRenderer& renderer, const int cx, const int cy,
              const InkPointHomeGeometry::RatingStarFill fill) {
  int x[10] = {}, y[10] = {};
  for (int i = 0; i < 10; ++i) {
    x[i] = cx + InkPointHomeGeometry::kRatingStarX[i];
    y[i] = cy + InkPointHomeGeometry::kRatingStarY[i];
  }
  if (fill == InkPointHomeGeometry::RatingStarFill::Full) {
    renderer.fillPolygon(x, y, 10, true);
  } else if (fill == InkPointHomeGeometry::RatingStarFill::Half) {
    int halfX[InkPointHomeGeometry::kHalfRatingStarX.size()] = {};
    int halfY[InkPointHomeGeometry::kHalfRatingStarY.size()] = {};
    for (size_t i = 0; i < InkPointHomeGeometry::kHalfRatingStarX.size(); ++i) {
      halfX[i] = cx + InkPointHomeGeometry::kHalfRatingStarX[i];
      halfY[i] = cy + InkPointHomeGeometry::kHalfRatingStarY[i];
    }
    renderer.fillPolygon(halfX, halfY,
                         static_cast<int>(InkPointHomeGeometry::kHalfRatingStarX.size()), true);
  }
  // Draw last so both unfilled stars and the unfilled half retain the complete
  // five-point outline.
  for (int i = 0; i < 10; ++i) renderer.drawLine(x[i], y[i], x[(i + 1) % 10], y[(i + 1) % 10]);
}

std::string upper(const std::string& value) {
  std::string result = value;
  for (char& c : result) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  return result;
}

std::vector<std::string> wrapWords(const GfxRenderer& renderer, const int fontId, const std::string& text,
                                   const int maxWidth, const size_t maxLines,
                                   const EpdFontFamily::Style style = EpdFontFamily::REGULAR) {
  std::vector<std::string> lines;
  std::string line;
  auto appendWord = [&](std::string word) {
    while (!word.empty()) {
      std::string candidate = line.empty() ? word : line + " " + word;
      if (renderer.getTextWidth(fontId, candidate.c_str(), style) <= maxWidth) {
        line = std::move(candidate);
        return;
      }
      if (!line.empty()) {
        lines.push_back(line);
        line.clear();
        if (lines.size() == maxLines) return;
        continue;
      }
      size_t count = 1;
      while (count < word.size() && renderer.getTextWidth(fontId, word.substr(0, count + 1).c_str(), style) <= maxWidth)
        ++count;
      line = word.substr(0, count);
      word.erase(0, count);
      if (!word.empty()) {
        lines.push_back(line);
        line.clear();
        if (lines.size() == maxLines) return;
      }
    }
  };
  size_t start = 0;
  while (start < text.size() && lines.size() < maxLines) {
    const size_t end = text.find(' ', start);
    appendWord(text.substr(start, end == std::string::npos ? std::string::npos : end - start));
    if (end == std::string::npos || lines.size() == maxLines) break;
    start = end + 1;
  }
  if (!line.empty() && lines.size() < maxLines) lines.push_back(line);
  return lines;
}

std::string fitText(const GfxRenderer& renderer, int fontId, std::string value, int maxWidth,
                    EpdFontFamily::Style style = EpdFontFamily::REGULAR) {
  if (renderer.getTextWidth(fontId, value.c_str(), style) <= maxWidth) return value;
  while (!value.empty() && renderer.getTextWidth(fontId, (value + "...").c_str(), style) > maxWidth)
    value.pop_back();
  return value + "...";
}

// Wrap only at whitespace.  This is intentionally separate from wrapWords(),
// whose hard-token splitting is useful for prose but wrong for a book title:
// a title must never turn DUNGEON into DUN / GEON merely to fill a column.
std::vector<std::string> wrapWholeWords(const GfxRenderer& renderer, const int fontId,
                                        const std::string& text, const int maxWidth,
                                        const size_t maxLines, bool& fits,
                                        const EpdFontFamily::Style style = EpdFontFamily::REGULAR) {
  std::vector<std::string> lines;
  std::string line;
  fits = true;
  size_t start = 0;
  while (start < text.size()) {
    while (start < text.size() && text[start] == ' ') ++start;
    if (start == text.size()) break;
    const size_t end = text.find(' ', start);
    const std::string word = text.substr(start, end == std::string::npos ? std::string::npos : end - start);
    if (renderer.getTextWidth(fontId, word.c_str(), style) > maxWidth) {
      fits = false;
      return {};
    }
    const std::string candidate = line.empty() ? word : line + " " + word;
    if (renderer.getTextWidth(fontId, candidate.c_str(), style) <= maxWidth) {
      line = candidate;
    } else {
      if (lines.size() + 1 >= maxLines) {
        fits = false;
        return {};
      }
      lines.push_back(line);
      line = word;
    }
    if (end == std::string::npos) break;
    start = end + 1;
  }
  if (!line.empty()) lines.push_back(line);
  return lines;
}

// Last-resort title fitting for malformed metadata containing an over-wide
// token. Preserve every usable whole word and represent only the impossible
// token with a bounded marker; never collapse the complete title into a
// character-truncated line (which can visually join unrelated words).
std::vector<std::string> wrapBoundedTitle(const GfxRenderer& renderer, const int fontId,
                                          const std::string& text, const int maxWidth,
                                          const size_t maxLines,
                                          const EpdFontFamily::Style style) {
  std::vector<std::string> lines;
  std::string line;
  size_t start = 0;
  while (start < text.size() && lines.size() < maxLines) {
    while (start < text.size() && text[start] == ' ') ++start;
    if (start == text.size()) break;
    const size_t end = text.find(' ', start);
    std::string word = text.substr(start, end == std::string::npos ? std::string::npos : end - start);
    if (renderer.getTextWidth(fontId, word.c_str(), style) > maxWidth) word = "...";
    const std::string candidate = line.empty() ? word : line + " " + word;
    if (renderer.getTextWidth(fontId, candidate.c_str(), style) <= maxWidth) {
      line = candidate;
    } else {
      lines.push_back(line);
      line = word;
    }
    if (end == std::string::npos) break;
    start = end + 1;
  }
  if (!line.empty() && lines.size() < maxLines) lines.push_back(line);
  return lines;
}
}  // namespace

int HomeActivity::getMenuItemCount() const {
  int count = 6;  // File Browser, Recents, File transfer, Settings, Stats, Games
  if (!recentBooks.empty()) {
    count += recentBooks.size();
  }
  if (hasOpdsServers) {
    count++;
  }
  return count;
}

void HomeActivity::loadRecentBooks(int maxBooks) {
  recentBooks.clear();
  const auto& books = RECENT_BOOKS.getBooks();
  recentBooks.reserve(std::min(static_cast<int>(books.size()), maxBooks));

  for (const RecentBook& book : books) {
    // Limit to maximum number of recent books
    if (recentBooks.size() >= maxBooks) {
      break;
    }

    // Skip if file no longer exists
    if (RecentBooksStore::isMissing(book)) {
      continue;
    }

    RecentBook bookWithProgress = book;
    bookWithProgress.progressPercent = BookProgressBadge::read(book.path).value_or(-1);
    recentBooks.push_back(bookWithProgress);
  }
}

void HomeActivity::loadRecentCovers(int coverHeight) {
  recentsLoading = true;
  bool showingLoading = false;
  Rect popupRect;

  int progress = 0;
  for (RecentBook& book : recentBooks) {
    if (!book.coverBmpPath.empty()) {
      std::string coverPath = UITheme::getCoverThumbPath(book.coverBmpPath, coverHeight);
      if (!Storage.exists(coverPath.c_str())) {
        // If epub, try to load the metadata for title/author and cover
        if (FsHelpers::hasEpubExtension(book.path)) {
          Epub epub(book.path, "/.crosspoint");
          // Skip loading css since we only need metadata here
          epub.load(false, true);

          // Try to generate thumbnail image for Continue Reading card
          if (!showingLoading) {
            showingLoading = true;
            popupRect = GUI.drawPopup(renderer, tr(STR_LOADING_POPUP));
          }
          GUI.fillPopupProgress(renderer, popupRect, 10 + progress * (90 / recentBooks.size()));
          bool success = epub.generateThumbBmp(coverHeight);
          if (!success) {
            RECENT_BOOKS.updateBook(book.path, book.title, book.author, "");
            book.coverBmpPath = "";
          }
          coverRendered = false;
          requestUpdate();
        } else if (FsHelpers::hasXtcExtension(book.path)) {
          // Handle XTC file
          Xtc xtc(book.path, "/.crosspoint");
          if (xtc.load()) {
            // Try to generate thumbnail image for Continue Reading card
            if (!showingLoading) {
              showingLoading = true;
              popupRect = GUI.drawPopup(renderer, tr(STR_LOADING_POPUP));
            }
            GUI.fillPopupProgress(renderer, popupRect, 10 + progress * (90 / recentBooks.size()));
            bool success = xtc.generateThumbBmp(coverHeight);
            if (!success) {
              RECENT_BOOKS.updateBook(book.path, book.title, book.author, "");
              book.coverBmpPath = "";
            }
            coverRendered = false;
            requestUpdate();
          }
        }
      }
    }
    progress++;
  }

  recentsLoaded = true;
  recentsLoading = false;
}

void HomeActivity::onEnter() {
  Activity::onEnter();

  hasOpdsServers = OPDS_STORE.hasServers();

  const auto& metrics = UITheme::getInstance().getMetrics();
  loadRecentBooks(metrics.homeRecentBooksCount);

  if (usesInkPointHome() && !recentBooks.empty()) {
    const auto& book = recentBooks.front();
    rating = HardcoverRating::loadLastGood({book.path, book.isbn, book.title, book.author});
    bookStats = BookReadingStats::read(book.path);
    chapterProgress = ChapterProgress::read(book.path);
  }

  const auto base = static_cast<int>(recentBooks.size());
  selectorIndex = initialMenuItem == HomeMenuItem::NONE ? 0 : base + menuItemToIndex(initialMenuItem, hasOpdsServers);

  // Trigger first update
  requestUpdate();
}

void HomeActivity::onExit() {
  Activity::onExit();

  // Free the stored cover buffer if any
  freeCoverBuffer();
}

bool HomeActivity::storeCoverBuffer() {
  // render() must have already set the cover rect; without it we'd be back to
  // cloning the whole framebuffer.
  if (coverRectW <= 0 || coverRectH <= 0) return false;
  freeCoverBuffer();
  const size_t needed = renderer.getRegionByteSize(coverRectX, coverRectY, coverRectW, coverRectH);
  if (needed == 0) return false;
  coverBuffer = static_cast<uint8_t*>(malloc(needed));
  if (!coverBuffer) {
    LOG_ERR("HOME", "OOM: cover buffer (%u bytes)", (unsigned)needed);
    return false;
  }
  coverBufferSize = needed;
  if (!renderer.copyRegionToBuffer(coverRectX, coverRectY, coverRectW, coverRectH, coverBuffer, coverBufferSize)) {
    free(coverBuffer);
    coverBuffer = nullptr;
    coverBufferSize = 0;
    return false;
  }
  return true;
}

bool HomeActivity::restoreCoverBuffer() {
  if (!coverBuffer || coverRectW <= 0 || coverRectH <= 0) return false;
  return renderer.copyBufferToRegion(coverRectX, coverRectY, coverRectW, coverRectH, coverBuffer, coverBufferSize);
}

void HomeActivity::freeCoverBuffer() {
  if (coverBuffer) {
    free(coverBuffer);
    coverBuffer = nullptr;
  }
  coverBufferSize = 0;
  coverBufferStored = false;
}

void HomeActivity::loop() {
  if (usesInkPointHome()) {
    loopInkPointHome();
    return;
  }
  const int menuCount = getMenuItemCount();
  const auto& metrics = UITheme::getInstance().getMetrics();

  auto activateSelection = [this] {
    if (selectorIndex < recentBooks.size()) {
      onSelectBook(recentBooks[selectorIndex].path);
      return;
    }
    const int menuIndex = selectorIndex - static_cast<int>(recentBooks.size());
    switch (indexToMenuItem(menuIndex, hasOpdsServers)) {
      case HomeMenuItem::FILE_BROWSER:
        onFileBrowserOpen();
        break;
      case HomeMenuItem::RECENTS:
        onRecentsOpen();
        break;
      case HomeMenuItem::OPDS_BROWSER:
        onOpdsBrowserOpen();
        break;
      case HomeMenuItem::FILE_TRANSFER:
        onFileTransferOpen();
        break;
      case HomeMenuItem::SETTINGS_MENU:
        onSettingsOpen();
        break;
      case HomeMenuItem::STATS:
        onStatsOpen();
        break;
      case HomeMenuItem::GAMES:
        onGamesOpen();
        break;
      default:
        break;
    }
  };

  buttonNavigator.onNext([this, menuCount] {
    selectorIndex = ButtonNavigator::nextIndex(selectorIndex, menuCount);
    requestUpdate();
  });

  buttonNavigator.onPrevious([this, menuCount] {
    selectorIndex = ButtonNavigator::previousIndex(selectorIndex, menuCount);
    requestUpdate();
  });

  const auto swipe = mappedInput.wasSwipe();
  if (swipe == MappedInputManager::SwipeDir::Up) {
    selectorIndex = ButtonNavigator::nextIndex(selectorIndex, menuCount);
    requestUpdate();
    return;
  }
  if (swipe == MappedInputManager::SwipeDir::Down) {
    selectorIndex = ButtonNavigator::previousIndex(selectorIndex, menuCount);
    requestUpdate();
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) backPressSeen = true;

  // Back is otherwise unused on the home menu: open the most recently read
  // book directly (recentBooks is most-recent-first and already pruned of
  // files missing from the SD card). backPressSeen guards against the stale
  // release of the Back press that closed the previous activity.
  if (mappedInput.wasReleased(MappedInputManager::Button::Back) && backPressSeen && !recentBooks.empty()) {
    onSelectBook(recentBooks[0].path);
    return;
  }

  int tx = 0;
  int ty = 0;

  // Cover row can show more than one recent-book tile side by side (e.g. Lyra3CoversTheme's
  // 3-up row), so a touch must be mapped to the tile it actually landed on rather than always
  // selecting index 0. Mirrors the tileX/tileWidth math the cover-row themes use to draw them.
  const auto tileIndexForX = [&](int x) {
    const int tileCount = std::max(1, metrics.homeRecentBooksCount);
    const int tileWidth = (renderer.getScreenWidth() - 2 * metrics.contentSidePadding) / tileCount;
    if (tileWidth <= 0) return 0;
    const int tile = (x - metrics.contentSidePadding) / tileWidth;
    return std::max(0, std::min(tile, static_cast<int>(recentBooks.size()) - 1));
  };

  if (!recentBooks.empty() && mappedInput.wasScreenTouchDown(tx, ty) && tx >= 0 && tx < renderer.getScreenWidth() &&
      ty >= metrics.homeTopPadding && ty < metrics.homeTopPadding + metrics.homeCoverTileHeight) {
    const int tappedTile = tileIndexForX(tx);
    if (selectorIndex != tappedTile) {
      selectorIndex = tappedTile;
      requestUpdate();
    }
    return;
  }

  if (!recentBooks.empty() && mappedInput.wasScreenTapped(tx, ty) && tx >= 0 && tx < renderer.getScreenWidth() &&
      ty >= metrics.homeTopPadding && ty < metrics.homeTopPadding + metrics.homeCoverTileHeight) {
    selectorIndex = tileIndexForX(tx);
    activateSelection();
    return;
  }

  const int menuTop = metrics.homeTopPadding + metrics.homeCoverTileHeight + metrics.homeMenuTopOffset;
  const int renderedMenuSelection =
      metrics.homeContinueReadingInMenu ? selectorIndex : selectorIndex - recentBooks.size();
  const int renderedMenuCount =
      menuCount - (metrics.homeContinueReadingInMenu ? 0 : static_cast<int>(recentBooks.size()));
  // Dead zone below the last row: no touch handling at all in the bottom homeBottomInset
  // pixels, regardless of how many menu rows would otherwise fit in the space.
  const int menuRowStep = metrics.menuRowHeight + metrics.menuSpacing;
  const int menuTouchableHeight = renderer.getScreenHeight() - menuTop - metrics.homeBottomInset;
  const int maxTouchableRows = menuRowStep > 0 ? std::max(0, menuTouchableHeight / menuRowStep) : 0;
  const int touchableMenuCount = std::min(renderedMenuCount, maxTouchableRows);
  int menuRow = -1;
  const auto menuTouch = mappedInput.rowTouch(menuRow, menuTop, menuRowStep, touchableMenuCount, 0, INT32_MAX,
                                              metrics.menuRowHeight);
  if (menuTouch != MappedInputManager::RowTouch::None) {
    const int touchedIndex =
        metrics.homeContinueReadingInMenu ? menuRow : menuRow + static_cast<int>(recentBooks.size());
    if (menuTouch == MappedInputManager::RowTouch::Down) {
      if (selectorIndex != touchedIndex) {
        selectorIndex = touchedIndex;
        requestUpdate();
      }
    } else {
      selectorIndex = touchedIndex;
      activateSelection();
    }
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    activateSelection();
  }
}

void HomeActivity::render(RenderLock&&) {
  if (usesInkPointHome()) {
    renderInkPointHome();
    return;
  }
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();
  bool bufferRestored = coverBufferStored && restoreCoverBuffer();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.homeTopPadding},
                 metrics.homeContinueReadingInMenu && !recentBooks.empty() ? recentBooks[0].title.c_str() : nullptr);

  // Record the tile rect so storeCoverBuffer (called from the theme) knows
  // which sub-region of the framebuffer to snapshot. ~16 KB in Portrait
  // instead of the 48 KB full framebuffer the previous bind captured.
  coverRectX = 0;
  coverRectY = metrics.homeTopPadding;
  coverRectW = pageWidth;
  coverRectH = metrics.homeCoverTileHeight;

  GUI.drawRecentBookCover(renderer, Rect{0, metrics.homeTopPadding, pageWidth, metrics.homeCoverTileHeight},
                          recentBooks, selectorIndex, coverRendered, coverBufferStored, bufferRestored,
                          std::bind(&HomeActivity::storeCoverBuffer, this));

  // Build menu items dynamically
  std::vector<const char*> menuItems = {tr(STR_BROWSE_FILES), tr(STR_MENU_RECENT_BOOKS), tr(STR_FILE_TRANSFER),
                                        tr(STR_SETTINGS_TITLE), tr(STR_READING_STATS), tr(STR_GAMES_TITLE)};
  std::vector<UIIcon> menuIcons = {Folder, Recent, Transfer, Settings, Stats, Games};

  if (hasOpdsServers) {
    menuItems.insert(menuItems.begin() + 2, tr(STR_OPDS_BROWSER));
    menuIcons.insert(menuIcons.begin() + 2, Library);
  }

  if (metrics.homeContinueReadingInMenu && !recentBooks.empty()) {
    // Insert Continue Reading at the top if enabled in theme
    menuItems.insert(menuItems.begin(), tr(STR_CONTINUE_READING));
    menuIcons.insert(menuIcons.begin(), Book);
  }

  const int menuTop = metrics.homeTopPadding + metrics.homeCoverTileHeight + metrics.homeMenuTopOffset;
  const int menuRenderHeight = std::min(
      pageHeight - (metrics.headerHeight + metrics.homeTopPadding + metrics.verticalSpacing +
                    metrics.homeMenuTopOffset + metrics.buttonHintsHeight),
      pageHeight - menuTop - metrics.homeBottomInset);
  GUI.drawButtonMenu(
      renderer, Rect{0, menuTop, pageWidth, menuRenderHeight}, static_cast<int>(menuItems.size()),
      metrics.homeContinueReadingInMenu ? selectorIndex : selectorIndex - recentBooks.size(),
      [&menuItems](int index) { return std::string(menuItems[index]); },
      [&menuIcons](int index) { return menuIcons[index]; });

  const auto labels = mappedInput.mapLabels(recentBooks.empty() ? "" : tr(STR_RESUME), tr(STR_SELECT), tr(STR_DIR_UP),
                                            tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer(cleanInitialRefresh && !firstRenderDone ? HalDisplay::HALF_REFRESH : HalDisplay::FAST_REFRESH);

  if (!firstRenderDone) {
    firstRenderDone = true;
    requestUpdate();
  } else if (!recentsLoaded && !recentsLoading) {
    recentsLoading = true;
    loadRecentCovers(metrics.homeCoverHeight);
  }
}

void HomeActivity::onSelectBook(const std::string& path) { activityManager.goToReader(path); }

void HomeActivity::onFileBrowserOpen() { activityManager.goToFileBrowser(); }

void HomeActivity::onRecentsOpen() { activityManager.goToRecentBooks(); }

void HomeActivity::onSettingsOpen() { activityManager.goToSettings(); }

void HomeActivity::onFileTransferOpen() { activityManager.goToFileTransfer(); }

void HomeActivity::onOpdsBrowserOpen() { activityManager.goToBrowser(); }

void HomeActivity::onStatsOpen() { activityManager.goToStats(); }

void HomeActivity::onGamesOpen() { activityManager.goToGames(); }

bool HomeActivity::usesInkPointHome() const {
  return renderer.getScreenWidth() == 480 && renderer.getScreenHeight() == 800;
}

void HomeActivity::loopInkPointHome() {
  buttonNavigator.onNext([this] { inkPointFocus = ButtonNavigator::nextIndex(inkPointFocus, 8); requestUpdate(); });
  buttonNavigator.onPrevious([this] { inkPointFocus = ButtonNavigator::previousIndex(inkPointFocus, 8); requestUpdate(); });

  auto activate = [this](int target) {
    switch (target) {
      case 0: if (!recentBooks.empty()) onSelectBook(recentBooks.front().path); break;
      case 1: break;  // Home is already active.
      case 2: onRecentsOpen(); break;      // Library/recent-books catalogue.
      case 3: onFileBrowserOpen(); break;
      case 4: onGamesOpen(); break;
      case 5: onFileTransferOpen(); break;
      case 6: onSettingsOpen(); break;
      case 7: onStatsOpen(); break;  // Stat block and its chevron.
    }
  };

  int x = 0, y = 0;
  if (mappedInput.wasScreenTapped(x, y)) {
    if (!recentBooks.empty() && x >= kInkCoverLane.x && x < kInkCoverRight &&
        y >= kInkCoverLane.y && y < kInkCoverLane.y + kInkCoverLane.height) {
      activate(0);
      return;
    }
    if (x >= kInkStats.x && x < kInkStats.x + kInkStats.width && y >= kInkStats.y &&
        y < kInkStats.y + kInkStats.height) {
      inkPointFocus = 7;
      activate(7);
      return;
    }
    if (y >= kInkFooter.y && y < kInkFooter.y + kInkFooter.height) {
      for (int i = 0; i < 6; ++i) {
        const int left = kInkFooter.x + i * (kInkTabWidth + kInkTabGap);
        if (x >= left && x < left + kInkTabWidth) { inkPointFocus = i + 1; activate(i + 1); return; }
      }
    }
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) activate(inkPointFocus);
}

void HomeActivity::renderInkPointHome() {
  renderer.clearScreen();

  InkPointShell::drawHeader(renderer, "Now reading");

  const RecentBook* book = recentBooks.empty() ? nullptr : &recentBooks.front();
  bool coverDrawn = false;
  int coverW = kInkCoverLane.width;
  int coverH = kInkCoverLane.height;
  int coverX = kInkCoverRight - coverW;
  int coverY = kInkCoverLane.y + 2;
  if (book && !book->coverBmpPath.empty()) {
    // Thumbnails are cached per requested height, so ask for the lane height:
    // a smaller thumb cannot be enlarged (drawBitmap only ever scales down) and
    // would float inside an oversized frame.
    const std::string thumbPath = UITheme::getCoverThumbPath(book->coverBmpPath, kInkCoverLane.height);
    HalFile file;
    if (Storage.openFileForRead("HOME", thumbPath, file)) {
      Bitmap bitmap(file);
      const int bitmapW = bitmap.parseHeaders() == BmpReaderError::Ok ? bitmap.getWidth() : 0;
      const int bitmapH = bitmap.getHeight();
      if (bitmapW > 0 && bitmapH > 0) {
        // The drawn size is the fit-down size, never larger than the source.
        // Frame, progress bar and focus ring are all derived from it, so they
        // hug the cover instead of the lane.
        coverW = std::min(bitmapW, std::min(kInkCoverLane.width, (kInkCoverLane.height * bitmapW) / bitmapH));
        coverH = std::min(bitmapH, std::min(kInkCoverLane.height, (coverW * bitmapH) / bitmapW));
        coverX = kInkCoverRight - coverW;
        coverY = kInkCoverLane.y + 2;
        renderer.drawBitmap(bitmap, coverX, coverY, coverW, coverH);
        coverDrawn = true;
      }
    }
  }
  if (!coverDrawn) {
    renderer.drawRect(coverX, coverY, coverW, coverH, 2, true);
    if (book) {
      const int cx = coverX + coverW / 2;
      const auto titleLines = wrapWords(renderer, NOTOSERIF_18_FONT_ID, book->title, coverW - 36, 4,
                                        EpdFontFamily::BOLD);
      const int firstBaseline = coverY + 165 - static_cast<int>(titleLines.size() - 1) * 16;
      for (size_t i = 0; i < titleLines.size(); ++i)
        drawCentered(renderer, NOTOSERIF_18_FONT_ID, cx, firstBaseline + static_cast<int>(i) * 32,
                     titleLines[i].c_str(), true, EpdFontFamily::BOLD);
      const auto authorLines = wrapWords(renderer, UI_12_FONT_ID, book->author, coverW - 36, 2);
      for (size_t i = 0; i < authorLines.size(); ++i)
        drawCentered(renderer, UI_12_FONT_ID, cx, coverY + 235 + static_cast<int>(i) * 24,
                     authorLines[i].c_str());
    } else {
      drawCentered(renderer, UI_12_FONT_ID, coverX + coverW / 2, coverY + 210, "No recent book");
    }
  }

  constexpr int rightX = kInkStatsX;
  constexpr int rightWidth = kInkStatsWidth;
  int metadataBottom = InkPointShell::kContentTop;
  if (book) {
    const std::string title = upper(book->title);
    // Choose the largest readable UI face that fits the complete title using
    // whole-word lines. Caveat remains heading/accent-only.
    struct TitleFit { int font; size_t maxLines; int step; };
    constexpr TitleFit candidates[] = {
        {NOTOSANS_14_FONT_ID, 4, 31},
        {NOTOSANS_12_FONT_ID, 5, 27},
        {UI_10_FONT_ID, 6, 22},
    };
    int titleFont = UI_10_FONT_ID;
    int titleStep = 22;
    bool titleFits = false;
    std::vector<std::string> lines;
    for (const auto& candidate : candidates) {
      lines = wrapWholeWords(renderer, candidate.font, title, rightWidth, candidate.maxLines,
                             titleFits, EpdFontFamily::BOLD);
      if (titleFits) {
        titleFont = candidate.font;
        titleStep = candidate.step;
        break;
      }
    }
    // A pathological token cannot wrap. Bound only that token, retaining the
    // remaining whole words on subsequent lines.
    if (!titleFits)
      lines = wrapBoundedTitle(renderer, UI_10_FONT_ID, title, rightWidth, 6, EpdFontFamily::BOLD);
    int y = InkPointShell::kContentTop;
    for (const auto& titleLine : lines) {
      renderer.drawText(titleFont, rightX, y, titleLine.c_str(), true, EpdFontFamily::BOLD);
      y += titleStep;
    }
    // The wider column makes truncation unnecessary for all but absurd names,
    // so wrap onto a second line instead of ellipsising a real author.
    const auto authorLines = wrapWords(renderer, UI_10_FONT_ID, book->author, rightWidth, 2);
    for (size_t i = 0; i < authorLines.size(); ++i)
      renderer.drawText(UI_10_FONT_ID, rightX, y + 5 + static_cast<int>(i) * 18, authorLines[i].c_str());
    y += static_cast<int>(authorLines.size() > 1 ? authorLines.size() - 1 : 0) * 18;
    if (rating && rating->publicationYear > 0) {
      char year[8]; std::snprintf(year, sizeof(year), "%d", rating->publicationYear);
      renderer.drawText(UI_10_FONT_ID, rightX, y + 27, year);
    }
    if (rating) {
      for (int i = 0; i < 5; ++i)
        drawStar(renderer, rightX + 11 + i * 24, y + 62,
                 InkPointHomeGeometry::ratingStarFill(rating->valueX100, i));
      metadataBottom = y + 74;
    } else {
      metadataBottom = y + 18;
    }
  }

  // Keep the entire right gutter flowing from the metadata above it. Cap its
  // start so even a six-line title leaves the stat link above the footer.
  constexpr int kChevronSafeBottom = InkPointShell::kFooterTop - 14;
  const int statsHeight = 3 * kInkStatsStep + 20 + InkPointHomeGeometry::kStatsToChevronGap +
                          kInkChevronHalf + kInkChevronRuleGap + kInkChevronRuleHeight;
  const int statStart = std::min(metadataBottom + 24, kChevronSafeBottom - statsHeight);

  // Available values use Caveat; unavailable time/chapter values retain the
  // readable UI em dash. Whole-book ETA is withheld until it is honest.
  constexpr const char* labels[3] = {"TIME READ", "CHAPTER LEFT", "BOOK LEFT"};
  char timeRead[16] = {}, chapterLeft[16] = {}, bookLeft[16] = {};
  bool valueAvailable[3] = {false, false, false};
  // Hours are noise below the hour mark, and a bare "0h 00m" reads as broken
  // for a book only just opened.
  const auto formatMinutes = [](char* out, size_t len, uint32_t minutes) {
    if (minutes == 0) std::snprintf(out, len, "<1m");
    else if (minutes < 60) std::snprintf(out, len, "%lum", static_cast<unsigned long>(minutes));
    else
      std::snprintf(out, len, "%luh %02lum", static_cast<unsigned long>(minutes / 60),
                    static_cast<unsigned long>(minutes % 60));
  };
  if (book && bookStats.available) {
    formatMinutes(timeRead, sizeof(timeRead), bookStats.totalSeconds / 60);
    valueAvailable[0] = true;
    const auto eta = InkPointHomeGeometry::estimateEtas(
        bookStats.totalSeconds, bookStats.forwardPages, bookStats.etaConfident(),
        chapterProgress.pagesLeft(), chapterProgress.available, book->progressPercent);
    if (eta.chapterMinutes) {
      formatMinutes(chapterLeft, sizeof(chapterLeft), *eta.chapterMinutes);
      valueAvailable[1] = true;
    }
    if (eta.bookMinutes) {
      formatMinutes(bookLeft, sizeof(bookLeft), *eta.bookMinutes);
      valueAvailable[2] = true;
    }
  }
  const char* values[3] = {timeRead, chapterLeft, bookLeft};
  const auto statsLayout = InkPointHomeGeometry::statsLayout(rightX, statStart, kInkStatsStep);
  const int statX[3] = {statsLayout.timeX, statsLayout.chapterX, statsLayout.bookX};
  const int statY[3] = {statsLayout.timeY, statsLayout.chapterY, statsLayout.bookY};
  for (int i = 0; i < 3; ++i) {
    // Whole-book ETA is the only optional group: without a confident measured
    // pace it is hidden, rather than implying precision with a permanent slot.
    if (i == 2 && !valueAvailable[i]) continue;
    renderer.drawText(UI_10_FONT_ID, statX[i], statY[i], labels[i]);
    if (valueAvailable[i]) renderer.drawText(CAVEAT_18_FONT_ID, statX[i], statY[i] + 20, values[i]);
    else renderer.drawText(UI_12_FONT_ID, statX[i], statY[i] + 20, "\xE2\x80\x94");
  }
  drawStatsChevron(renderer, statsLayout.chevronY);

  // The local-date quote is selected from a 366-record audited shuffled deck.
  // The day comes from the same RTC-plus-user-offset date the stats tracker
  // rolls over on -- libc time() is only set as a side effect of an NTP sync and
  // reverts to 1970 on reboot, so localtime() served a stale quote indefinitely.
  const int today = READING_STATS.getCurrentDate();
  const int quoteDay = today > 0 ? DailyQuote::dayOfYearFromYmd(today) : -1;
  const auto& daily =
      quoteDay >= 0 ? DailyQuote::select(today / 10000, quoteDay) : DailyQuote::select(2026, 0);
  const auto quoteLines = wrapWords(renderer, CAVEAT_15_FONT_ID, daily.quote, 400, 3);
  // Attribution is intentionally one line: quote and attribution are centred
  // as one measured block in the actual free band, not a guessed screen area.
  const std::string attribution = renderer.truncatedText(SMALL_FONT_ID,
                                                           DailyQuote::attributionLine(daily).c_str(), 400);
  constexpr int quoteToAttribution = 16;
  const int quoteLineHeight = renderer.getLineHeight(CAVEAT_15_FONT_ID);
  const int attributionLineHeight = renderer.getLineHeight(SMALL_FONT_ID);
  const auto quoteLayout = InkPointHomeGeometry::centerQuoteBlock(
      coverY + coverH + InkPointHomeGeometry::kQuoteBandTopOffsetFromCoverBottom,
      InkPointShell::kFooterTop,
      static_cast<int>(quoteLines.size()),
      quoteLineHeight, quoteToAttribution, attributionLineHeight,
      renderer.getFontAscenderSize(CAVEAT_15_FONT_ID), renderer.getFontAscenderSize(SMALL_FONT_ID));
  int quoteBaseline = quoteLayout.quoteBaseline;
  for (const auto& line : quoteLines) {
    drawCentered(renderer, CAVEAT_15_FONT_ID, 240, quoteBaseline, line.c_str());
    quoteBaseline += quoteLineHeight;
  }
  drawCentered(renderer, SMALL_FONT_ID, 240, quoteLayout.attributionBaseline, attribution.c_str());

  InkPointShell::drawFooter(renderer, InkPointShell::Destination::Home, inkPointFocus - 1);
  // The approved Home intentionally leaves the cover unframed. Touch and
  // Confirm still activate the full cover lane; focus must not add an outline.
  if (inkPointFocus == 7)
    renderer.drawRect(kInkStats.x, kInkStats.y, kInkStats.width, kInkStats.height, 2, true);

  renderer.displayBuffer(cleanInitialRefresh && !firstRenderDone ? HalDisplay::HALF_REFRESH : HalDisplay::FAST_REFRESH);

  // Thumbnails are cached per requested height, and this layout asks for a
  // height the classic Home never requests, so the first InkPoint render has to
  // generate them itself or the cover silently stays absent forever.
  if (!firstRenderDone) {
    firstRenderDone = true;
    requestUpdate();
  } else if (!recentsLoaded && !recentsLoading) {
    recentsLoading = true;
    loadRecentCovers(kInkCoverLane.height);
  }
}
