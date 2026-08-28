add_executable(koreader_wifi_awake_test
  KoreaderWifiAwakeHarness.cpp
  ${REPO_ROOT}/src/network/WifiAwakeLock.cpp
)
target_include_directories(koreader_wifi_awake_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/stubs
  ${REPO_ROOT}/src/network
)
target_link_libraries(koreader_wifi_awake_test PRIVATE crosspoint_test_common)
add_test(NAME koreader_wifi_awake_lifecycle COMMAND koreader_wifi_awake_test)
