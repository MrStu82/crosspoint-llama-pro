#pragma once

enum class StrId { STR_ENTERING_SLEEP, STR_CROSSPOINT, STR_SLEEPING };
inline const char* trImpl(StrId) { return "fixture"; }
#define tr(id) trImpl(StrId::id)
