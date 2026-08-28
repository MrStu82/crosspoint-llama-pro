#!/usr/bin/env python3
"""Additive pixel gate for the approved Phase 5B Home masks."""

import argparse
import struct
from pathlib import Path


# Simulator screenshots are the bezel-cropped 476x796 drawable. These masks
# include the complete raster glyph extents for the four approved semantic
# changes and nothing else.
MASKS = {
    "half_star": (355, 257, 21, 20),
    "old_book_eta": (256, 414, 112, 62),
    "adjacent_book_eta": (360, 361, 110, 57),
    "cover_progress": (13, 466, 230, 23),
}


class Bmp:
    def __init__(self, path: Path):
        raw = path.read_bytes()
        if raw[:2] != b"BM":
            raise AssertionError(f"{path}: not a BMP")
        offset = struct.unpack_from("<I", raw, 10)[0]
        self.width, raw_height = struct.unpack_from("<ii", raw, 18)
        self.height = abs(raw_height)
        if struct.unpack_from("<H", raw, 28)[0] != 32:
            raise AssertionError(f"{path}: expected 32bpp")
        stride = self.width * 4
        self.ink = []
        for y in range(self.height):
            source_y = self.height - 1 - y if raw_height > 0 else y
            row = []
            for x in range(self.width):
                at = offset + source_y * stride + x * 4
                row.append(sum(raw[at : at + 3]) < 384)
            self.ink.append(row)

    def ink_count(self, box: tuple) -> int:
        left, top, width, height = box
        return sum(
            self.ink[y][x]
            for y in range(top, top + height)
            for x in range(left, left + width)
        )


def changed_pixels(before: Bmp, after: Bmp):
    assert (before.width, before.height) == (476, 796)
    assert (after.width, after.height) == (476, 796)
    return [
        (x, y)
        for y in range(before.height)
        for x in range(before.width)
        if before.ink[y][x] != after.ink[y][x]
    ]


def in_approved_mask(x: int, y: int) -> bool:
    return any(
        left <= x < left + width and top <= y < top + height
        for left, top, width, height in MASKS.values()
    )


def assert_additive(before: Bmp, after: Bmp, fixture: str):
    changed = changed_pixels(before, after)
    outside = [(x, y) for x, y in changed if not in_approved_mask(x, y)]
    assert not outside, f"{fixture}: {len(outside)} changed pixels outside approved masks"
    return changed


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--base-measured", type=Path, required=True)
    parser.add_argument("--phase5b-measured", type=Path, required=True)
    parser.add_argument("--base-insufficient", type=Path, required=True)
    parser.add_argument("--phase5b-insufficient", type=Path, required=True)
    args = parser.parse_args()

    base_measured = Bmp(args.base_measured)
    measured = Bmp(args.phase5b_measured)
    base_insufficient = Bmp(args.base_insufficient)
    insufficient = Bmp(args.phase5b_insufficient)

    measured_changed = assert_additive(base_measured, measured, "measured")
    insufficient_changed = assert_additive(base_insufficient, insufficient, "insufficient")

    # The fifth star gains real solid ink on its left half while the complete
    # outline, including the unfilled right half, remains visible.
    star_left = measured.ink_count((355, 257, 11, 20))
    star_right = measured.ink_count((366, 257, 10, 20))
    assert star_left > star_right * 2, (star_left, star_right)
    assert star_right >= 10, "right-half outline disappeared"

    # A confident whole-book estimate occupies the adjacent group. The same
    # group remains pixel-identical to the baseline blank area without enough
    # evidence, while the former third-row group is removed in both states.
    adjacent = MASKS["adjacent_book_eta"]
    assert measured.ink_count(adjacent) > base_measured.ink_count(adjacent) + 300
    assert insufficient.ink_count(adjacent) == base_insufficient.ink_count(adjacent)
    assert insufficient.ink_count(MASKS["old_book_eta"]) < base_insufficient.ink_count(
        MASKS["old_book_eta"]
    )

    # The progress outline begins directly beneath the cover at y=466 and is
    # exactly 14 pixels thick; its six-pixel fill is centred inside it. The
    # existing focus ring then closes below it at y=484..485.
    assert measured.ink_count((18, 466, 220, 1)) >= 218
    assert measured.ink_count((18, 479, 220, 1)) >= 218
    assert measured.ink_count((18, 480, 220, 4)) <= 16
    assert measured.ink_count((15, 484, 226, 2)) >= 440

    print(
        "PASS "
        f"measured_changed={len(measured_changed)} "
        f"insufficient_changed={len(insufficient_changed)} "
        f"star_left={star_left} star_right={star_right}"
    )


if __name__ == "__main__":
    main()
