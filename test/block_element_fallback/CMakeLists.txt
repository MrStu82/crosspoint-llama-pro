add_executable(BlockElementFallbackTest
  BlockElementFallbackTest.cpp
)

target_include_directories(BlockElementFallbackTest PRIVATE
  ${REPO_ROOT}/lib/GfxRenderer
  ${REPO_ROOT}/lib/EpdFont
)

target_link_libraries(BlockElementFallbackTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(BlockElementFallbackTest)
