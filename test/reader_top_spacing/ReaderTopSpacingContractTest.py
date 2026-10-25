#!/usr/bin/env python3
"""Exact source geometry contract for the reader's top status bar prototype."""

from pathlib import Path


root = Path(__file__).resolve().parents[2]
theme = (root / "src/components/themes/BaseTheme.cpp").read_text()

assert "constexpr int readerTopProgressBarY = 0;" in theme
assert "constexpr int readerTopStatusGap = 1;" in theme

# With a visible top progress bar, its fill occupies y=[0, height-1] and the
# status text layout begins at y=height+1: one blank row, no collision. The
# effective height includes the existing fillMargin extension used by readers.
assert "const int progressBarHeight = sb.progressBarHeightPx + (fillMargin ? orientedMarginBottom - 1 : 0);" in theme
assert "readerTopProgressBarY + progressBarHeight + readerTopStatusGap" in theme
assert "renderer.fillRect(barMarginLeft, progressBarY, barWidth, progressBarHeight, true);" in theme
assert "const int progressBarY = isTopEdge ? topInset + readerTopProgressBarY" in theme

# Top bezel offset: every top-edge y (bar and text lane) shifts by topInset, which
# readers pass as readerTopBezelOffset() (0 by default and outside portrait).
assert "? topInset + readerTopProgressBarY + progressBarHeight + readerTopStatusGap" in theme
assert ": topInset + orientedMarginTop + paddingBottom" in theme
settings = (root / "src/CrossPointSettings.h").read_text()
assert "uint8_t topBezelOffset = 0;" in settings
assert "return orientation == PORTRAIT ? topBezelOffset : 0;" in settings
epub = (root / "src/activities/reader/EpubReaderActivity.cpp").read_text()
txt = (root / "src/activities/reader/TxtReaderActivity.cpp").read_text()
assert epub.count("SETTINGS.readerTopBezelOffset()") == 3  # text top, dictionary geometry, top bar
assert "orientedMarginTop += SETTINGS.readerTopBezelOffset();" in epub
assert "Edge::TOP, SETTINGS.readerTopBezelOffset());" in epub
assert txt.count("SETTINGS.readerTopBezelOffset()") == 2  # text top, top bar
assert "cachedOrientedMarginTop += cachedScreenMargin + SETTINGS.readerTopBezelOffset();" in txt
assert "Edge::TOP, SETTINGS.readerTopBezelOffset());" in txt

# The change is top-edge-only; bottom placement and total viewport reservation
# stay byte-for-byte governed by the existing expressions.
assert "renderer.getScreenHeight() - orientedMarginBottom - sb.progressBarHeightPx" in theme
ui_theme = (root / "src/components/UITheme.cpp").read_text()
assert "sb.progressBarHeightPx + metrics.progressBarMarginTop" in ui_theme

# Addendum #9840: bottom bar y and bottom margin are identical at every offset.
# Evaluate drawStatusBar's own y expressions for the BOTTOM and TOP edges at offset 0 vs 12.
import re
body = theme[theme.index("const CrossPointSettings::Edge edge, const int topInset) const {"):]
body = body[: body.index("\n}\n")]
text_expr = re.search(r"auto textY = (.*?);", body, re.S).group(1)
bar_expr = re.search(r"const int progressBarY = (.*?);", body, re.S).group(1)
# Compile the extracted expressions verbatim with the host C++ compiler (exact C semantics).
import subprocess, tempfile
def expr(e):
    return (re.sub(r"\s+", " ", e).replace("UITheme::getInstance().getStatusBarHeight(edge)", "statusBarHeight")
            .replace("renderer.getScreenHeight()", "screenHeight"))
prog = """#include <cstdio>
struct M { int statusBarVerticalMargin = 5; } metrics;
struct S { int progressBarHeightPx = 4; bool shows; bool showsProgressBar() const { return shows; } };
int main() {
  const int screenHeight = 800, statusBarHeight = 22, orientedMarginTop = 9, orientedMarginBottom = 3, paddingBottom = 4;
  const int readerTopProgressBarY = 0, readerTopStatusGap = 1;
  for (int shows = 0; shows < 2; ++shows) for (int fillMargin = 0; fillMargin < 2; ++fillMargin) {
    S sb{4, shows != 0};
    const int progressBarHeight = sb.progressBarHeightPx + (fillMargin ? orientedMarginBottom - 1 : 0);
    int y[2][2][2];
    for (int isTopEdge = 0; isTopEdge < 2; ++isTopEdge) for (int k = 0; k < 2; ++k) {
      const int topInset = k ? 12 : 0;
      y[isTopEdge][k][0] = TEXT; y[isTopEdge][k][1] = BAR;
    }
    for (int i = 0; i < 2; ++i) {
      if (y[0][0][i] != y[0][1][i]) { printf("bottom moved %d %d\\n", y[0][0][i], y[0][1][i]); return 1; }
      if (y[1][1][i] != y[1][0][i] + 12) { printf("top not +12\\n"); return 1; }
    }
  }
  return 0;
}
""".replace("TEXT", expr(text_expr)).replace("BAR", expr(bar_expr))
with tempfile.TemporaryDirectory() as d:
    open(d + "/g.cpp", "w").write(prog)
    subprocess.run(["c++", "-std=c++17", "-Wall", "-Werror", "-Wno-unused-variable", "-o", d + "/g", d + "/g.cpp"], check=True)
    subprocess.run([d + "/g"], check=True)
# Bottom calls never pass the offset; bottom margins never read it.
assert "Edge::BOTTOM);" in epub and "Edge::BOTTOM);" in txt
for src in (epub, txt):
    for line in src.splitlines():
        if "MarginBottom" in line:
            assert "readerTopBezelOffset" not in line, line
print("PASS bottom edge invariant: bottom bar y/text y identical at offset 0 vs 12; top +12; bottom margins offset-free")

print("PASS reader top geometry: bar y=bezel, text y=bezel+barHeight+1, text top +bezel, bottom/reservation unchanged")
