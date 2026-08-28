#!/usr/bin/env python3
"""Pin the clean simulator build seam that incremental objects previously hid."""

import sys
from pathlib import Path


def main() -> None:
    root = Path(sys.argv[1]).resolve()
    platformio = (root / "platformio.ini").read_text()
    simulator = platformio.split("[env:simulator_x4_pro]", 1)[1]
    assert "\n\t-Ilib/hal\n" in simulator, "simulator cannot discover project HAL policy headers"

    main_cpp = (root / "src/main.cpp").read_text()
    assert "gpio_policy::verifyPowerButtonWakeup(gpio)" in main_cpp
    assert "!gpio.verifyPowerButtonWakeup()" not in main_cpp

    gate = (root / "scripts/build_clean_simulator.py").read_text()
    clean = '"run", "-e", environment, "-t", "clean"'
    build = '"run", "-e", environment]'
    assert clean in gate and build in gate
    assert gate.index(clean) < gate.index(build), "clean must precede compilation"
    assert 'rglob("*.o")' in gate, "gate must reject objects surviving clean"
    assert 'program.exe' in gate and 'program"' in gate, "gate must require a linked simulator"

    print("PASS simulator clean-build contract")


if __name__ == "__main__":
    main()
