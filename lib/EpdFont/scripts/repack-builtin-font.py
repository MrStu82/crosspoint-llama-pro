#!/usr/bin/env python3
"""Losslessly repack an existing generated built-in font header.

This is the migration path for generated headers whose source configuration has
since evolved: it recompresses the existing decoded glyph payload with Zopfli and
converts dense kerning to split/CSR form without re-rasterising any glyph.
"""

from __future__ import annotations

import argparse
import re
import zlib
from pathlib import Path

import zopfli.zlib


def numbers(body: str) -> list[int]:
    body = re.sub(r"/\*.*?\*/", "", body, flags=re.S)
    body = re.sub(r"//.*", "", body)
    return [int(value, 0) for value in re.findall(r"-?0[xX][0-9A-Fa-f]+|-?\d+", body)]


def array_match(text: str, declaration: str) -> re.Match:
    match = re.search(declaration + r"\s*=\s*\{(.*?)\};", text, flags=re.S)
    if not match:
        raise ValueError(f"array not found: {declaration}")
    return match


def raw_deflate_zopfli(data: bytes) -> bytes:
    wrapped = zopfli.zlib.compress(data)
    raw = wrapped[2:-4]
    if zlib.decompress(raw, -15) != data:
        raise RuntimeError("Zopfli raw-DEFLATE round-trip failed")
    return raw


def format_scalar_array(typ: str, name: str, values: list[int], width: int = 16) -> str:
    lines = []
    for pos in range(0, len(values), width):
        chunk = values[pos:pos + width]
        if typ == "uint16_t":
            rendered = ", ".join(f"0x{value:04X}" for value in chunk)
        elif typ == "int8_t":
            rendered = ", ".join(f"{value:4d}" for value in chunk)
        else:
            rendered = ", ".join(f"{value:3d}" for value in chunk)
        lines.append(f"    {rendered},")
    return f"static const {typ} {name}[] = {{\n" + "\n".join(lines) + "\n};"


def split_initializer(body: str) -> list[str]:
    return [value.strip() for value in body.split(",") if value.strip()]


