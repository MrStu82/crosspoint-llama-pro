#pragma once

#include <string>
#include <string_view>

namespace FsHelpers {
inline bool ends(std::string_view value, std::string_view suffix) {
  return value.size() >= suffix.size() && value.substr(value.size() - suffix.size()) == suffix;
}
inline bool hasBmpExtension(std::string_view value) { return ends(value, ".bmp"); }
inline bool hasBmpExtension(const std::string& value) { return hasBmpExtension(std::string_view(value)); }
inline bool hasBmpExtension(const char* value) { return hasBmpExtension(std::string_view(value)); }
inline bool hasPngExtension(std::string_view value) { return ends(value, ".png"); }
inline bool hasPngExtension(const std::string& value) { return hasPngExtension(std::string_view(value)); }
inline bool hasEpubExtension(const std::string& value) { return ends(value, ".epub"); }
inline bool hasTxtExtension(const std::string& value) { return ends(value, ".txt"); }
inline bool hasXtcExtension(const std::string& value) { return ends(value, ".xtc"); }
}  // namespace FsHelpers
