#pragma once
#include <cstdint>
struct FrontlightStub { uint8_t brightness()const{return 1;} void setBrightness(uint8_t){} void setColorTemperature(uint8_t){} void off(){} }; extern FrontlightStub frontlightManager;
