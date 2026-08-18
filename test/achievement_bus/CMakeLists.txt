add_executable(AchievementBusTest
  AchievementBusTest.cpp
  stubs/GameStateStub.cpp
  ${REPO_ROOT}/src/game/AchievementBus.cpp
)

target_include_directories(AchievementBusTest PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/stubs  # no-op Logging/HalStorage/Serialization (real ones need Arduino/FreeRTOS)
  ${REPO_ROOT}/src/game
)

target_link_libraries(AchievementBusTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(AchievementBusTest)
