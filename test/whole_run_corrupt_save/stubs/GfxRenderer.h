#pragma once
// Test stub. This is a LOGIC/control-flow test (whole-run corrupt-save
// notice -> resolveWholeRunCorruptNotice()), not a pixel-rendering test, so
// the real GfxRenderer (full EPD framebuffer/font/HAL pipeline) is
// deliberately not linked -- it would compile fine (see
// test/sponsor_hp_clamp/ precedent) but adds real render-pipeline link time
// for zero coverage benefit here, since GameRenderer itself is also stubbed
// below and never actually touches a renderer.
//
// setCustomLut() is the ONLY GfxRenderer method reachable from the tested
// code path: confirmed by grep across src/activities/Activity.cpp (the sole
// caller, via enterGameLutMode()/exitGameLutMode()) and
// src/activities/game/GameActivity.cpp (which never calls renderer.* itself,
// only gameRenderer.*, which is a separate stub with no real GfxRenderer
// dependency). If a future test extends this harness to touch more of
// GameActivity, re-grep before assuming this stays minimal.
class GfxRenderer {
 public:
  void setCustomLut(bool enabled, const unsigned char* lutData) {
    (void)enabled;
    (void)lutData;
  }
};
