#include "HardcoverCredentialStore.h"

#include <HalStorage.h>
#include <ObfuscationUtils.h>

#include <cctype>

#include "activities/reader/ProgressFile.h"

namespace {
constexpr size_t kMaxTokenBytes = 512;
constexpr size_t kMinTokenBytes = 16;

std::string trimmed(std::string value) {
  while (!value.empty() && (value.back() == '\n' || value.back() == '\r' || value.back() == ' ' || value.back() == '\t'))
    value.pop_back();
  size_t start = 0;
  while (start < value.size() && (value[start] == '\n' || value[start] == '\r' || value[start] == ' ' || value[start] == '\t'))
    ++start;
  return value.substr(start);
}
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

HardcoverCredentialStore::ImportResult HardcoverCredentialStore::importTokenFile() {
  HalFile file;
  if (!Storage.openFileForRead("HCT", getImportPath(), file)) return ImportResult::NOT_FOUND;
  const size_t size = file.size();
  if (size == 0 || size > kMaxTokenBytes + 8) return ImportResult::INVALID;
  std::string candidate(size, '\0');
  if (file.read(candidate.data(), size) != static_cast<int>(size)) return ImportResult::INVALID;
  file.close();
  candidate = trimmed(std::move(candidate));
  if (!isValidToken(candidate)) return ImportResult::INVALID;

  const std::string previous = token;
  token = std::move(candidate);
  if (!saveAtomic()) {
    token = previous;
    return ImportResult::SAVE_FAILED;
  }
  if (!Storage.remove(getImportPath())) return ImportResult::REMOVE_FAILED;
  return ImportResult::IMPORTED;
}

bool HardcoverCredentialStore::forget() {
  const std::string previous = token;
  token.clear();
  if (saveAtomic()) return true;
  token = previous;
  return false;
}
