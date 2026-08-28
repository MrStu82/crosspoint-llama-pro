#include <gtest/gtest.h>

#include <Epub/Page.h>
#include <Epub/parsers/ChapterHtmlSlimParser.h>
#include <GfxRenderer.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace {

struct ParsedChapter {
  bool ok = false;
  std::vector<std::unique_ptr<Page>> pages;
};

struct LineInfo {
  size_t page;
  int16_t x;
  int16_t y;
  std::string text;
  bool bold;
};

std::string fixturePath(const char* name) { return std::string(TABLE_FIXTURE_DIR) + "/" + name; }

ParsedChapter parseFixture(const char* name, uint16_t width = 240, uint16_t height = 160) {
  ParsedChapter result;
  GfxRenderer renderer;
  const std::string path = fixturePath(name);
  ChapterHtmlSlimParser parser(
      nullptr, path, renderer, 1, 1.0f, false, static_cast<uint8_t>(CssTextAlign::Left), width, height, false, false,
      false, false,
      [&result](std::unique_ptr<Page> page, uint16_t, uint16_t, uint32_t) {
        result.pages.push_back(std::move(page));
      },
      false, "", "", 2);
  result.ok = parser.parseAndBuildPages();
  return result;
}

std::vector<LineInfo> lines(const ParsedChapter& chapter) {
  std::vector<LineInfo> out;
  for (size_t pageIndex = 0; pageIndex < chapter.pages.size(); ++pageIndex) {
    for (const auto& element : chapter.pages[pageIndex]->elements) {
      if (element->getTag() != TAG_PageLine) continue;
      const auto& line = static_cast<const PageLine&>(*element);
      const auto& block = line.getBlock();
      std::string text;
      bool bold = false;
      for (uint16_t word = 0; word < block->wordCount(); ++word) {
        if (!text.empty()) text.push_back(' ');
        text += block->wordText(word);
        bold = bold || (block->wordStyle(word) & EpdFontFamily::BOLD) != 0;
      }
      out.push_back({pageIndex, line.xPos, line.yPos, std::move(text), bold});
    }
  }
  return out;
}

bool hasGridPair(const std::vector<LineInfo>& rendered) {
  for (size_t i = 0; i < rendered.size(); ++i) {
    for (size_t j = i + 1; j < rendered.size(); ++j) {
      if (rendered[i].page == rendered[j].page && rendered[i].y == rendered[j].y && rendered[i].x != rendered[j].x) {
        return true;
      }
    }
  }
  return false;
}

std::string allText(const std::vector<LineInfo>& rendered) {
  std::string text;
  for (const auto& line : rendered) {
    if (!text.empty()) text.push_back(' ');
    text += line.text;
  }
  return text;
}

TEST(EpubTables, HeadersAndBodyRowsRenderAsColumns) {
  const auto chapter = parseFixture("headers_body.xhtml");
  ASSERT_TRUE(chapter.ok);
  const auto rendered = lines(chapter);
  ASSERT_TRUE(hasGridPair(rendered));
  EXPECT_NE(allText(rendered).find("Name Value Alpha One Beta Two"), std::string::npos);
  const auto header = std::find_if(rendered.begin(), rendered.end(), [](const LineInfo& line) {
    return line.text == "Name" || line.text == "Value";
  });
  ASSERT_NE(header, rendered.end());
  EXPECT_TRUE(header->bold);
}

TEST(EpubTables, ColspanAndRowspanFallBackToStackedFlow) {
  const auto chapter = parseFixture("spans.xhtml");
  ASSERT_TRUE(chapter.ok);
  const auto rendered = lines(chapter);
  EXPECT_FALSE(hasGridPair(rendered));
  EXPECT_NE(allText(rendered).find("Combined Tall First Second"), std::string::npos);
}

