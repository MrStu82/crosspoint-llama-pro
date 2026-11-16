#pragma once

#include <cstddef>

// Picco wireless update: the app only stages /update/firmware.bin (uploaded over the
// Wi-Fi file transfer page) and hands it to the stock safeboot installer. The app never
// writes the app slot itself.
namespace picco_update {

constexpr const char* FIRMWARE_PATH = "/update/firmware.bin";
constexpr const char* OTA_PENDING_PATH = "/update/.ota_pending";
constexpr const char* FACTORY_RLOOP_PATH = "/update/.factory_rloop";
constexpr size_t APP_SLOT_SIZE = 0xE70000;  // stock partitions_picco.csv ota_0

// Mirrors safeboot's pre-install checks: size fits the slot and equals the image length,
// magic 0xE9, chip id, <=16 segments, app_desc magic, checksum, appended SHA-256 over the
// whole file. Returns nullptr when the image passes, else a short on-screen reason.
const char* preflight(const char* path);

}  // namespace picco_update
