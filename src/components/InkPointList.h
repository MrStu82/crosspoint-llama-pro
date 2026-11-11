#pragma once

#include <functional>
#include <string>

#include "InkPointShell.h"

class GfxRenderer;
class MappedInputManager;

// Shared InkPoint list menu (approved settings-menu.png, sha 8dfe0575): Caveat
// title with a clock/battery status line, circled Home, 88px icon + label +
// chevron/value rows with dotted dividers, a focus ring, and a paged
// "▲ n/m ▼" footer. The whole row is the touch target.
namespace InkPointList {

enum class Icon : int { None = 0, Display, Reader, Controls, System };

constexpr int kTop = InkPointShell::kContentTop;
constexpr int kRowHeight = 88;
constexpr int kPageItems = 7;
// A screen with a one-line note (status, warning, preview) gives up the last
// row slot; the note band then starts at noteTop(kNotePageItems).
constexpr int kNotePageItems = 6;

struct Rows {
  int count = 0;
  int selected = -1;  // -1: no focus ring
  int pageItems = kPageItems;
  std::function<std::string(int)> label;
  std::function<std::string(int)> value;     // non-empty replaces the chevron
  std::function<std::string(int)> subtitle;  // optional second line in small type
  std::function<Icon(int)> icon;             // optional; rows return Icon::None to omit
};

bool active(const GfxRenderer& renderer);
int pageItemsOr(const GfxRenderer& renderer, int fallback, int pageItems = kPageItems);
constexpr int noteTop(const int pageItems) { return kTop + pageItems * kRowHeight; }

// Header, Home button, rows and pager. The caller clears and displays.
void draw(const GfxRenderer& renderer, const char* title, const Rows& rows, const char* statusCentre = nullptr);

enum class Touch { None, Consumed, Activated };
// Home tap goes home (Consumed); pager taps move `selected` a page (Consumed);
// touch-down on a row focuses it (Consumed); a row tap sets it (Activated).
Touch touch(const MappedInputManager& input, int& selected, int count, int pageItems = kPageItems);

}  // namespace InkPointList
