#pragma once
#include <cstdint>
#include "EpdFontFamily.h"
#include "HalDisplay.h"
enum Color : uint8_t { Clear=0, White=1, Black=16 };
class GfxRenderer { public:
 int getScreenWidth() const{return 480;} int getScreenHeight() const{return 800;}
 int getTextWidth(int,const char*,EpdFontFamily::Style=EpdFontFamily::REGULAR) const{return 20;}
 int getLineHeight(int) const{return 16;}
 void drawText(int,int,int,const char*,bool=true,EpdFontFamily::Style=EpdFontFamily::REGULAR) const{}
 void drawRect(int,int,int,int,int=1,bool=true) const{} void fillRect(int,int,int,int,bool=true) const{}
 void fillRoundedRect(int,int,int,int,int,Color) const{} void displayWindow(int,int,int,int) const{}
 void displayBuffer(HalDisplay::RefreshMode=HalDisplay::FAST_REFRESH) const{}
 void promoteNextRefresh(HalDisplay::RefreshMode) const{}
};
