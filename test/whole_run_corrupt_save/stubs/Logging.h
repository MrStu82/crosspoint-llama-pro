#pragma once
// Test stub -- mirrors test/game_save/stubs/Logging.h (same no-op-to-stdio pattern).
#include <cstdio>

#define LOG_ERR(tag, fmt, ...) fprintf(stderr, "[ERR][%s] " fmt "\n", tag, ##__VA_ARGS__)
#define LOG_INF(tag, fmt, ...) fprintf(stdout, "[INF][%s] " fmt "\n", tag, ##__VA_ARGS__)
#define LOG_DBG(tag, fmt, ...) fprintf(stdout, "[DBG][%s] " fmt "\n", tag, ##__VA_ARGS__)
