#pragma once

#include <algorithm>

namespace ReaderToolsGeometry {

constexpr int PANEL_WIDTH = 412;
constexpr int PANEL_HEIGHT_EPUB = 510;
constexpr int HEADER_HEIGHT = 84;
constexpr int ROW_HEIGHT = 70;
constexpr int SHADOW_OFFSET = 7;
constexpr int CORNER_RADIUS = 12;
constexpr int BORDER_WIDTH = 3;

struct Layout {
  int x;
  int y;
  int width;
  int height;
  int rowHeight;
};

inline Layout layout(const int screenWidth, const int screenHeight, const int rowCount) {
  const int width = std::min(PANEL_WIDTH, screenWidth - 16);
  const int preferredHeight = HEADER_HEIGHT + rowCount * ROW_HEIGHT + BORDER_WIDTH * 2;
  const int height = std::min(preferredHeight, screenHeight - 16);
  const int rowHeight = rowCount > 0 ? (height - HEADER_HEIGHT - BORDER_WIDTH * 2) / rowCount : ROW_HEIGHT;
  return {(screenWidth - width) / 2, (screenHeight - height) / 2, width, height, rowHeight};
}

inline int hitRow(const Layout& panel, const int x, const int y, const int rowCount) {
  if (x < panel.x || x >= panel.x + panel.width || y < panel.y + HEADER_HEIGHT ||
      y >= panel.y + HEADER_HEIGHT + panel.rowHeight * rowCount) {
    return -1;
  }
  return (y - panel.y - HEADER_HEIGHT) / panel.rowHeight;
}

inline bool contains(const Layout& panel, const int x, const int y) {
  return x >= panel.x && x < panel.x + panel.width && y >= panel.y && y < panel.y + panel.height;
}

}  // namespace ReaderToolsGeometry
