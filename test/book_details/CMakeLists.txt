add_executable(BookDetailsTextTest
  BookDetailsTextTest.cpp
  ${REPO_ROOT}/src/util/BookDetailsText.cpp
  ${REPO_ROOT}/src/util/HtmlToPlainText.cpp
  ${REPO_ROOT}/lib/Epub/Epub/htmlEntities.cpp
)

target_include_directories(BookDetailsTextTest PRIVATE
  ${REPO_ROOT}/src/util
  ${REPO_ROOT}/lib/Epub
)

target_link_libraries(BookDetailsTextTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(BookDetailsTextTest)
