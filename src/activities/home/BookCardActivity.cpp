#include "BookCardActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>
#include <vector>

#include "LibraryShelfActivity.h"
#include "MappedInputManager.h"
#include "components/InkPointShell.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "network/HardcoverRating.h"

namespace {
constexpr int kMargin = 20;
constexpr int kCoverW = 120;
constexpr int kCoverH = 180;
constexpr int kInfoX = kMargin + kCoverW + 16;
constexpr int kInfoW = 480 - kMargin - kInfoX;
constexpr int kReadY = 646;
constexpr int kReadH = 66;
constexpr int kTextW = 480 - 2 * kMargin;

// Section label, with a small black "sync" pill when the text came from the
// Library sync rather than the book itself. Returns the y below it.
int drawSectionLabel(const GfxRenderer& r, const int y, const char* label, const bool fromSync) {
  r.drawText(SMALL_FONT_ID, kMargin, y, label, true, EpdFontFamily::BOLD);
  if (fromSync) {
    const int x = kMargin + r.getTextWidth(SMALL_FONT_ID, label, EpdFontFamily::BOLD) + 8;
    const int w = r.getTextWidth(SMALL_FONT_ID, "sync") + 12;
    r.fillRoundedRect(x, y - 1, w, r.getLineHeight(SMALL_FONT_ID) + 2, 6, Color::Black);
    r.drawText(SMALL_FONT_ID, x + 6, y, "sync", false);
  }
  return y + r.getLineHeight(SMALL_FONT_ID) + 4;
}
}  // namespace

void BookCardActivity::onEnter() {
  Activity::onEnter();
  requestUpdate();
}

void BookCardActivity::load() {
  details = BookDetailsStore::load(book.path);
  const auto rating = HardcoverRating::loadLastGood({book.path, book.isbn, book.title, book.author});
  pages = details.printPages ? details.printPages : (rating ? rating->pages : 0);
  if (details.date.empty() && rating && rating->publicationYear > 0) year = std::to_string(rating->publicationYear);
  if (details.description.empty() && rating && !rating->description.empty()) {
    details.description = rating->description;
    blurbFromSync = true;
  }
  loaded = true;
}

void BookCardActivity::loop() {
  if (const auto destination = InkPointShell::tappedDestination(mappedInput)) {
    InkPointShell::navigate(*destination);
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Back) ||
      mappedInput.wasTapInRect(InkPointShell::kHeaderActionX, InkPointShell::kHeaderActionY,
                               InkPointShell::kHeaderActionWidth, InkPointShell::kHeaderActionHeight)) {
    finish();
    return;
  }
  const auto swipe = mappedInput.wasSwipe();
  const int pageCount = (details.description.empty() || details.bio.empty()) ? 1 : 2;
  const int target = (mappedInput.wasReleased(MappedInputManager::Button::PageForward) ||
                      swipe == MappedInputManager::SwipeDir::Left)
                         ? 1
                     : (mappedInput.wasReleased(MappedInputManager::Button::PageBack) ||
                        swipe == MappedInputManager::SwipeDir::Right)
                         ? 0
                         : page;
  if (target != page && target < pageCount) {
    page = target;
    requestUpdate();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm) ||
      (page == 0 && mappedInput.wasTapInRect(kMargin, kReadY, kTextW, kReadH)))
    onSelectBook(book.path);
}

