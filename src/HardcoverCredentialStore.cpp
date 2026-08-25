#include "HardcoverCredentialStore.h"

#include <ObfuscationUtils.h>

namespace {
constexpr size_t kMaxTokenBytes = 512;
}

void HardcoverCredentialStore::toJson(JsonDocument& doc) const {
  doc["token_obf"] = obfuscation::obfuscateToBase64(token);
  doc["token_len"] = token.size();
}

bool HardcoverCredentialStore::fromJson(JsonVariantConst doc) {
  bool ok = false;
  bool tooLong = false;
  const std::string decoded = obfuscation::deobfuscateFromBase64(doc["token_obf"] | "", kMaxTokenBytes, &ok, &tooLong);
  const size_t expected = doc["token_len"] | 0U;
  if (tooLong || !ok || decoded.size() != expected) {
    token.clear();
    return expected == 0;
  }
  token = decoded;
  return true;
}

void HardcoverCredentialStore::setToken(const std::string& value) {
  token = value.size() <= kMaxTokenBytes ? value : std::string{};
}
