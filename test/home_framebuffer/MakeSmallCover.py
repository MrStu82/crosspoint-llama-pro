#!/usr/bin/env python3
"""Derive a small cover bitmap from a full-size one, for the Home cover fixture.

The acceptance cover is 273x438 -- larger than the Home cover lane in both
axes, so it always scales down to fill it.  That is exactly why the simulator
could not reproduce the cover-stretch defect: a smaller-than-lane cover is the
case that used to be blown up to the lane's size.  This crops a byte-aligned
centre window out of a 1bpp BMP, which keeps the source's own header, palette
and row format intact rather than synthesising a second BMP encoder.
"""

import argparse
import struct
from pathlib import Path


def crop(source: Path, target: Path, width: int, height: int) -> None:
    raw = source.read_bytes()
    if raw[:2] != b"BM":
        raise AssertionError(f"{source}: not a BMP")
    offset, = struct.unpack_from("<I", raw, 10)
    header_size, = struct.unpack_from("<I", raw, 14)
    src_w, src_h = struct.unpack_from("<ii", raw, 18)
    bpp, = struct.unpack_from("<H", raw, 28)
    compression, = struct.unpack_from("<I", raw, 30)
    if bpp != 1 or compression != 0 or src_h <= 0:
        raise AssertionError(f"{source}: expected uncompressed bottom-up 1bpp, got {bpp}bpp comp={compression}")
    if width > src_w or height > src_h:
        raise AssertionError(f"{source}: {src_w}x{src_h} is smaller than the requested {width}x{height}")
    if width % 8:
        raise AssertionError("width must be a whole number of bytes so rows can be sliced, not re-packed")

    src_stride = ((src_w * bpp + 31) // 32) * 4
    dst_stride = ((width * bpp + 31) // 32) * 4
    x_byte = ((src_w - width) // 2) // 8
    y_start = (src_h - height) // 2

    rows = []
    for row in range(height):
        at = offset + (y_start + row) * src_stride + x_byte
        rows.append(raw[at : at + dst_stride].ljust(dst_stride, b"\x00"))

    # The source's own file header and palette are reused verbatim; only the
    # dimensions and the three size/offset fields change.
    head = bytearray(raw[:offset])
    struct.pack_into("<ii", head, 18, width, height)
    struct.pack_into("<I", head, 34, dst_stride * height)
    struct.pack_into("<I", head, 2, len(head) + dst_stride * height)
    struct.pack_into("<I", head, 10, len(head))
    target.write_bytes(bytes(head) + b"".join(rows))

    ink = sum(8 - bin(byte).count("1") for row in rows for byte in row)
    print(f"{target}: {width}x{height} zero-bit pixels={ink}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--target", type=Path, required=True)
    parser.add_argument("--width", type=int, default=120)
    parser.add_argument("--height", type=int, default=180)
    args = parser.parse_args()
    crop(args.source, args.target, args.width, args.height)


if __name__ == "__main__":
    main()
