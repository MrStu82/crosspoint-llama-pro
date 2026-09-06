#include "TxtReaderActivity.h"
#include "TxtContentIdentity.h"

#include <BidiUtils.h>
#include <FontCacheManager.h>
#include <SdCardFont.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Serialization.h>
#include <Utf8.h>

#include "CrossPointSettings.h"
#include "CrossPointState.h"
#include "MappedInputManager.h"
#include "ProgressFile.h"
#include "ReaderUtils.h"
#include "ReaderToolsActivity.h"
#include "RecentBooksStore.h"
#include "activities/home/StatsManager.h"
#include "activities/reader/EpubReaderPercentSelectionActivity.h"
#include "activities/settings/TextSettingsActivity.h"
#include "util/BookReadingStats.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/BookProgressBadge.h"

namespace {
constexpr size_t CHUNK_SIZE = 8 * 1024;  // 8KB chunk for reading
}  // namespace

void TxtReaderActivity::onEnter() {
  Activity::onEnter();

  if (!txt) {
    return;
  }

  ReaderUtils::applyOrientation(renderer, SETTINGS.orientation);

  txt->setupCacheDir();

  // Save current txt as last opened file and add to recent books
  auto filePath = txt->getPath();
  auto fileName = filePath.substr(filePath.rfind('/') + 1);
  APP_STATE.openEpubPath = filePath;
  APP_STATE.saveToFile();
  RECENT_BOOKS.addBook(filePath, fileName, "", "");
  StatsManager::getInstance().incrementBooksOpened();
  sessionStartTime = millis();

  // Trigger first update
  requestUpdate();
}

void TxtReaderActivity::onExit() {
  dwellTracker.pause();
  Activity::onExit();

  if (txt && totalPages > 0)
    BookReadingStats::updatePosition(txt->getPath(), rateFingerprint(), BookReadingRate::ContentBasis::ExactPages,
                                     static_cast<uint32_t>(std::max(0, totalPages - currentPage - 1)), 0);

  if (sessionStartTime != 0UL) {
    const unsigned long secs = (millis() - sessionStartTime) / 1000;
    StatsManager::getInstance().addReadingTimeSeconds(secs);
    BookReadingStats::add(txt->getPath(), secs, 0);
    sessionStartTime = 0UL;
  }
  StatsManager::getInstance().save();

  // Reset orientation back to portrait for the rest of the UI
  renderer.setOrientation(GfxRenderer::Orientation::Portrait);

  pageOffsets.clear();
  currentPageLines.clear();
  APP_STATE.readerActivityLoadCount = 0;
  APP_STATE.saveToFile();
  txt.reset();
}

uint32_t TxtReaderActivity::rateFingerprint() const {
  const uint32_t layout = BookReadingRate::layoutFingerprint(2, viewportWidth, linesPerPage,
                                             SETTINGS.getReaderFontId(), SETTINGS.fontPointSize,
                                             0, SETTINGS.screenMargin,
                                             SETTINGS.paragraphAlignment, SETTINGS.orientation, 2);
  uint32_t fingerprint = BookReadingRate::hashValue(layout, BookReadingRate::hashString(SETTINGS.sdFontFamilyName));
  for (uint8_t byte : cacheIdentity.content) fingerprint = BookReadingRate::hashValue(fingerprint, byte);
  for (uint8_t byte : cacheIdentity.font) fingerprint = BookReadingRate::hashValue(fingerprint, byte);
  return fingerprint == 0 ? 1 : fingerprint;
}

uint32_t TxtReaderActivity::visiblePageKey() const {
  return BookReadingRate::pageKey(rateFingerprint(), static_cast<uint32_t>(std::max(0, currentPage)));
}

void TxtReaderActivity::onUncovered() {
  if (txt && initialized && !pageOffsets.empty()) dwellTracker.markVisible(millis(), visiblePageKey());
}

