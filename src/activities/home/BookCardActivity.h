#pragma once

#include <string>

#include "RecentBooksStore.h"
#include "activities/Activity.h"
#include "util/BookDetails.h"

// Library "back-cover" card: cover, details, blurb, author bio and a Read
// button. Pushed over the shelf; Back returns to it. Blurb and bio take a
// page each (absent ones skipped), turned by page buttons or a swipe.
class BookCardActivity final : public Activity {
  RecentBook book;
  BookDetails details;
  uint32_t pages = 0;
  std::string year;            // from Hardcover when the EPUB has no date
  bool blurbFromSync = false;  // blurb came from the Library sync cache
  bool loaded = false;
  int page = 0;  // 0: details + blurb + Read; 1: author bio when both exist

  void load();

 public:
  explicit BookCardActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, RecentBook book)
      : Activity("BookCard", renderer, mappedInput), book(std::move(book)) {}
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
