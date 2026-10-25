#include "FirmwareFlasher.h"

#include <Arduino.h>
#include <HalStorage.h>
#include <Logging.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <mbedtls/sha256.h>
#include <spi_flash_mmap.h>

#include "SafetyGuards.h"

#include <algorithm>
#include <cstring>
#include <memory>

#include "GzipFirmware.h"
#include "OtaBootSwitch.h"

namespace firmware_flash {

namespace {
constexpr uint8_t ESP_IMAGE_MAGIC = 0xE9;
constexpr size_t MIN_FIRMWARE_SIZE = 64 * 1024;
constexpr size_t SEC = SPI_FLASH_SEC_SIZE;  // 4 KiB
constexpr size_t BLK = 64 * 1024;           // 64 KiB block-erase granularity
constexpr size_t CHUNK = 4096;
constexpr size_t SHA_TRAILER = 32;
constexpr uint8_t CHECKSUM_SEED = 0xEF;
constexpr size_t HEADER_SIZE = 24;
constexpr size_t SEG_HEADER_SIZE = 8;
}  // namespace

const char* resultName(Result r) {
  switch (r) {
    case Result::OK:
      return "OK";
    case Result::OPEN_FAIL:
      return "OPEN_FAIL";
    case Result::TOO_SMALL:
      return "TOO_SMALL";
    case Result::TOO_LARGE:
      return "TOO_LARGE";
    case Result::BAD_MAGIC:
      return "BAD_MAGIC";
    case Result::BAD_SEGMENTS:
      return "BAD_SEGMENTS";
    case Result::BAD_CHECKSUM:
      return "BAD_CHECKSUM";
    case Result::BAD_SHA:
      return "BAD_SHA";
    case Result::BAD_CHIP:
      return "BAD_CHIP";
    case Result::BAD_SIZE:
      return "BAD_SIZE";
    case Result::NO_PARTITION:
      return "NO_PARTITION";
    case Result::OOM:
      return "OOM";
    case Result::READ_FAIL:
      return "READ_FAIL";
    case Result::ERASE_FAIL:
      return "ERASE_FAIL";
    case Result::WRITE_FAIL:
      return "WRITE_FAIL";
    case Result::OTADATA_FAIL:
      return "OTADATA_FAIL";
    case Result::BAD_GZIP:
      return "BAD_GZIP";
  }
  return "?";
}

uint16_t runningPartitionChipId() {
  const esp_partition_t* running = esp_ota_get_running_partition();
  uint16_t chip = 0xFFFF;
  if (!running || esp_partition_read(running, 12, &chip, sizeof(chip)) != ESP_OK) return 0xFFFF;
  return chip;
}

namespace {
// Sequential exact-length reader over the image bytes: an SD file (plain .bin) or the
// freshly written OTA partition (inflated .bin.gz). Returns false on a short read.
struct ImageReader {
  void* ctx;
  bool (*read)(void* ctx, uint8_t* buf, size_t len);
};

// Stream `length` bytes from `src` starting at the current read offset, feeding them through
// both the XOR-checksum and SHA256 accumulators. Used by validateImage so the whole image
// is verified end-to-end without holding it in RAM (ESP32-C3 only has ~380 KB).
Result feedHashAndChecksum(const ImageReader& src, size_t length, uint8_t* xorAccum, mbedtls_sha256_context* sha,
                           uint8_t* buf) {
  size_t remaining = length;
  while (remaining > 0) {
    const size_t want = std::min<size_t>(CHUNK, remaining);
    if (!src.read(src.ctx, buf, want)) return Result::READ_FAIL;
    if (sha) mbedtls_sha256_update(sha, buf, want);
    if (xorAccum) {
      uint8_t acc = *xorAccum;
      for (size_t i = 0; i < want; i++) acc ^= buf[i];
      *xorAccum = acc;
    }
    remaining -= want;
  }
  return Result::OK;
}

// Full-image integrity check over `fileSize` bytes from `src` (see validateImageFile).
Result validateImage(const ImageReader& src, const size_t fileSize, const size_t partitionSize) {
  if (fileSize < MIN_FIRMWARE_SIZE) {
    LOG_ERR("FLASH", "validate: too small: %u", static_cast<unsigned>(fileSize));
    return Result::TOO_SMALL;
  }
  if (partitionSize > 0 && fileSize > partitionSize) {
    LOG_ERR("FLASH", "validate: too large: %u > %u", static_cast<unsigned>(fileSize),
            static_cast<unsigned>(partitionSize));
    return Result::TOO_LARGE;
  }

  uint8_t header[HEADER_SIZE];
  if (!src.read(src.ctx, header, HEADER_SIZE)) {
    LOG_ERR("FLASH", "validate: header read failed");
    return Result::READ_FAIL;
  }
  if (header[0] != ESP_IMAGE_MAGIC) {
    LOG_ERR("FLASH", "validate: bad magic 0x%02X", header[0]);
    return Result::BAD_MAGIC;
  }
  uint16_t imageChip = 0;
  std::memcpy(&imageChip, header + 12, sizeof(imageChip));
  if (!safety_guards::imageChipMatchesDevice(imageChip, runningPartitionChipId())) {
    LOG_ERR("FLASH", "validate: image chip does not match running partition");
    return Result::BAD_CHIP;
  }
  const uint8_t segCount = header[1];
  const bool hashAppended = header[23] != 0;

  auto buf = std::unique_ptr<uint8_t[]>(new (std::nothrow) uint8_t[CHUNK]);
  if (!buf) {
    return Result::OOM;
  }

  mbedtls_sha256_context shaCtx;
  mbedtls_sha256_init(&shaCtx);
  mbedtls_sha256_starts(&shaCtx, /*is224=*/0);
  mbedtls_sha256_update(&shaCtx, header, HEADER_SIZE);

  uint8_t xorAccum = CHECKSUM_SEED;
  size_t pos = HEADER_SIZE;

  for (uint8_t i = 0; i < segCount; i++) {
    if (!safety_guards::rangeFits(pos, SEG_HEADER_SIZE, fileSize)) {
      LOG_ERR("FLASH", "validate: seg %u header overruns EOF at %u", i, static_cast<unsigned>(pos));
      mbedtls_sha256_free(&shaCtx);
      return Result::BAD_SEGMENTS;
    }
    uint8_t segHdr[SEG_HEADER_SIZE];
    if (!src.read(src.ctx, segHdr, SEG_HEADER_SIZE)) {
      mbedtls_sha256_free(&shaCtx);
      return Result::READ_FAIL;
    }
    mbedtls_sha256_update(&shaCtx, segHdr, SEG_HEADER_SIZE);
    pos += SEG_HEADER_SIZE;

    uint32_t dataLen;
    std::memcpy(&dataLen, segHdr + 4, sizeof(dataLen));
    if (!safety_guards::rangeFits(pos, dataLen, fileSize)) {
      LOG_ERR("FLASH", "validate: seg %u data overruns EOF (%u + %u > %u)", i, static_cast<unsigned>(pos),
              static_cast<unsigned>(dataLen), static_cast<unsigned>(fileSize));
      mbedtls_sha256_free(&shaCtx);
      return Result::BAD_SEGMENTS;
    }

    const Result feedRes = feedHashAndChecksum(src, dataLen, &xorAccum, &shaCtx, buf.get());
    if (feedRes != Result::OK) {
      mbedtls_sha256_free(&shaCtx);
      return feedRes;
    }
    pos += dataLen;
  }

  // pad_end is the 16-byte aligned offset at which the checksum byte sits at pad_end - 1.
  const size_t padEnd = (pos + 16) & ~static_cast<size_t>(15);
  const size_t expectedTotal = padEnd + (hashAppended ? SHA_TRAILER : 0);
  if (expectedTotal != fileSize) {
    LOG_ERR("FLASH", "validate: size mismatch body+pad=%u sha=%u expected=%u actual=%u", static_cast<unsigned>(padEnd),
            static_cast<unsigned>(hashAppended ? SHA_TRAILER : 0), static_cast<unsigned>(expectedTotal),
            static_cast<unsigned>(fileSize));
    mbedtls_sha256_free(&shaCtx);
    return Result::BAD_SIZE;
  }

  // Read the padding bytes (which include the stored checksum at the last byte) into the SHA stream.
  const size_t padLen = padEnd - pos;
  uint8_t padBuf[16];
  if (padLen > sizeof(padBuf)) {
    mbedtls_sha256_free(&shaCtx);
    return Result::BAD_SIZE;
  }
  if (padLen > 0 && !src.read(src.ctx, padBuf, padLen)) {
    mbedtls_sha256_free(&shaCtx);
    return Result::READ_FAIL;
  }
  mbedtls_sha256_update(&shaCtx, padBuf, padLen);

  const uint8_t storedChecksum = padBuf[padLen - 1];
  if ((xorAccum & 0xFF) != storedChecksum) {
    LOG_ERR("FLASH", "validate: checksum mismatch computed=0x%02X stored=0x%02X", xorAccum, storedChecksum);
    mbedtls_sha256_free(&shaCtx);
    return Result::BAD_CHECKSUM;
  }

  if (hashAppended) {
    uint8_t computed[SHA_TRAILER];
    mbedtls_sha256_finish(&shaCtx, computed);
    uint8_t stored[SHA_TRAILER];
    if (!src.read(src.ctx, stored, SHA_TRAILER)) {
      mbedtls_sha256_free(&shaCtx);
      return Result::READ_FAIL;
    }
    if (std::memcmp(computed, stored, SHA_TRAILER) != 0) {
      LOG_ERR("FLASH", "validate: SHA256 mismatch");
      mbedtls_sha256_free(&shaCtx);
      return Result::BAD_SHA;
    }
  }

  mbedtls_sha256_free(&shaCtx);
  return Result::OK;
}
}  // namespace

Result validateImageFile(const char* sdPath, size_t partitionSize) {
  HalFile file;
  if (!Storage.openFileForRead("FLASH", sdPath, file) || !file) {
    LOG_ERR("FLASH", "validate: open failed: %s", sdPath);
    return Result::OPEN_FAIL;
  }
  const ImageReader src{&file, [](void* f, uint8_t* buf, size_t len) {
                          return static_cast<HalFile*>(f)->read(buf, len) == static_cast<int>(len);
                        }};
  return validateImage(src, file.fileSize(), partitionSize);
}

Result flashFromSdPath(const char* sdPath, ProgressCb onProgress, void* ctx, bool alreadyValidated) {
  // Resolve destination first so we can size-check during validation. The full image-integrity
  // pass below verifies header, segment table, XOR checksum and SHA256 trailer end-to-end before
  // we touch otadata, so a truncated/corrupted .bin can never become the next boot target.
  const esp_partition_t* dest = esp_ota_get_next_update_partition(nullptr);
  if (!dest) {
    LOG_ERR("FLASH", "no next-update partition");
    return Result::NO_PARTITION;
  }

  // When the caller already ran validateImageFile() against this same partition
  // size (e.g. SdFirmwareUpdateActivity validates before the confirmation
  // prompt), skip the redundant integrity scan. We still keep the partition
  // lookup so the rest of the flashing path stays unchanged.
  if (!alreadyValidated) {
    const Result validateRes = validateImageFile(sdPath, dest->size);
    if (validateRes != Result::OK) {
      LOG_ERR("FLASH", "image validation failed: %s", resultName(validateRes));
      return validateRes;
    }
  }

  HalFile file;
  if (!Storage.openFileForRead("FLASH", sdPath, file) || !file) {
    LOG_ERR("FLASH", "open failed: %s", sdPath);
    return Result::OPEN_FAIL;
  }

  const size_t firmwareSize = file.fileSize();
  LOG_INF("FLASH", "src=%s size=%u dest=%s @0x%x partsize=%u", sdPath, static_cast<unsigned>(firmwareSize), dest->label,
          static_cast<unsigned>(dest->address), static_cast<unsigned>(dest->size));

  auto buffer = std::unique_ptr<uint8_t[]>(new (std::nothrow) uint8_t[CHUNK]);
  if (!buffer) {
    LOG_ERR("FLASH", "OOM");
    file.close();
    return Result::OOM;
  }

  // Interleave erase + write so the progress bar advances 0→100% smoothly
  // rather than stalling for several seconds during a single up-front erase.
  size_t streamPos = 0;
  size_t erasedUpto = 0;
  while (streamPos < firmwareSize) {
    if (streamPos >= erasedUpto) {
      size_t eraseLen = std::min<size_t>(BLK, dest->size - streamPos);
      eraseLen = (eraseLen + SEC - 1) & ~(SEC - 1);
      eraseLen = std::min<size_t>(eraseLen, dest->size - streamPos);
      if (esp_partition_erase_range(dest, streamPos, eraseLen) != ESP_OK) {
        LOG_ERR("FLASH", "erase @%u (len=%u) failed", static_cast<unsigned>(streamPos),
                static_cast<unsigned>(eraseLen));
        file.close();
        return Result::ERASE_FAIL;
      }
      erasedUpto = streamPos + eraseLen;
    }

    const size_t want = std::min<size_t>(CHUNK, firmwareSize - streamPos);
    const int read = file.read(buffer.get(), want);
    if (read <= 0 || static_cast<size_t>(read) != want) {
      LOG_ERR("FLASH", "read @%u: got=%d want=%u", static_cast<unsigned>(streamPos), read, static_cast<unsigned>(want));
      file.close();
      return Result::READ_FAIL;
    }
    if (esp_partition_write(dest, streamPos, buffer.get(), want) != ESP_OK) {
      LOG_ERR("FLASH", "write @%u failed", static_cast<unsigned>(streamPos));
      file.close();
      return Result::WRITE_FAIL;
    }
    streamPos += want;
    if (onProgress) onProgress(streamPos, firmwareSize, ctx);
    delay(1);
  }
  file.close();

  if (!ota_boot::switchTo(dest)) {
    LOG_ERR("FLASH", "otadata switch failed");
    return Result::OTADATA_FAIL;
  }
  return Result::OK;
}

namespace {
struct GzSink {
  const esp_partition_t* dest;
  HalFile* file;
  size_t gzSize;
  ProgressCb onProgress;
  void* ctx;
  size_t erasedUpto;
  Result err;
};

// Writes arrive in order; erase 64 KiB ahead of them, as flashFromSdPath does.
bool gzWrite(void* p, const size_t offset, const uint8_t* data, const size_t len) {
  auto* s = static_cast<GzSink*>(p);
  while (offset + len > s->erasedUpto) {
    size_t eraseLen = std::min<size_t>(BLK, s->dest->size - s->erasedUpto);
    eraseLen = std::min<size_t>((eraseLen + SEC - 1) & ~(SEC - 1), s->dest->size - s->erasedUpto);
    if (eraseLen == 0 || esp_partition_erase_range(s->dest, s->erasedUpto, eraseLen) != ESP_OK) {
      LOG_ERR("FLASH", "gz erase @%u failed", static_cast<unsigned>(s->erasedUpto));
      s->err = Result::ERASE_FAIL;
      return false;
    }
    s->erasedUpto += eraseLen;
  }
  if (esp_partition_write(s->dest, offset, data, len) != ESP_OK) {
    LOG_ERR("FLASH", "gz write @%u failed", static_cast<unsigned>(offset));
    s->err = Result::WRITE_FAIL;
    return false;
  }
  if (s->onProgress) s->onProgress(s->file->position(), s->gzSize, s->ctx);
  delay(1);
  return true;
}

struct PartitionCursor {
  const esp_partition_t* part;
  size_t pos;
};
}  // namespace

Result flashGzFromSdPath(const char* sdPath, ProgressCb onProgress, void* ctx) {
  const esp_partition_t* dest = esp_ota_get_next_update_partition(nullptr);
  if (!dest) {
    LOG_ERR("FLASH", "no next-update partition");
    return Result::NO_PARTITION;
  }
  HalFile file;
  if (!Storage.openFileForRead("FLASH", sdPath, file) || !file) {
    LOG_ERR("FLASH", "open failed: %s", sdPath);
    return Result::OPEN_FAIL;
  }
  const size_t gzSize = file.fileSize();
  LOG_INF("FLASH", "gz src=%s size=%u dest=%s @0x%x partsize=%u", sdPath, static_cast<unsigned>(gzSize), dest->label,
          static_cast<unsigned>(dest->address), static_cast<unsigned>(dest->size));

  GzSink sink{dest, &file, gzSize, onProgress, ctx, 0, Result::OK};
  size_t imageSize = 0;
  const auto gz = gzip_firmware::inflate(
      [](void* f, uint8_t* buf, size_t len) -> size_t {
        const int n = static_cast<HalFile*>(f)->read(buf, len);
        return n > 0 ? static_cast<size_t>(n) : 0;
      },
      &file, gzSize, dest->size, &gzWrite, &sink, &imageSize);
  file.close();
  if (gz != gzip_firmware::Status::OK) {
    LOG_ERR("FLASH", "gz inflate failed: %s", gzip_firmware::statusName(gz));
    if (gz == gzip_firmware::Status::SINK_FAIL) return sink.err;
    if (gz == gzip_firmware::Status::READ_FAIL) return Result::READ_FAIL;
    if (gz == gzip_firmware::Status::OOM) return Result::OOM;
    return Result::BAD_GZIP;
  }

  // Validate what actually landed in flash, not the stream we think we wrote.
  PartitionCursor cur{dest, 0};
  const ImageReader src{&cur, [](void* p, uint8_t* buf, size_t len) {
                          auto* c = static_cast<PartitionCursor*>(p);
                          if (esp_partition_read(c->part, c->pos, buf, len) != ESP_OK) return false;
                          c->pos += len;
                          return true;
                        }};
  const Result validateRes = validateImage(src, imageSize, dest->size);
  if (validateRes != Result::OK) {
    LOG_ERR("FLASH", "inflated image validation failed: %s", resultName(validateRes));
    return validateRes;
  }

  if (!ota_boot::switchTo(dest)) {
    LOG_ERR("FLASH", "otadata switch failed");
    return Result::OTADATA_FAIL;
  }
  return Result::OK;
}

}  // namespace firmware_flash
