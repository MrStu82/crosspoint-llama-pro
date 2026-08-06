#pragma once

// App-layer frontlight singleton. Wraps the freeink-sdk FrontlightManager (which is
// already a complete, self-guarded abstraction — present()/hasColorTemperature() make it
// safe to construct and call on any board, frontlight or not). No further Hal wrapper is
// needed on top of it; this header just gives the rest of the app an extern to reach it,
// mirroring how activityManager/mappedInputManager are declared (defined in main.cpp,
// extern'd from an app-layer header) rather than editing the vendored SDK.
//
// Every write to the frontlight MUST go through this instance's setBrightness()/
// setColorTemperature() — never touch LEDC/GPIO directly. If the GPIO8/GPIO9 mapping
// turns out to be wrong, the fix is one BoardConfig profile entry, not new call sites.

#include <FrontlightManager.h>

extern FrontlightManager frontlightManager;
