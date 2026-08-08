add_executable(StatsMigrationTest
  StatsMigrationTest.cpp
  ${REPO_ROOT}/src/activities/home/StatsMigration.cpp
)
target_include_directories(StatsMigrationTest PRIVATE
  ${REPO_ROOT}/src/activities/home
)
target_link_libraries(StatsMigrationTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)
gtest_discover_tests(StatsMigrationTest)
