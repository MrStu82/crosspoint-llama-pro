#pragma once

#include <string>

class Epub {
 public:
  Epub(const std::string&, const char*) {}
  bool load(bool, bool) { return false; }
  bool generateCoverBmp(bool) { return false; }
  std::string getCoverBmpPath(bool) const { return {}; }
};
