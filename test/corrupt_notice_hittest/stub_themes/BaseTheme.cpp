// Empty definitions for BaseTheme's out-of-line methods, satisfying the linker (Itanium ABI
// key-function rule: constructing a BaseTheme/derived object needs its vtable, anchored by
// the first non-inline virtual method's definition) WITHOUT pulling in the real BaseTheme.cpp
// and its heavy transitive deps (GfxRenderer draw calls, HalClock, HalPowerManager,
// RecentBooksStore, I18n, icon bitmaps). None of these bodies are ever invoked by
// drawCorruptSaveNotice() -- UITheme's real, unmodified constructor only needs the type to be
// constructible and its vtable to link.
#include "components/themes/BaseTheme.h"

void BaseTheme::drawProgressBar(const GfxRenderer&, Rect, size_t, size_t) const {}
void BaseTheme::drawBatteryLeft(const GfxRenderer&, Rect, bool) const {}
void BaseTheme::drawBatteryRight(const GfxRenderer&, Rect, bool) const {}
void BaseTheme::fillBatteryIcon(const GfxRenderer&, Rect, uint16_t) const {}
void BaseTheme::drawButtonHints(GfxRenderer&, const char*, const char*, const char*, const char*) const {}
void BaseTheme::drawSideButtonHints(const GfxRenderer&, const char*, const char*) const {}
int BaseTheme::getListRowStep(bool) const { return 0; }
int BaseTheme::getListPageItems(int, bool) const { return 0; }
void BaseTheme::drawList(const GfxRenderer&, Rect, int, int, const std::function<std::string(int)>&,
                          const std::function<std::string(int)>&, const std::function<UIIcon(int)>&,
                          const std::function<std::string(int)>&, bool, const std::function<bool(int)>&) const {}
void BaseTheme::drawHeader(const GfxRenderer&, Rect, const char*, const char*) const {}
void BaseTheme::drawSubHeader(const GfxRenderer&, Rect, const char*, const char*) const {}
void BaseTheme::drawTabBar(const GfxRenderer&, Rect, const std::vector<TabInfo>&, bool) const {}
bool BaseTheme::tabIndexFromPoint(const GfxRenderer&, Rect, const std::vector<TabInfo>&, int, int, int&) const {
  return false;
}
void BaseTheme::drawRecentBookCover(GfxRenderer&, Rect, const std::vector<RecentBook>&, const int, bool&, bool&,
                                     bool&, std::function<bool()>) const {}
void BaseTheme::drawButtonMenu(GfxRenderer&, Rect, int, int, const std::function<std::string(int)>&,
                                const std::function<UIIcon(int)>&) const {}
Rect BaseTheme::drawPopup(const GfxRenderer&, const char*) const { return Rect(); }
void BaseTheme::drawOptionPopup(const GfxRenderer&, const char*, const std::vector<std::string>&, int) const {}
void BaseTheme::fillPopupProgress(const GfxRenderer&, const Rect&, const int) const {}
void BaseTheme::drawStatusBar(GfxRenderer&, const float, const int, const int, std::string, const int, const int,
                               const bool, const bool, const bool, const CrossPointSettings::Edge) const {}
void BaseTheme::drawHelpText(const GfxRenderer&, Rect, const char*) const {}
void BaseTheme::drawTextField(const GfxRenderer&, Rect, const int, bool, int, int) const {}
void BaseTheme::drawBatteryOutline(const GfxRenderer&, int, int, int, int) {}
void BaseTheme::drawBatteryLightningBolt(const GfxRenderer&, int, int) {}
