#if FREEINK_DEVICE_PICCO
#include "PiccoUpdate.h"

#include <HalStorage.h>
#include <Logging.h>

#include <cstdint>
#include <cstring>

#include "FirmwareFlasher.h"

namespace picco_update {

namespace {
constexpr size_t HEADER_SIZE = 24;
constexpr size_t SEG_HEADER_SIZE = 8;
constexpr uint8_t MAX_SEGMENTS = 16;
constexpr uint32_t APP_DESC_MAGIC = 0xABCD5432;

const char* reasonFor(const firmware_flash::Result r) {
  using firmware_flash::Result;
  switch (r) {
    case Result::OPEN_FAIL:
      return "No /update/firmware.bin";
    case Result::TOO_SMALL:
      return "Image too small";
    case Result::TOO_LARGE:
      return "Image larger than app slot";
    case Result::BAD_MAGIC:
      return "Not an ESP32 image";
    case Result::BAD_CHIP:
      return "Image is not for this chip";
    case Result::BAD_SEGMENTS:
      return "Bad segment table";
    case Result::BAD_CHECKSUM:
      return "Image checksum mismatch";
    case Result::BAD_SHA:
      return "SHA-256 mismatch";
    case Result::BAD_SIZE:
      return "File size differs from image length";
    default:
      return "Could not read image";
  }
}
}  // namespace

const char* preflight(const char* path) {
  const auto r = firmware_flash::validateImageFile(path, APP_SLOT_SIZE);
  if (r != firmware_flash::Result::OK) {
    LOG_ERR("PUPD", "preflight: %s", firmware_flash::resultName(r));
    return reasonFor(r);
  }

  uint8_t head[HEADER_SIZE + SEG_HEADER_SIZE + sizeof(uint32_t)];
  HalFile file;
  const bool ok = Storage.openFileForRead("PUPD", path, file) && file &&
                  file.read(head, sizeof(head)) == static_cast<int>(sizeof(head));
  file.close();
  if (!ok) return "Could not read image";
  if (head[1] > MAX_SEGMENTS) return "Too many segments";
  if (head[23] == 0) return "Image has no SHA-256";
  uint32_t descMagic = 0;
  std::memcpy(&descMagic, head + HEADER_SIZE + SEG_HEADER_SIZE, sizeof(descMagic));
  if (descMagic != APP_DESC_MAGIC) return "Not an app image";
  return nullptr;
}

}  // namespace picco_update
#endif
