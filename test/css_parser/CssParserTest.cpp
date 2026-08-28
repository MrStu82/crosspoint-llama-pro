#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "Arduino.h"
#include "CssParser.h"

namespace fs = std::filesystem;

namespace {

size_t allocationGateCalls = 0;
size_t failAtFirstAttemptCall = 0;
bool failEveryAttemptAfterPartial = false;

bool allocationGate(const size_t) {
  ++allocationGateCalls;
  if (failEveryAttemptAfterPartial) return allocationGateCalls != 4 && allocationGateCalls != 8;
  return allocationGateCalls != failAtFirstAttemptCall;
}

bool alwaysFailAllocation(const size_t) {
  ++allocationGateCalls;
  return false;
}

class CssParserTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
    directory_ = fs::temp_directory_path() / "crosspoint_css_retry_test" / info->name();
    fs::remove_all(directory_);
    fs::create_directories(directory_);
    allocationGateCalls = 0;
    failAtFirstAttemptCall = 0;
    failEveryAttemptAfterPartial = false;
    ESP.freeHeap = UINT32_MAX;
    Storage.clearFailures();
    CssParser::setCacheAllocationGateForTesting(nullptr);
  }

  void TearDown() override {
    CssParser::setCacheAllocationGateForTesting(nullptr);
    ESP.freeHeap = UINT32_MAX;
    Storage.clearFailures();
    fs::remove_all(directory_);
  }

  std::string cachePath() const { return directory_.string(); }
  fs::path cacheFile() const { return directory_ / "css_rules.cache"; }
  fs::path cacheTempFile() const { return directory_ / "css_rules.cache.tmp"; }

  bool loadCss(CssParser& parser, const std::string& css) const {
    const fs::path sourcePath = directory_ / "input.css";
    std::ofstream output(sourcePath, std::ios::binary | std::ios::trunc);
    output.write(css.data(), static_cast<std::streamsize>(css.size()));
    output.close();

    HalFile source;
    EXPECT_TRUE(Storage.openFileForRead("TST", sourcePath.string(), source));
    return parser.loadFromStream(source);
  }

  void buildCache() const {
    CssParser writer(cachePath());
    ASSERT_TRUE(loadCss(writer,
                        "p { text-align: justify; }\n"
                        ".bold { font-weight: bold; }\n"
                        "p.note { font-style: italic; }\n"));
    ASSERT_TRUE(writer.saveToCache());
  }

  fs::path directory_;
};

TEST_F(CssParserTest, NormalParserAndCacheV9ApiRemainUnchanged) {
  CssParser parser(cachePath());
  ASSERT_TRUE(loadCss(parser,
                      "P { text-align: center; }\n"
                      ".Note { font-weight: bold; }\n"
                      "p.note { font-style: italic; }\n"
                      "div > p, #hero, article p { display: none; }\n"));

  EXPECT_EQ(3u, parser.ruleCount());
  const CssStyle style = parser.resolveStyle("p", "NOTE");
  EXPECT_EQ(CssTextAlign::Center, style.textAlign);
  EXPECT_EQ(CssFontWeight::Bold, style.fontWeight);
  EXPECT_EQ(CssFontStyle::Italic, style.fontStyle);
  EXPECT_NE(CssDisplay::None, parser.resolveStyle("p", "").display);

  ASSERT_TRUE(parser.saveToCache());
  std::ifstream cache(cacheFile(), std::ios::binary);
  ASSERT_TRUE(cache.good());
  EXPECT_EQ(CssParser::CSS_CACHE_VERSION, static_cast<uint8_t>(cache.get()));

  CssParser reader(cachePath());
  ASSERT_TRUE(reader.loadFromCache());
  EXPECT_EQ(3u, reader.ruleCount());
  EXPECT_EQ(CssFontWeight::Bold, reader.resolveStyle("p", "note").fontWeight);
}

