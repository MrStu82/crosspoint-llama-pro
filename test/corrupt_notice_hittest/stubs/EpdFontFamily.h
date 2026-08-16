#pragma once
// Shadow of lib/EpdFont/EpdFontFamily.h. Angle-bracket include (`#include <EpdFontFamily.h>`
// in UITheme.h and, transitively, this harness's own GfxRenderer.h stub) means this stub,
// placed at the top level of a -I stub directory, shadows the real header cleanly -- the
// real one pulls in EpdFont.h/Bitmap.h/HalDisplay.h, explicitly excluded by parent's msg
// 3946/3948 ("no display, no Bitmap"). Confirmed via grep across GameRenderer.cpp/.h,
// UITheme.cpp/.h that only the Style enum is ever referenced in this harness's tested code
// paths -- no EpdFontFamily instance is ever constructed -- so this enum-only stub is
// sufficient.
#include <cstdint>

class EpdFontFamily {
 public:
  enum Style : uint8_t {
    REGULAR = 0,
    BOLD = 1,
    ITALIC = 2,
    BOLD_ITALIC = 3,
  };
};
