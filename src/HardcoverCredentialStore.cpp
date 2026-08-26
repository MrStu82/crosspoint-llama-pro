#include "HardcoverCredentialStore.h"

#include <ObfuscationUtils.h>

#include <cctype>

#include "activities/reader/ProgressFile.h"
#include "util/AtomicCredentialUpdate.h"

namespace {
constexpr size_t kMaxTokenBytes = 512;
constexpr size_t kMinTokenBytes = 16;

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

bool HardcoverCredentialStore::replaceTokenAtomic(const std::string& value) {
  if (!value.empty() && !isValidToken(value)) return false;
  return AtomicCredentialUpdate::replace(token, value, [this] { return saveAtomic(); });
}

bool HardcoverCredentialStore::isValidToken(const std::string& value) {
  if (value.size() < kMinTokenBytes || value.size() > kMaxTokenBytes || value.rfind("hc_pat_", 0) != 0) return false;
  for (const unsigned char c : value) {
    if (!(std::isalnum(c) || c == '_' || c == '-' || c == '.')) return false;
  }
  return true;
}

bool HardcoverCredentialStore::saveAtomic() {
  JsonDocument doc;
  toJson(doc);
  std::string json;
  serializeJson(doc, json);
  return ProgressFile::writeAtomic("/.crosspoint", reinterpret_cast<const uint8_t*>(json.data()), json.size(),
                                   "hardcover.json");
}

bool HardcoverCredentialStore::forget() {
  return replaceTokenAtomic("");
}
