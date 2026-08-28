#pragma once

#include <string>

namespace FsHelpers {
inline bool hasPngExtension(const std::string& path) {
  return path.size() >= 4 && path.substr(path.size() - 4) == ".png";
}
}  // namespace FsHelpers
