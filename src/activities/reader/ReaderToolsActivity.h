#pragma once

#include <array>
#include <cstdint>

#include <I18n.h>

#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

class ReaderToolsActivity final : public Activity {
 public:
  enum class Format : uint8_t { Epub, Txt, Xtc };
  enum class Action : uint8_t { GoToPercent, AddBookmark, Bookmarks, Dictionary, KOReaderSync, TextSettings };

  ReaderToolsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, Format format);

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool handleHomeGesture() override;

 private:
  struct Item {
    Action action;
    StrId label;
  };

  void closeCancelled();
  void activate(int index);

  std::array<Item, 6> items{};
  int itemCount = 0;
  int selectedIndex = -1;
  ButtonNavigator buttonNavigator;
};