void BookCardActivity::render(RenderLock&&) {
  if (!loaded) {
    // First open of a book reads its OPF/TOC once; later opens hit the SD cache.
    if (!BookDetailsStore::loadCached(book.path)) GUI.drawPopup(renderer, tr(STR_LOADING_POPUP));
    load();
  }
  renderer.clearScreen();
  InkPointShell::drawHeader(renderer, "Book");
  renderer.drawRoundedRect(InkPointShell::kHeaderActionX, InkPointShell::kHeaderActionY,
                           InkPointShell::kHeaderActionWidth, InkPointShell::kHeaderActionHeight, 2, 7, true);
  const char* back = "\xE2\x80\xB9 Back";
  renderer.drawText(UI_10_FONT_ID,
                    InkPointShell::kHeaderActionX +
                        (InkPointShell::kHeaderActionWidth - renderer.getTextWidth(UI_10_FONT_ID, back)) / 2,
                    InkPointShell::kHeaderActionY + 8, back);

  struct Section {
    const char* label;
    const std::string* text;
    bool fromSync;
  };
  std::vector<Section> sections;
  if (!details.description.empty()) sections.push_back({"ABOUT THIS BOOK", &details.description, blurbFromSync});
  if (!details.bio.empty()) sections.push_back({"ABOUT THE AUTHOR", &details.bio, details.bioFromSync});
  const int lineH = renderer.getLineHeight(UI_10_FONT_ID);
  const auto drawSection = [&](int y, const Section& section, const int bottom) {
    y = drawSectionLabel(renderer, y, section.label, section.fromSync);
    if (sections.size() > 1) {
      const char* indicator = page == 0 ? "1/2" : "2/2";
      renderer.drawText(SMALL_FONT_ID, 480 - kMargin - renderer.getTextWidth(SMALL_FONT_ID, indicator),
                        y - renderer.getLineHeight(SMALL_FONT_ID) - 4, indicator);
    }
    for (const auto& line :
         renderer.wrappedText(UI_10_FONT_ID, section.text->c_str(), kTextW, std::max(1, (bottom - y) / lineH))) {
      renderer.drawText(UI_10_FONT_ID, kMargin, y, line.c_str());
      y += lineH;
    }
  };
  const int top = InkPointShell::kContentTop;
  if (page == 1 && sections.size() > 1) {
    drawSection(top, sections[1], InkPointShell::kFooterTop - 12);
    InkPointShell::drawFooter(renderer, InkPointShell::Destination::Library);
    renderer.displayBuffer();
    return;
  }

  // Cover and details column.
  LibraryCover::draw(renderer, book, kMargin, top, kCoverW, kCoverH);
  int y = top;
  for (const auto& line : renderer.wrappedText(UI_12_FONT_ID, book.title.c_str(), kInfoW, 3, EpdFontFamily::BOLD)) {
    renderer.drawText(UI_12_FONT_ID, kInfoX, y, line.c_str(), true, EpdFontFamily::BOLD);
    y += renderer.getLineHeight(UI_12_FONT_ID);
  }
  const std::string author = renderer.truncatedText(UI_10_FONT_ID, book.author.c_str(), kInfoW);
  renderer.drawText(UI_10_FONT_ID, kInfoX, y + 2, author.c_str());
  y += renderer.getLineHeight(UI_10_FONT_ID) + 10;

  std::vector<std::string> facts;
  if (!details.publisher.empty()) facts.push_back(details.publisher);
  if (!details.date.empty() || !year.empty()) facts.push_back(details.date.empty() ? year : details.date);
  if (pages) facts.push_back(std::to_string(pages) + " pages");
  if (book.progressPercent >= 0) facts.push_back(std::to_string(book.progressPercent) + "% read");
  if (!book.isbn.empty()) facts.push_back("ISBN " + book.isbn);
  const int factStep = renderer.getLineHeight(SMALL_FONT_ID) + 4;
  for (const auto& fact : facts) {
    if (y + factStep > top + kCoverH + 8) break;
    renderer.drawText(SMALL_FONT_ID, kInfoX, y, renderer.truncatedText(SMALL_FONT_ID, fact.c_str(), kInfoW).c_str());
    y += factStep;
  }

  // The first available of blurb/bio fills the space above the Read button.
  y = std::max(y, top + kCoverH) + 18;
  if (!sections.empty()) drawSection(y, sections[0], kReadY - 12);
  else renderer.drawText(SMALL_FONT_ID, kMargin, y, "Sync on Wi-Fi to fetch blurb/bio");

  // Read: the same open-book path as Home's cover tap.
  renderer.fillRoundedRect(kMargin, kReadY, kTextW, kReadH, 10, Color::Black);
  const std::string read = book.progressPercent > 0 && book.progressPercent < 100
                               ? "Read \xC2\xB7 continue at " + std::to_string(book.progressPercent) + "%"
                               : std::string("Read");
  const int readW = renderer.getTextWidth(UI_12_FONT_ID, read.c_str(), EpdFontFamily::BOLD);
  renderer.drawText(UI_12_FONT_ID, 240 - readW / 2,
                    kReadY + (kReadH - renderer.getLineHeight(UI_12_FONT_ID)) / 2, read.c_str(), false,
                    EpdFontFamily::BOLD);

  InkPointShell::drawFooter(renderer, InkPointShell::Destination::Library);
  renderer.displayBuffer();
}
