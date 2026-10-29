#include <gtest/gtest.h>

#include "AuthorBio.h"

TEST(AuthorBio, HardcoverPayloadCarriesNameAsVariable) {
  const auto payload = AuthorBio::buildHardcoverPayload("Terry \"T\" Pratchett");
  EXPECT_NE(payload.find("authors(where:{name:{_eq:$name}}"), std::string::npos);
  EXPECT_NE(payload.find(R"("name":"Terry \"T\" Pratchett")"), std::string::npos);
}

TEST(AuthorBio, HardcoverTakesFirstNonEmptyBio) {
  EXPECT_EQ(AuthorBio::parseHardcover(R"({"data":{"authors":[{"bio":null},{"bio":""},{"bio":"Writer."}]}})"),
            "Writer.");
  EXPECT_EQ(AuthorBio::parseHardcover(R"({"errors":[{"message":"x"}],"data":null})"), "");
  EXPECT_EQ(AuthorBio::parseHardcover(R"({"data":{"authors":[]}})"), "");
}

TEST(AuthorBio, OpenLibrarySearchUrlPercentEncodes) {
  EXPECT_EQ(AuthorBio::openLibrarySearchUrl("J.R.R. Tolkien & Co"),
            "https://openlibrary.org/search/authors.json?limit=1&fields=key,name&q=J.R.R.%20Tolkien%20%26%20Co");
}

TEST(AuthorBio, OpenLibraryKeyRequiresMatchingName) {
  const char* body = R"({"numFound":2,"docs":[{"key":"OL26320A","name":"J.R.R. Tolkien"}]})";
  EXPECT_EQ(AuthorBio::parseOpenLibraryKey(body, "J. R. R. Tolkien"), "/authors/OL26320A");
  EXPECT_EQ(AuthorBio::parseOpenLibraryKey(body, "Christopher Tolkien"), "");
  EXPECT_EQ(AuthorBio::parseOpenLibraryKey(R"({"numFound":0,"docs":[]})", "Anyone"), "");
}

TEST(AuthorBio, OpenLibraryBioBothShapesWithoutSourceOrEmphasis) {
  EXPECT_EQ(AuthorBio::parseOpenLibraryBio(R"({"bio":"Wrote *The Carpet People*.\r\n\r\n([Source][1])\r\n\r\n[1]: http://x"})"),
            "Wrote The Carpet People.\r\n\r\n");
  EXPECT_EQ(AuthorBio::parseOpenLibraryBio(R"({"bio":{"type":"/type/text","value":"Scholar."}})"), "Scholar.");
  EXPECT_EQ(AuthorBio::parseOpenLibraryBio(R"({"name":"No bio"})"), "");
}
