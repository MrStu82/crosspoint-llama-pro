add_executable(DirtyRectPlannerHarness
  DirtyRectPlannerHarness.cpp
)

target_include_directories(DirtyRectPlannerHarness PRIVATE
  ${REPO_ROOT}/src/game
)

# Plain CHECK()-based harness (not GTest) that returns its failure count as
# the process exit code -- ctest treats any nonzero exit as a failing test.
add_test(NAME DirtyRectPlannerHarness COMMAND DirtyRectPlannerHarness)
