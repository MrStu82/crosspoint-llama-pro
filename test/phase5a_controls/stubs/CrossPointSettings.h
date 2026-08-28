#pragma once
#include <cstdint>
struct CrossPointSettings { enum { TOUCH_READER_OFF=0, TOUCH_READER_ON=1 }; uint8_t frontlightBrightness=1, frontlightWarmPercent=0, frontlightOn=1, screenInverted=0, orientation=0, touchReaderControls=1; void saveToFile(){} }; extern CrossPointSettings SETTINGS;
