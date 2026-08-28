add_executable(dictionary_styled_pages_test
  DictionaryStyledPagesTest.cpp
  ${REPO_ROOT}/src/util/DictHtmlPages.cpp
  ${REPO_ROOT}/src/util/HtmlToPlainText.cpp
  ${REPO_ROOT}/lib/Epub/Epub/htmlEntities.cpp
)

target_include_directories(dictionary_styled_pages_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/stubs
  ${REPO_ROOT}/src/util
  ${REPO_ROOT}/lib/Epub
  ${REPO_ROOT}/lib/Memory
)

target_link_libraries(dictionary_styled_pages_test PRIVATE crosspoint_test_common GTest::gtest_main)
gtest_discover_tests(dictionary_styled_pages_test)
