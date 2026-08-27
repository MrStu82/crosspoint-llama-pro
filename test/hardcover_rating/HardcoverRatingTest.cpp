#include <gtest/gtest.h>

#include "BookReadingStats.h"
#include "HardcoverRating.h"

TEST(HardcoverRating, IsbnIsPreferredInGraphqlVariables) {
  const HardcoverBookIdentity book{"book-key", "9780593820247", "Dungeon Crawler Carl", "Matt Dinniman"};
  const auto payload = HardcoverRating::buildSearchPayload(book, true);
  EXPECT_NE(payload.find("9780593820247"), std::string::npos);
  EXPECT_EQ(payload.find("Dungeon Crawler Carl Matt Dinniman"), std::string::npos);
  EXPECT_EQ(payload.find("Bearer"), std::string::npos);
}

TEST(HardcoverRating, FallbackSearchesTitleOnlyAndNeverLeaksAuthorIntoQuery) {
  const HardcoverBookIdentity book{"book-key", "", "Dungeon Crawler Carl", "Matt Dinniman"};
  const auto payload = HardcoverRating::buildSearchPayload(book, false);
  EXPECT_NE(payload.find("Dungeon Crawler Carl"), std::string::npos);
  EXPECT_EQ(payload.find("Matt Dinniman"), std::string::npos);
}

TEST(HardcoverRating, IsbnMatchParsesTypesenseResult) {
  const HardcoverBookIdentity book{"book-key", "9780593820247", "Dungeon Crawler Carl", "Matt Dinniman"};
  const char* body = R"({"data":{"search":{"error":null,"results":{"hits":[{"document":{
    "id":"123","slug":"dungeon-crawler-carl","title":"Dungeon Crawler Carl",
    "author_names":["Matt Dinniman"],"isbns":["9780593820247"],"release_year":2020,
    "rating":4.44,"ratings_count":123}}]}}}})";
  const auto rating = HardcoverRating::parseSearchResponse(book, body, 123456, true);
  ASSERT_TRUE(rating);
  EXPECT_EQ(rating->sourceId, "123");
  EXPECT_EQ(rating->valueX100, 444);
  EXPECT_EQ(rating->ratingCount, 123u);
  EXPECT_EQ(rating->publicationYear, 2020);
}

TEST(HardcoverRating, ExactFallbackSkipsNearMatchAndAcceptsEncodedResultsBlob) {
  const HardcoverBookIdentity book{"book-key", "", "Dungeon Crawler Carl", "Matt Dinniman"};
  const char* body = R"({"data":{"search":{"error":null,"results":"{\"hits\":[{\"document\":{\"id\":1,\"slug\":\"wrong\",\"title\":\"Dungeon Crawler Carl 2\",\"author_names\":[\"Matt Dinniman\"],\"rating\":4.9,\"ratings_count\":4}},{\"document\":{\"id\":2,\"slug\":\"dungeon-crawler-carl\",\"title\":\" dungeon crawler carl \",\"author_names\":[\"Matt Dinniman\"],\"rating\":4.44,\"ratings_count\":123}}]}"}}})";
  const auto rating = HardcoverRating::parseSearchResponse(book, body, 123456, false);
  ASSERT_TRUE(rating);
  EXPECT_EQ(rating->sourceId, "2");
}

// Captured Hardcover search shape: title-only search returns a companion before
// the requested book. The parser must author-gate it, while accepting harmless
// punctuation variation in the real author name.
TEST(HardcoverRating, CapturedTitleSearchRejectsCompanionAndCanonicalizesAuthorPunctuation) {
  const HardcoverBookIdentity book{"book-key", "", "The Hobbit", "J.R.R. Tolkien"};
  const char* body = R"({"data":{"search":{"error":null,"results":{"hits":[
    {"document":{"id":"companion","title":"The Hobbit, or There and Back Again","author_names":["J. R. R. Tolkien"],"rating":4.8,"ratings_count":12}},
    {"document":{"id":"real","slug":"the-hobbit","title":"The Hobbit","author_names":["J. R. R. Tolkien"],"rating":4.3,"ratings_count":100}}
  ]}}}})";
  const auto rating = HardcoverRating::parseSearchResponse(book, body, 123456, false);
  ASSERT_TRUE(rating);
  EXPECT_EQ(rating->sourceId, "real");
}

TEST(HardcoverRating, CapturedIsbnSearchCanonicalizesPunctuation) {
  const HardcoverBookIdentity book{"book-key", "978-0-261-10221-7", "The Hobbit", "J.R.R. Tolkien"};
  const char* body = R"({"data":{"search":{"error":null,"results":{"hits":[{"document":{
    "id":"real","slug":"the-hobbit","title":"The Hobbit","author_names":["J. R. R. Tolkien"],
    "isbns":["978 0 261 10221 7"],"rating":4.3,"ratings_count":100}}]}}}})";
  const auto rating = HardcoverRating::parseSearchResponse(book, body, 123456, true);
  ASSERT_TRUE(rating);
  EXPECT_EQ(rating->sourceId, "real");
}

TEST(HardcoverRating, ErrorsMissingRatingsAndNearMatchesStayUnresolved) {
  const HardcoverBookIdentity book{"book-key", "", "Dungeon Crawler Carl", "Matt Dinniman"};
  EXPECT_FALSE(HardcoverRating::parseSearchResponse(book, "not-json", 1, false));
  EXPECT_FALSE(HardcoverRating::parseSearchResponse(
      book, R"({"data":{"search":{"results":{"hits":[{"document":{"id":1,"title":"Other","author_names":["Matt Dinniman"],"rating":4.5,"ratings_count":2}}]}}}})", 1, false));
  EXPECT_FALSE(HardcoverRating::parseSearchResponse(
      book, R"({"errors":[{"message":"insufficient_scope"}]})", 1, false));
}

TEST(BookReadingStats, EtaNeedsMeaningfulBookSpecificSample) {
  EXPECT_FALSE(BookReadingStatsValue{}.available);
  EXPECT_FALSE((BookReadingStatsValue{299, 5}).etaConfident());
  EXPECT_FALSE((BookReadingStatsValue{300, 4}).etaConfident());
  const BookReadingStatsValue confident{600, 10};
  EXPECT_TRUE(confident.etaConfident());
  EXPECT_EQ(confident.secondsPerPage(), 60u);
}

TEST(HardcoverRating, CapturedTitleSearchRanksExactAuthorThenYearAndLeavesAmbiguityForConfirmation) {
  const HardcoverBookIdentity book{"book-key", "", "Dune", "Frank Herbert"};
  const char* body = R"({"data":{"search":{"error":null,"results":{"hits":[
    {"document":{"id":"other","title":"Dune","author_names":["Brian Herbert"],"release_year":2021,"rating":4.0,"ratings_count":9}},
    {"document":{"id":"old","title":"Dune","author_names":["Frank Herbert"],"release_year":1965,"rating":4.2,"ratings_count":99}},
    {"document":{"id":"new","title":"Dune","author_names":["Frank Herbert"],"release_year":2024,"rating":4.3,"ratings_count":100}}
  ]}}}})";
  const auto candidates = HardcoverRating::parseSearchCandidates(book, body, 123456, false);
  ASSERT_EQ(candidates.size(), 3u);
  EXPECT_EQ(candidates[0].snapshot.sourceId, "new");
  EXPECT_EQ(candidates[1].snapshot.sourceId, "old");
  EXPECT_FALSE(HardcoverRating::parseSearchResponse(book, body, 123456, false));
}
