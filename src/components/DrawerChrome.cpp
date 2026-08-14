#include "components/DrawerChrome.h"

#include "GfxRenderer.h"

namespace DrawerChrome {

void clearRegion(const GfxRenderer& renderer, Rect region) {
  renderer.fillRect(region.x, region.y, region.width, region.height, false);
}

bool isOutsideTap(Edge edge, Rect region, int tx, int ty) {
  (void)tx;
  if (edge == Edge::Bottom) {
    return ty < region.y;
  }
  return ty >= region.y + region.height;
}

}  // namespace DrawerChrome
