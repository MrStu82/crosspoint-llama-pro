add_executable(DungeonGeneratorTest
  DungeonGeneratorTest.cpp
  ${REPO_ROOT}/src/game/DungeonGenerator.cpp
)

target_include_directories(DungeonGeneratorTest PRIVATE
  ${REPO_ROOT}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/stubs
)

target_link_libraries(DungeonGeneratorTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(DungeonGeneratorTest)
