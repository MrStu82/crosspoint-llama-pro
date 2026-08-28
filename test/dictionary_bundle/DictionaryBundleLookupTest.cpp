#include <gtest/gtest.h>

#include <DictZip.h>
#include <Dictionary.h>
#include <DictionaryRegistry.h>
#include <HalStorage.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace DictionaryRegistry {
bool resolveBasePath(const char* folderName, std::string& basePathOut) {
  if (!folderName || *folderName == '\0') return false;
  basePathOut = std::string("/dictionaries/") + folderName + "/dict";
  return true;
}
void discover(std::vector<DictionaryEntry>&) {}
}  // namespace DictionaryRegistry

namespace DictZip {
bool extractEntry(const char*, uint32_t, uint32_t, HalFile&, ExtractError* outError) {
  if (outError) *outError = ExtractError::ReadError;
  return false;
}
}  // namespace DictZip

namespace {

struct Entry {
  std::string word;
  std::string definition;
};

struct Synonym {
  std::string word;
  uint32_t ordinal;
};

void appendBe32(std::string& out, uint32_t value) {
  out.push_back(static_cast<char>((value >> 24) & 0xff));
  out.push_back(static_cast<char>((value >> 16) & 0xff));
  out.push_back(static_cast<char>((value >> 8) & 0xff));
  out.push_back(static_cast<char>(value & 0xff));
}

void writeBytes(const std::filesystem::path& path, const std::string& bytes) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  ASSERT_TRUE(out.is_open()) << path;
  out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  ASSERT_TRUE(out.good()) << path;
}

class DictionaryBundleLookupTest : public ::testing::Test {
 protected:
  void SetUp() override {
    root = std::filesystem::current_path() / "dictionary_bundle_fixture";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    Storage.root = root;
  }

  void TearDown() override { std::filesystem::remove_all(root); }

  void writeDictionary(const std::string& name, const std::vector<Entry>& entries,
                       const std::optional<std::vector<Synonym>>& synonyms = std::nullopt,
                       const std::string& ifo = "") {
    const auto dir = root / "dictionaries" / name;
    std::filesystem::create_directories(dir);

    std::string dict;
    std::string idx;
    for (const auto& entry : entries) {
      const uint32_t offset = static_cast<uint32_t>(dict.size());
      dict += entry.definition;
      idx += entry.word;
      idx.push_back('\0');
      appendBe32(idx, offset);
      appendBe32(idx, static_cast<uint32_t>(entry.definition.size()));
    }
    writeBytes(dir / "dict.idx", idx);
    writeBytes(dir / "dict.dict", dict);
    if (!ifo.empty()) writeBytes(dir / "dict.ifo", ifo);

    if (synonyms) {
      std::string syn;
      for (const auto& entry : *synonyms) {
        syn += entry.word;
        syn.push_back('\0');
        appendBe32(syn, entry.ordinal);
      }
      writeBytes(dir / "dict.syn", syn);
    }
  }

  std::filesystem::path root;
};

TEST_F(DictionaryBundleLookupTest, ResolvesSynonymOrdinalToHeadwordAndDefinition) {
  writeDictionary("synonyms", {{"cat", "a feline"}, {"theater", "a place for drama"}},
                  std::vector<Synonym>{{"theatre", 1}});
  Dictionary dictionary;
  ASSERT_TRUE(dictionary.open("synonyms"));
  EXPECT_TRUE(dictionary.needsIndex());
  ASSERT_TRUE(dictionary.buildIndex());
  EXPECT_FALSE(dictionary.needsIndex());

  std::string definition;
  std::string headword;
  Dictionary::LookupResult result = Dictionary::LookupResult::NotFound;
  ASSERT_TRUE(dictionary.lookup("theatre", definition, headword, &result));
  EXPECT_EQ(result, Dictionary::LookupResult::Found);
  EXPECT_EQ(headword, "theater");
  EXPECT_EQ(definition, "a place for drama");
}

TEST_F(DictionaryBundleLookupTest, MissingSynonymFileKeepsDirectAndStemFallbacksUsable) {
  writeDictionary("missing_syn", {{"dog", "a canine"}});
  Dictionary dictionary;
  ASSERT_TRUE(dictionary.open("missing_syn"));
  ASSERT_TRUE(dictionary.buildIndex());

  std::string definition;
  std::string headword;
  ASSERT_TRUE(dictionary.lookup("dogs", definition, headword));
  EXPECT_EQ(headword, "dog");
  EXPECT_EQ(definition, "a canine");
}

TEST_F(DictionaryBundleLookupTest, TruncatedSynonymEntryFallsBackToStemming) {
  writeDictionary("corrupt_syn", {{"dog", "a canine"}});
  const auto synPath = root / "dictionaries" / "corrupt_syn" / "dict.syn";
  writeBytes(synPath, std::string("dogs\0\0\0", 7));  // ordinal is two bytes short

  Dictionary dictionary;
  ASSERT_TRUE(dictionary.open("corrupt_syn"));
  ASSERT_TRUE(dictionary.buildIndex());

  std::string definition;
  std::string headword;
  ASSERT_TRUE(dictionary.lookup("dogs", definition, headword));
  EXPECT_EQ(headword, "dog");
  EXPECT_EQ(definition, "a canine");
}

TEST_F(DictionaryBundleLookupTest, HtmlMetadataSelectsStyledPathWithoutChangingPlainMetadata) {
  writeDictionary("html", {{"entry", "<b>styled</b>"}}, std::nullopt,
                  "StarDict's dict ifo file\nversion=2.4.2\nsametypesequence=h\n");
  Dictionary html;
  ASSERT_TRUE(html.open("html"));
  EXPECT_TRUE(html.definitionsAreHtml());

  writeDictionary("plain", {{"entry", "plain"}}, std::nullopt,
                  "StarDict's dict ifo file\nversion=2.4.2\nsametypesequence=m\n");
  Dictionary plain;
  ASSERT_TRUE(plain.open("plain"));
  EXPECT_FALSE(plain.definitionsAreHtml());
}

}  // namespace