void TxtReaderActivity::recordQualifiedForward(const uint16_t dwellSeconds) {
  if (!txt || !contentIdentityReady || totalPages <= 0) return;
  BookReadingStats::recordQualifiedPage(
      txt->getPath(), {dwellSeconds, rateFingerprint(), BookReadingRate::hashString(txt->getPath().c_str()),
                       BookReadingRate::ContentBasis::ExactPages,
                       static_cast<uint32_t>(std::max(0, totalPages - currentPage - 1)), 0, 0});
}

void TxtReaderActivity::openReaderTools() {
  startActivityForResult(std::make_unique<ReaderToolsActivity>(renderer, mappedInput, ReaderToolsActivity::Format::Txt),
                         [this](const ActivityResult& result) {
                           if (result.isCancelled) return;
                           const auto action = static_cast<ReaderToolsActivity::Action>(
                               std::get<MenuResult>(result.data).action);
                           if (action == ReaderToolsActivity::Action::GoToPercent) {
                             const int percent = totalPages > 1 ? currentPage * 100 / (totalPages - 1) : 0;
                             startActivityForResult(
                                 std::make_unique<EpubReaderPercentSelectionActivity>(renderer, mappedInput, percent),
                                 [this](const ActivityResult& selected) {
                                   if (!selected.isCancelled && totalPages > 0) {
                                     currentPage =
                                         std::get<PercentResult>(selected.data).percent * (totalPages - 1) / 100;
                                     requestUpdate();
                                   }
                                 });
                           } else if (action == ReaderToolsActivity::Action::TextSettings) {
                             saveProgress();
                             startActivityForResult(
                                 std::make_unique<TextSettingsActivity>(renderer, mappedInput, nullptr,
                                                                         TextSettingsActivity::Tab::Family),
                                 [this](const ActivityResult&) {
                                   initialized = false;
                                   pageOffsets.clear();
                                   currentPageLines.clear();
                                   requestUpdate();
                                 });
                           }
                         });
}

void TxtReaderActivity::loop() {
  if (ReaderUtils::handleBackNavigation(mappedInput, activityManager, txt ? txt->getPath().c_str() : "",
                                        {this, [](void* ctx) { static_cast<TxtReaderActivity*>(ctx)->onGoHome(); }})) {
    return;
  }

  const auto touch = ReaderUtils::detectTouchPageTurn(renderer, mappedInput);
  if (ReaderUtils::shouldOpenReaderTools(touch) && initialized && totalPages > 0) {
    openReaderTools();
    return;
  }
  auto [prevTriggered, nextTriggered, fromTilt] = ReaderUtils::detectPageTurn(mappedInput);
  prevTriggered = prevTriggered || touch.prev;
  nextTriggered = nextTriggered || touch.next;
  if (!prevTriggered && !nextTriggered) {
    return;
  }

  if (prevTriggered && currentPage > 0) {
    dwellTracker.pause();
    currentPage--;
    if (sessionStartTime != 0UL) {
      const unsigned long secs = (millis() - sessionStartTime) / 1000;
      StatsManager::getInstance().addReadingTimeSeconds(secs);
      BookReadingStats::add(txt->getPath(), secs, 0);
      sessionStartTime += secs * 1000;
    }
    // Backward turns (rereading) don't count as pages read.
    requestUpdate();
  } else if (nextTriggered) {
    if (currentPage < totalPages - 1) {
      const auto dwell = dwellTracker.takeQualifiedForward(millis(), visiblePageKey(), true);
      currentPage++;
      if (sessionStartTime != 0UL) {
      const unsigned long secs = (millis() - sessionStartTime) / 1000;
      StatsManager::getInstance().addReadingTimeSeconds(secs);
      BookReadingStats::add(txt->getPath(), secs, 0);
      sessionStartTime += secs * 1000;
      }
      StatsManager::getInstance().incrementPagesRead();
      if (dwell) recordQualifiedForward(*dwell);
      requestUpdate();
    } else {
      onGoHome();
    }
  }
}

