#pragma once

#include <optional>

class GfxRenderer;
class MappedInputManager;

namespace InkPointShell {

enum class Destination : int { Home = 0, Library, Files, Games, Transfer, Settings };

constexpr int kStatusBottom = 18;
// The Caveat heading's ink reaches y=94 on panel once descenders are counted,
// so the previous 88px content top sat inside the title. Every InkPoint screen
// derives its content top from this one constant, which makes the breathing
// room below the page title global rather than per-screen.
constexpr int kHeaderBottom = 94;
constexpr int kContentTop = 114;
constexpr int kFooterTop = 728;
constexpr int kFooterHeight = 60;

bool enabled(const GfxRenderer& renderer);
void drawHeader(const GfxRenderer& renderer, const char* title);
void drawFooter(const GfxRenderer& renderer, Destination active, int focus = -1);
std::optional<Destination> tappedDestination(const MappedInputManager& input);
void navigate(Destination destination);

}  // namespace InkPointShell
