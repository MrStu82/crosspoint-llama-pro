#!/usr/bin/env python3
"""Production-simulator framebuffer contract for the approved Reader Tools overlay."""
import argparse
import struct
from pathlib import Path

PANEL = (34, 145, 446, 655)
OVERLAY_BOUNDS = (34, 145, 453, 662)  # includes the approved 7 px shadow
HEADER_HEIGHT = 84
ROW_HEIGHT = 70


class Bmp:
    def __init__(self, path: Path):
        raw = path.read_bytes()
        assert raw[:2] == b"BM", path
        self.raw = raw
        self.off = struct.unpack_from("<I", raw, 10)[0]
        self.width, raw_h = struct.unpack_from("<ii", raw, 18)
        self.height = abs(raw_h)
        self.bottom_up = raw_h > 0
        assert self.width >= 470 and self.height >= 790, path
        assert struct.unpack_from("<H", raw, 28)[0] == 32
        self.stride = self.width * 4
        self.dx = (480 - self.width) // 2
        self.dy = (800 - self.height) // 2

    def pixel(self, x, y):
        px, py = x - self.dx, y - self.dy
        if px < 0 or px >= self.width or py < 0 or py >= self.height:
            return None
        sy = self.height - 1 - py if self.bottom_up else py
        at = self.off + sy * self.stride + px * 4
        return self.raw[at:at + 3]

    def ink(self, left, top, right, bottom):
        pixels = (self.pixel(x, y) for y in range(top, bottom) for x in range(left, right))
        return sum(pixel is not None and sum(pixel) < 384 for pixel in pixels)


def main():
    ap = argparse.ArgumentParser()
    for name in ("before", "after_short", "overlay", "dismissed"):
        ap.add_argument("--" + name.replace("_", "-"), dest=name, type=Path, required=True)
    args = ap.parse_args()
    before, page, overlay, dismissed = [Bmp(getattr(args, n)) for n in ("before", "after_short", "overlay", "dismissed")]

    assert any(before.pixel(x, y) != page.pixel(x, y) for y in range(800) for x in range(480)), "centre tap did not turn page"
    assert all(page.pixel(x, y) == dismissed.pixel(x, y) for y in range(800) for x in range(480)), "outside dismissal did not restore reader"

    left, top, right, bottom = OVERLAY_BOUNDS
    for y in range(800):
        for x in range(480):
            if not (left <= x < right and top <= y < bottom):
                if page.pixel(x, y) is not None:
                    assert page.pixel(x, y) == overlay.pixel(x, y), (x, y)

    panel_left, panel_top, panel_right, _ = PANEL
    title_ink = overlay.ink(panel_left + 40, panel_top + 18, panel_right - 40, panel_top + HEADER_HEIGHT - 12)
    assert title_ink >= 100, title_ink
    row_ink = []
    for row in range(6):
        row_top = panel_top + HEADER_HEIGHT + row * ROW_HEIGHT
        ink = overlay.ink(panel_left + 20, row_top + 10, panel_right - 20, row_top + ROW_HEIGHT - 10)
        assert ink >= 60, (row, ink)
        row_ink.append(ink)
    shadow_ink = overlay.ink(panel_right, panel_top + 20, panel_right + 7, panel_top + 500)
    assert shadow_ink >= 2500, shadow_ink
    print(f"PASS Reader Tools framebuffer centre_tap=unchanged outside_pixels=unchanged rows={row_ink} title={title_ink} shadow={shadow_ink} dismiss=restored")


if __name__ == "__main__":
    main()