void TxtReaderActivity::initializeReader() {
  reader_diagnostics::Scope profile(reader_diagnostics::Stage::TxtOpen);
  if (initialized) {
    return;
  }

  // Store current settings for cache validation
  cachedFontId = SETTINGS.getReaderFontId();
  cachedScreenMargin = SETTINGS.screenMargin;
  cachedParagraphAlignment = SETTINGS.paragraphAlignment;

  // Calculate viewport dimensions
  renderer.getOrientedViewableTRBL(&cachedOrientedMarginTop, &cachedOrientedMarginRight, &cachedOrientedMarginBottom,
                                   &cachedOrientedMarginLeft);
  cachedOrientedMarginTop += cachedScreenMargin;
  cachedOrientedMarginLeft += cachedScreenMargin;
  cachedOrientedMarginRight += cachedScreenMargin;
  cachedOrientedMarginBottom +=
      std::max(cachedScreenMargin, static_cast<uint8_t>(UITheme::getInstance().getStatusBarHeight()));

  viewportWidth = renderer.getScreenWidth() - cachedOrientedMarginLeft - cachedOrientedMarginRight;
  const int viewportHeight = renderer.getScreenHeight() - cachedOrientedMarginTop - cachedOrientedMarginBottom;
  const int lineHeight = renderer.getLineHeight(cachedFontId);

  linesPerPage = viewportHeight / lineHeight;
  if (linesPerPage < 1) linesPerPage = 1;

  LOG_DBG("TRS", "Viewport: %dx%d, lines per page: %d", viewportWidth, viewportHeight, linesPerPage);

  // Full digest only at open/reinitialization. Failed reads disable cache reuse.
  HalFile identityFile;
  const unsigned long digestStart = millis();
  contentIdentityReady = txt->getFileSize() <= UINT32_MAX &&
      Storage.openFileForRead("TRS", txt->getPath(), identityFile) &&
      identityFile.size() == txt->getFileSize() && txt_index::digest(identityFile, cacheIdentity.content);
  identityFile.close();
  cacheIdentity.font.fill(0);
  if (const auto* font = renderer.sdCardFontSource(cachedFontId)) {
    contentIdentityReady = contentIdentityReady &&
        Storage.openFileForRead("TRS", font->sourcePath(), identityFile) &&
        txt_index::digest(identityFile, cacheIdentity.font);
    identityFile.close();
  }
  cacheIdentity.fileSize = static_cast<uint32_t>(txt->getFileSize());
  cacheIdentity.layout = rateFingerprint();
  LOG_DBG("TRS", "Content identity: bytes=%zu ms=%lu valid=%d", txt->getFileSize(),
          millis() - digestStart, contentIdentityReady);

  // Try to load cached page index first
  if (!loadPageIndexCache()) {
    // Cache not found, build page index
    buildPageIndex();
    // Save to cache for next time
    savePageIndexCache();
  }

  // Load saved progress
  loadProgress();

  initialized = true;
}

void TxtReaderActivity::buildPageIndex() {
  pageOffsets.clear();
  pageOffsets.push_back(0);  // First page starts at offset 0

  size_t offset = 0;
  const size_t fileSize = txt->getFileSize();

  LOG_DBG("TRS", "Building page index for %zu bytes...", fileSize);

  GUI.drawPopup(renderer, tr(STR_INDEXING));

  while (offset < fileSize) {
    std::vector<std::string> tempLines;
    size_t nextOffset = offset;

    if (!loadPageAtOffset(offset, tempLines, nextOffset)) {
      break;
    }

    if (nextOffset <= offset) {
      // No progress made, avoid infinite loop
      break;
    }

    offset = nextOffset;
    if (offset < fileSize) {
      pageOffsets.push_back(offset);
    }

    // Yield to other tasks periodically
    if (pageOffsets.size() % 20 == 0) {
      vTaskDelay(1);
    }
  }

  totalPages = pageOffsets.size();
  LOG_DBG("TRS", "Built page index: %d pages", totalPages);
}

