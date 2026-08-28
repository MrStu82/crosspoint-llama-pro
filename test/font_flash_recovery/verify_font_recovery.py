#!/usr/bin/env python3
"""Deterministic regression gate for generated built-in font recovery.

Compares the generated headers against a Git baseline without relying on a firmware
link map.  It proves that glyph metadata, decoded pixels, ligatures, and kerning
semantics are unchanged, then reports exact target payload-array bytes.
"""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
import zlib
from pathlib import Path


GENERATED = [
    *(f"notoserif_{size}_{style}.h" for size in (12, 14, 16, 18)
      for style in ("regular", "italic", "bold", "bolditalic")),
    *(f"notosans_{size}_{style}.h" for size in (12, 14, 16, 18)
      for style in ("regular", "italic", "bold", "bolditalic")),
    "notosans_8_regular.h",
    "ubuntu_10_regular.h",
    "ubuntu_10_bold.h",
    "ubuntu_12_regular.h",
    "ubuntu_12_bold.h",
]

UNCHANGED_FORK_FIXTURES = [
    "notosans_20_bold_digits.h",
    "notosans_40_bold_digits.h",
    "caveat_15_regular.h",
    "caveat_18_regular.h",
    "caveat_27_regular.h",
    "caveat_30_regular.h",
    "caveat_42_regular.h",
]

TYPE_BYTES = {
    "uint8_t": 1,
    "int8_t": 1,
    "uint16_t": 2,
    "EpdKernClassEntry": 3,
    "EpdGlyph": 16,
    "EpdUnicodeInterval": 12,
    "EpdFontGroup": 20,
    "EpdLigaturePair": 8,
}


def git_show(repo: Path, base: str, filename: str) -> str:
    path = f"lib/EpdFont/builtinFonts/{filename}"
    return subprocess.check_output(
        ["git", "show", f"{base}:{path}"], cwd=repo, text=True
    )


def strip_comments(body: str) -> str:
    body = re.sub(r"/\*.*?\*/", "", body, flags=re.S)
    return re.sub(r"//.*", "", body)


def arrays(text: str) -> dict[str, tuple[str, list[int] | list[list[int]]]]:
    # Unicode labels in generated comments can themselves be '{' or '}', so remove
    # comments before matching balanced initialiser braces.
    text = strip_comments(text)
    result = {}
    declaration = re.compile(
        r"static\s+(?:const|constexpr)\s+"
        r"(uint8_t|int8_t|uint16_t|EpdKernClassEntry|EpdGlyph|EpdUnicodeInterval|EpdFontGroup|EpdLigaturePair)\s+"
        r"(\w+)\s*\[[^]]*\]\s*=\s*\{"
    )
    for match in declaration.finditer(text):
        start = match.end()
        depth = 1
        i = start
        while depth and i < len(text):
            if text[i] == "{":
                depth += 1
            elif text[i] == "}":
                depth -= 1
            i += 1
        if depth:
            raise AssertionError(f"unterminated array {match.group(2)}")
        body = strip_comments(text[start:i - 1])
        typ, name = match.group(1), match.group(2)
        if "{" in body:
            entries = []
            for entry in re.findall(r"\{([^{}]*)\}", body, flags=re.S):
                entries.append([int(v, 0) for v in re.findall(r"-?0[xX][0-9A-Fa-f]+|-?\d+", entry)])
            result[name] = (typ, entries)
        else:
            values = [int(v, 0) for v in re.findall(r"-?0[xX][0-9A-Fa-f]+|-?\d+", body)]
            result[name] = (typ, values)
    return result


def suffix(a: dict, ending: str):
    found = [value for name, value in a.items() if name.endswith(ending)]
    return found[0] if found else None


def values(a: dict, ending: str):
    item = suffix(a, ending)
    return None if item is None else item[1]


def payload_bytes(a: dict) -> int:
    return sum(TYPE_BYTES[typ] * len(items) for typ, items in a.values())


def decoded_groups(a: dict) -> list[bytes]:
    bitmap = bytes(values(a, "Bitmaps") or [])
    groups = values(a, "Groups") or []
    decoded = []
    for offset, compressed_size, uncompressed_size, _glyph_count, _first_index in groups:
        raw = zlib.decompress(bitmap[offset:offset + compressed_size], -15)
        assert len(raw) == uncompressed_size
        decoded.append(raw)
    return decoded


def dense_kerning(a: dict) -> tuple[list[list[int]], list[list[int]], list[int]]:
    left_packed = values(a, "KernLeftClasses")
    right_packed = values(a, "KernRightClasses")
    if left_packed is not None:
        left = left_packed
        right = right_packed
    else:
        left = list(map(list, zip(values(a, "KernLeftCodepoints"), values(a, "KernLeftClassIds"))))
        right = list(map(list, zip(values(a, "KernRightCodepoints"), values(a, "KernRightClassIds"))))

    matrix = values(a, "KernMatrix")
    if matrix is not None:
        return left, right, matrix

    offsets = values(a, "KernRowOffsets")
    cols = values(a, "KernSparseCols")
    sparse_values = values(a, "KernSparseValues")
    rows = max(class_id for _cp, class_id in left)
    columns = max(class_id for _cp, class_id in right)
    matrix = [0] * (rows * columns)
    for row in range(rows):
        for pos in range(offsets[row], offsets[row + 1]):
            matrix[row * columns + cols[pos]] = sparse_values[pos]
    return left, right, matrix


def normalised_groups(a: dict):
    # Compression offsets and sizes may change; decode layout must not.
    return [[uncompressed, count, first] for _off, _compressed, uncompressed, count, first
            in (values(a, "Groups") or [])]


def verify_generated(filename: str, before: str, after: str) -> dict[str, int]:
    old, new = arrays(before), arrays(after)
    for ending in ("Glyphs", "Intervals", "LigaturePairs", "GlyphToGroup"):
        assert values(old, ending) == values(new, ending), f"{filename}: changed {ending}"
    assert normalised_groups(old) == normalised_groups(new), f"{filename}: changed group layout"
    assert decoded_groups(old) == decoded_groups(new), f"{filename}: decoded bitmap mismatch"
    assert dense_kerning(old) == dense_kerning(new), f"{filename}: kerning mismatch"
    return {"before": payload_bytes(old), "after": payload_bytes(new)}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--base", default="f044130b7e2f0468740d71eba9223b3b6c56c316")
    parser.add_argument("--json-output", type=Path)
    args = parser.parse_args()
    font_dir = args.repo / "lib/EpdFont/builtinFonts"

    report = {"base": args.base, "fonts": {}, "before": 0, "after": 0}
    for filename in GENERATED:
        metric = verify_generated(filename, git_show(args.repo, args.base, filename),
                                  (font_dir / filename).read_text())
        report["fonts"][filename] = metric
        report["before"] += metric["before"]
        report["after"] += metric["after"]

    for filename in UNCHANGED_FORK_FIXTURES:
        assert git_show(args.repo, args.base, filename) == (font_dir / filename).read_text(), \
            f"{filename}: fork-only generated font changed"

    report["recovered"] = report["before"] - report["after"]
    report["verified_generated_fonts"] = len(GENERATED)
    report["verified_unchanged_fork_fonts"] = len(UNCHANGED_FORK_FIXTURES)
    if args.json_output:
        args.json_output.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({key: report[key] for key in
                      ("verified_generated_fonts", "verified_unchanged_fork_fonts", "before", "after", "recovered")},
                     sort_keys=True))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (AssertionError, subprocess.CalledProcessError) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        raise SystemExit(1)