TEST_F(CssParserTest, FirstAttemptOomClearsPartialStateAndSingleRetrySucceeds) {
  buildCache();
  failAtFirstAttemptCall = 4;  // second selector allocation, after one inserted rule
  CssParser::setCacheAllocationGateForTesting(allocationGate);

  CssParser reader(cachePath());
  EXPECT_TRUE(reader.loadFromCache());
  EXPECT_EQ(3u, reader.ruleCount());
  EXPECT_EQ(11u, allocationGateCalls);  // four first-attempt checks + seven complete retry checks
  EXPECT_EQ(CssTextAlign::Justify, reader.resolveStyle("p", "").textAlign);
  EXPECT_EQ(CssFontWeight::Bold, reader.resolveStyle("span", "bold").fontWeight);
  EXPECT_EQ(CssFontStyle::Italic, reader.resolveStyle("p", "note").fontStyle);
}

TEST_F(CssParserTest, RetryFailureIsHonestAndLeavesNoPartialRules) {
  buildCache();
  failEveryAttemptAfterPartial = true;
  CssParser::setCacheAllocationGateForTesting(allocationGate);

  CssParser reader(cachePath());
  EXPECT_FALSE(reader.loadFromCache());
  EXPECT_TRUE(reader.empty());
  EXPECT_EQ(8u, allocationGateCalls);
  EXPECT_TRUE(reader.hasCache());  // valid disk cache is available for a later higher-memory load

  CssParser::setCacheAllocationGateForTesting(nullptr);
  EXPECT_TRUE(reader.loadFromCache());
  EXPECT_EQ(3u, reader.ruleCount());
}

TEST_F(CssParserTest, RetryIsStrictlyBoundedToTwoAttempts) {
  buildCache();
  CssParser::setCacheAllocationGateForTesting(alwaysFailAllocation);

  CssParser reader(cachePath());
  EXPECT_FALSE(reader.loadFromCache());
  EXPECT_TRUE(reader.empty());
  EXPECT_EQ(2u, allocationGateCalls);
  EXPECT_TRUE(reader.hasCache());
}

TEST_F(CssParserTest, RealLowHeapGuardUsesTheSameBoundedCleanupPath) {
  buildCache();
  ESP.freeHeap = 1024;

  CssParser reader(cachePath());
  EXPECT_FALSE(reader.loadFromCache());
  EXPECT_TRUE(reader.empty());
  EXPECT_TRUE(reader.hasCache());
}

TEST_F(CssParserTest, InvalidCacheIsDeletedAndNeverReused) {
  buildCache();
  {
    std::fstream cache(cacheFile(), std::ios::binary | std::ios::in | std::ios::out);
    const uint8_t staleVersion = CssParser::CSS_CACHE_VERSION - 1;
    cache.write(reinterpret_cast<const char*>(&staleVersion), sizeof(staleVersion));
  }

  CssParser reader(cachePath());
  EXPECT_FALSE(reader.loadFromCache());
  EXPECT_TRUE(reader.empty());
  EXPECT_FALSE(reader.hasCache());
  EXPECT_FALSE(reader.loadFromCache());
}

TEST_F(CssParserTest, FailedAtomicReplacementPreservesPreviousCache) {
  CssParser writer(cachePath());
  ASSERT_TRUE(loadCss(writer, ".original { font-weight: bold; }\n"));
  ASSERT_TRUE(writer.saveToCache());

  writer.clear();
  ASSERT_TRUE(loadCss(writer, ".replacement { font-style: italic; }\n"));
  Storage.failNextRename(cacheTempFile().string(), cacheFile().string());
  EXPECT_FALSE(writer.saveToCache());
  EXPECT_TRUE(fs::exists(cacheFile()));
  EXPECT_FALSE(fs::exists(cacheTempFile()));

  CssParser reader(cachePath());
  ASSERT_TRUE(reader.loadFromCache());
  EXPECT_EQ(1u, reader.ruleCount());
  EXPECT_EQ(CssFontWeight::Bold, reader.resolveStyle("span", "original").fontWeight);
  EXPECT_FALSE(reader.resolveStyle("span", "replacement").hasFontStyle());
}

}  // namespace
