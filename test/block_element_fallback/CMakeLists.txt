add_executable(BlockElementFallbackTest
  BlockElementFallbackTest.cpp
)

target_include_directories(BlockElementFallbackTest PRIVATE
  ${REPO_ROOT}/lib/GfxRenderer
)

target_link_libraries(BlockElementFallbackTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(BlockElementFallbackTest)
