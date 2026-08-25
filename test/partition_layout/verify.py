#!/usr/bin/env python3
"""Validate the X4 Pro's fixed 16 MiB dual-OTA partition layout."""
import csv
from pathlib import Path

FLASH_END = 0x1000000
MIN_SPIFFS = 0x100000  # retain at least 1 MiB of optional internal filesystem capacity
rows = []
with Path('partitions.csv').open() as f:
    for raw in csv.reader(line for line in f if not line.lstrip().startswith('#')):
        name, typ, subtype, offset, size, *_ = (field.strip() for field in raw)
        rows.append((name, typ, subtype, int(offset, 0), int(size, 0)))

by_name = {row[0]: row for row in rows}
assert len(by_name) == len(rows), 'duplicate partition name'
previous_end = 0
for name, _, _, offset, size in sorted(rows, key=lambda row: row[3]):
    assert offset >= previous_end, f'{name} overlaps previous partition'
    assert offset + size <= FLASH_END, f'{name} exceeds 16 MiB flash'
    previous_end = offset + size
app0, app1 = by_name['app0'], by_name['app1']
assert app0[2] == 'ota_0' and app1[2] == 'ota_1', 'dual OTA slots required'
assert app0[4] == app1[4], 'rollback slots must be equal sized'
assert app0[3] + app0[4] == app1[3], 'OTA slots must be contiguous'
spiffs = by_name['spiffs']
assert app1[3] + app1[4] == spiffs[3], 'filesystem must follow OTA slots'
assert spiffs[4] >= MIN_SPIFFS, 'at least 1 MiB filesystem capacity required'
assert spiffs[3] + spiffs[4] == by_name['coredump'][3], 'filesystem must end at coredump'
assert by_name['coredump'][3] + by_name['coredump'][4] == FLASH_END
print(f'PASS dual OTA: app0/app1 each 0x{app0[4]:x} ({app0[4]} bytes)')
print(f'PASS filesystem: 0x{spiffs[4]:x} ({spiffs[4]} bytes)')
print('PASS no overlaps; coredump ends exactly at 0x1000000')