bool TxtReaderActivity::loadPageAtOffset(size_t offset, std::vector<std::string>& outLines, size_t& nextOffset) {
  reader_diagnostics::Scope profile(reader_diagnostics::Stage::TxtLayout);
  outLines.clear();
  const size_t fileSize = txt->getFileSize();

  if (offset >= fileSize) {
    return false;
  }

  // Read a chunk from file
  size_t chunkSize = std::min(CHUNK_SIZE, fileSize - offset);
  auto* buffer = static_cast<uint8_t*>(malloc(chunkSize + 1));
  if (!buffer) {
    LOG_ERR("TRS", "Failed to allocate %zu bytes", chunkSize);
    return false;
  }

  if (!txt->readContent(buffer, offset, chunkSize)) {
    free(buffer);
    return false;
  }
  buffer[chunkSize] = '\0';

  // Prime the SD card font's advance table with this chunk's codepoints.
  // Without this, every getTextAdvanceX() call in the wrap loop below triggers
  // on-demand glyph loads through the 8-slot overflow ring buffer, which
  // thrashes for any text with more than 8 unique chars (i.e. all English),
  // floods the heap with short-lived bitmap allocations, and eventually
  // corrupts FreeRTOS state. The advance table persists across calls per
  // font, so the cost amortizes to ~ASCII-size after the first chunk.
  if (renderer.isSdCardFont(cachedFontId)) {
    renderer.ensureSdCardFontReady(cachedFontId, reinterpret_cast<const char*>(buffer), /*styleMask=*/0x01);
  }

  // Parse lines from buffer
  size_t pos = 0;

  while (pos < chunkSize && static_cast<int>(outLines.size()) < linesPerPage) {
    // Find end of line
    size_t lineEnd = pos;
    while (lineEnd < chunkSize && buffer[lineEnd] != '\n') {
      lineEnd++;
    }

    // Check if we have a complete line
    bool lineComplete = (lineEnd < chunkSize) || (offset + lineEnd >= fileSize);

    if (!lineComplete && static_cast<int>(outLines.size()) > 0) {
      // Incomplete line and we already have some lines, stop here
      break;
    }

    // Calculate the actual length of line content in the buffer (excluding newline)
    size_t lineContentLen = lineEnd - pos;

    // Check for carriage return
    bool hasCR = (lineContentLen > 0 && buffer[pos + lineContentLen - 1] == '\r');
    size_t displayLen = hasCR ? lineContentLen - 1 : lineContentLen;

    // Extract line content for display (without CR/LF)
    std::string line(reinterpret_cast<char*>(buffer + pos), displayLen);

    // Track position within this source line (in bytes from pos)
    size_t lineBytePos = 0;

    // Emit at least one visual line for each source line (including blank lines),
    // then continue with wrapping when needed.
    do {
      if (line.empty()) {
        outLines.emplace_back();
        break;
      }

      int lineWidth = renderer.getTextAdvanceX(cachedFontId, line.c_str(), EpdFontFamily::REGULAR);

      if (lineWidth <= viewportWidth) {
        outLines.push_back(line);
        lineBytePos = displayLen;  // Consumed entire display content
        line.clear();
        break;
      }

      // Find break point
      size_t breakPos = line.length();
      while (breakPos > 0 && renderer.getTextAdvanceX(cachedFontId, line.substr(0, breakPos).c_str(),
                                                      EpdFontFamily::REGULAR) > viewportWidth) {
        // Try to break at space
        size_t spacePos = line.rfind(' ', breakPos - 1);
        if (spacePos != std::string::npos && spacePos > 0) {
          breakPos = spacePos;
        } else {
          // Break at character boundary for UTF-8
          breakPos--;
          // Make sure we don't break in the middle of a UTF-8 sequence
          while (breakPos > 0 && (line[breakPos] & 0xC0) == 0x80) {
            breakPos--;
          }
        }
      }

      if (breakPos == 0) {
        breakPos = 1;
      }

      outLines.push_back(line.substr(0, breakPos));

      // Skip space at break point
      size_t skipChars = breakPos;
      if (breakPos < line.length() && line[breakPos] == ' ') {
        skipChars++;
      }
      lineBytePos += skipChars;
      line = line.substr(skipChars);
    } while (!line.empty() && static_cast<int>(outLines.size()) < linesPerPage);

    // Determine how much of the source buffer we consumed
    if (line.empty()) {
      // Fully consumed this source line, move past the newline
      pos = lineEnd + 1;
    } else {
      // Partially consumed - page is full mid-line
      // Move pos to where we stopped in the line (NOT past the line)
      pos = pos + lineBytePos;
      break;
    }
  }

  // Ensure we make progress even if calculations go wrong
  if (pos == 0 && !outLines.empty()) {
    // Fallback: at minimum, consume something to avoid infinite loop
    pos = 1;
  }

  nextOffset = offset + pos;

  // Make sure we don't go past the file
  if (nextOffset > fileSize) {
    nextOffset = fileSize;
  }

  free(buffer);

  return !outLines.empty();
}

