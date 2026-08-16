#pragma once
// Minimal host stand-in, same precedent as test/game_save/stubs/Arduino.h and
// test/corrupt_save_notice_render/stubs/Arduino.h. Shadows the real
// framework-arduinoespressif32 Arduino.h (via -I test/whole_run_corrupt_save/stubs
// coming first on the include path) so GameState.cpp's and
// resolveWholeRunCorruptNotice()'s real, unmodified millis()-based seed
// expressions compile unchanged on host.
#include <cstdint>

inline unsigned long g_fakeMillis = 0;
inline unsigned long millis() { return g_fakeMillis; }
inline unsigned long micros() { return g_fakeMillis * 1000UL; }
