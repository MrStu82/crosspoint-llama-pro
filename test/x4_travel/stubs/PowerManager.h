#pragma once
namespace freeink {struct PowerManager {static void armWakeOnPins(uint64_t m,bool low){events.push_back("wake"+std::to_string(m)+":"+std::to_string(low));}};}