def repack(path: Path) -> None:
    text = path.read_text()
    bitmap_match = array_match(text, r"static const uint8_t (\w+)Bitmaps\[\d+\]")
    font_name = bitmap_match.group(1)
    bitmap = bytes(numbers(bitmap_match.group(2)))

    groups_match = re.search(
        rf"static const EpdFontGroup {re.escape(font_name)}Groups\[\]\s*=\s*\{{(.*?)\}};",
        text, flags=re.S)
    if groups_match:
        groups = [numbers(entry) for entry in re.findall(r"\{([^{}]*)\}", groups_match.group(1), flags=re.S)]
        compressed = bytearray()
        new_groups = []
        for offset, size, uncompressed_size, glyph_count, first_glyph in groups:
            decoded = zlib.decompress(bitmap[offset:offset + size], -15)
            if len(decoded) != uncompressed_size:
                raise ValueError(f"{font_name}: invalid uncompressed group size")
            encoded = raw_deflate_zopfli(decoded)
            new_groups.append((len(compressed), len(encoded), uncompressed_size, glyph_count, first_glyph))
            compressed.extend(encoded)

        bitmap_lines = []
        for pos in range(0, len(compressed), 16):
            bitmap_lines.append("    " + ", ".join(f"0x{value:02X}" for value in compressed[pos:pos + 16]) + ",")
        bitmap_block = (f"static const uint8_t {font_name}Bitmaps[{len(compressed)}] = {{\n"
                        + "\n".join(bitmap_lines) + "\n};")
        text = text[:bitmap_match.start()] + bitmap_block + text[bitmap_match.end():]

        groups_match = re.search(
            rf"static const EpdFontGroup {re.escape(font_name)}Groups\[\]\s*=\s*\{{(.*?)\}};",
            text, flags=re.S)
        group_lines = [f"    {{ {offset}, {size}, {raw_size}, {count}, {first} }},"
                       for offset, size, raw_size, count, first in new_groups]
        groups_block = (f"static const EpdFontGroup {font_name}Groups[] = {{\n"
                        + "\n".join(group_lines) + "\n};")
        text = text[:groups_match.start()] + groups_block + text[groups_match.end():]

    left_match = re.search(
        rf"static const EpdKernClassEntry {re.escape(font_name)}KernLeftClasses\[\]\s*=\s*\{{(.*?)\}};",
        text, flags=re.S)
    if left_match:
        right_match = array_match(
            text, rf"static const EpdKernClassEntry {re.escape(font_name)}KernRightClasses\[\]")
        matrix_match = array_match(text, rf"static const int8_t {re.escape(font_name)}KernMatrix\[\]")
        left = [numbers(entry) for entry in re.findall(r"\{([^{}]*)\}", left_match.group(1), flags=re.S)]
        right = [numbers(entry) for entry in re.findall(r"\{([^{}]*)\}", right_match.group(1), flags=re.S)]
        matrix = numbers(matrix_match.group(1))
        rows = max(class_id for _cp, class_id in left)
        columns = max(class_id for _cp, class_id in right)
        if len(matrix) != rows * columns:
            raise ValueError(f"{font_name}: dense kerning dimensions do not match matrix")

        row_offsets: list[int] = []
        sparse_cols: list[int] = []
        sparse_values: list[int] = []
        for row in range(rows):
            row_offsets.append(len(sparse_cols))
            for column, value in enumerate(matrix[row * columns:(row + 1) * columns]):
                if value:
                    sparse_cols.append(column)
                    sparse_values.append(value)
        row_offsets.append(len(sparse_cols))
        if len(sparse_cols) > 0xFFFF or columns > 256:
            raise ValueError(f"{font_name}: sparse kerning indices exceed storage width")

        blocks = [
            format_scalar_array("uint16_t", f"{font_name}KernLeftCodepoints", [cp for cp, _ in left], 12),
            format_scalar_array("uint8_t", f"{font_name}KernLeftClassIds", [class_id for _, class_id in left]),
            format_scalar_array("uint16_t", f"{font_name}KernRightCodepoints", [cp for cp, _ in right], 12),
            format_scalar_array("uint8_t", f"{font_name}KernRightClassIds", [class_id for _, class_id in right]),
            format_scalar_array("uint16_t", f"{font_name}KernRowOffsets", row_offsets),
            format_scalar_array("uint8_t", f"{font_name}KernSparseCols", sparse_cols),
            format_scalar_array("int8_t", f"{font_name}KernSparseValues", sparse_values),
        ]
        text = text[:left_match.start()] + "\n\n".join(blocks) + text[matrix_match.end():]

        init_match = re.search(
            rf"static (const|constexpr) EpdFontData {re.escape(font_name)}\s*=\s*\{{(.*?)\}};",
            text, flags=re.S)
        if not init_match:
            raise ValueError(f"{font_name}: EpdFontData initializer not found")
        old = split_initializer(init_match.group(2))
        if len(old) != 20:
            raise ValueError(f"{font_name}: expected 20 EpdFontData fields, got {len(old)}")
        new = [
            *old[:11],
            "nullptr", "nullptr", "nullptr",
            *old[14:20],
            "nullptr", "nullptr", "nullptr",
            f"{font_name}KernLeftCodepoints", f"{font_name}KernLeftClassIds",
            f"{font_name}KernRightCodepoints", f"{font_name}KernRightClassIds",
            f"{font_name}KernRowOffsets", f"{font_name}KernSparseCols", f"{font_name}KernSparseValues",
        ]
        initializer = (f"static {init_match.group(1)} EpdFontData {font_name} = {{\n"
                       + "".join(f"    {value},\n" for value in new) + "};")
        text = text[:init_match.start()] + initializer + text[init_match.end():]

    path.write_text(text)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("headers", nargs="+", type=Path)
    args = parser.parse_args()
    for header in args.headers:
        repack(header)
        print(f"Repacked {header}")


if __name__ == "__main__":
    main()
