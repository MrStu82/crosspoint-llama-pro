file(CREATE_LINK
  ${REPO_ROOT}/src/CrossPointSettings.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/CrossPointSettingsUnderTest.cpp
  SYMBOLIC
)

add_executable(extra_wide_settings_test
  ExtraWideSettingsTest.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/CrossPointSettingsUnderTest.cpp
)
target_include_directories(extra_wide_settings_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/stubs
  ${REPO_ROOT}/src
  ${REPO_ROOT}/lib/Epub
)
target_link_libraries(extra_wide_settings_test PRIVATE crosspoint_test_common ArduinoJson GTest::gtest_main)
gtest_discover_tests(extra_wide_settings_test)

add_executable(extra_wide_layout_test
  ExtraWideLayoutTest.cpp
  ${REPO_ROOT}/test/epub_tables/TableImageBlockStub.cpp
  ${REPO_ROOT}/test/epub_tables/HyphenatorStub.cpp
  ${REPO_ROOT}/lib/Epub/Epub/parsers/ChapterHtmlSlimParser.cpp
  ${REPO_ROOT}/lib/Epub/Epub/ParsedText.cpp
  ${REPO_ROOT}/lib/Epub/Epub/Page.cpp
  ${REPO_ROOT}/lib/Epub/Epub/blocks/TextBlock.cpp
  ${REPO_ROOT}/lib/Epub/Epub/css/CssParser.cpp
  ${REPO_ROOT}/lib/Epub/Epub/htmlEntities.cpp
  ${REPO_ROOT}/lib/Epub/Epub/hyphenation/HyphenationCommon.cpp
  ${REPO_ROOT}/lib/Utf8/Utf8.cpp
  ${REPO_ROOT}/lib/expat/xmlparse.c
  ${REPO_ROOT}/lib/expat/xmlrole.c
  ${REPO_ROOT}/lib/expat/xmltok.c
)
target_include_directories(extra_wide_layout_test PRIVATE
  ${REPO_ROOT}/test/epub_tables/stubs
  ${REPO_ROOT}/lib/Epub
  ${REPO_ROOT}/lib/EpdFont
  ${REPO_ROOT}/lib/Memory
  ${REPO_ROOT}/lib/Serialization
  ${REPO_ROOT}/lib/Utf8
  ${REPO_ROOT}/lib/XmlParserUtils
  ${REPO_ROOT}/lib/expat
)
target_compile_definitions(extra_wide_layout_test PRIVATE
  EXTRA_WIDE_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
  XML_GE=1
  XML_CONTEXT_BYTES=1024
)
target_link_libraries(extra_wide_layout_test PRIVATE crosspoint_test_common GTest::gtest_main)
gtest_discover_tests(extra_wide_layout_test)
