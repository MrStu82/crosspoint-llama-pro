#pragma once

#include <cstdint>
#include <optional>
#include <string>

struct HardcoverBookIdentity {
  std::string canonicalKey;
  std::string isbn;
  std::string title;
  std::string author;
};

struct RatingSnapshot {
  std::string canonicalBookKey;
  int valueX100 = 0;
  uint32_t ratingCount = 0;
  int publicationYear = 0;
  std::string sourceId;
  std::string sourceUrl;
  int64_t fetchedAt = 0;
  uint8_t schemaVersion = 1;

  bool valid() const {
    return !canonicalBookKey.empty() && valueX100 > 0 && valueX100 <= 500 && ratingCount > 0 &&
           !sourceId.empty();
  }
};

// Isolated adapter for Hardcover's beta GraphQL catalog search. Rendering only
// consumes RatingSnapshot, so a provider/schema change stays out of Home UI.
namespace HardcoverRating {

std::string buildSearchPayload(const HardcoverBookIdentity& book, bool byIsbn);
std::optional<RatingSnapshot> parseSearchResponse(const HardcoverBookIdentity& book, const std::string& json,
                                                  int64_t fetchedAt, bool byIsbn);
std::optional<RatingSnapshot> loadLastGood(const HardcoverBookIdentity& book);
bool storeLastGood(const RatingSnapshot& snapshot);

// Fetches only when Wi-Fi is already connected and a scoped PAT is configured.
// Failed/beta-schema responses retain the last-good cache and never blank UI.
std::optional<RatingSnapshot> refresh(const HardcoverBookIdentity& book, int64_t fetchedAt);

}  // namespace HardcoverRating