void TxtReaderActivity::render(RenderLock&&) {
  if (!txt) {
    return;
  }

  // Initialize reader if not done
  if (!initialized) {
    initializeReader();
  }

  if (pageOffsets.empty()) {
    renderer.clearScreen();
    renderer.drawCenteredText(UI_12_FONT_ID, 300, tr(STR_EMPTY_FILE), true, EpdFontFamily::BOLD);
    renderer.displayBuffer();
    return;
  }

  // Bounds check
  if (currentPage < 0) currentPage = 0;
  if (currentPage >= totalPages) currentPage = totalPages - 1;

  // Load current page content
  size_t offset = pageOffsets[currentPage];
  size_t nextOffset;
  currentPageLines.clear();
  loadPageAtOffset(offset, currentPageLines, nextOffset);

  renderer.clearScreen();
  renderPage();

  // Save progress
  saveProgress();
  dwellTracker.markVisible(millis(), visiblePageKey());
}

void TxtReaderActivity::renderPage() {
  const int lineHeight = renderer.getLineHeight(cachedFontId);
  const int contentWidth = viewportWidth;

  // Render text lines with alignment
  auto renderLines = [&]() {
    int y = cachedOrientedMarginTop;
    for (const auto& line : currentPageLines) {
      if (!line.empty()) {
        int x = cachedOrientedMarginLeft;
        const bool lineIsRtl = BidiUtils::startsWithRtl(line.c_str(), BidiUtils::RTL_PARAGRAPH_PROBE_DEPTH);
        uint8_t effectiveAlignment = cachedParagraphAlignment;
        if (lineIsRtl && (effectiveAlignment == CrossPointSettings::LEFT_ALIGN ||
                          effectiveAlignment == CrossPointSettings::JUSTIFIED)) {
          effectiveAlignment = CrossPointSettings::RIGHT_ALIGN;
        }
        const int textWidth = renderer.getTextAdvanceX(cachedFontId, line.c_str(), EpdFontFamily::REGULAR);

        // Apply text alignment
        switch (effectiveAlignment) {
          case CrossPointSettings::LEFT_ALIGN:
          default:
            // x already set to left margin
            break;
          case CrossPointSettings::CENTER_ALIGN: {
            x = cachedOrientedMarginLeft + (contentWidth - textWidth) / 2;
            break;
          }
          case CrossPointSettings::RIGHT_ALIGN: {
            x = cachedOrientedMarginLeft + contentWidth - textWidth;
            break;
          }
          case CrossPointSettings::JUSTIFIED:
            // For plain text, justified is treated as left-aligned
            // (true justification would require word spacing adjustments)
            break;
        }

        renderer.drawText(cachedFontId, x, y, line.c_str());
      }
      y += lineHeight;
    }
  };

  // Font prewarm: scan pass accumulates text, then prewarm, then real render
  auto* fcm = renderer.getFontCacheManager();
  auto scope = fcm->createPrewarmScope();
  renderLines();  // scan pass — text accumulated, no drawing
  scope.endScanAndPrewarm();

  // BW rendering
  renderLines();
  renderStatusBar();

  ReaderUtils::displayWithRefreshCycle(renderer, pagesUntilFullRefresh);

  if (SETTINGS.textAntiAliasing) {
    ReaderUtils::renderAntiAliased(renderer, [&renderLines]() { renderLines(); });
  }
  // scope destructor clears font cache via FontCacheManager
}

