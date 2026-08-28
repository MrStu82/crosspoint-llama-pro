#include <gtest/gtest.h>

#include <Arduino.h>
#include <Epub/parsers/ChapterHtmlSlimParser.h>
#include <HalStorage.h>

#include <memory>
#include <string>
#include <vector>

#include "DictHtmlPageBudget.h"
#include "DictHtmlPages.h"
#include "DictionaryDefinitionPaging.h"
#include "HtmlToPlainText.h"

class GfxRenderer {};

namespace {

TEST(DictionaryStyledPages, KeepsFirstPageAtTheLowerRetainHeapFloor) {
  DictionaryParserStub::reset();
  ESP.freeHeap = 50 * 1024;
  ESP.maxAllocHeap = 24 * 1024;
  GfxRenderer renderer;
  std::vector<std::unique_ptr<Page>> pages;

  ASSERT_TRUE(buildDictionaryHtmlPages(renderer, "<h1>Sense</h1><p><b>styled</b> definition</p>", 420, 600, pages));
  ASSERT_EQ(pages.size(), 2u);
  EXPECT_NE(StyledStorageStub::staged.find("<html><body>"), std::string::npos);
  EXPECT_NE(StyledStorageStub::staged.find("<h1>Sense</h1>"), std::string::npos);
  EXPECT_NE(StyledStorageStub::staged.find("<b>styled</b>"), std::string::npos);
}

TEST(DictionaryStyledPages, RetainBudgetDistinguishesParserWorkingSetFromEntryGate) {
  EXPECT_EQ(DictHtmlPageBudget::retainedPageLimit(0, 0, 8, 29400, 12000), DictHtmlPageBudget::Limit::None);
  EXPECT_EQ(DictHtmlPageBudget::retainedPageLimit(0, 0, 8, 15000, 12000), DictHtmlPageBudget::Limit::Heap);
  EXPECT_EQ(DictHtmlPageBudget::retainedPageLimit(DictHtmlPageBudget::MAX_PAGES, 0, 1, UINT32_MAX, UINT32_MAX),
            DictHtmlPageBudget::Limit::PageCount);
}

TEST(DictionaryStyledPages, FailedStyledLayoutHasDeterministicPlainTextHeadingFallback) {
  DictionaryParserStub::reset();
  DictionaryParserStub::parseOk = false;
  ESP.freeHeap = 50 * 1024;
  ESP.maxAllocHeap = 24 * 1024;
  GfxRenderer renderer;
  std::vector<std::unique_ptr<Page>> pages;
  const std::string definition = "<h1>Sense</h1><p><b>plain</b> fallback</p>";

  EXPECT_FALSE(buildDictionaryHtmlPages(renderer, definition, 420, 600, pages));
  EXPECT_TRUE(pages.empty());
  EXPECT_EQ(htmlToPlainText(definition), "Sense\n\nplain fallback");
}

TEST(DictionaryStyledPages, StyledPaginationStartsOnDisplayedPageOne) {
  const DictionaryPagePosition initial = initialDictionaryPage(3);
  EXPECT_EQ(initial.current, 0);
  EXPECT_EQ(initial.total, 3);
  EXPECT_EQ(initial.current + 1, 1);  // the UI's displayed page number
}

TEST(DictionaryStyledPages, BackButtonsAndTouchKeepExistingNavigationBounds) {
  auto transition = transitionDictionaryPage(0, 3, DictionaryPageCommand::Close);
  EXPECT_TRUE(transition.close);
  EXPECT_FALSE(transition.changed);

  EXPECT_EQ(dictionaryTapCommand(99, 300), DictionaryPageCommand::Previous);
  EXPECT_EQ(dictionaryTapCommand(100, 300), DictionaryPageCommand::Next);
  transition = transitionDictionaryPage(0, 3, dictionaryTapCommand(250, 300));
  EXPECT_EQ(transition.page, 1);
  EXPECT_TRUE(transition.changed);

  transition = transitionDictionaryPage(2, 3, DictionaryPageCommand::Next);
  EXPECT_EQ(transition.page, 2);
  EXPECT_FALSE(transition.changed);
  transition = transitionDictionaryPage(2, 3, DictionaryPageCommand::Previous);
  EXPECT_EQ(transition.page, 1);
  EXPECT_TRUE(transition.changed);
}

}  // namespace
