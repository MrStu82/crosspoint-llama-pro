// Empty definitions for LyraTheme's out-of-line overrides -- same key-function/linker
// rationale as BaseTheme.cpp. Never invoked by drawCorruptSaveNotice()'s tested path.
#include "components/themes/lyra/LyraTheme.h"

void LyraTheme::fillBatteryIcon(const GfxRenderer&, Rect, uint16_t) const {}
void LyraTheme::drawHeader(const GfxRenderer&, Rect, const char*, const char*) const {}
void LyraTheme::drawSubHeader(const GfxRenderer&, Rect, const char*, const char*) const {}
void LyraTheme::drawTabBar(const GfxRenderer&, Rect, const std::vector<TabInfo>&, bool) const {}
bool LyraTheme::tabIndexFromPoint(const GfxRenderer&, Rect, const std::vector<TabInfo>&, int, int, int&) const {
  return false;
}
int LyraTheme::getListRowStep(bool) const { return 0; }
int LyraTheme::getListPageItems(int, bool) const { return 0; }
void LyraTheme::drawList(const GfxRenderer&, Rect, int, int, const std::function<std::string(int)>&,
                          const std::function<std::string(int)>&, const std::function<UIIcon(int)>&,
                          const std::function<std::string(int)>&, bool, const std::function<bool(int)>&) const {}
void LyraTheme::drawButtonHints(GfxRenderer&, const char*, const char*, const char*, const char*) const {}
void LyraTheme::drawSideButtonHints(const GfxRenderer&, const char*, const char*) const {}
void LyraTheme::drawButtonMenu(GfxRenderer&, Rect, int, int, const std::function<std::string(int)>&,
                                const std::function<UIIcon(int)>&) const {}
void LyraTheme::drawRecentBookCover(GfxRenderer&, Rect, const std::vector<RecentBook>&, const int, bool&, bool&,
                                     bool&, std::function<bool()>) const {}
void LyraTheme::drawEmptyRecents(const GfxRenderer&, const Rect) const {}
