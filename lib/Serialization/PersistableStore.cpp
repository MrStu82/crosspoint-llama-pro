#include "PersistableStore.h"
#include "../../src/activities/reader/ProgressFile.h"

#include <HalStorage.h>
#include <Logging.h>
#include <ObfuscationUtils.h>

#include <cstring>
#include <limits>

bool PersistableStoreBase::writeDocToFile(const char* path, const JsonDocument& doc) {
  Storage.mkdir("/.crosspoint");
  std::string json;
  serializeJson(doc, json);
  const std::string fullPath(path);
  const auto slash = fullPath.rfind('/');
  if (slash == std::string::npos || slash + 1 == fullPath.size()) return false;
  return ProgressFile::writeAtomic(fullPath.substr(0, slash),
      reinterpret_cast<const uint8_t*>(json.data()), json.size(), fullPath.substr(slash + 1));
}

bool PersistableStoreBase::readDocFromFile(const char* path, JsonDocument& doc) {
  HalFile file;
  if (!ProgressFile::openForRead("PERSIST", path, file)) return false;
  std::string json(file.size(), '\0');
  if (json.empty() || file.read(reinterpret_cast<uint8_t*>(json.data()), json.size()) != static_cast<int>(json.size()))
    return false;
  file.close();
  auto error = deserializeJson(doc, json);
  if (error) {
    LOG_ERR("PERSIST", "JSON parse error in %s: %s", path, error.c_str());
    return false;
  }
  return true;
}

std::string PersistableStoreBase::extractPassword(JsonVariantConst doc, bool& needsResave) {
  bool valid = false;
  return extractPassword(doc, needsResave, std::numeric_limits<size_t>::max(), valid);
}

std::string PersistableStoreBase::extractPassword(JsonVariantConst doc, bool& needsResave, const size_t maxLength,
                                                  bool& valid) {
  valid = true;
  bool ok = false;
  bool tooLong = false;
  std::string pass = obfuscation::deobfuscateFromBase64(doc["password_obf"] | "", maxLength, &ok, &tooLong);
  if (tooLong) {
    valid = false;
    return "";
  }
  if (!ok) {
    // Deobfuscation failed — fall back to legacy plaintext password.
    const char* legacyPassword = doc["password"] | "";
    const size_t legacyLength = strlen(legacyPassword);
    if (legacyLength > maxLength) {
      valid = false;
      return "";
    }
    pass.assign(legacyPassword, legacyLength);
    if (!pass.empty()) needsResave = true;
  }
  // A successfully decoded empty string is a legitimate value; preserve as-is.
  return pass;
}
