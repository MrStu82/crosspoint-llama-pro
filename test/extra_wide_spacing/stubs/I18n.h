#pragma once

#include "I18nKeys.h"

#include <cstring>

class I18n {
 public:
  static Language languageFromCode(const char* code) {
    (void)code;
    return Language::EN;
  }
};
