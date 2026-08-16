#pragma once
// Shadow of lib/FsHelpers/FsHelpers.h. UITheme.cpp's UITheme::getFileIcon() (compiled in,
// never called by drawCorruptSaveNotice()'s tested path) calls these -- symbols must still
// resolve for the link to succeed. All bodies are inert.
#include <string_view>

namespace FsHelpers {
inline bool hasBmpExtension(std::string_view fileName) {
  (void)fileName;
  return false;
}
inline bool hasEpubExtension(std::string_view fileName) {
  (void)fileName;
  return false;
}
inline bool hasXtcExtension(std::string_view fileName) {
  (void)fileName;
  return false;
}
inline bool hasTxtExtension(std::string_view fileName) {
  (void)fileName;
  return false;
}
inline bool hasMarkdownExtension(std::string_view fileName) {
  (void)fileName;
  return false;
}
}  // namespace FsHelpers
