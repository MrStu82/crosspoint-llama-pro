#pragma once

#include <string>
#include <vector>

#include "RecentBooksStore.h"
#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

// Library "Shelf" view (InkPoint touch layout only): a cover carousel over the
// same books as the List. Swipe or tap a side cover to browse; tap the centre
// cover for the book card.
class LibraryShelfActivity final : public Activity {
  ButtonNavigator buttonNavigator;
  std::vector<RecentBook> books;
  size_t index = 0;
  std::string metaLine;  // year · pages · % read for books[index]

  void select(size_t next);
  void openCard();

 public:
  explicit LibraryShelfActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("LibraryShelf", renderer, mappedInput) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
};

// Cover helpers shared by the shelf and the book card. Both reuse Home's
// thumbnail height, so no extra thumbnail is generated per book.
namespace LibraryCover {
// Generates the thumbnail if missing; false when the book has no cover image.
bool ensureThumb(RecentBook& book);
// Fits the cover inside the box, bottom-centred; draws a typographic cover
// (title, rule, author) when there is no image. Returns the drawn width.
int draw(const GfxRenderer& renderer, const RecentBook& book, int x, int y, int maxW, int maxH);
}  // namespace LibraryCover
