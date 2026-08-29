#include <gtest/gtest.h>

#include <fstream>
#include <sstream>

#include "BookReadingStats.h"
#include "HardcoverRating.h"

namespace {
std::string fixture(const char* name) {
  std::ifstream input(std::string(HARDCOVER_FIXTURE_DIR) + "/" + name);
  std::ostringstream content;
  content << input.rdbuf();
  return content.str();
}
}  // namespace

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

TEST(BookReadingStats, EtaRequiresQualifiedRateAndRemainingPageEvidence) {
  EXPECT_FALSE(BookReadingStatsValue{}.available);
  BookReadingStatsValue confident;
  confident.available = true;
  confident.remainingAvailable = true;
  confident.currentRate = true;
  confident.remainingPagesQ16 = 40U * BookReadingRate::kQ16One;
  confident.pagesPerMinuteQ16 = BookReadingRate::kQ16One;
  EXPECT_TRUE(confident.etaConfident());
  ASSERT_TRUE(confident.bookMinutes());
  EXPECT_EQ(*confident.bookMinutes(), 40U);
  confident.remainingAvailable = false;
  EXPECT_FALSE(confident.etaConfident());
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

#include "HardcoverSyncResult.h"

TEST(HardcoverRating, SyncResultOmitsZeroCategoriesAndUsesRequiredCopy) {
  EXPECT_EQ(HardcoverSyncResult::format(2, 0, 0, 0, 0),
            (std::vector<std::string>{"Hardcover sync finished", "Updated: 2"}));
  EXPECT_EQ(HardcoverSyncResult::format(0, 3, 0, 1, 2),
            (std::vector<std::string>{"Hardcover sync finished", "Unmatched: 3",
              "Hardcover sign-in failed. Check token in Settings.",
              "Couldn't reach Hardcover. Check Wi-Fi and retry."}));
}

TEST(HardcoverRating, FuzzyCandidatesAreRankedButNeverReturnedAsExactMatch) {
  const HardcoverBookIdentity book{"book-key", "", "The Hobbit", "J.R.R. Tolkien"};
  const char* body = R"({"data":{"search":{"results":{"hits":[{"document":{"id":"near","title":"The Hobbit, or There and Back Again","author_names":["J. R. R. Tolkien"],"release_year":1937,"rating":4.3,"ratings_count":100}}]}}}})";
  HardcoverSearchDiagnostics stats;
  const auto candidates = HardcoverRating::parseSearchCandidates(book, body, 1, false, &stats);
  ASSERT_EQ(candidates.size(), 1u);
  EXPECT_EQ(candidates[0].snapshot.sourceId, "near");
  EXPECT_FALSE(HardcoverRating::parseSearchResponse(book, body, 1, false));
  EXPECT_EQ(stats.returned, 1);
  EXPECT_EQ(stats.titleRejected, 1);
}

TEST(HardcoverRating, SelectionPersistsOnlyCandidateChoicesAndSkipDoesNotPersist) {
  EXPECT_TRUE(HardcoverSyncResult::shouldPersistSelection(0, 1));
  EXPECT_TRUE(HardcoverSyncResult::shouldPersistSelection(4, 5));
  EXPECT_FALSE(HardcoverSyncResult::shouldPersistSelection(5, 5)); // Skip
}

TEST(HardcoverRating, EmptyCapturedResponseHasNoSuggestions) {
  const HardcoverBookIdentity book{"book-key", "", "Missing", "Nobody"};
  const auto candidates = HardcoverRating::parseSearchCandidates(book, R"({"data":{"search":{"results":{"hits":[]}}}})", 1, false);
  EXPECT_TRUE(candidates.empty());
  EXPECT_EQ(HardcoverSyncResult::format(0, 1, 1, 0, 0),
            (std::vector<std::string>{"Hardcover sync finished", "Unmatched: 1", "No Hardcover suggestions found."}));
}

TEST(HardcoverRating, CapturedLiveDccKeepsTheValidCatalogCandidate) {
  const HardcoverBookIdentity book{"book-key", "", "Dungeon Crawler Carl: A LitRPG/Gamelit Adventure", "Matt Dinniman"};
  HardcoverSearchDiagnostics stats;
  const auto candidates = HardcoverRating::parseSearchCandidates(book, fixture("hardcover-live-dcc.json"), 1, false, &stats);
  ASSERT_EQ(candidates.size(), 1u);
  EXPECT_EQ(candidates[0].snapshot.sourceId, "446681");
  EXPECT_EQ(candidates[0].title, "Dungeon Crawler Carl");
  EXPECT_EQ(candidates[0].author, "Matt Dinniman");
  EXPECT_EQ(stats.returned, 1);
  EXPECT_EQ(stats.invalid, 0);
}

TEST(HardcoverRating, CapturedLiveNeverendingRejectsNotebookAndFindsMichaelEnde) {
  const HardcoverBookIdentity book{"book-key", "", "The Neverending Story", "Michael Ende"};
  HardcoverSearchDiagnostics stats;
  const auto body = fixture("hardcover-live-neverending.json");
  const auto candidates = HardcoverRating::parseSearchCandidates(book, body, 1, false, &stats);
  ASSERT_EQ(candidates.size(), 3u);
  EXPECT_EQ(candidates[0].snapshot.sourceId, "144950");
  EXPECT_EQ(candidates[0].author, "Michael Ende");
  EXPECT_EQ(stats.returned, 5);
  EXPECT_EQ(stats.invalid, 2);
  const auto exact = HardcoverRating::parseSearchResponse(book, body, 1, false);
  ASSERT_TRUE(exact);
  EXPECT_EQ(exact->sourceId, "144950");
}
