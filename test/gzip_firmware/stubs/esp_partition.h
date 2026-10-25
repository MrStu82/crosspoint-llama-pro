#pragma once
// In-memory NOR-flash partition: erase sets 0xFF, write ANDs (so a write into
// unerased flash is caught as corruption), like the real chip.
#include <cstddef>
#include <cstdint>
#include <algorithm>
#include <vector>

#include "spi_flash_mmap.h"

using esp_err_t = int;
constexpr esp_err_t ESP_OK = 0;
constexpr esp_err_t ESP_FAIL = -1;

struct esp_partition_t {
  const char* label;
  uint32_t address;
  uint32_t size;
  std::vector<uint8_t>* bytes;
};

inline esp_err_t esp_partition_read(const esp_partition_t* p, size_t off, void* dst, size_t len) {
  if (off > p->size || len > p->size - off) return ESP_FAIL;
  std::copy(p->bytes->begin() + off, p->bytes->begin() + off + len, static_cast<uint8_t*>(dst));
  return ESP_OK;
}
inline esp_err_t esp_partition_erase_range(const esp_partition_t* p, size_t off, size_t len) {
  if (off % SPI_FLASH_SEC_SIZE || len % SPI_FLASH_SEC_SIZE || off > p->size || len > p->size - off) return ESP_FAIL;
  std::fill(p->bytes->begin() + off, p->bytes->begin() + off + len, 0xFF);
  return ESP_OK;
}
inline esp_err_t esp_partition_write(const esp_partition_t* p, size_t off, const void* src, size_t len) {
  if (off > p->size || len > p->size - off) return ESP_FAIL;
  const auto* s = static_cast<const uint8_t*>(src);
  for (size_t i = 0; i < len; i++) (*p->bytes)[off + i] &= s[i];
  return ESP_OK;
}
