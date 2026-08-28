add_executable(frontlight_mapping_test
  FrontlightMappingTest.cpp
  ${REPO_ROOT}/freeink-sdk/libs/hardware/FrontlightManager/src/FrontlightManager.cpp
)
target_include_directories(frontlight_mapping_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/stubs
  ${REPO_ROOT}/freeink-sdk/libs/hardware/FrontlightManager/include
)
target_link_libraries(frontlight_mapping_test PRIVATE crosspoint_test_common GTest::gtest_main)
add_test(NAME frontlight_mapping COMMAND frontlight_mapping_test)
