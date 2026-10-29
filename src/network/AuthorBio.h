#pragma once

#include <string>

// Author bio for the Library card when the EPUB has none: Hardcover's author
// record first, then Open Library. Parsers are pure (host-tested).
namespace AuthorBio {

std::string buildHardcoverPayload(const std::string& author);
// First non-empty bio among Hardcover authors with that exact name.
std::string parseHardcover(const std::string& json);
std::string openLibrarySearchUrl(const std::string& author);
// "/authors/OL…A" key of the first hit whose name matches `author`
// (letters/digits only, so "J.R.R." == "J. R. R."); empty otherwise.
std::string parseOpenLibraryKey(const std::string& json, const std::string& author);
// Bio as either a string or {"type":"/type/text","value":…}, without the
// trailing "([Source][1])" link block and markdown emphasis.
std::string parseOpenLibraryBio(const std::string& json);

// Needs Wi-Fi; Hardcover only when a token is configured. Empty when neither has one.
std::string fetch(const std::string& author);

}  // namespace AuthorBio
