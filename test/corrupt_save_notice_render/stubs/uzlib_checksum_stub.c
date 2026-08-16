// Not part of any real .h/.cpp -- provides uzlib_adler32/uzlib_crc32, which the vendored
// lib/uzlib/src subset declares (uzlib.h) but never defines anywhere in this repo. Only
// uzlib_uncompress_chksum() (linked in as dead code via tinflate.c, never called -- the real
// InflateReader.cpp exclusively calls uzlib_uncompress()) references them, so real bodies are
// unreachable at runtime; these exist purely to satisfy the linker.
#include <stdint.h>

uint32_t uzlib_adler32(const void *data, unsigned int length, uint32_t prev_sum) {
  (void)data;
  (void)length;
  return prev_sum;
}

uint32_t uzlib_crc32(const void *data, unsigned int length, uint32_t crc) {
  (void)data;
  (void)length;
  return crc;
}
