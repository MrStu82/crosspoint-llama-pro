#if FREEINK_DEVICE_PICCO
#include <ArduinoJson.h>
#include <HalStorage.h>
#include <Logging.h>
#include <mbedtls/sha256.h>
#include <strings.h>

#include <cstdio>
#include <cstring>

#include "HttpDownloader.h"
#include "PiccoUpdate.h"

namespace picco_update {

namespace {
constexpr size_t MANIFEST_MAX = 1024;
constexpr size_t SHA_HEX_LEN = 64;
}  // namespace

const char* fetchManifest(Manifest& out) {
  std::string body;
  const bool fetched = HttpDownloader::fetchUrlVerified(MANIFEST_URL, [&body](const uint8_t* data, size_t len) {
    if (body.size() + len > MANIFEST_MAX) return false;
    body.append(reinterpret_cast<const char*>(data), len);
    return true;
  });
  if (!fetched) return "Could not reach update server";

  JsonDocument doc;
  if (deserializeJson(doc, body)) return "Bad update manifest";
  const char* version = doc["version"];
  const char* sha = doc["sha256"];
  const char* url = doc["url"];
  if (!version || !*version || !sha || std::strlen(sha) != SHA_HEX_LEN || !url || !doc["size"].is<uint32_t>()) {
    return "Bad update manifest";
  }
  if (std::strncmp(url, "https://", 8) != 0) return "Update URL is not https";
  out.version = version;
  out.size = doc["size"].as<uint32_t>();
  out.sha256 = sha;
  out.url = url;
  if (out.size == 0 || out.size > APP_SLOT_SIZE) return "Update does not fit the app slot";
  return nullptr;
}

const char* download(const Manifest& m, const ProgressFn onProgress, void* const ctx) {
  if (Storage.exists(PART_PATH)) Storage.remove(PART_PATH);
  HalFile file;
  if (!Storage.openFileForWrite("PUPD", PART_PATH, file) || !file) return "Could not write to SD card";

  mbedtls_sha256_context sha;
  mbedtls_sha256_init(&sha);
  mbedtls_sha256_starts(&sha, /*is224=*/0);
  size_t count = 0;
  bool tooLong = false;
  bool writeFailed = false;
  const bool fetched = HttpDownloader::fetchUrlVerified(m.url, [&](const uint8_t* data, size_t len) {
    if (count + len > m.size) {
      tooLong = true;
      return false;
    }
    if (file.write(data, len) != len) {
      writeFailed = true;
      return false;
    }
    mbedtls_sha256_update(&sha, data, len);
    count += len;
    if (onProgress) onProgress(ctx, count);
    return true;
  });
  file.flush();
  file.close();
  uint8_t digest[32];
  mbedtls_sha256_finish(&sha, digest);
  mbedtls_sha256_free(&sha);
  char hex[SHA_HEX_LEN + 1];
  for (size_t i = 0; i < sizeof(digest); ++i) snprintf(hex + i * 2, 3, "%02x", digest[i]);

  const char* why = nullptr;
  if (writeFailed) {
    why = "Could not write to SD card";
  } else if (tooLong) {
    why = "Download larger than manifest size";
  } else if (!fetched) {
    why = "Download failed";
  } else if (count != m.size) {
    why = "Download size mismatch";
  } else if (strcasecmp(hex, m.sha256.c_str()) != 0) {
    why = "Download SHA-256 mismatch";
  }
  if (why) {
    LOG_ERR("PUPD", "download: %s (%u of %u bytes)", why, (unsigned)count, (unsigned)m.size);
    Storage.remove(PART_PATH);
    return why;
  }

  if (Storage.exists(FIRMWARE_PATH)) Storage.remove(FIRMWARE_PATH);
  if (!Storage.rename(PART_PATH, FIRMWARE_PATH)) {
    Storage.remove(PART_PATH);
    return "Could not save firmware.bin";
  }
  LOG_INF("PUPD", "downloaded %s (%u bytes)", m.version.c_str(), (unsigned)count);
  return nullptr;
}

}  // namespace picco_update
#endif
