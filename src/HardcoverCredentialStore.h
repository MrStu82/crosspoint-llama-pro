#pragma once

#include <ArduinoJson.h>
#include <PersistableStore.h>

#include <string>

// Device-bound, obfuscated-at-rest storage using the same established path as
// Wi-Fi/OPDS/KOReader credentials. The token is only exposed in memory to the
// Hardcover client and is never compiled, logged, or returned by the web API.
class HardcoverCredentialStore : public PersistableStore<HardcoverCredentialStore> {
 private:
  std::string token;
  HardcoverCredentialStore() = default;
  friend class PersistableStore<HardcoverCredentialStore>;

 public:
  enum class ImportResult { NOT_FOUND, INVALID, SAVE_FAILED, REMOVE_FAILED, IMPORTED };

  static const char* getFilePath() { return "/.crosspoint/hardcover.json"; }
  static const char* getImportPath() { return "/hardcover.token"; }
  void toJson(JsonDocument& doc) const;
  bool fromJson(JsonVariantConst doc);
  const std::string& getToken() const { return token; }
  bool hasToken() const { return !token.empty(); }
  void setToken(const std::string& value);
  bool saveAtomic();
  ImportResult importTokenFile();
  bool forget();
  static bool isValidToken(const std::string& value);
};

#define HARDCOVER_STORE HardcoverCredentialStore::getInstance()
