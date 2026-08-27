add_executable(HardcoverRatingTest
  HardcoverRatingTest.cpp
  ${REPO_ROOT}/src/network/HardcoverRatingParser.cpp
)

target_include_directories(HardcoverRatingTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/stubs ${REPO_ROOT}/src/network ${REPO_ROOT}/src/util)
target_link_libraries(HardcoverRatingTest PRIVATE crosspoint_test_common ArduinoJson GTest::gtest_main)
target_compile_definitions(HardcoverRatingTest PRIVATE HARDCOVER_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
gtest_discover_tests(HardcoverRatingTest)
