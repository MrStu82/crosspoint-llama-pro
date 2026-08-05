#pragma once

#include <optional>
#include <string>

// Home-screen "continue reading" progress badge (0-100, whole-book percent).
//
// Stored as a single byte in its own small cache file, separate from each reader's own
// progress.bin (which tracks resume state: spine/page/offset, not a whole-book percent).
// Computing whole-book percent needs the book briefly parsed anyway (spine sizes for EPUB,
// page count for XTC/TXT); each reader activity already does that work once per save, so it
// writes the percent it already computed here rather than the home screen re-parsing every
// recent book just to render a badge.
namespace BookProgressBadge {

// Writes `percent` (clamped 0-100) to `<cachePath>/book_progress.bin`. Called by a reader
// activity's saveProgress(), which already has the cache path and the computed percent.
void write(const std::string& cachePath, int percent);

// Reads the last-saved whole-book percent for the book at `path`, or nullopt if none is
// cached yet (book never opened, or its extension isn't a recognised reader type).
std::optional<int> read(const std::string& path);

}  // namespace BookProgressBadge
