#include "HomeActivity.h"

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
#include "components/UITheme.h"
#include "components/InkPointShell.h"
#include "fontIds.h"
#include "network/HardcoverRating.h"
#include "util/BookProgressBadge.h"
#include "util/BookReadingStats.h"

namespace {
const Rect kInkCover{20, 99, 290, 438};
const Rect kInkFooter{14, 728, 452, 60};
constexpr int kInkTabWidth = 72;
constexpr int kInkTabGap = 4;

int textTop(const GfxRenderer& renderer, int fontId, int baseline) {
  return baseline - renderer.getFontAscenderSize(fontId);
}

void drawCentered(const GfxRenderer& renderer, int fontId, int cx, int baseline, const char* text,
                  bool black = true, EpdFontFamily::Style style = EpdFontFamily::REGULAR) {
  renderer.drawText(fontId, cx - renderer.getTextWidth(fontId, text, style) / 2,
                    textTop(renderer, fontId, baseline), text, black, style);
}

void drawStar(const GfxRenderer& renderer, int cx, int cy, bool filled, bool partial) {
  constexpr int px[10] = {0, 3, 10, 5, 6, 0, -6, -5, -10, -3};
  constexpr int py[10] = {-10, -3, -3, 2, 9, 5, 9, 2, -3, -3};
  for (int i = 0; i < 10; ++i) renderer.drawLine(cx + px[i], cy + py[i], cx + px[(i + 1) % 10], cy + py[(i + 1) % 10]);
  if (filled) {
    for (int y = cy - 7; y <= cy + 5; ++y) {
      const int half = 2 + (y - (cy - 7)) / 2;
      renderer.drawLine(cx - std::min(8, half), y, cx + std::min(8, half), y);
    }
  } else if (partial) {
    for (int y = cy - 7; y <= cy + 5; y += 2) renderer.drawLine(cx - 7, y, cx - 1, y);
  }
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
  size_t start = 0;
  while (start < text.size() && lines.size() < maxLines) {
    const size_t end = text.find(' ', start);
    const std::string word = text.substr(start, end == std::string::npos ? std::string::npos : end - start);
    const std::string candidate = line.empty() ? word : line + " " + word;
    if (!line.empty() && renderer.getTextWidth(fontId, candidate.c_str(), style) > maxWidth) {
      lines.push_back(line);
      line = word;
    } else {
      line = candidate;
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
  if (firstRenderDone && !ratingRefreshAttempted && !recentBooks.empty()) {
    ratingRefreshAttempted = true;
    const auto now = static_cast<int64_t>(std::time(nullptr));
    const bool stale = !rating || now <= 0 || rating->fetchedAt <= 0 || now - rating->fetchedAt >= 24 * 60 * 60;
    if (stale) {
      const auto& book = recentBooks.front();
      auto refreshed = HardcoverRating::refresh({book.path, book.isbn, book.title, book.author}, now);
      if (refreshed) {
        rating = std::move(refreshed);
        requestUpdate();
      }
    }
  }

  buttonNavigator.onNext([this] { inkPointFocus = ButtonNavigator::nextIndex(inkPointFocus, 7); requestUpdate(); });
  buttonNavigator.onPrevious([this] { inkPointFocus = ButtonNavigator::previousIndex(inkPointFocus, 7); requestUpdate(); });

  auto activate = [this](int target) {
    switch (target) {
      case 0: if (!recentBooks.empty()) onSelectBook(recentBooks.front().path); break;
      case 1: break;  // Home is already active.
      case 2: onRecentsOpen(); break;      // Library/recent-books catalogue.
      case 3: onFileBrowserOpen(); break;
      case 4: onGamesOpen(); break;
      case 5: onFileTransferOpen(); break;
      case 6: onSettingsOpen(); break;
    }
  };

  int x = 0, y = 0;
  if (mappedInput.wasScreenTapped(x, y)) {
    if (!recentBooks.empty() && x >= kInkCover.x && x < kInkCover.x + kInkCover.width &&
        y >= kInkCover.y && y < kInkCover.y + kInkCover.height) {
      activate(0);
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
  int coverW = kInkCover.width;
  int coverH = kInkCover.height;
  if (book && !book->coverBmpPath.empty()) {
    const auto& metrics = UITheme::getInstance().getMetrics();
    const std::string thumbPath = UITheme::getCoverThumbPath(book->coverBmpPath, metrics.homeCoverHeight);
    HalFile file;
    if (Storage.openFileForRead("HOME", thumbPath, file)) {
      Bitmap bitmap(file);
      if (bitmap.parseHeaders() == BmpReaderError::Ok) {
        coverW = std::min(kInkCover.width, (kInkCover.height * bitmap.getWidth()) / bitmap.getHeight());
        coverH = std::min(kInkCover.height, (coverW * bitmap.getHeight()) / bitmap.getWidth());
        renderer.drawBitmap(bitmap, kInkCover.x, kInkCover.y, coverW, coverH);
        coverDrawn = true;
      }
    }
  }
  if (!coverDrawn) {
    renderer.drawRect(kInkCover.x, kInkCover.y, coverW, coverH, 2, true);
    if (book) {
      const int cx = kInkCover.x + coverW / 2;
      const auto titleLines = wrapWords(renderer, NOTOSERIF_18_FONT_ID, book->title, coverW - 36, 4,
                                        EpdFontFamily::BOLD);
      const int firstBaseline = kInkCover.y + 165 - static_cast<int>(titleLines.size() - 1) * 16;
      for (size_t i = 0; i < titleLines.size(); ++i)
        drawCentered(renderer, NOTOSERIF_18_FONT_ID, cx, firstBaseline + static_cast<int>(i) * 32,
                     titleLines[i].c_str(), true, EpdFontFamily::BOLD);
      const auto authorLines = wrapWords(renderer, UI_12_FONT_ID, book->author, coverW - 36, 2);
      for (size_t i = 0; i < authorLines.size(); ++i)
        drawCentered(renderer, UI_12_FONT_ID, cx, kInkCover.y + 235 + static_cast<int>(i) * 24,
                     authorLines[i].c_str());
    } else {
      drawCentered(renderer, UI_12_FONT_ID, kInkCover.x + coverW / 2, kInkCover.y + 210, "No recent book");
    }
  }

  // Thin progress bar is physically attached to the cover's lower edge.
  const int progressY = kInkCover.y + coverH;
  renderer.drawRect(kInkCover.x, progressY, coverW, 4);
  if (book && book->progressPercent >= 0) {
    renderer.fillRect(kInkCover.x + 1, progressY + 1,
                      std::max(0, std::min(coverW - 2, (coverW - 2) * book->progressPercent / 100)), 2);
  }

  if (book) {
    const int rightX = 330;
    const std::string title = upper(book->title);
    // Narrow-column title: wrap by words rather than clipping. UI face remains
    // deliberately separate from Caveat's heading/accent role.
    std::vector<std::string> lines;
    std::string line;
    size_t start = 0;
    while (start < title.size() && lines.size() < 4) {
      size_t end = title.find(' ', start);
      std::string word = title.substr(start, end == std::string::npos ? std::string::npos : end - start);
      std::string candidate = line.empty() ? word : line + " " + word;
      if (!line.empty() && renderer.getTextWidth(NOTOSANS_14_FONT_ID, candidate.c_str(), EpdFontFamily::BOLD) > 130) {
        lines.push_back(line); line = word;
      } else line = candidate;
      if (end == std::string::npos) break;
      start = end + 1;
    }
    if (!line.empty() && lines.size() < 4) lines.push_back(line);
    int y = 103;
    for (const auto& titleLine : lines) {
      renderer.drawText(NOTOSANS_14_FONT_ID, rightX, y, titleLine.c_str(), true, EpdFontFamily::BOLD);
      y += 31;
    }
    renderer.drawText(UI_10_FONT_ID, rightX, y + 5, book->author.c_str());
    if (rating && rating->publicationYear > 0) {
      char year[8]; std::snprintf(year, sizeof(year), "%d", rating->publicationYear);
      renderer.drawText(UI_10_FONT_ID, rightX, y + 27, year);
    }
    if (rating) {
      const int whole = rating->valueX100 / 100;
      const int fraction = rating->valueX100 % 100;
      for (int i = 0; i < 5; ++i) drawStar(renderer, rightX + 11 + i * 24, y + 62, i < whole, i == whole && fraction > 0);
    }

    constexpr const char* labels[3] = {"TIME READ", "CHAPTER LEFT", "BOOK LEFT"};
    char timeRead[16], chapterLeft[16] = "\xe2\x80\x94", bookLeft[16] = "\xe2\x80\x94";
    const uint32_t minutes = bookStats.totalSeconds / 60;
    std::snprintf(timeRead, sizeof(timeRead), "%luh %02lum", static_cast<unsigned long>(minutes / 60),
                  static_cast<unsigned long>(minutes % 60));
    if (bookStats.etaConfident() && book->progressPercent > 0 && book->progressPercent < 100) {
      const uint32_t totalPagesEstimate = bookStats.forwardPages * 100U / static_cast<uint32_t>(book->progressPercent);
      const uint32_t remainingPages = totalPagesEstimate > bookStats.forwardPages ? totalPagesEstimate - bookStats.forwardPages : 0;
      const uint32_t leftMinutes = remainingPages * bookStats.secondsPerPage() / 60;
      std::snprintf(bookLeft, sizeof(bookLeft), "%luh %02lum", static_cast<unsigned long>(leftMinutes / 60),
                    static_cast<unsigned long>(leftMinutes % 60));
    }
    const char* values[3] = {timeRead, chapterLeft, bookLeft};
    for (int i = 0; i < 3; ++i) {
      const int statY = 326 + i * 66;
      renderer.drawText(UI_10_FONT_ID, rightX, statY, labels[i]);
      renderer.drawText(CAVEAT_18_FONT_ID, rightX, statY + 20, values[i]);
    }
  }

  // Accepted two-line quote and one-line attribution, vertically balanced in
  // the band below the cover and above persistent navigation.
  drawCentered(renderer, CAVEAT_15_FONT_ID, 240, 601,
               "Eliminate all other factors, and");
  drawCentered(renderer, CAVEAT_15_FONT_ID, 240, 626,
               "the one which remains must be the truth.");
  drawCentered(renderer, SMALL_FONT_ID, 240, 653,
               "Sherlock Holmes, The Sign of the Four, Arthur Conan Doyle");

  InkPointShell::drawFooter(renderer, InkPointShell::Destination::Home, inkPointFocus - 1);
  if (inkPointFocus == 0 && book) renderer.drawRect(kInkCover.x - 3, kInkCover.y - 3, coverW + 6, coverH + 10, 2, true);

  renderer.displayBuffer(cleanInitialRefresh && !firstRenderDone ? HalDisplay::HALF_REFRESH : HalDisplay::FAST_REFRESH);
  firstRenderDone = true;
}