void TxtReaderActivity::renderStatusBar() const {
  const float progress = totalPages > 0 ? (currentPage + 1) * 100.0f / totalPages : 0;
  std::string title;
  if (SETTINGS.statusBarSpec(CrossPointSettings::Edge::BOTTOM).showsTitle()) {
    title = txt->getTitle();
  }
  GUI.drawStatusBar(renderer, progress, currentPage + 1, totalPages, title, 0, 0, true, false, false,
                    CrossPointSettings::Edge::BOTTOM);

  std::string topTitle;
  if (SETTINGS.statusBarSpec(CrossPointSettings::Edge::TOP).showsTitle()) {
    topTitle = txt->getTitle();
  }
  GUI.drawStatusBar(renderer, progress, currentPage + 1, totalPages, topTitle, 0, 0, true, false, false,
                    CrossPointSettings::Edge::TOP);
}

void TxtReaderActivity::saveProgress() const {
  uint8_t data[4];
  data[0] = currentPage & 0xFF;
  data[1] = (currentPage >> 8) & 0xFF;
  data[2] = 0;
  data[3] = 0;
  if (!ProgressFile::writeAtomic(txt->getCachePath(), data, sizeof(data))) {
    LOG_ERR("TRS", "Failed to save progress: page %d", currentPage);
    return;
  }

  if (totalPages > 0) {
    int percent = static_cast<int>((currentPage + 1) * 100.0f / totalPages + 0.5f);
    if (percent > 100) percent = 100;
    BookProgressBadge::write(txt->getCachePath(), percent);
  }
}

void TxtReaderActivity::loadProgress() {
  HalFile f;
  if (ProgressFile::openForRead("TRS", txt->getCachePath() + "/progress.bin", f)) {
    uint8_t data[4];
    if (f.read(data, 4) == 4) {
      currentPage = data[0] + (data[1] << 8);
      if (currentPage >= totalPages) {
        currentPage = totalPages - 1;
      }
      if (currentPage < 0) {
        currentPage = 0;
      }
      LOG_DBG("TRS", "Loaded progress: page %d/%d", currentPage, totalPages);
    }
  }
}

bool TxtReaderActivity::loadPageIndexCache() {
  if (!contentIdentityReady) return false;
  HalFile f;
  if (!Storage.openFileForRead("TRS", txt->getCachePath() + "/index.bin", f) ||
      !txt_index::load(f, cacheIdentity, pageOffsets)) return false;
  totalPages = pageOffsets.size();
  return true;
}

void TxtReaderActivity::savePageIndexCache() const {
  if (!contentIdentityReady) return;
  HalFile f;
  if (!Storage.openFileForWrite("TRS", txt->getCachePath() + "/index.bin", f) ||
      !txt_index::save(f, cacheIdentity, pageOffsets))
    LOG_ERR("TRS", "Failed to save derived page index");
}

ScreenshotInfo TxtReaderActivity::getScreenshotInfo() const {
  ScreenshotInfo info;
  info.readerType = ScreenshotInfo::ReaderType::Txt;
  if (txt) {
    const std::string t = txt->getTitle();
    snprintf(info.title, sizeof(info.title), "%s", t.c_str());
  }
  info.currentPage = currentPage + 1;
  info.totalPages = totalPages;
  info.progressPercent = totalPages > 0 ? static_cast<int>((currentPage + 1) * 100.0f / totalPages + 0.5f) : 0;
  if (info.progressPercent > 100) info.progressPercent = 100;
  return info;
}
