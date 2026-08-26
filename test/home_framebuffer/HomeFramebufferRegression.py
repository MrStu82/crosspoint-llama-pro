#!/usr/bin/env python3
"""Pixel-level guards for the X4 Pro Home acceptance fixtures.

The simulator writes a 32-bit bottom-up BMP.  These checks deliberately avoid
OCR and third-party image packages: they assert the visible ink that previously
regressed (three unavailable-value markers and the three whole title words).
"""

import argparse
import struct
from pathlib import Path


class Bmp:
    def __init__(self, path: Path):
        self.raw = path.read_bytes()
        if self.raw[:2] != b"BM":
            raise AssertionError(f"{path}: not a BMP")
        self.offset = struct.unpack_from("<I", self.raw, 10)[0]
        self.width, raw_height = struct.unpack_from("<ii", self.raw, 18)
        self.bottom_up = raw_height > 0
        self.height = abs(raw_height)
        self.bpp = struct.unpack_from("<H", self.raw, 28)[0]
        if self.bpp != 32:
            raise AssertionError(f"{path}: expected 32bpp, got {self.bpp}")
        self.stride = ((self.width * self.bpp + 31) // 32) * 4

    def ink_count(self, left: int, top: int, right: int, bottom: int) -> int:
        count = 0
        for y in range(max(0, top), min(self.height, bottom)):
            source_y = self.height - 1 - y if self.bottom_up else y
            for x in range(max(0, left), min(self.width, right)):
                at = self.offset + source_y * self.stride + x * 4
                # Black/white simulator framebuffer; tolerate channel noise.
                if sum(self.raw[at : at + 3]) < 384:
                    count += 1
        return count


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--populated", type=Path, required=True)
    parser.add_argument("--unavailable", type=Path, required=True)
    args = parser.parse_args()

    populated = Bmp(args.populated)
    unavailable = Bmp(args.unavailable)
    for image in (populated, unavailable):
        assert image.width >= 470 and image.height >= 790, (image.width, image.height)

    # DUNGEON / CRAWLER / CARL must be three intact whole-word ink bands in
    # the 130px right column, rather than a clipped or hard-split title.
    title_bands = ((100, 135), (131, 169), (162, 203))
    title_counts = [populated.ink_count(327, top, 462, bottom) for top, bottom in title_bands]
    assert all(count >= 35 for count in title_counts), title_counts
    assert populated.ink_count(462, 100, 476, 203) == 0, "title ink crossed right-column edge"

    # With no reading-stat file, each value row must still contain a visible
    # UI-font em dash. Labels sit above these narrow value-only regions.
    placeholder_counts = [
        unavailable.ink_count(327, 344 + row * 66, 370, 380 + row * 66)
        for row in range(3)
    ]
    assert all(count >= 8 for count in placeholder_counts), placeholder_counts

    print(f"PASS title_bands={title_counts} unavailable_markers={placeholder_counts}")


if __name__ == "__main__":
    main()
