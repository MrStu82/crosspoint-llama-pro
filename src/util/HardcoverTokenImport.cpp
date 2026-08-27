#include "HardcoverTokenImport.h"

#include <HalStorage.h>
#include <Logging.h>

#include "HardcoverCredentialStore.h"

namespace {
constexpr size_t kMaxFileBytes = 1024;
constexpr const char* kTag = "HCIMP";
}  // namespace

namespace HardcoverTokenImport {

Status run() {
  HalFile file;
  if (!Storage.openFileForRead(kTag, kSourcePath, file)) return Status::NoFile;

  // A token is at most 512 bytes; anything longer is not one, and reading the
  // whole file would only spend heap to reach the same answer.
  char buffer[kMaxFileBytes + 1] = {};
  const int read = file.read(buffer, kMaxFileBytes);
  file.close();
  if (read <= 0) {
    LOG_ERR(kTag, "token file present but unreadable");
    return Status::Unreadable;
  }

  const std::string token = sanitise(std::string(buffer, static_cast<size_t>(read)));
  if (!HardcoverCredentialStore::isValidToken(token)) {
    LOG_ERR(kTag, "rejected token file: failed validation");
    return Status::Invalid;
  }

  if (!HARDCOVER_STORE.replaceTokenAtomic(token)) {
    LOG_ERR(kTag, "replaceTokenAtomic failed");
    return Status::StoreFailed;
  }

  if (!Storage.remove(kSourcePath)) {
    LOG_ERR(kTag, "token stored but plaintext file could not be removed");
    return Status::StoredNotWiped;
  }

  LOG_INF(kTag, "token imported and plaintext removed");
  return Status::Accepted;
}

}  // namespace HardcoverTokenImport
