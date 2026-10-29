#include <ArduinoJson.h>

#include <cctype>
#include <cstdio>

#include "AuthorBio.h"

namespace {
std::string normalized(const std::string& value) {
  std::string result;
  for (const unsigned char c : value)
    if (std::isalnum(c)) result.push_back(static_cast<char>(std::tolower(c)));
  return result;
}
}  // namespace

namespace AuthorBio {

std::string buildHardcoverPayload(const std::string& author) {
  JsonDocument payload;
  payload["query"] = "query Bio($name:String!){authors(where:{name:{_eq:$name}},limit:5){bio}}";
  payload["variables"]["name"] = author;
  std::string json;
  serializeJson(payload, json);
  return json;
}

std::string parseHardcover(const std::string& json) {
  JsonDocument root;
  if (deserializeJson(root, json) || !root["errors"].isNull()) return {};
  const JsonArrayConst authors = root["data"]["authors"];
  for (JsonObjectConst author : authors) {
    const char* bio = author["bio"] | "";
    if (bio[0]) return bio;
  }
  return {};
}

std::string openLibrarySearchUrl(const std::string& author) {
  std::string url = "https://openlibrary.org/search/authors.json?limit=1&fields=key,name&q=";
  for (const unsigned char c : author) {
    if (std::isalnum(c) || c == '-' || c == '.' || c == '_' || c == '~') {
      url.push_back(static_cast<char>(c));
    } else {
      char hex[4];
      std::snprintf(hex, sizeof(hex), "%%%02X", c);
      url += hex;
    }
  }
  return url;
}

std::string parseOpenLibraryKey(const std::string& json, const std::string& author) {
  JsonDocument root;
  if (deserializeJson(root, json)) return {};
  const JsonObjectConst doc = root["docs"][0];
  const std::string key = doc["key"] | "";
  if (key.empty() || normalized(doc["name"] | "") != normalized(author)) return {};
  return "/authors/" + key;
}

std::string parseOpenLibraryBio(const std::string& json) {
  JsonDocument root;
  if (deserializeJson(root, json)) return {};
  std::string bio = root["bio"].is<const char*>() ? root["bio"].as<const char*>() : (root["bio"]["value"] | "");
  bio = bio.substr(0, bio.find("([Source]"));
  std::string text;
  for (const char c : bio)
    if (c != '*') text.push_back(c);
  return text;
}

}  // namespace AuthorBio
