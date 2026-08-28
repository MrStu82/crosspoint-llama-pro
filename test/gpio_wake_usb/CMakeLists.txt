add_executable(gpio_wake_usb_test GpioWakeUsbPolicyHarness.cpp)
target_include_directories(gpio_wake_usb_test PRIVATE ${REPO_ROOT}/lib/hal)
target_link_libraries(gpio_wake_usb_test PRIVATE crosspoint_test_common)
add_test(NAME gpio_wake_usb_policy COMMAND gpio_wake_usb_test)
add_test(NAME wake_resume_main_contract
         COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/WakeResumeMainContractTest.py ${REPO_ROOT})
