#pragma once
#include <array>
#include <algorithm>
#include <cstdint>
#if defined(CROSSPOINT_SIMULATOR)
// The pinned simulator's mbedtls shim is XOR, not SHA256. Use host crypto
// rather than accepting a false content identity in simulator validation.
#include <openssl/evp.h>
#else
#include <mbedtls/sha256.h>
#endif

namespace txt_index {
// Called once per reader initialization, never during a page turn. Fixed
// buffer, full content digest: equal size/mtime alone cannot identify a book.
template <class File> bool digest(File& f, std::array<uint8_t, 32>& result) {
#if defined(CROSSPOINT_SIMULATOR)
  EVP_MD_CTX* sha = EVP_MD_CTX_new();
  if (!sha) return false;
  bool ok = EVP_DigestInit_ex(sha, EVP_sha256(), nullptr) == 1;
#else
  mbedtls_sha256_context sha;
  mbedtls_sha256_init(&sha);
  bool ok = mbedtls_sha256_starts(&sha, 0) == 0;
#endif
  uint8_t buffer[512];
  size_t remaining = f.size();
  while (ok && remaining) {
    const size_t want = std::min(sizeof(buffer), remaining);
    const int got = f.read(buffer, want);
#if defined(CROSSPOINT_SIMULATOR)
    ok = got == static_cast<int>(want) && EVP_DigestUpdate(sha, buffer, want) == 1;
#else
    ok = got == static_cast<int>(want) && mbedtls_sha256_update(&sha, buffer, want) == 0;
#endif
    if (ok) remaining -= want;
  }
#if defined(CROSSPOINT_SIMULATOR)
  if (ok) ok = EVP_DigestFinal_ex(sha, result.data(), nullptr) == 1;
  EVP_MD_CTX_free(sha);
#else
  if (ok) ok = mbedtls_sha256_finish(&sha, result.data()) == 0;
  mbedtls_sha256_free(&sha);
#endif
  return ok;
}
} // namespace txt_index
