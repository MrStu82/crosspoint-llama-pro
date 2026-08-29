add_executable(book_reading_rate_test BookReadingRateTest.cpp)
target_include_directories(book_reading_rate_test PRIVATE ${REPO_ROOT}/src/util)
target_link_libraries(book_reading_rate_test PRIVATE GTest::gtest_main crosspoint_test_common)
gtest_discover_tests(book_reading_rate_test)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME book_eta_integration_contract
         COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/BookEtaIntegrationContractTest.py)
