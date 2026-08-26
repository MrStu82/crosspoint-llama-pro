#!/usr/bin/env python3
"""Pixel-level guards for the X4 Pro Home acceptance fixtures.

The simulator writes a 32-bit bottom-up BMP.  These checks deliberately avoid
OCR and third-party image packages: they assert the visible ink for the defects
that have actually regressed here -- a cover stretched to its lane, a middle
stat no code path could populate, a missing Stats affordance, and metadata
clipped by a too-narrow right column.
"""

import argparse
import struct
from pathlib import Path

# Layout constants mirrored from HomeActivity.cpp / InkPointShell.h.  Kept
# named rather than inlined so a future layout change reads as a deliberate
# edit here instead of a magic-number hunt.
PANEL_WIDTH = 480
PANEL_HEIGHT = 800
HEADER_BOTTOM = 94
CONTENT_TOP = 104
COVER_LANE = (20, CONTENT_TOP, 220, 434)  # x, y, w, h -- the maximum, not the frame
COVER_RIGHT = 240
RIGHT_X = 260
RIGHT_EDGE = 460
STAT_LABEL_TOP = 334
STAT_STEP = 58
STAT_VALUE_OFFSET = 20
CHEVRON_CY = 520


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
        # The simulator writes the viewable area, not the full panel: it drops
        # the bezel margin from every edge.  Every constant below is a panel
        # coordinate, so translate once here rather than pre-shifting each one.
        self.dx = (PANEL_WIDTH - self.width) // 2
        self.dy = (PANEL_HEIGHT - self.height) // 2

    def ink_count(self, left: int, top: int, right: int, bottom: int) -> int:
        left, right = left - self.dx, right - self.dx
        top, bottom = top - self.dy, bottom - self.dy
        count = 0
        for y in range(max(0, top), min(self.height, bottom)):
            source_y = self.height - 1 - y if self.bottom_up else y
            for x in range(max(0, left), min(self.width, right)):
                at = self.offset + source_y * self.stride + x * 4
                # Black/white simulator framebuffer; tolerate channel noise.
                if sum(self.raw[at : at + 3]) < 384:
                    count += 1
        return count


def stat_value_box(row: int) -> tuple:
    top = STAT_LABEL_TOP + row * STAT_STEP + STAT_VALUE_OFFSET
    return (RIGHT_X, top, RIGHT_X + 110, top + 26)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--populated", type=Path, required=True)
    parser.add_argument("--unavailable", type=Path, required=True)
    parser.add_argument("--long", type=Path, required=True)
    parser.add_argument("--small-cover", type=Path, required=True)
    # INK-04 is global, so it is proven on a second screen whose content starts
    # highest: Settings puts its tab strip immediately under the heading.
    parser.add_argument("--settings", type=Path, required=True)
    args = parser.parse_args()

    populated = Bmp(args.populated)
    unavailable = Bmp(args.unavailable)
    long_title = Bmp(args.long)
    small_cover = Bmp(args.small_cover)
    for image in (populated, unavailable, long_title, small_cover):
        assert image.width >= 470 and image.height >= 790, (image.width, image.height)

    # INK-04: the Caveat page title owns everything up to and including
    # kHeaderBottom (its lowest descender row, measured at y=94), so the band
    # between that row and kContentTop must be clear of any screen content.
    title_gap = populated.ink_count(RIGHT_X, HEADER_BOTTOM + 1, RIGHT_EDGE, CONTENT_TOP - 1)
    assert title_gap == 0, f"content ink inside the heading's descender band: {title_gap}"
    settings_gap = Bmp(args.settings).ink_count(0, HEADER_BOTTOM + 1, PANEL_WIDTH, CONTENT_TOP - 1)
    assert settings_gap == 0, f"Settings content inside the heading's descender band: {settings_gap}"

    # INK-02: the whole title has to live in the widened right column, with no
    # ink escaping its right edge into the bezel margin.
    title_ink = populated.ink_count(RIGHT_X, CONTENT_TOP, RIGHT_EDGE, 230)
    assert title_ink >= 120, title_ink
    assert populated.ink_count(RIGHT_EDGE, CONTENT_TOP, 480, 230) == 0, "title ink crossed right-column edge"
    assert long_title.ink_count(RIGHT_EDGE, CONTENT_TOP, 480, 260) == 0, "long title crossed right-column edge"
    long_bands = [long_title.ink_count(RIGHT_X, top, RIGHT_EDGE, top + 22) for top in range(CONTENT_TOP, 240, 22)]
    assert sum(count >= 20 for count in long_bands) >= 3, long_bands

    # INK-02/INK-05: with reading stats and progress present, TIME READ and
    # PROGRESS both carry a real numeric value.  An em dash is a single thin
    # rule, so a genuine value is several times its ink -- which is precisely
    # the difference the old CHAPTER LEFT row could never show.
    dashes = [unavailable.ink_count(*stat_value_box(row)) for row in range(3)]
    assert all(8 <= count <= 60 for count in dashes), dashes
    values = [populated.ink_count(*stat_value_box(row)) for row in range(2)]
    assert all(count > max(dashes) * 2 for count in values), (values, dashes)

    # INK-03: three right-pointing triangles beneath the stat block, hard
    # right-aligned, and nothing else on that row.
    chevron = populated.ink_count(424, CHEVRON_CY - 8, RIGHT_EDGE + 4, CHEVRON_CY + 8)
    assert chevron >= 60, chevron
    assert populated.ink_count(RIGHT_X, CHEVRON_CY - 8, 420, CHEVRON_CY + 8) == 0, "stray ink left of the chevron"

    # INK-01: a cover smaller than the lane is drawn at its own size, hard
    # right-aligned to the lane's right edge, with its focus ring hugging it.
    # The lane to its left and below it must be empty -- that emptiness is the
    # whole defect: it used to be filled by a stretched cover.
    small_w, small_h = 120, 180
    drawn_left = COVER_RIGHT - small_w
    cover_ink = small_cover.ink_count(drawn_left, CONTENT_TOP, COVER_RIGHT, CONTENT_TOP + small_h)
    assert cover_ink >= 200, cover_ink
    left_slack = small_cover.ink_count(COVER_LANE[0], CONTENT_TOP, drawn_left - 4, COVER_LANE[1] + COVER_LANE[3])
    assert left_slack == 0, f"ink in the lane left of the drawn cover: {left_slack}"
    below_slack = small_cover.ink_count(drawn_left - 4, CONTENT_TOP + small_h + 14, COVER_RIGHT + 4, 540)
    assert below_slack == 0, f"ink in the lane below the drawn cover: {below_slack}"
    # Both rings are derived from the drawn size, so their left stroke reports
    # it: the full-size cover still fills the lane's width (ring at x=17), the
    # small one does not (ring at x=117).  A fit-down, not a blanket shrink.
    wide_ring = populated.ink_count(COVER_LANE[0] - 3, 200, COVER_LANE[0] - 1, 300)
    assert wide_ring >= 90, f"full-size cover no longer fills the lane width: {wide_ring}"
    small_ring = small_cover.ink_count(drawn_left - 3, 150, drawn_left - 1, 250)
    assert small_ring >= 90, f"small cover's frame is not hugging it: {small_ring}"

    print(
        f"PASS title_ink={title_ink} long_bands={long_bands} dashes={dashes} "
        f"values={values} chevron={chevron} cover_ink={cover_ink}"
    )


if __name__ == "__main__":
    main()
