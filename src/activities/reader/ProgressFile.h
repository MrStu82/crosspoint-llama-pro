#pragma once

#include <HalStorage.h>
#include "../../util/ReaderDiagnostics.h"
#include <Logging.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <string>

namespace ProgressFile {
// Serializes replacement/recovery across destinations. No caller may retain
// this lock while requesting a render. The storage HAL takes its own lock below.
inline std::mutex transactionMutex;

// A .bak is a formerly committed file, never an unverified temporary payload.
// Restore it only if the canonical name is absent. Do not promote stale .tmp.
inline bool recover(const std::string& path) {
  if (Storage.exists(path.c_str())) return true;
  const std::string backup = path + ".bak";
  return Storage.exists(backup.c_str()) && Storage.rename(backup.c_str(), path.c_str());
}

inline bool openForRead(const char* module, const std::string& path, HalFile& file) {
  std::lock_guard<std::mutex> lock(transactionMutex);
  if (!recover(path)) return false;
  return Storage.openFileForRead(module, path, file);
}

// FAT does not provide replace-rename. Keep the old committed file as .bak
// until the new canonical name exists. Readers recover .bak after interruption.
// Read back the closed temporary file before moving the old name. Physical
// media corruption/torn directory sectors still require filesystem recovery.
inline bool writeAtomic(const std::string& cachePath, const uint8_t* data, size_t len,
                        const std::string& filename = "progress.bin") {
  reader_diagnostics::Scope profile(reader_diagnostics::Stage::Persistence);
  std::lock_guard<std::mutex> lock(transactionMutex);
  const std::string finalPath = cachePath + "/" + filename;
  const std::string tmpPath = finalPath + ".tmp";
  const std::string backupPath = finalPath + ".bak";
  if (Storage.exists(backupPath.c_str()) && !recover(finalPath)) return false;
  {
    HalFile f;
    if (!Storage.openFileForWrite("PRG", tmpPath, f)) return false;
    if (f.write(data, len) != len) return false;
    f.flush();
    if (!f.close()) return false;
  }
  {
    HalFile f;
    if (!Storage.openFileForRead("PRG", tmpPath, f) || f.size() != len) return false;
    uint8_t check[64];
    for (size_t offset = 0; offset < len;) {
      const size_t n = std::min(sizeof(check), len - offset);
      if (f.read(check, n) != static_cast<int>(n) || std::memcmp(check, data + offset, n) != 0) return false;
      offset += n;
    }
    if (!f.close()) return false;
  }
  if (Storage.exists(backupPath.c_str()) && !Storage.remove(backupPath.c_str())) return false;
  if (Storage.exists(finalPath.c_str()) && !Storage.rename(finalPath.c_str(), backupPath.c_str())) return false;
  if (!Storage.rename(tmpPath.c_str(), finalPath.c_str())) {
    recover(finalPath);  // best effort; a later read retries recovery
    return false;
  }
  // A cleanup failure is harmless: canonical wins, backup is removed next write.
  Storage.remove(backupPath.c_str());
  return true;
}
}  // namespace ProgressFile
