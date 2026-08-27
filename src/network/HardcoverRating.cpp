#include "HardcoverRating.h"

#include <ArduinoJson.h>
#include <HalStorage.h>
#include <Logging.h>
#include <WiFi.h>

#include <cstdio>
#include <cctype>

#include "HardcoverCredentialStore.h"
#include "HttpDownloader.h"
#include "activities/reader/ProgressFile.h"

namespace {
std::string normalized(const std::string& value) {
  std::string result;
  for (const unsigned char c : value) if (std::isalnum(c)) result.push_back(static_cast<char>(std::tolower(c)));
  return result;
}
constexpr const char* kCacheDir = "/.crosspoint/home";
constexpr const char* kEndpoint = "https://api.hardcover.app/v1/graphql";

std::string cacheFileFor(const std::string& canonicalKey) {
  uint32_t hash = 2166136261U;
  for (const unsigned char byte : canonicalKey) hash = (hash ^ byte) * 16777619U;
  char name[40];
  std::snprintf(name, sizeof(name), "hardcover-rating-%08lx.json", static_cast<unsigned long>(hash));
  return name;
}

std::optional<RatingSnapshot> snapshotFromJson(JsonVariantConst value) {
  RatingSnapshot result;
  result.canonicalBookKey = value["canonicalBookKey"] | "";
  result.valueX100 = value["valueX100"] | 0;
  result.ratingCount = value["ratingCount"] | 0U;
  result.publicationYear = value["publicationYear"] | 0;
  result.sourceId = value["sourceId"] | "";
  result.sourceUrl = value["sourceUrl"] | "";
  result.fetchedAt = value["fetchedAt"] | 0LL;
  result.schemaVersion = value["schemaVersion"] | 0;
  return result.valid() ? std::optional<RatingSnapshot>(result) : std::nullopt;
}

struct QueryResult {
  bool networkOk = false;
  std::vector<HardcoverCandidate> candidates;
  HardcoverSearchDiagnostics diagnostics;
};

QueryResult query(const HardcoverBookIdentity& book, int64_t fetchedAt, bool byIsbn) {
  std::string response;
  if (!HttpDownloader::postJson(kEndpoint, HardcoverRating::buildSearchPayload(book, byIsbn),
                                HARDCOVER_STORE.getToken(), response))
    return {};
  HardcoverSearchDiagnostics diagnostics;
  auto candidates = HardcoverRating::parseSearchCandidates(book, response, fetchedAt, byIsbn, &diagnostics);
  return {true, std::move(candidates), diagnostics};
}
}  // namespace

namespace HardcoverRating {

std::optional<RatingSnapshot> loadLastGood(const HardcoverBookIdentity& book) {
  HalFile file;
  const std::string path = std::string(kCacheDir) + "/" + cacheFileFor(book.canonicalKey);
  if (!Storage.openFileForRead("HCR", path, file)) return std::nullopt;
  JsonDocument root;
  if (deserializeJson(root, file)) return std::nullopt;
  auto snapshot = snapshotFromJson(root.as<JsonVariantConst>());
  if (!snapshot || snapshot->canonicalBookKey != book.canonicalKey) return std::nullopt;
  return snapshot;
}

bool storeLastGood(const RatingSnapshot& snapshot) {
  if (!snapshot.valid()) return false;
  if (!Storage.exists(kCacheDir) && !Storage.mkdir(kCacheDir)) return false;
  JsonDocument root;
  root["canonicalBookKey"] = snapshot.canonicalBookKey;
  root["valueX100"] = snapshot.valueX100;
  root["ratingCount"] = snapshot.ratingCount;
  root["publicationYear"] = snapshot.publicationYear;
  root["sourceId"] = snapshot.sourceId;
  root["sourceUrl"] = snapshot.sourceUrl;
  root["fetchedAt"] = snapshot.fetchedAt;
  root["schemaVersion"] = snapshot.schemaVersion;
  std::string json;
  serializeJson(root, json);
  return ProgressFile::writeAtomic(kCacheDir, reinterpret_cast<const uint8_t*>(json.data()), json.size(),
                                   cacheFileFor(snapshot.canonicalBookKey));
}

HardcoverRefreshResult refresh(const HardcoverBookIdentity& book, int64_t fetchedAt) {
  const auto stale = loadLastGood(book);
  if (!HARDCOVER_STORE.hasToken()) return {HardcoverRefreshStatus::TokenMissing, stale, {}};
  if (WiFi.status() != WL_CONNECTED || WiFi.localIP() == IPAddress(0, 0, 0, 0))
    return {HardcoverRefreshStatus::NetworkFailure, stale, {}};

  // ISBN is a fast-path only. A title search is the retrieval fallback because
  // concatenating author into the provider query demonstrably loses books.
  if (!book.isbn.empty()) {
    const auto isbn = query(book, fetchedAt, true);
    if (isbn.networkOk && isbn.candidates.size() == 1) {
      const auto& snapshot = isbn.candidates.front().snapshot;
      if (!storeLastGood(snapshot)) LOG_ERR("HCR", "Could not persist rating; retaining in-memory value");
      return {HardcoverRefreshStatus::Updated, snapshot, {}};
    }
  }
  const auto title = query(book, fetchedAt, false);
  if (!title.networkOk) return {HardcoverRefreshStatus::NetworkFailure, stale, {}};
  LOG_DBG("HCR", "search returned=%u invalid=%u title-rejected=%u author-rejected=%u", title.diagnostics.returned, title.diagnostics.invalid, title.diagnostics.titleRejected, title.diagnostics.authorRejected);
  if (title.candidates.empty()) return {HardcoverRefreshStatus::NoMatch, stale, {}};
  std::vector<HardcoverCandidate> exact;
  for (const auto& candidate : title.candidates)
    if (normalized(candidate.title) == normalized(book.title) && normalized(candidate.author) == normalized(book.author)) exact.push_back(candidate);
  if (exact.size() == 1) {
    const auto& snapshot = exact.front().snapshot;
    if (!storeLastGood(snapshot)) LOG_ERR("HCR", "Could not persist rating; retaining in-memory value");
    return {HardcoverRefreshStatus::Updated, snapshot, {}};
  }
  return {HardcoverRefreshStatus::Ambiguous, stale, title.candidates};
}

}  // namespace HardcoverRating
