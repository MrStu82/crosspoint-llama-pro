#include <gtest/gtest.h>

#include <Epub/Page.h>
#include <Epub/parsers/ChapterHtmlSlimParser.h>
#include <GfxRenderer.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace {

struct ParsedChapter {
  bool ok = false;
  std::vector<std::unique_ptr<Page>> pages;
};

ParsedChapter parseFixture(float lineCompression, uint16_t height) {
  ParsedChapter result;
  GfxRenderer renderer;
  const std::string path = std::string(EXTRA_WIDE_FIXTURE_DIR) + "/pagination.xhtml";
  ChapterHtmlSlimParser parser(
      nullptr, path, renderer, 1, lineCompression, false, static_cast<uint8_t>(CssTextAlign::Left), 180, height,
      false, false, false, false,
      [&result](std::unique_ptr<Page> page, uint16_t, uint16_t, uint32_t) {
        result.pages.push_back(std::move(page));
      },
      false, "", "", 2);
  result.ok = parser.parseAndBuildPages();
  return result;
}

std::vector<int16_t> linePositions(const ParsedChapter& chapter) {
  std::vector<int16_t> positions;
  for (const auto& page : chapter.pages) {
    for (const auto& element : page->elements) {
      if (element->getTag() == TAG_PageLine) {
        positions.push_back(static_cast<const PageLine&>(*element).yPos);
      }
    }
  }
  return positions;
}

TEST(ExtraWideLayout, UsesTheExactFourteenPixelLineHeight) {
  const auto chapter = parseFixture(1.2f, 800);
  ASSERT_TRUE(chapter.ok);
  const auto positions = linePositions(chapter);
  ASSERT_GE(positions.size(), 3u);
  EXPECT_EQ(positions[1] - positions[0], 14);
  EXPECT_EQ(positions[2] - positions[1], 14);
}

TEST(ExtraWideLayout, PaginationIsMonotonicAcrossAllExistingChoices) {
  const float compression[] = {0.95f, 1.0f, 1.1f, 1.2f};
  std::vector<size_t> pageCounts;
  for (const float value : compression) {
    auto chapter = parseFixture(value, 56);
    ASSERT_TRUE(chapter.ok);
    pageCounts.push_back(chapter.pages.size());
  }

  ASSERT_EQ(pageCounts.size(), 4u);
  EXPECT_LE(pageCounts[0], pageCounts[1]);
  EXPECT_LE(pageCounts[1], pageCounts[2]);
  EXPECT_LE(pageCounts[2], pageCounts[3]);
  EXPECT_LT(pageCounts[0], pageCounts[3]);
}

}  // namespace
