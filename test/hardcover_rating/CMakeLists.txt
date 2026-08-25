add_executable(HardcoverRatingTest
  HardcoverRatingTest.cpp
  ${REPO_ROOT}/src/network/HardcoverRatingParser.cpp
)

target_include_directories(HardcoverRatingTest PRIVATE ${REPO_ROOT}/src/network ${REPO_ROOT}/src/util)
target_link_libraries(HardcoverRatingTest PRIVATE crosspoint_test_common ArduinoJson GTest::gtest_main)
gtest_discover_tests(HardcoverRatingTest)
