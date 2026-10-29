#include "AuthorBio.h"

#include <WiFi.h>

#include "HardcoverCredentialStore.h"
#include "HttpDownloader.h"

namespace AuthorBio {

std::string fetch(const std::string& author) {
  if (author.empty() || WiFi.status() != WL_CONNECTED) return {};
  std::string response;
  if (HARDCOVER_STORE.hasToken() &&
      HttpDownloader::postJson("https://api.hardcover.app/v1/graphql", buildHardcoverPayload(author),
                               HARDCOVER_STORE.getToken(), response)) {
    std::string bio = parseHardcover(response);
    if (!bio.empty()) return bio;
  }
  if (!HttpDownloader::fetchUrl(openLibrarySearchUrl(author), response)) return {};
  const std::string key = parseOpenLibraryKey(response, author);
  if (key.empty() || !HttpDownloader::fetchUrl("https://openlibrary.org" + key + ".json", response)) return {};
  return parseOpenLibraryBio(response);
}

}  // namespace AuthorBio
