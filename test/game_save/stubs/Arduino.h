#pragma once
// Minimal host stand-in, same precedent as
// test/corrupt_save_notice_render/stubs/Arduino.h and
// build-scripts/halclock_test/stubs/Arduino.h. Shadows the real
// framework-arduinoespressif32 Arduino.h (via -I test/game_save/stubs coming
// first on the include path) so GameState.cpp's real, unmodified
// millis()-based seed expression compiles unchanged on host.
#include <cstdint>

inline unsigned long g_fakeMillis = 0;
inline unsigned long millis() { return g_fakeMillis; }
inline unsigned long micros() { return g_fakeMillis * 1000UL; }
