find_package(ZLIB REQUIRED)

add_executable(transparent_png_composition_test
  TransparentPngCompositionTest.cpp
  ${REPO_ROOT}/lib/Epub/Epub/converters/PngToFramebufferConverter.cpp
  ${REPO_ROOT}/lib/Epub/Epub/converters/ImageToFramebufferDecoder.cpp
)
target_include_directories(transparent_png_composition_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/png_stubs
  ${REPO_ROOT}/lib/Epub
  ${REPO_ROOT}/lib/Epub/Epub/converters
  ${REPO_ROOT}/lib/Memory
)
target_compile_definitions(transparent_png_composition_test PRIVATE
  TRANSPARENT_SLEEP_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
)
target_link_libraries(transparent_png_composition_test PRIVATE
  crosspoint_test_common
  GTest::gtest_main
  ZLIB::ZLIB
)
gtest_discover_tests(transparent_png_composition_test)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(
  NAME TransparentSleep.WakeCleanupContract
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/WakeCleanupContractTest.py ${REPO_ROOT}
)

add_executable(transparent_sleep_core_test TransparentSleepCoreTest.cpp)
target_include_directories(transparent_sleep_core_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/core_stubs
  ${REPO_ROOT}/lib/Memory
  ${REPO_ROOT}/src
)
target_link_libraries(transparent_sleep_core_test PRIVATE crosspoint_test_common GTest::gtest_main)
gtest_discover_tests(transparent_sleep_core_test)

add_executable(transparent_sleep_state_test
  TransparentSleepStateTest.cpp
  ${REPO_ROOT}/src/CrossPointState.cpp
)
target_include_directories(transparent_sleep_state_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/state_stubs
  ${REPO_ROOT}/src
)
target_link_libraries(transparent_sleep_state_test PRIVATE
  crosspoint_test_common
  ArduinoJson
  GTest::gtest_main
)
gtest_discover_tests(transparent_sleep_state_test)
