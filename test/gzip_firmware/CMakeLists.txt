find_library(MBEDCRYPTO mbedcrypto REQUIRED)
add_executable(gzip_firmware_test
  GzipFirmwareTest.cpp
  ${REPO_ROOT}/src/network/GzipFirmware.cpp
  ${REPO_ROOT}/src/network/FirmwareFlasher.cpp
  ${REPO_ROOT}/lib/miniz/src/InflateStream.cpp
  ${REPO_ROOT}/lib/miniz/src/miniz_impl.c
)
target_include_directories(gzip_firmware_test BEFORE PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/stubs
  ${REPO_ROOT}/src/network
  ${REPO_ROOT}/lib/hal
  ${REPO_ROOT}/lib/miniz/src
  ${REPO_ROOT}/lib/Memory
)
target_link_libraries(gzip_firmware_test PRIVATE GTest::gtest_main crosspoint_test_common ${MBEDCRYPTO})
gtest_discover_tests(gzip_firmware_test)
