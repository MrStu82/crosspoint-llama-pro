"""
PlatformIO pre-build script: add TinyUSB's real header include paths.

Enabling CONFIG_TINYUSB_ENABLED/CONFIG_TINYUSB_MSC_ENABLED (via build_flags,
see [env:x4pro]) pulls esp32-hal-tinyusb.c and USBMSC.h/USB.h into the build,
both of which #include "tusb.h" unconditionally. The precompiled
framework-arduinoespressif32-libs package ships these headers under
esp32s3/include/arduino_tinyusb/, and the vendored pioarduino-build.py
script that's supposed to add that path to CPPPATH never actually reaches
the real compile command for this project (confirmed via verbose build log
for both lib_deps sources and core framework sources) - so we add it
directly here via PlatformIO's own package-resolution API instead of
depending on that broken pipeline.
"""

Import("env")  # noqa: F821 (SCons-injected global)
import os

platform = env.PioPlatform()
libs_dir = platform.get_package_dir("framework-arduinoespressif32-libs")
mcu = env.get("BOARD_MCU", "esp32s3")

env.Append(
    CPPPATH=[
        os.path.join(libs_dir, mcu, "include", "arduino_tinyusb", "tinyusb", "src"),
        os.path.join(libs_dir, mcu, "include", "arduino_tinyusb", "include"),
    ]
)
