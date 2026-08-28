#pragma once

#include <cstddef>
#include <string>

namespace obfuscation {
inline std::string obfuscateToBase64(const std::string& value) { return value; }
inline std::string deobfuscateFromBase64(const char* value, size_t, bool* ok, bool* tooLong) {
  if (ok) *ok = true;
  if (tooLong) *tooLong = false;
  return value ? value : "";
}
}  // namespace obfuscation
