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

- Trantor host suite: 165/165 passed (`/home/skippy/build/reader-host-test.log`).
- Native `simulator_x4_pro`: SUCCESS (`/home/skippy/build/reader-simulator-build.log`).
- Input sequence: Home `TAP:150,590` → Reader tab `TAP:150,105` → Text Settings `TAP:110,160` → Style tab `TAP:280,325`.
- Captures:
  - `/workspace/agent/reader-options-simulator.png` — built-in Noto Serif/Noto Sans/Lexend Deca/Bitter.
  - `/workspace/agent/reader-style-simulator.png` — explicit Focus Reading, Guide Dots, Force Paragraph Indents and Hyphenation On/Off rows.
- Simulator is the actual product project, pinned through `platformio.ini`; destructive USB-MSC is intentionally unavailable while settings/render paths stay real.
