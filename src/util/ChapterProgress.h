#pragma once

#include <string>

// Where the reader is inside the *current chapter*, read back from the reader's
// own progress.bin. The chapter boundaries are the EPUB spine boundaries: the
// reader paginates one spine item at a time and persists that item's page count
// alongside the page it stopped on, so "pages left in this chapter" is a fact
// the reader has already established, not an estimate derived from a different
// metric.
struct ChapterProgressValue {
  int currentPage = 0;
  int pageCount = 0;
  bool available = false;

  int pagesLeft() const { return pageCount > currentPage ? pageCount - currentPage : 0; }
};

namespace ChapterProgress {
// Reads <cache>/progress.bin for the given book. Returns an unavailable value
// for a non-EPUB, an unopened book, or a progress file written before the page
// count field existed (the 4-byte form) -- never a guess.
ChapterProgressValue read(const std::string& bookPath);
}  // namespace ChapterProgress
