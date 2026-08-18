#pragma once

#include "HalStorage.h"

// Minimal stand-in for the real lib/Serialization/Serialization.h. Since the
// stub HalFile never actually opens, load()/save() never reach these calls
// with a live file -- they only need to exist so AchievementBus.cpp compiles.
namespace serialization {
template <typename T>
void readPod(HalFile&, T&) {}

template <typename T>
void writePod(HalFile&, const T&) {}
}  // namespace serialization
