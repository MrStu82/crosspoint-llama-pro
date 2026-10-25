#pragma once

#include <cstddef>
#include <cstdint>

// Streaming gunzip for firmware images: inflates a single-member gzip file
// (as written by `gzip -9n app.bin`) chunk by chunk into a sink, without
// buffering the image. Uses the vendored, ROM-renamed tinfl via InflateStream
// (see MinizConfig.h for why the ESP32 ROM tinfl must not be used).
//
// Verifies the gzip framing end to end: header, a deflate stream that ends
// exactly at the 8-byte trailer, CRC32 and ISIZE. It knows nothing about ESP
// images; the caller validates the inflated image before it can become a boot
// target.

namespace gzip_firmware {

enum class Status {
  OK,
  BAD_HEADER,   // not gzip, unsupported method/flags, or header runs past EOF
  CORRUPT,      // deflate stream corrupt or truncated
  TOO_LARGE,    // inflated output would exceed maxOut
  BAD_TRAILER,  // CRC32/ISIZE mismatch or trailer missing
  READ_FAIL,
  SINK_FAIL,
  OOM,
};

// Read up to len bytes; returns the count (0 at EOF or on error).
using ReadFn = size_t (*)(void* ctx, uint8_t* buf, size_t len);
// Consume len inflated bytes destined for output offset `offset`. false aborts.
using SinkFn = bool (*)(void* ctx, size_t offset, const uint8_t* data, size_t len);

// Inflate a gzip stream of `gzSize` bytes read sequentially through `read`.
// On OK, *outLen holds the inflated size. Output past `maxOut` is refused
// before it reaches the sink.
Status inflate(ReadFn read, void* readCtx, size_t gzSize, size_t maxOut, SinkFn sink, void* sinkCtx, size_t* outLen);

const char* statusName(Status s);

}  // namespace gzip_firmware
