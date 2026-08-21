// Empty definition for Lyra3CoversTheme's single out-of-line override -- same
// key-function/linker rationale as BaseTheme.cpp. Never invoked by
// drawCorruptSaveNotice()'s tested path.
#include "components/themes/lyra/Lyra3CoversTheme.h"

void Lyra3CoversTheme::drawRecentBookCover(GfxRenderer&, Rect, const std::vector<RecentBook>&, const int, bool&,
                                            bool&, bool&, std::function<bool()>) const {}
