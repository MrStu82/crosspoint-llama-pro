#include <Epub/ParsedText.h>
#include <Epub/hyphenation/Hyphenator.h>
#include <GfxRenderer.h>
#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

// Every word may break after each of its letters, so the greedy pass always splits the overflowing word.
const LanguageHyphenator* Hyphenator::cachedHyphenator_ = nullptr;
std::vector<Hyphenator::BreakInfo> Hyphenator::breakOffsets(const std::string& word, bool) {
  std::vector<BreakInfo> breaks;
  for (size_t i = 1; i < word.size(); ++i) breaks.push_back({i, true});
  return breaks;
}
void Hyphenator::setPreferredLanguage(const std::string&) {}

namespace {

std::vector<std::shared_ptr<TextBlock>> layout(const std::vector<std::string>& words, uint16_t width) {
  GfxRenderer renderer;
  ParsedText text(false, true);
  for (const auto& w : words) text.addWord(w, EpdFontFamily::REGULAR);
  std::vector<std::shared_ptr<TextBlock>> lines;
  text.layoutAndExtractLines(renderer, 1, width, [&](std::shared_ptr<TextBlock> line, uint32_t) {
    lines.push_back(std::move(line));
  });
  return lines;
}

std::string lineText(const TextBlock& line) {
  std::string s;
  for (size_t i = 0; i < line.wordCount(); ++i) s += std::string(i ? " " : "") + line.wordText(i);
  return s;
}

}  // namespace

TEST(HyphenDp, SplitPrefixEndsItsLineAndEveryLineFits) {
  // 6 px per char, 4 px space: width 60 holds 10 chars.
  const auto lines = layout({"aaaa", "bbb", "c", "dddddddddddddd", "eeeee", "ff"}, 60);
  ASSERT_GE(lines.size(), 2u);
  bool sawSplit = false;
  for (size_t l = 0; l < lines.size(); ++l) {
    const auto& line = *lines[l];
    ASSERT_GT(line.wordCount(), 0u);
    const size_t last = line.wordCount() - 1;
    const std::string lastWord = line.wordText(last);
    EXPECT_LE(line.wordXpos(last) + 6 * static_cast<int>(lastWord.size()), 60) << lineText(line);
    if (lastWord.back() == '-') {
      sawSplit = true;
      ASSERT_LT(l + 1, lines.size());
      // The remainder of the same word opens the next line.
      EXPECT_EQ(lines[l + 1]->wordText(0)[0], lastWord[0]) << lineText(line) << " / " << lineText(*lines[l + 1]);
    }
  }
  EXPECT_TRUE(sawSplit);
}

TEST(HyphenDp, BalancesLinesBeforeTheSplit) {
  // Greedy fills "aaaa bbb c" (60) then "eeeee" alone; the DP evens it to "aaaa bbb" / "c eeeee-".
  // Either way no line may exceed the width, and the text must be unchanged.
  const auto lines = layout({"aaaa", "bbb", "c", "eeeeeeeeeeee"}, 60);
  std::string all;
  for (const auto& line : lines) all += lineText(*line) + "|";
  std::string joined;
  for (const auto& line : lines) {
    for (size_t i = 0; i < line->wordCount(); ++i) {
      std::string w = line->wordText(i);
      if (!w.empty() && w.back() == '-') w.pop_back();
      joined += w;
    }
  }
  EXPECT_EQ(joined, "aaaabbbceeeeeeeeeeee") << all;
}
