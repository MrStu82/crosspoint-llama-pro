#pragma once

// No-op stand-in for the real lib/Logging/Logging.h, which pulls in
// HardwareSerial. AchievementBus.cpp only calls these as fire-and-forget
// diagnostics; the test doesn't need them to go anywhere.
#define LOG_ERR(tag, fmt, ...) ((void)0)
#define LOG_INF(tag, fmt, ...) ((void)0)
#define LOG_DBG(tag, fmt, ...) ((void)0)
