#pragma once

#include <cstdint>
#include <optional>
#include <string>

// Offline details for the Library book card, read from the EPUB once and
// cached on SD next to its book.bin.
struct BookDetails {
  std::string publisher;
  std::string date;  // YYYY-MM-DD (or as much as the OPF gives)
  std::string description;
  std::string bio;
  uint32_t printPages = 0;
  bool bioFromSync = false;  // bio came from the Library sync, not the EPUB
};

namespace BookDetailsStore {
// Cache only; never opens the EPUB. Non-EPUBs have no details.
std::optional<BookDetails> loadCached(const std::string& bookPath);
// Cache first, else extract from the EPUB and cache the result.
BookDetails load(const std::string& bookPath);
// Library sync: when the book has no bio, fetch one for `author` (Wi-Fi) and
// cache it. An EPUB bio is never replaced.
void backfillBio(const std::string& bookPath, const std::string& author);
}  // namespace BookDetailsStore
