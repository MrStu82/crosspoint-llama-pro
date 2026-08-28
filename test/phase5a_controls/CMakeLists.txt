add_executable(phase5a_controls_test Phase5AControlsTest.cpp)
target_include_directories(phase5a_controls_test PRIVATE ${REPO_ROOT})
target_link_libraries(phase5a_controls_test PRIVATE crosspoint_test_common GTest::gtest_main)
add_test(NAME phase5a_controls COMMAND phase5a_controls_test)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME phase5a_contract
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/Phase5AContractTest.py)

add_library(phase5a_sheet_compile OBJECT
  ${REPO_ROOT}/src/components/BrightnessSheet.cpp
  SheetCompileStubs.cpp
)
target_include_directories(phase5a_sheet_compile BEFORE PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/stubs
  ${REPO_ROOT}/src
)
target_link_libraries(phase5a_sheet_compile PRIVATE crosspoint_test_common)
