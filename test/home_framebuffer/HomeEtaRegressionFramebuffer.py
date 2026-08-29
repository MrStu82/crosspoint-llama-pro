#!/usr/bin/env python3
"""Pixel contract for stable Home ETA rows using the deterministic ETA fixture."""
import argparse
import struct
from pathlib import Path

PANEL_W, PANEL_H = 480, 800
RIGHT_X, RIGHT_EDGE = 260, 460
# ETA fixture uses a one-line Noto Sans 14 title, one-line author, no rating:
# y=104+31, metadataBottom=y+18, then Home's fixed 24px gap.
STAT_START, STAT_STEP, VALUE_OFFSET = 177, 58, 20


class Bmp:
    def __init__(self, path: Path):
        raw = path.read_bytes()
        assert raw[:2] == b"BM", path
        self.raw = raw
        self.off = struct.unpack_from("<I", raw, 10)[0]
        self.width, raw_h = struct.unpack_from("<ii", raw, 18)
        self.height = abs(raw_h)
        self.bottom_up = raw_h > 0
        assert struct.unpack_from("<H", raw, 28)[0] == 32
        self.stride = self.width * 4
        self.dx = (PANEL_W - self.width) // 2
        self.dy = (PANEL_H - self.height) // 2

    def ink(self, left, top, right, bottom):
        total = 0
        for py in range(max(0, top-self.dy), min(self.height, bottom-self.dy)):
            sy = self.height-1-py if self.bottom_up else py
            for px in range(max(0, left-self.dx), min(self.width, right-self.dx)):
                at = self.off + sy*self.stride + px*4
                total += sum(self.raw[at:at+3]) < 384
        return total


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--measured", type=Path, required=True)
    ap.add_argument("--no-data", type=Path, required=True)
    args = ap.parse_args()
    measured, no_data = Bmp(args.measured), Bmp(args.no_data)
    assert measured.width >= 470 and measured.height >= 790
    assert (measured.width, measured.height) == (no_data.width, no_data.height)

    label_counts = []
    measured_values = []
    no_data_values = []
    for row in range(3):
        y = STAT_START + row * STAT_STEP
        # drawText's y is the text baseline origin; glyph ink begins at the
        # face ascender. Keep label and value bands separate for both UI and
        # Caveat faces rather than treating y as a bitmap top coordinate.
        labels = (measured.ink(RIGHT_X, y+12, RIGHT_EDGE, y+32),
                  no_data.ink(RIGHT_X, y+12, RIGHT_EDGE, y+32))
        assert min(labels) >= 20, (row, labels)
        # Labels and geometry are identical regardless of ETA availability.
        assert labels[0] == labels[1], (row, labels)
        label_counts.append(labels[0])
        measured_values.append(measured.ink(RIGHT_X, y+40, RIGHT_EDGE, y+64))
        no_data_values.append(no_data.ink(RIGHT_X, y+32, RIGHT_EDGE, y+56))

    # Seeded 30s/page + exact remaining state must draw TIME READ, CHAPTER LEFT
    # and BOOK LEFT values. True no-rate state keeps both ETA rows and writes
    # CALIBRATING, which is intentionally much more than a thin em dash.
    assert all(v >= 20 for v in measured_values), measured_values
    assert no_data_values[1] >= 70 and no_data_values[2] >= 70, no_data_values

    chevron_y = STAT_START + 3*STAT_STEP + VALUE_OFFSET + 8
    measured_chevron = measured.ink(414, chevron_y-8, 460, chevron_y+15)
    no_data_chevron = no_data.ink(414, chevron_y-8, 460, chevron_y+15)
    assert measured_chevron >= 150 and measured_chevron == no_data_chevron, (
        measured_chevron, no_data_chevron)
    print("PASS Home ETA framebuffer labels=%s measured=%s no_data=%s chevron=%d" %
          (label_counts, measured_values, no_data_values, measured_chevron))


if __name__ == "__main__":
    main()
