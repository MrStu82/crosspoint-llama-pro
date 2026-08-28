#pragma once

namespace TableTestLogging {
template <typename... Args>
void discard(Args&&...) {}
}  // namespace TableTestLogging

#define LOG_ERR(...) TableTestLogging::discard(__VA_ARGS__)
#define LOG_INF(...) TableTestLogging::discard(__VA_ARGS__)
#define LOG_DBG(...) TableTestLogging::discard(__VA_ARGS__)
