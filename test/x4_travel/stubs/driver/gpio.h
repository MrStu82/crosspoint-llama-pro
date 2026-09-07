#pragma once
using gpio_num_t=int;
inline void gpio_hold_dis(int p){events.push_back("release"+std::to_string(p));}
inline void gpio_hold_en(int p){events.push_back("hold"+std::to_string(p));}
