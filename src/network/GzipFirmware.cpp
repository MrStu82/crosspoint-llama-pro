#include "GzipFirmware.h"

#include <InflateStream.h>
#include <MinizConfig.h>

#include <memory>
#include <new>

namespace gzip_firmware {

namespace {
constexpr size_t CHUNK = 4096;
constexpr size_t HEADER_MIN = 10;
constexpr size_t TRAILER = 8;
constexpr uint8_t FHCRC = 0x02, FEXTRA = 0x04, FNAME = 0x08, FCOMMENT = 0x10, FRESERVED = 0xE0;

// Buffered input that never hands out the 8-byte trailer: the deflate stream
// must end exactly where the trailer starts, or tinfl reports it truncated.
struct Input {
  ReadFn read;
  void* ctx;
  size_t bodyLeft;  // body bytes not yet pulled from the file
  size_t pos = 0;
  size_t len = 0;
  bool failed = false;
  uint8_t buf[CHUNK] = {};

  bool refill() {
    pos = 0;
    len = 0;
    if (bodyLeft == 0) return false;
    const size_t want = bodyLeft < CHUNK ? bodyLeft : CHUNK;
    const size_t got = read(ctx, buf, want);
    if (got != want) {
      failed = true;
      return false;
    }
    len = got;
    bodyLeft -= got;
    return true;
  }
  bool byte(uint8_t* b) {
    if (pos == len && !refill()) return false;
    *b = buf[pos++];
    return true;
  }
  bool skip(size_t n) {
    for (uint8_t b; n > 0; --n) {
      if (!byte(&b)) return false;
    }
    return true;
  }
  bool skipString() {
    for (uint8_t b = 1; b != 0;) {
      if (!byte(&b)) return false;
    }
    return true;
  }
  static size_t fill(void* self, const uint8_t** data) {
    auto* in = static_cast<Input*>(self);
    if (in->pos == in->len && !in->refill()) return 0;
    *data = in->buf + in->pos;
    const size_t n = in->len - in->pos;
    in->pos = in->len;
    return n;
  }
};

bool parseHeader(Input& in) {
  uint8_t h[HEADER_MIN];
  for (auto& b : h) {
    if (!in.byte(&b)) return false;
  }
  if (h[0] != 0x1F || h[1] != 0x8B || h[2] != 8 || (h[3] & FRESERVED)) return false;
  const uint8_t flags = h[3];
  if (flags & FEXTRA) {
    uint8_t lo, hi;
    if (!in.byte(&lo) || !in.byte(&hi) || !in.skip(lo | (hi << 8))) return false;
  }
  if ((flags & FNAME) && !in.skipString()) return false;
  if ((flags & FCOMMENT) && !in.skipString()) return false;
  if ((flags & FHCRC) && !in.skip(2)) return false;
  return true;
}

uint32_t le32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | (static_cast<uint32_t>(p[3]) << 24); }
}  // namespace

const char* statusName(const Status s) {
  switch (s) {
    case Status::OK:
      return "OK";
    case Status::BAD_HEADER:
      return "BAD_HEADER";
    case Status::CORRUPT:
      return "CORRUPT";
    case Status::TOO_LARGE:
      return "TOO_LARGE";
    case Status::BAD_TRAILER:
      return "BAD_TRAILER";
    case Status::READ_FAIL:
      return "READ_FAIL";
    case Status::SINK_FAIL:
      return "SINK_FAIL";
    case Status::OOM:
      return "OOM";
  }
  return "?";
}

Status inflate(ReadFn read, void* readCtx, const size_t gzSize, const size_t maxOut, SinkFn sink, void* sinkCtx,
               size_t* outLen) {
  *outLen = 0;
  if (gzSize < HEADER_MIN + TRAILER) return Status::BAD_HEADER;

  auto in = std::unique_ptr<Input>(new (std::nothrow) Input{read, readCtx, gzSize - TRAILER});
  auto out = std::unique_ptr<uint8_t[]>(new (std::nothrow) uint8_t[CHUNK]);
  InflateStream stream;
  if (!in || !out || !stream.init(/*streaming=*/true)) return Status::OOM;
  if (!parseHeader(*in)) return in->failed ? Status::READ_FAIL : Status::BAD_HEADER;
  stream.setFill(&Input::fill, in.get());

  // Hand the sink whole CHUNKs (only the last may be short) so flash writes stay
  // sector-aligned exactly like the plain .bin path.
  size_t total = 0;
  size_t fill = 0;
  uint32_t crc = MZ_CRC32_INIT;
  for (bool done = false; !done;) {
    size_t produced = 0;
    const auto st = stream.readAtMost(out.get() + fill, CHUNK - fill, &produced);
    if (in->failed) return Status::READ_FAIL;
    if (st == InflateStream::Status::Error) return Status::CORRUPT;
    if (produced > maxOut - total - fill) return Status::TOO_LARGE;
    fill += produced;
    done = st == InflateStream::Status::Done;
    if (fill == CHUNK || (done && fill > 0)) {
      if (!sink(sinkCtx, total, out.get(), fill)) return Status::SINK_FAIL;
      crc = static_cast<uint32_t>(mz_crc32(crc, out.get(), fill));
      total += fill;
      fill = 0;
    }
  }

  // The stream must have consumed the whole body, leaving the file at the trailer.
  if (in->bodyLeft != 0) return Status::BAD_TRAILER;
  uint8_t trailer[TRAILER];
  if (read(readCtx, trailer, TRAILER) != TRAILER) return Status::BAD_TRAILER;
  if (le32(trailer) != crc || le32(trailer + 4) != static_cast<uint32_t>(total)) return Status::BAD_TRAILER;
  *outLen = total;
  return Status::OK;
}

}  // namespace gzip_firmware
