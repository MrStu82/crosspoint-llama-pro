add_executable(GfxLineGeometryTest
  GfxLineGeometryTest.cpp
)

target_include_directories(GfxLineGeometryTest PRIVATE
  ${REPO_ROOT}/lib/GfxRenderer
)

target_link_libraries(GfxLineGeometryTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(GfxLineGeometryTest)
