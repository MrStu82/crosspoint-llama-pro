#pragma once

#include <WString.h>

// Web file-access protection shared by the HTTP file manager and WebDAV.
namespace web_paths {

// Folders/files hidden from web file access. Names starting with "." are always protected.
constexpr const char* HIDDEN_ITEMS[] = {"System Volume Information", "XTCache"};

inline bool isProtectedItemName(const String& name) {
  if (name.startsWith(".")) return true;
  for (const auto* item : HIDDEN_ITEMS) {
    if (name.equals(item)) return true;
  }
  return false;
}

// Checks every segment of the path, not just the last one, so /.crosspoint/foo or
// /System Volume Information/foo are protected too.
inline bool isProtectedPath(const String& path) {
  int start = 0;
  while (start < static_cast<int>(path.length())) {
    if (path.charAt(start) == '/') {
      start++;
      continue;
    }
    int end = path.indexOf('/', start);
    if (end == -1) end = path.length();
    if (isProtectedItemName(path.substring(start, end))) return true;
    start = end + 1;
  }
  return false;
}

}  // namespace web_paths
