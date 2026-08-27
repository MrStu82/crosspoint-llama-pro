#pragma once

#include <string>
#include <vector>

namespace HardcoverSyncResult {
inline bool shouldPersistSelection(size_t selected, size_t candidates) { return selected < candidates; }

inline std::vector<std::string> format(size_t updated, size_t unmatched, size_t noSuggestions,
                                       size_t tokenFailures, size_t networkFailures) {
  std::vector<std::string> lines{"Hardcover sync finished"};
  if (updated) lines.push_back("Updated: " + std::to_string(updated));
  if (unmatched) lines.push_back("Unmatched: " + std::to_string(unmatched));
  if (noSuggestions) lines.push_back("No Hardcover suggestions found.");
  if (tokenFailures) lines.push_back("Hardcover sign-in failed. Check token in Settings.");
  if (networkFailures) lines.push_back("Couldn't reach Hardcover. Check Wi-Fi and retry.");
  return lines;
}
}  // namespace HardcoverSyncResult
