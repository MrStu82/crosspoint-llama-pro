#pragma once

#include <optional>

class GfxRenderer;
class MappedInputManager;

namespace InkPointShell {

enum class Destination : int { Home = 0, Library, Files, Games, Transfer, Settings };

constexpr int kStatusBottom = 18;
constexpr int kHeaderBottom = 82;
constexpr int kContentTop = 88;
constexpr int kFooterTop = 728;
constexpr int kFooterHeight = 60;

bool enabled(const GfxRenderer& renderer);
void drawHeader(const GfxRenderer& renderer, const char* title);
void drawFooter(const GfxRenderer& renderer, Destination active, int focus = -1);
std::optional<Destination> tappedDestination(const MappedInputManager& input);
void navigate(Destination destination);

}  // namespace InkPointShell
