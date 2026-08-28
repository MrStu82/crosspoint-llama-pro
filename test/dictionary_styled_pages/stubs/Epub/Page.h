#pragma once

#include <memory>
#include <vector>

class GfxRenderer;

class Page {
 public:
  std::vector<int> elements;
  void render(GfxRenderer&, int, int, int) const {}
};
