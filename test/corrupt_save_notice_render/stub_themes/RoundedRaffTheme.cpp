// Empty definitions for RoundedRaffTheme's out-of-line overrides -- same key-function/linker
// rationale as BaseTheme.cpp. Never invoked by drawCorruptSaveNotice()'s tested path.
#include "components/themes/roundedraff/RoundedRaffTheme.h"

void RoundedRaffTheme::drawHeader(const GfxRenderer&, Rect, const char*, const char*) const {}
void RoundedRaffTheme::drawTabBar(const GfxRenderer&, Rect, const std::vector<TabInfo>&, bool) const {}
bool RoundedRaffTheme::tabIndexFromPoint(const GfxRenderer&, Rect, const std::vector<TabInfo>&, int, int, int&) const {
  return false;
}
void RoundedRaffTheme::drawRecentBookCover(GfxRenderer&, Rect, const std::vector<RecentBook>&, int, bool&, bool&,
                                            bool&, std::function<bool()>) const {}
void RoundedRaffTheme::drawButtonMenu(GfxRenderer&, Rect, int, int, const std::function<std::string(int)>&,
                                       const std::function<UIIcon(int)>&) const {}
void RoundedRaffTheme::drawTextField(const GfxRenderer&, Rect, int, bool, int, int) const {}
int RoundedRaffTheme::getListRowStep(bool) const { return 0; }
int RoundedRaffTheme::getListPageItems(int, bool) const { return 0; }
void RoundedRaffTheme::drawList(const GfxRenderer&, Rect, int, int, const std::function<std::string(int)>&,
                                 const std::function<std::string(int)>&, const std::function<UIIcon(int)>&,
                                 const std::function<std::string(int)>&, bool,
                                 const std::function<bool(int)>&) const {}
void RoundedRaffTheme::drawButtonHints(GfxRenderer&, const char*, const char*, const char*, const char*) const {}
