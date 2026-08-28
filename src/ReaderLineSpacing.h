#pragma once

#include <I18n.h>

#include <array>

namespace ReaderLineSpacing {

inline constexpr std::array<StrId, 4> LABEL_IDS = {
    StrId::STR_TIGHT,
    StrId::STR_NORMAL,
    StrId::STR_WIDE,
    StrId::STR_EXTRA_WIDE,
};

}  // namespace ReaderLineSpacing
