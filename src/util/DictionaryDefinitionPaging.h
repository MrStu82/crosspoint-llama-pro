#pragma once

#include <cstddef>

enum class DictionaryPageCommand { Close, Previous, Next };

struct DictionaryPagePosition {
  int current;
  int total;
};

struct DictionaryPageTransition {
  int page;
  bool close;
  bool changed;
};

constexpr DictionaryPagePosition initialDictionaryPage(size_t pageCount) {
  return {0, pageCount == 0 ? 1 : static_cast<int>(pageCount)};
}

constexpr DictionaryPageCommand dictionaryTapCommand(int tapX, int screenWidth) {
  return tapX < screenWidth / 3 ? DictionaryPageCommand::Previous : DictionaryPageCommand::Next;
}

constexpr DictionaryPageTransition transitionDictionaryPage(int current, int total, DictionaryPageCommand command) {
  if (command == DictionaryPageCommand::Close) return {current, true, false};
  if (command == DictionaryPageCommand::Previous && current > 0) return {current - 1, false, true};
  if (command == DictionaryPageCommand::Next && current + 1 < total) return {current + 1, false, true};
  return {current, false, false};
}
