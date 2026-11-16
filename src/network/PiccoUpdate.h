#pragma once

#include <cstddef>
#include <string>

// Picco wireless update: the app only stages /update/firmware.bin (uploaded over the
// Wi-Fi file transfer page, or pulled from MANIFEST_URL) and hands it to the stock
// safeboot installer. The app never writes the app slot itself.
namespace picco_update {

constexpr const char* FIRMWARE_PATH = "/update/firmware.bin";
constexpr const char* PART_PATH = "/update/firmware.bin.part";
constexpr const char* MANIFEST_URL = "https://geekpad.tail1a4f72.ts.net:10000/fw/picco/latest.json";
constexpr const char* OTA_PENDING_PATH = "/update/.ota_pending";
constexpr const char* FACTORY_RLOOP_PATH = "/update/.factory_rloop";
constexpr size_t APP_SLOT_SIZE = 0xE70000;  // stock partitions_picco.csv ota_0

// Mirrors safeboot's pre-install checks: size fits the slot and equals the image length,
// magic 0xE9, chip id, <=16 segments, app_desc magic, checksum, appended SHA-256 over the
// whole file. Returns nullptr when the image passes, else a short on-screen reason.
const char* preflight(const char* path);

struct Manifest {
  std::string version;
  size_t size = 0;
  std::string sha256;  // hex of the whole file
  std::string url;
};

// GETs MANIFEST_URL (TLS verified against the IDF CA bundle). nullptr on success.
const char* fetchManifest(Manifest& out);

// Streams m.url to PART_PATH while hashing; requires byte count == m.size and
// SHA-256 == m.sha256, then renames it to FIRMWARE_PATH. On any failure the .part
// is deleted and nothing else is written. nullptr on success, else the reason.
const char* download(const Manifest& m);

}  // namespace picco_update
