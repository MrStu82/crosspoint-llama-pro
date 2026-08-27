#include "HardcoverRating.h"

#include <ArduinoJson.h>
#include <Logging.h>

#include <cctype>
#include <cmath>
#include <algorithm>

namespace {
std::string normalized(const std::string& value) {
  // Hardcover's search is punctuation-insensitive; match the same way so
  // "J.R.R." and "J. R. R." identify the same author/ISBN. Keeping only
  // letters and digits also makes ISBN hyphens irrelevant without accepting
  // a title suffix such as "The Hobbit, or ...".
  std::string result;
  for (const unsigned char c : value) {
    if (std::isalnum(c)) result.push_back(static_cast<char>(std::tolower(c)));
  }
  return result;
}

bool containsNormalized(JsonVariantConst value, const std::string& expected) {
  for (const char* entry : value.as<JsonArrayConst>()) {
    if (normalized(entry ? entry : "") == normalized(expected)) return true;
  }
  return false;
}

bool titleMatches(JsonObjectConst doc, const HardcoverBookIdentity& book) {
  return normalized(doc["title"] | "") == normalized(book.title);
}
bool matches(JsonObjectConst doc, const HardcoverBookIdentity& book, bool byIsbn) {
  return !byIsbn || (!book.isbn.empty() && containsNormalized(doc["isbns"], book.isbn));
}
int similarity(const std::string& left, const std::string& right) {
  const auto a = normalized(left), b = normalized(right);
  const size_t n = std::min(a.size(), b.size());
  size_t same = 0; while (same < n && a[same] == b[same]) ++same;
  return static_cast<int>(same * 1000 / std::max<size_t>(1, std::max(a.size(), b.size())));
}
std::string firstAuthor(JsonObjectConst doc) {
  for (const char* entry : doc["author_names"].as<JsonArrayConst>()) if (entry && entry[0]) return entry;
  return "Unknown author";
}

JsonVariantConst searchResults(JsonDocument& root, JsonDocument& decoded) {
  JsonVariantConst results = root["data"]["search"]["results"];
  if (results.is<const char*>()) {
    if (deserializeJson(decoded, results.as<const char*>())) return {};
    return decoded.as<JsonVariantConst>();
  }
  return results;
}
}  // namespace

namespace HardcoverRating {

std::string buildSearchPayload(const HardcoverBookIdentity& book, bool byIsbn) {
  JsonDocument payload;
  payload["query"] =
      "query Search($query:String!){search(query:$query,query_type:\"Book\",per_page:5,page:1){error results}}";
  payload["variables"]["query"] = byIsbn && !book.isbn.empty() ? book.isbn : book.title;
  std::string json;
  serializeJson(payload, json);
  return json;
}

std::vector<HardcoverCandidate> parseSearchCandidates(const HardcoverBookIdentity& book, const std::string& json,
                                                          int64_t fetchedAt, bool byIsbn, HardcoverSearchDiagnostics* diagnostics) {
  HardcoverSearchDiagnostics local;
  auto& stats = diagnostics ? *diagnostics : local;
  JsonDocument root;
  const auto error = deserializeJson(root, json, DeserializationOption::NestingLimit(12));
  if (error) {
    LOG_DBG("HCR", "search response JSON: %s", error.c_str());
    return {};
  }
  if (!root["errors"].isNull() || !root["data"]["search"]["error"].isNull()) return {};
  JsonDocument decoded;
  const JsonVariantConst results = searchResults(root, decoded);
  std::vector<HardcoverCandidate> candidates;
  for (JsonObjectConst hit : results["hits"].as<JsonArrayConst>()) {
    const JsonObjectConst doc = hit["document"].as<JsonObjectConst>();
    ++stats.returned;
    if (!matches(doc, book, byIsbn)) { ++stats.titleRejected; continue; }
    const float average = doc["rating"] | 0.0f;
    const uint32_t count = doc["ratings_count"] | 0U;
    const char* slug = doc["slug"] | "";
    const std::string id = doc["id"].is<const char*>() ? doc["id"].as<const char*>() : std::to_string(doc["id"] | 0);
    if (!(average > 0.0f && average <= 5.0f) || count == 0 || (id == "0" && slug[0] == '\0')) { ++stats.invalid; continue; }
    HardcoverCandidate candidate;
    candidate.title = doc["title"] | "";
    candidate.author = firstAuthor(doc);
    candidate.snapshot = {book.canonicalKey, static_cast<int>(std::lround(average * 100.0f)), count,
                          doc["release_year"] | 0, id != "0" ? id : slug,
                          slug[0] ? std::string("https://hardcover.app/books/") + slug : "https://hardcover.app", fetchedAt};
    if (!titleMatches(doc, book)) ++stats.titleRejected;
    if (!containsNormalized(doc["author_names"], book.author)) ++stats.authorRejected;
    candidates.push_back(std::move(candidate));
  }
  std::stable_sort(candidates.begin(), candidates.end(), [&book](const HardcoverCandidate& a, const HardcoverCandidate& b) {
    const int aTitle = similarity(a.title, book.title), bTitle = similarity(b.title, book.title);
    if (aTitle != bTitle) return aTitle > bTitle;
    const int aAuthor = similarity(a.author, book.author), bAuthor = similarity(b.author, book.author);
    if (aAuthor != bAuthor) return aAuthor > bAuthor;
    return a.snapshot.publicationYear > b.snapshot.publicationYear;
  });
  if (candidates.size() > 5) candidates.resize(5);
  return candidates;
}

std::optional<RatingSnapshot> parseSearchResponse(const HardcoverBookIdentity& book, const std::string& json,
                                                  int64_t fetchedAt, bool byIsbn) {
  const auto candidates = parseSearchCandidates(book, json, fetchedAt, byIsbn);
  std::vector<HardcoverCandidate> exact;
  for (const auto& candidate : candidates) {
    if (byIsbn || (normalized(candidate.title) == normalized(book.title) && normalized(candidate.author) == normalized(book.author))) exact.push_back(candidate);
  }
  if (exact.size() != 1) return std::nullopt;
  return exact.front().snapshot;
}

}  // namespace HardcoverRating
