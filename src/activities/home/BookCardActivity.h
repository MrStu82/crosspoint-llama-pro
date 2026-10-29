#pragma once

#include <string>
#include <vector>

#include "RecentBooksStore.h"
#include "activities/Activity.h"
#include "util/BookDetails.h"

// Library "back-cover" card: cover, details, blurb, author bio and a Read
// button. Pushed over the shelf; Back returns to it. Blurb and bio sit in
// [Blurb | Author] tabs (absent ones hidden); page buttons or a swipe page
// through the active tab's text, then move on to the next tab.
class BookCardActivity final : public Activity {
  RecentBook book;
  BookDetails details;
  uint32_t pages = 0;
  std::string year;            // from Hardcover when the EPUB has no date
  bool blurbFromSync = false;  // blurb came from the Library sync cache
  bool loaded = false;
  struct Tab {
    const char* label;
    std::vector<std::string> lines;  // whole text, wrapped once, never truncated
    bool fromSync;
  };
  std::vector<Tab> tabs;
  int linesPerPage = 1;
  int tab = 0;
  int textPage = 0;

  int pageCount(int index) const;
  void step(int delta);

  void load();

 public:
  explicit BookCardActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, RecentBook book)
      : Activity("BookCard", renderer, mappedInput), book(std::move(book)) {}
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
