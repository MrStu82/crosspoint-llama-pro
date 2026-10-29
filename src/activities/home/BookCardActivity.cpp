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

constexpr int kTabY = InkPointShell::kContentTop + kCoverH + 26;
constexpr int kTabW = 96;
constexpr int kTabGap = 8;
constexpr int kTextBottom = kReadY - 12;

int tabBarH(const GfxRenderer& r) { return r.getLineHeight(SMALL_FONT_ID) + 10; }
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
  const int lineH = renderer.getLineHeight(UI_10_FONT_ID);
  linesPerPage = std::max(1, (kTextBottom - (kTabY + tabBarH(renderer) + 10)) / lineH);
  const auto addTab = [&](const char* label, const std::string& text, const bool fromSync) {
    if (!text.empty()) tabs.push_back({label, renderer.wrappedText(UI_10_FONT_ID, text.c_str(), kTextW, 1000), fromSync});
  };
  addTab("Blurb", details.description, blurbFromSync);
  addTab("Author", details.bio, details.bioFromSync);
  loaded = true;
}

int BookCardActivity::pageCount(const int index) const {
  return std::max<int>(1, (tabs[index].lines.size() + linesPerPage - 1) / linesPerPage);
}

// Next/prev: page through the active tab, then on to the neighbouring tab
// (wrapping); prev lands on that tab's last page.
void BookCardActivity::step(const int delta) {
  if (tabs.empty()) return;
  textPage += delta;
  if (textPage >= pageCount(tab)) {
    tab = (tab + 1) % tabs.size();
    textPage = 0;
  } else if (textPage < 0) {
    tab = (tab + tabs.size() - 1) % tabs.size();
    textPage = pageCount(tab) - 1;
  }
  requestUpdate();
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
  if (!loaded) return;
  const auto swipe = mappedInput.wasSwipe();
  if (mappedInput.wasReleased(MappedInputManager::Button::PageForward) || swipe == MappedInputManager::SwipeDir::Left) {
    step(1);
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::PageBack) || swipe == MappedInputManager::SwipeDir::Right) {
    step(-1);
    return;
  }
  for (int i = 0; i < static_cast<int>(tabs.size()); ++i) {
    if (mappedInput.wasTapInRect(kMargin + i * (kTabW + kTabGap), kTabY, kTabW, tabBarH(renderer))) {
      tab = i;
      textPage = 0;
      requestUpdate();
      return;
    }
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm) ||
      mappedInput.wasTapInRect(kMargin, kReadY, kTextW, kReadH))
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
  const int top = InkPointShell::kContentTop;

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

  // [Blurb | Author] tabs, active inverted; a "sync" pill marks synced text.
  const int barH = tabBarH(renderer);
  const int labelY = kTabY + 5;
  if (tabs.empty()) renderer.drawText(SMALL_FONT_ID, kMargin, labelY, "Sync on Wi-Fi to fetch blurb/bio");
  for (int i = 0; i < static_cast<int>(tabs.size()); ++i) {
    const int x = kMargin + i * (kTabW + kTabGap);
    const bool active = i == tab;
    if (active) renderer.fillRoundedRect(x, kTabY, kTabW, barH, 7, Color::Black);
    else renderer.drawRoundedRect(x, kTabY, kTabW, barH, 2, 7, true);
    const int labelW = renderer.getTextWidth(SMALL_FONT_ID, tabs[i].label, EpdFontFamily::BOLD);
    renderer.drawText(SMALL_FONT_ID, x + (kTabW - labelW) / 2, labelY, tabs[i].label, !active, EpdFontFamily::BOLD);
  }
  if (!tabs.empty()) {
    int right = 480 - kMargin;
    if (pageCount(tab) > 1) {
      const std::string indicator = std::to_string(textPage + 1) + "/" + std::to_string(pageCount(tab));
      right -= renderer.getTextWidth(SMALL_FONT_ID, indicator.c_str());
      renderer.drawText(SMALL_FONT_ID, right, labelY, indicator.c_str());
      right -= 10;
    }
    if (tabs[tab].fromSync) {
      const int w = renderer.getTextWidth(SMALL_FONT_ID, "sync") + 12;
      renderer.fillRoundedRect(right - w, labelY - 1, w, renderer.getLineHeight(SMALL_FONT_ID) + 2, 6, Color::Black);
      renderer.drawText(SMALL_FONT_ID, right - w + 6, labelY, "sync", false);
    }
    const int lineH = renderer.getLineHeight(UI_10_FONT_ID);
    const auto& lines = tabs[tab].lines;
    int ty = kTabY + barH + 10;
    for (size_t i = textPage * linesPerPage; i < lines.size() && i < static_cast<size_t>((textPage + 1) * linesPerPage);
         ++i) {
      renderer.drawText(UI_10_FONT_ID, kMargin, ty, lines[i].c_str());
      ty += lineH;
    }
  }

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