TEST(EpubTables, NestedAndEmptyTablesStayBoundedAndKeepText) {
  const auto chapter = parseFixture("nested_empty.xhtml");
  ASSERT_TRUE(chapter.ok);
  const auto rendered = lines(chapter);
  const std::string text = allText(rendered);
  EXPECT_NE(text.find("Outer"), std::string::npos);
  EXPECT_NE(text.find("Nested"), std::string::npos);
  EXPECT_NE(text.find("Tail"), std::string::npos);
  EXPECT_NE(text.find("Peer"), std::string::npos);
  EXPECT_LT(rendered.size(), 16u);
}

TEST(EpubTables, MalformedTableFailsCleanly) {
  const auto chapter = parseFixture("malformed.xhtml");
  EXPECT_FALSE(chapter.ok);
}

TEST(EpubTables, NarrowViewportFallsBackInsteadOfCreatingUnreadableColumns) {
  const auto chapter = parseFixture("narrow.xhtml", 60, 160);
  ASSERT_TRUE(chapter.ok);
  const auto rendered = lines(chapter);
  EXPECT_FALSE(hasGridPair(rendered));
  EXPECT_NE(allText(rendered).find("Left wraps safely Right wraps safely"), std::string::npos);
}

TEST(EpubTables, RowsContinueAcrossPageBoundariesWithoutLoss) {
  const auto chapter = parseFixture("page_boundaries.xhtml", 180, 28);
  ASSERT_TRUE(chapter.ok);
  ASSERT_GT(chapter.pages.size(), 1u);
  const auto rendered = lines(chapter);
  EXPECT_TRUE(hasGridPair(rendered));
  EXPECT_NE(allText(rendered).find("Key Reading A One B Two C Three D Four E Five"), std::string::npos);
}

TEST(EpubTables, PlainChapterLayoutRemainsOrdinaryFlow) {
  const auto chapter = parseFixture("plain.xhtml");
  ASSERT_TRUE(chapter.ok);
  const auto rendered = lines(chapter);
  ASSERT_FALSE(rendered.empty());
  EXPECT_FALSE(hasGridPair(rendered));
  EXPECT_EQ(allText(rendered), "A plain chapter remains ordinary flowing prose.");
  for (const auto& page : chapter.pages) {
    EXPECT_TRUE(std::none_of(page->elements.begin(), page->elements.end(), [](const auto& element) {
      return element->getTag() == TAG_PageHorizontalRule;
    }));
  }
}

TEST(EpubTables, ExistingRubyFootnoteAndImageHandlingSurvivesTableLayout) {
  const auto chapter = parseFixture("preserved_features.xhtml");
  ASSERT_TRUE(chapter.ok);
  const auto rendered = lines(chapter);
  const std::string text = allText(rendered);
  EXPECT_NE(text.find("漢"), std::string::npos);
  EXPECT_NE(text.find("Icon"), std::string::npos);  // bounded alt text inside a table cell
  EXPECT_NE(text.find("Footnote text"), std::string::npos);
  EXPECT_NE(text.find("After image handling continues."), std::string::npos);

  bool hasRuby = false;
  bool hasFootnote = false;
  bool hasImage = false;
  for (const auto& page : chapter.pages) {
    hasFootnote = hasFootnote || !page->footnotes.empty();
    for (const auto& element : page->elements) {
      hasImage = hasImage || element->getTag() == TAG_PageImage;
      if (element->getTag() == TAG_PageLine) {
        hasRuby = hasRuby || static_cast<const PageLine&>(*element).getBlock()->hasRuby();
      }
    }
  }
  EXPECT_TRUE(hasRuby);
  EXPECT_TRUE(hasFootnote);
  EXPECT_FALSE(hasImage);  // imageRendering=2 remains suppression outside table cells
}

TEST(EpubTables, ColumnAndCellCapsFallBackWithinBoundedElements) {
  const auto chapter = parseFixture("memory_bounds.xhtml", 300, 400);
  ASSERT_TRUE(chapter.ok);
  const auto rendered = lines(chapter);
  EXPECT_FALSE(hasGridPair(rendered));
  EXPECT_NE(allText(rendered).find("thirtythree bounded peer"), std::string::npos);
  size_t elements = 0;
  for (const auto& page : chapter.pages) elements += page->elements.size();
  EXPECT_LT(elements, 64u);
}

}  // namespace
