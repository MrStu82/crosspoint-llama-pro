#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

struct HardcoverBookIdentity {
  std::string canonicalKey;
  std::string isbn;
  std::string title;
  std::string author;
};

enum class HardcoverRefreshStatus : uint8_t {
  Updated,
  TokenMissing,
  NetworkFailure,
  NoMatch,
  Ambiguous,
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

struct HardcoverSearchDiagnostics {
  uint8_t returned = 0;
  uint8_t invalid = 0;
  uint8_t titleRejected = 0;
  uint8_t authorRejected = 0;
};

struct HardcoverCandidate {
  RatingSnapshot snapshot;
  std::string title;
  std::string author;
};

// Isolated adapter for Hardcover's beta GraphQL catalog search. Rendering only
// consumes RatingSnapshot, so a provider/schema change stays out of Home UI.
namespace HardcoverRating {

std::string buildSearchPayload(const HardcoverBookIdentity& book, bool byIsbn);
std::vector<HardcoverCandidate> parseSearchCandidates(const HardcoverBookIdentity& book, const std::string& json,
                                                          int64_t fetchedAt, bool byIsbn, HardcoverSearchDiagnostics* diagnostics = nullptr);
std::optional<RatingSnapshot> parseSearchResponse(const HardcoverBookIdentity& book, const std::string& json,
                                                  int64_t fetchedAt, bool byIsbn);
std::optional<RatingSnapshot> loadLastGood(const HardcoverBookIdentity& book);
bool storeLastGood(const RatingSnapshot& snapshot);

// Fetches only when Wi-Fi is already connected and a scoped PAT is configured.
// Failed/beta-schema responses retain the last-good cache and never blank UI.
struct HardcoverRefreshResult {
  HardcoverRefreshStatus status = HardcoverRefreshStatus::NoMatch;
  std::optional<RatingSnapshot> snapshot;
  std::vector<HardcoverCandidate> candidates;
};

HardcoverRefreshResult refresh(const HardcoverBookIdentity& book, int64_t fetchedAt);

}  // namespace HardcoverRating
