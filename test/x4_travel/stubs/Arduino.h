#pragma once
#include <cstdint>
#include <initializer_list>
#include <vector>
#include <string>
extern std::vector<std::string> events;
extern uint32_t nowMs, releaseAt; extern bool stuck;
#define OUTPUT 1
#define INPUT_PULLUP 2
#define LOW 0
inline uint32_t millis(){return nowMs;}
inline void delay(int n){nowMs+=n;}
inline int digitalRead(int){return stuck || nowMs<releaseAt ? LOW:1;}
inline void pinMode(int p,int m){events.push_back("mode"+std::to_string(p)+":"+std::to_string(m));}
inline void digitalWrite(int p,int v){events.push_back("write"+std::to_string(p)+":"+std::to_string(v));}
inline void ledcDetach(int p){events.push_back("detach"+std::to_string(p));}
inline void ledcDetachPin(int p){ledcDetach(p);}
