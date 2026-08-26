add_executable(daily_quote_test
  DailyQuoteTest.cpp
  ${REPO_ROOT}/src/util/DailyQuote.cpp
)
target_include_directories(daily_quote_test PRIVATE ${REPO_ROOT}/src/util)
target_link_libraries(daily_quote_test PRIVATE GTest::gtest_main crosspoint_test_common)
gtest_discover_tests(daily_quote_test)
