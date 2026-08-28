#pragma once

namespace TestLogging {
template <typename... Args>
void discard(Args&&...) {}
}  // namespace TestLogging

#define LOG_ERR(...) TestLogging::discard(__VA_ARGS__)
#define LOG_INF(...) TestLogging::discard(__VA_ARGS__)
#define LOG_DBG(...) TestLogging::discard(__VA_ARGS__)
