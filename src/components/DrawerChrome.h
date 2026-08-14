#pragma once

#include "components/themes/BaseTheme.h"

class GfxRenderer;

// Shared logic for drawers that overlay a live framebuffer (a reader page, the home
// screen, etc.) instead of going through a normal full-screen Activity transition.
// Extracted from BrightnessSheet (the bottom drawer) — the reference implementation
// for both halves of this — so every such drawer clears and dismisses the same way
// regardless of which screen edge it's pinned to.
namespace DrawerChrome {

enum class Edge { Top, Bottom };

// Fills the drawer's own rect with the background color before it draws its contents.
// A windowed displayWindow() push only replaces the pixels inside `region`; anything in
// that rect the drawer doesn't explicitly paint (gaps between rows, unfilled list
// background, etc.) otherwise keeps whatever was underneath — this is what causes page
// content to bleed through a drawer that clears less than its full push region.
void clearRegion(const GfxRenderer& renderer, Rect region);

// True if (tx, ty) landed outside the drawer's rect on the side that exposes whatever's
// underneath: below `region` for a Top-edge drawer, above `region` for a Bottom-edge one.
bool isOutsideTap(Edge edge, Rect region, int tx, int ty);

}  // namespace DrawerChrome
