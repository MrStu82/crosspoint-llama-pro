#include "HardcoverRating.h"

#include <ArduinoJson.h>

#include <cctype>
#include <cmath>

namespace {
std::string normalized(const std::string& value) {
  std::string result;
  bool pendingSpace = false;
  for (const unsigned char c : value) {
    if (std::isspace(c)) { pendingSpace = !result.empty(); continue; }
    if (pendingSpace) result.push_back(' ');
    pendingSpace = false;
    result.push_back(static_cast<char>(std::tolower(c)));
  }
  return result;
}

bool containsNormalized(JsonVariantConst value, const std::string& expected) {
  for (const char* entry : value.as<JsonArrayConst>()) {
    if (normalized(entry ? entry : "") == normalized(expected)) return true;
  }
  return false;
}

bool matches(JsonObjectConst doc, const HardcoverBookIdentity& book, bool byIsbn) {
  if (byIsbn) return !book.isbn.empty() && containsNormalized(doc["isbns"], book.isbn);
  return normalized(doc["title"] | "") == normalized(book.title) && containsNormalized(doc["author_names"], book.author);
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
  payload["variables"]["query"] = byIsbn && !book.isbn.empty() ? book.isbn : book.title + " " + book.author;
  std::string json;
  serializeJson(payload, json);
  return json;
}

std::optional<RatingSnapshot> parseSearchResponse(const HardcoverBookIdentity& book, const std::string& json,
                                                  int64_t fetchedAt, bool byIsbn) {
  JsonDocument root;
  if (deserializeJson(root, json) || !root["errors"].isNull() || !root["data"]["search"]["error"].isNull())
    return std::nullopt;
  JsonDocument decoded;
  const JsonVariantConst results = searchResults(root, decoded);
  for (JsonObjectConst hit : results["hits"].as<JsonArrayConst>()) {
    const JsonObjectConst doc = hit["document"].as<JsonObjectConst>();
    if (!matches(doc, book, byIsbn)) continue;
    const float average = doc["rating"] | 0.0f;
    const uint32_t count = doc["ratings_count"] | 0U;
    const char* slug = doc["slug"] | "";
    const std::string id = doc["id"].is<const char*>() ? doc["id"].as<const char*>() : std::to_string(doc["id"] | 0);
    if (!(average > 0.0f && average <= 5.0f) || count == 0 || (id == "0" && slug[0] == '\0')) continue;
    RatingSnapshot result;
    result.canonicalBookKey = book.canonicalKey;
    result.valueX100 = static_cast<int>(std::lround(average * 100.0f));
    result.ratingCount = count;
    result.publicationYear = doc["release_year"] | 0;
    result.sourceId = id != "0" ? id : slug;
    result.sourceUrl = slug[0] ? std::string("https://hardcover.app/books/") + slug : "https://hardcover.app";
    result.fetchedAt = fetchedAt;
    return result;
  }
  return std::nullopt;
}

}  // namespace HardcoverRating
