// Pure host geometry proof for the two corrupt-save option rows.
// Compiles the real GameRenderer.cpp and calls the same public hit-test used
// by GameActivity::loop(); the private draw rect stays inaccessible.
#include "game/GameRenderer.h"
#include <cstdio>
#include <cstdint>

namespace { int failures = 0; }
#define CHECK(cond, ...) do { if (!(cond)) { fprintf(stderr, "FAIL: " __VA_ARGS__); fprintf(stderr, "\n"); failures++; } } while (0)

struct Bounds { int minX=INT32_MAX,maxX=INT32_MIN,minY=INT32_MAX,maxY=INT32_MIN,hits=0; };

int main() {
  GameRenderer renderer;
  renderer.initForTest(480, 800);
  Bounds found[2];
  int outsideHits = 0;
  for (int y=0; y<renderer.screenH; y++) {
    for (int x=0; x<renderer.screenW; x++) {
      const int option = renderer.hitTestCorruptSaveNoticeOption(x,y);
      if (option < 0) continue;
      if (option > 1) { outsideHits++; continue; }
      auto& b=found[option]; b.hits++; if(x<b.minX)b.minX=x; if(x>b.maxX)b.maxX=x; if(y<b.minY)b.minY=y; if(y>b.maxY)b.maxY=y;
    }
  }
  CHECK(outsideHits==0,"hit-test returned an option outside 0/1");
  for (int option=0; option<2; option++) {
    const auto& b=found[option];
    CHECK(b.hits>0,"option %d has no hit region",option);
    if (!b.hits) continue;
    const int cx=(b.minX+b.maxX)/2, cy=(b.minY+b.maxY)/2;
    CHECK(renderer.hitTestCorruptSaveNoticeOption(cx,cy)==option,"option %d center (%d,%d) missed",option,cx,cy);
    CHECK((b.maxX-b.minX+1)==340,"option %d width drifted: %d",option,b.maxX-b.minX+1);
    CHECK((b.maxY-b.minY+1)==34,"option %d height drifted: %d",option,b.maxY-b.minY+1);
  }
  CHECK(found[1].minY-found[0].maxY-1==8,"expected 8px gap between rows, got %d",found[1].minY-found[0].maxY-1);
  CHECK(renderer.hitTestCorruptSaveNoticeOption(-1,-1)==-1,"off-screen negative point hit");
  CHECK(renderer.hitTestCorruptSaveNoticeOption(481,801)==-1,"off-screen positive point hit");
  CHECK(renderer.hitTestCorruptSaveNoticeOption((found[0].minX+found[0].maxX)/2,found[0].maxY+4)==-1,"gap between options hit");
  if (failures) { printf("FAIL: %d assertion(s)\n",failures); return 1; }
  printf("PASS: Purge=[%d..%d,%d..%d], Leave=[%d..%d,%d..%d], exact 340x34 rows + 8px gap\n",
    found[0].minX,found[0].maxX,found[0].minY,found[0].maxY,
    found[1].minX,found[1].maxX,found[1].minY,found[1].maxY);
  return 0;
}
