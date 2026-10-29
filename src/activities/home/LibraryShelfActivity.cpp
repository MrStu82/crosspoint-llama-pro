#include "LibraryShelfActivity.h"

#include <Bitmap.h>
#include <Epub.h>
#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <Xtc.h>

#include <algorithm>
#include <memory>

#include "BookCardActivity.h"
#include "MappedInputManager.h"
#include "components/InkPointShell.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "network/HardcoverRating.h"
#include "util/BookDetails.h"
#include "util/BookProgressBadge.h"

namespace {
constexpr int kShelfY = 478;  // the rule every cover stands on
constexpr int kCentreX = 120;
constexpr int kCentreW = 240;
constexpr int kCentreH = 360;
constexpr int kSideW = 96;
constexpr int kSideH = 144;
constexpr int kLeftX = 8;
constexpr int kRightX = 480 - 8 - kSideW;

void drawCentred(const GfxRenderer& r, const int fontId, const int y, const std::string& text,
                 const EpdFontFamily::Style style = EpdFontFamily::REGULAR) {
  const std::string fitted = r.truncatedText(fontId, text.c_str(), 440, style);
  r.drawText(fontId, 240 - r.getTextWidth(fontId, fitted.c_str(), style) / 2, y, fitted.c_str(), true, style);
}
}  // namespace

namespace LibraryCover {

bool ensureThumb(RecentBook& book) {
  if (book.coverBmpPath.empty()) return false;
  if (Storage.exists(UITheme::getCoverThumbPath(book.coverBmpPath, InkPointShell::kHomeCoverHeight).c_str()))
    return true;
  bool ok = false;
  if (FsHelpers::hasEpubExtension(book.path)) {
    Epub epub(book.path, "/.crosspoint");
    ok = epub.load(false, true) && epub.generateThumbBmp(InkPointShell::kHomeCoverHeight);
  } else if (FsHelpers::hasXtcExtension(book.path)) {
    Xtc xtc(book.path, "/.crosspoint");
    ok = xtc.load() && xtc.generateThumbBmp(InkPointShell::kHomeCoverHeight);
  }
  if (!ok) {
    RECENT_BOOKS.updateBook(book.path, book.title, book.author, "");
    book.coverBmpPath.clear();
  }
  return ok;
}

int draw(const GfxRenderer& renderer, const RecentBook& book, const int x, const int y, const int maxW,
         const int maxH) {
  if (!book.coverBmpPath.empty()) {
    HalFile file;
    if (Storage.openFileForRead("SHELF", UITheme::getCoverThumbPath(book.coverBmpPath, InkPointShell::kHomeCoverHeight),
                                file)) {
      Bitmap bitmap(file);
      const int bw = bitmap.parseHeaders() == BmpReaderError::Ok ? bitmap.getWidth() : 0;
      const int bh = bitmap.getHeight();
      if (bw > 0 && bh > 0) {
        const int w = std::min(bw, std::min(maxW, (maxH * bw) / bh));
        const int h = std::min(bh, std::min(maxH, (w * bh) / bw));
        renderer.drawBitmap(bitmap, x + (maxW - w) / 2, y + maxH - h, w, h);
        return w;
      }
    }
  }
  // Typographic cover: the font scales with the box so side covers stay legible.
  const bool large = maxW >= 160;
  const int titleFont = large ? UI_12_FONT_ID : SMALL_FONT_ID;
  const int titleStep = renderer.getLineHeight(titleFont);
  renderer.drawRect(x, y, maxW, maxH, 2, true);
  const auto title = renderer.wrappedText(titleFont, book.title.c_str(), maxW - 16, large ? 5 : 4, EpdFontFamily::BOLD);
  int ty = y + maxH / 3 - static_cast<int>(title.size()) * titleStep / 2;
  for (const auto& line : title) {
    renderer.drawText(titleFont, x + (maxW - renderer.getTextWidth(titleFont, line.c_str(), EpdFontFamily::BOLD)) / 2,
                      ty, line.c_str(), true, EpdFontFamily::BOLD);
    ty += titleStep;
  }
  renderer.drawLine(x + maxW / 2 - 20, ty + 8, x + maxW / 2 + 20, ty + 8, true);
  const std::string author = renderer.truncatedText(SMALL_FONT_ID, book.author.c_str(), maxW - 16);
  renderer.drawText(SMALL_FONT_ID, x + (maxW - renderer.getTextWidth(SMALL_FONT_ID, author.c_str())) / 2, ty + 18,
                    author.c_str());
  return maxW;
}

}  // namespace LibraryCover

void LibraryShelfActivity::onEnter() {
  Activity::onEnter();
  if (RECENT_BOOKS.pruneMissing()) RECENT_BOOKS.saveToFile();
  books = RECENT_BOOKS.getBooks();
  for (auto& book : books) book.progressPercent = BookProgressBadge::read(book.path).value_or(-1);
  select(0);
}

void LibraryShelfActivity::onExit() {
  Activity::onExit();
  books.clear();
}

