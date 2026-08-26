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

TEST(HardcoverRating, FallbackUsesTitleAndAuthor) {
  const HardcoverBookIdentity book{"book-key", "", "Dungeon Crawler Carl", "Matt Dinniman"};
  const auto payload = HardcoverRating::buildSearchPayload(book, false);
  EXPECT_NE(payload.find("Dungeon Crawler Carl Matt Dinniman"), std::string::npos);
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
