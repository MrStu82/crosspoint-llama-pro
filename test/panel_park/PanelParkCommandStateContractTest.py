#!/usr/bin/env python3
"""Deterministic panel command/state contract for X4 Pro deep sleep."""

from pathlib import Path


root = Path(__file__).resolve().parents[2]
drivers = root / "freeink-sdk/libs/display/FreeInkDisplay/src/driver"


def function_body(source: str, signature: str) -> str:
    start = source.index(signature)
    brace = source.index("{", start)
    depth = 0
    for pos in range(brace, len(source)):
        if source[pos] == "{":
            depth += 1
        elif source[pos] == "}":
            depth -= 1
            if depth == 0:
                return source[brace + 1 : pos]
    raise AssertionError(f"unterminated function: {signature}")


ssd = (drivers / "Ssd1677Driver.cpp").read_text()
ssd_refresh = function_body(ssd, "void Ssd1677Driver::refresh(")
custom_start = ssd_refresh.index("else if (_customLutActive)")
custom_end = ssd_refresh.index("else {  // Fast", custom_start)
custom = ssd_refresh[custom_start:custom_end]

# An AA/custom-LUT 0xCC activation keeps analog/clock rails on. The software
# state may become OFF only inside the turnOff branch.
assert "displayMode = 0xCC;" in custom
assert "if (turnOff) {" in custom
assert custom.count("_isScreenOn = false;") == 1
assert custom.index("if (turnOff) {") < custom.index("_isScreenOn = false;")

ssd_sleep = function_body(ssd, "void Ssd1677Driver::deepSleep(")
ssd_order = (
    "bus.cmd(CMD_BORDER_WAVEFORM);",
    "bus.cmd(CMD_DISPLAY_UPDATE_CTRL2);",
    "bus.data(0x03);",
    "bus.cmd(CMD_MASTER_ACTIVATION);",
    "bus.waitBusy(\" display power-down\");",
    "_isScreenOn = false;",
    "bus.cmd(CMD_DEEP_SLEEP);",
)
positions = [ssd_sleep.index(token) for token in ssd_order]
assert positions == sorted(positions)
assert "if (_isScreenOn)" not in ssd_sleep

# All probe-selectable UC controllers must issue POF before DSLP even when the
# software state already says OFF. Waiting is conditional, and state is made
# truthful before the deep-sleep command.
uc_drivers = {
    "Uc8179Driver.cpp": "Uc8179Driver",
    "Uc8279Driver.cpp": "Uc8279Driver",
    "Uc8279X4Driver.cpp": "Uc8279X4Driver",
}
for filename, class_name in uc_drivers.items():
    source = (drivers / filename).read_text()
    body = function_body(source, f"void {class_name}::deepSleep(")
    order = (
        "bus.cmd(CMD_POWER_OFF);",
        "if (_isScreenOn) bus.waitBusy(",
        "_isScreenOn = false;",
        "bus.cmd(CMD_DEEP_SLEEP);",
        "bus.data(0xA5);",
    )
    positions = [body.index(token) for token in order]
    assert positions == sorted(positions), filename
    assert body.count("bus.cmd(CMD_POWER_OFF);") == 1, filename

# EpdBus bounds every busy-polarity branch at 30 seconds. Conditional UC waits
# therefore cannot hang forever, while already-off UC panels skip the wait.
epd_bus = (drivers.parent / "bus/EpdBus.cpp").read_text()
wait_body = function_body(epd_bus, "void EpdBus::waitBusy(BusyPolarity p")
assert wait_body.count("millis() - start > 30000") == 3

print("PASS panel POF/booster-off ordering, AA state truth, bounded-wait contract")