void LibraryShelfActivity::select(const size_t next) {
  index = next;
  metaLine.clear();
  if (books.empty()) {
    requestUpdate();
    return;
  }
  // Cached details only: the shelf never opens an EPUB just to flick past it.
  const auto& book = books[index];
  const auto details = BookDetailsStore::loadCached(book.path);
  const auto rating = HardcoverRating::loadLastGood({book.path, book.isbn, book.title, book.author});
  std::string year = details && details->date.size() >= 4 ? details->date.substr(0, 4) : "";
  if (year.empty() && rating && rating->publicationYear > 0) year = std::to_string(rating->publicationYear);
  const uint32_t pages = details && details->printPages ? details->printPages : (rating ? rating->pages : 0);
  const auto add = [this](const std::string& part) {
    if (part.empty()) return;
    if (!metaLine.empty()) metaLine += " \xC2\xB7 ";
    metaLine += part;
  };
  add(year);
  add(pages ? std::to_string(pages) + " pages" : "");
  add(book.progressPercent >= 0 ? std::to_string(book.progressPercent) + "% read" : "");
  requestUpdate();
}

void LibraryShelfActivity::openCard() {
  if (books.empty()) return;
  startActivityForResult(std::make_unique<BookCardActivity>(renderer, mappedInput, books[index]),
                         [this](const ActivityResult&) { select(index); });
}

void LibraryShelfActivity::loop() {
  if (const auto destination = InkPointShell::tappedDestination(mappedInput)) {
    InkPointShell::navigate(*destination);
    return;
  }
  if (InkPointShell::handleLibraryToggle(mappedInput, true)) return;
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    onGoHome();
    return;
  }
  if (books.empty()) return;

  const size_t count = books.size();
  const auto next = [this, count] { select((index + 1) % count); };
  const auto previous = [this, count] { select((index + count - 1) % count); };

  const auto swipe = mappedInput.wasSwipe();
  if (swipe == MappedInputManager::SwipeDir::Left) return next();
  if (swipe == MappedInputManager::SwipeDir::Right) return previous();
  if (mappedInput.wasTapInRect(kCentreX, InkPointShell::kContentTop, kCentreW, kShelfY - InkPointShell::kContentTop))
    return openCard();
  if (count > 1 && mappedInput.wasTapInRect(0, InkPointShell::kContentTop, kCentreX, kShelfY - InkPointShell::kContentTop))
    return previous();
  if (count > 1 &&
      mappedInput.wasTapInRect(kCentreX + kCentreW, InkPointShell::kContentTop, 480 - kCentreX - kCentreW,
                               kShelfY - InkPointShell::kContentTop))
    return next();
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) return openCard();
  buttonNavigator.onNextRelease(next);
  buttonNavigator.onPreviousRelease(previous);
}

void LibraryShelfActivity::render(RenderLock&&) {
  renderer.clearScreen();
  InkPointShell::drawHeader(renderer, "Library");
  InkPointShell::drawLibraryToggle(renderer, true);

  if (books.empty()) {
    drawCentred(renderer, UI_12_FONT_ID, 300, "No books yet");
  } else {
    const size_t count = books.size();
    // Neighbours only when they are distinct books.
    if (count > 1) {
      LibraryCover::draw(renderer, books[(index + count - 1) % count], kLeftX, kShelfY - kSideH, kSideW, kSideH);
      if (count > 2)
        LibraryCover::draw(renderer, books[(index + 1) % count], kRightX, kShelfY - kSideH, kSideW, kSideH);
    }
    LibraryCover::draw(renderer, books[index], kCentreX, kShelfY - kCentreH, kCentreW, kCentreH);
    renderer.fillRect(0, kShelfY, 480, 3, true);
    renderer.fillRectDither(0, kShelfY + 3, 480, 8, Color::LightGray);

    const auto& book = books[index];
    drawCentred(renderer, UI_12_FONT_ID, kShelfY + 26, book.title, EpdFontFamily::BOLD);
    drawCentred(renderer, UI_10_FONT_ID, kShelfY + 56, book.author);
    drawCentred(renderer, SMALL_FONT_ID, kShelfY + 84, metaLine);
    drawCentred(renderer, SMALL_FONT_ID, kShelfY + 124, std::to_string(index + 1) + " of " + std::to_string(count));
  }
  InkPointShell::drawFooter(renderer, InkPointShell::Destination::Library);
  renderer.displayBuffer();

  // Generate missing thumbnails for the visible covers after the first paint
  // (as Home does), then repaint once with them.
  if (books.empty()) return;
  const size_t count = books.size();
  bool generated = false;
  for (const size_t i : {index, (index + 1) % count, (index + count - 1) % count}) {
    auto& book = books[i];
    if (book.coverBmpPath.empty() ||
        Storage.exists(UITheme::getCoverThumbPath(book.coverBmpPath, InkPointShell::kHomeCoverHeight).c_str()))
      continue;
    LibraryCover::ensureThumb(book);
    generated = true;
  }
  if (generated) requestUpdate();
}
