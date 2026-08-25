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

## 2026-08-25 final safe SD-font revision

Partition expansion was explicitly cancelled. `partitions.csv` is byte-identical
to the original dual-OTA table (two 0x640000 slots, 0x360000 SPIFFS, coredump).
Lexend Deca and Bitter are now declared in the existing downloadable `.cpfont`
pipeline at 10/12/14/16 pt with regular/bold/italic/bold-italic styles. The small
bounded built-ins remain only as offline preview/fallback; broad Latin/Cyrillic
coverage is carried on SD rather than consuming OTA app space.
