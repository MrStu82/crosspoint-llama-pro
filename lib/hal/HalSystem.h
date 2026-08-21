#pragma once

#include <cstdint>
#include <string>

namespace HalSystem {
struct StackFrame {
  uint32_t sp;
  uint32_t spp[8];
};

// Panic-time PC/SP pair retained across reboot for symbolisation against the
// exact firmware ELF. The original StackFrame above is a RISC-V stack-word
// dump; Xtensa needs an explicit call-chain capture.
struct TraceFrame {
  uint32_t pc;
  uint32_t sp;
};

void begin();

// Dump panic info to SD card if necessary
void checkPanic();
void clearPanic();

std::string getPanicInfo(bool full = false);
bool isRebootFromPanic();
}  // namespace HalSystem
