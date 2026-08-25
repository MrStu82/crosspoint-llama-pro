# Reader convergence gate — 2026-08-25

## Frozen product line

- Base: `6e872b0531452b3e4a3e50651e1ce937d71d2610`
- CrossInk audit source: `cab4f24922f05811e7f44be1057f62ea2d978c52`
- Simulator source: `d07c68104f1de8e62a992e46250feeb5c6a22290`

## Scope proof

- Compatible upstream fixes were applied as focused commits; non-applicable fixes remain explicitly marked in `audits/crosspoint-reader-upstream-crossink-audit.md`.
- CrossInk-derived Lexend Deca/Bitter assets retain OFL attribution in `THIRD_PARTY_NOTICES.md` and `licenses/OFL-1.1.txt`.
- Requested 10/12/14/16 point families, limited symbol fallback, black redaction, Guide Dots, forced indents and pinned sleep favourite are native to the current architecture.
- Existing focus/Bionic reading, strikethrough, two-pixel underline and horizontal rules are retained. Simple tables retain the existing bounded, readable per-cell `Row N, Cell N` rendering rather than importing CrossInk's broader render-mode stack.

## Deterministic host proof

- Trantor host suite on the final candidate: 165/165 passed
  (`/home/skippy/build/reader-v2-host-test.log`).
- Native `simulator_x4_pro` on the final candidate: SUCCESS
  (`/home/skippy/build/reader-v2-simulator-build.log`).
- Input sequence: Home `TAP:150,590` → Reader tab `TAP:150,105` → Text Settings `TAP:110,160` → Style tab `TAP:280,325`.
- Captures:
  - `/workspace/agent/reader-options-simulator.png` — built-in Noto Serif/Noto Sans/Lexend Deca/Bitter.
  - `/workspace/agent/reader-style-simulator.png` — explicit Focus Reading, Guide Dots, Force Paragraph Indents and Hyphenation On/Off rows.
- Simulator is the actual product project, pinned through `platformio.ini`; destructive USB-MSC is intentionally unavailable while settings/render paths stay real.

## X4 Pro partition correction

The first implementation link exceeded the existing OTA-safe app partition by
962,374 bytes. The corrected generator keeps all 32 Lexend Deca/Bitter
family/size/style variants, but bounds the optional built-in coverage to
Western prose and ten explicitly requested miscellaneous glyphs. Broader
language and emoji coverage remains available through the existing SD
`.cpfont` system and the built-in Noto families. The committed source TTFs and
`convert-crossink-reader-fonts.sh` make the generated headers reproducible;
an immediate regeneration produced a clean worktree.

Final-candidate simulator captures:

- `/workspace/agent/reader-options-final.png` — SHA-256
  `6e776d5da69d02e8165e5bcd196ecbd35835d2023e0073d5d36840758ed4d21e`.
- `/workspace/agent/reader-style-final.png` — SHA-256
  `fe321be822d6eabbe000725d33a3d7a2ee04516dfc5aeac3843edd20b4103dd0`.

## 2026-08-25 approved dual-OTA partition expansion

Stuart chose partition expansion after the full approved font set exceeded the old
0x640000-byte OTA slot. The controlled layout keeps two equal rollback-capable
OTA slots and the terminal coredump partition:

- app0: `0x010000..0x750000` (`0x740000`, 7,602,176 bytes)
- app1: `0x750000..0xe90000` (`0x740000`, 7,602,176 bytes)
- spiffs: `0xe90000..0xff0000` (`0x160000`, 1,441,792 bytes)
- coredump: `0xff0000..0x1000000` (`0x10000`, 65,536 bytes)

The firmware's user content/settings storage is the SD-backed `HalStorage`; no
SPIFFS partition image is built. The retained 1.375 MiB internal filesystem is
still over 17x the OEM X4 Pro table's documented 0x14000 SPIFFS allocation.
`test/partition_layout/verify.py` deterministically rejects overlap, unequal or
missing OTA slots, less than 1 MiB filesystem capacity, and layouts not ending
at the 16 MiB flash boundary.
