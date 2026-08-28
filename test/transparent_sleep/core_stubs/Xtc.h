#pragma once

#include <string>

class Xtc {
 public:
  Xtc(const std::string&, const char*) {}
  bool load() { return false; }
  bool generateCoverBmp() { return false; }
  std::string getCoverBmpPath() const { return {}; }
};
