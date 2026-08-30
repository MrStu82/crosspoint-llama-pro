#include <gtest/gtest.h>

#include "activities/reader/ReaderToolsGeometry.h"

TEST(ReaderToolsGeometry, ApprovedPortraitLayoutIsExact) {
  const auto panel = ReaderToolsGeometry::layout(480, 800, 6);
  EXPECT_EQ(panel.x, 34);
  EXPECT_EQ(panel.y, 145);
  EXPECT_EQ(panel.width, 412);
  EXPECT_EQ(panel.height, ReaderToolsGeometry::PANEL_HEIGHT_EPUB);
  EXPECT_EQ(panel.rowHeight, 70);
}

TEST(ReaderToolsGeometry, SixRowsHaveIndependentSeventyPixelHitboxes) {
  const auto panel = ReaderToolsGeometry::layout(480, 800, 6);
  for (int row = 0; row < 6; row++) {
    const int y = panel.y + ReaderToolsGeometry::HEADER_HEIGHT + row * panel.rowHeight;
    EXPECT_EQ(ReaderToolsGeometry::hitRow(panel, panel.x + 20, y, 6), row);
    EXPECT_EQ(ReaderToolsGeometry::hitRow(panel, panel.x + 20, y + panel.rowHeight - 1, 6), row);
  }
  EXPECT_EQ(ReaderToolsGeometry::hitRow(panel, panel.x + 20, panel.y + ReaderToolsGeometry::HEADER_HEIGHT - 1, 6), -1);
  EXPECT_EQ(ReaderToolsGeometry::hitRow(panel, panel.x - 1, panel.y + ReaderToolsGeometry::HEADER_HEIGHT, 6), -1);
}

TEST(ReaderToolsGeometry, OutsideDismissBoundsExcludeShadow) {
  const auto panel = ReaderToolsGeometry::layout(480, 800, 6);
  EXPECT_TRUE(ReaderToolsGeometry::contains(panel, panel.x, panel.y));
  EXPECT_TRUE(ReaderToolsGeometry::contains(panel, panel.x + panel.width - 1, panel.y + panel.height - 1));
  EXPECT_FALSE(ReaderToolsGeometry::contains(panel, panel.x - 1, panel.y));
  EXPECT_FALSE(ReaderToolsGeometry::contains(panel, panel.x + panel.width, panel.y));
}

TEST(ReaderToolsGeometry, FilteredFormatsRemainCentredWithLargeRows) {
  const auto txt = ReaderToolsGeometry::layout(480, 800, 2);
  const auto xtc = ReaderToolsGeometry::layout(480, 800, 1);
  EXPECT_EQ(txt.rowHeight, 70);
  EXPECT_EQ(xtc.rowHeight, 70);
  EXPECT_EQ(txt.y * 2 + txt.height, 800);
  EXPECT_EQ(xtc.y * 2 + xtc.height, 800);
}
