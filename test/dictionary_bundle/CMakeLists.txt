add_executable(dictionary_bundle_lookup_test
  DictionaryBundleLookupTest.cpp
  ${REPO_ROOT}/src/util/Dictionary.cpp
)

target_include_directories(dictionary_bundle_lookup_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/stubs
  ${REPO_ROOT}/src/util
  ${REPO_ROOT}/lib/Memory
)

target_link_libraries(dictionary_bundle_lookup_test PRIVATE crosspoint_test_common GTest::gtest_main)
gtest_discover_tests(dictionary_bundle_lookup_test)
