add_executable(home_inkpoint_geometry_test HomeInkPointGeometryTest.cpp)
target_include_directories(home_inkpoint_geometry_test PRIVATE ${REPO_ROOT}/src/activities/home ${REPO_ROOT}/src)
target_link_libraries(home_inkpoint_geometry_test PRIVATE GTest::gtest_main crosspoint_test_common)
gtest_discover_tests(home_inkpoint_geometry_test)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME home_fourfix_source_contract
         COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/HomeFourFixContractTest.py)
