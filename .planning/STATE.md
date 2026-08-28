# State — Upstream Safety/Reader Convergence

## Current position

- **Phase:** 0 complete (planning/audit only).
- **Frozen implementation base:** `b529db98e4c7d83d28f9ade24d747dde98120efd`.
- **Frozen app:** `crosspoint-x4pro-v1.5.0-195-gb529db98e-app-0x10000.bin`.
- **No Phase-0 source changes or firmware build.**

## Evidence

- Raw GitHub PR metadata and changed-file lists: `.planning/evidence/upstream-pr-audit/`.
- Current-fork mapping/dedupe/conflict register: `.planning/UPSTREAM_PR_AUDIT.md`.
- All 29 selected PRs were queried; the evidence records PR head/base SHAs, merge status and
  upstream changed-file shape.

## Dedupe findings already proven at frozen HEAD

- #2962 resume rollback: current equivalent `cb4d6ce72`.
- #3034 file-list render race: current equivalent `f69dbd3c5`.
- #2959 image decode/viewport: current equivalent `961629fc4`.
- #3001 EPUB metadata: current equivalent `7e5c98c6e`.
- #2834 credential-store threading: current equivalent `0437c5183`; Phase 1 extends its audit to
  fork-local Hardcover persistence rather than re-importing it.
- #3009 retained sleep-image clear: current equivalent `3f3aa504e`.
- #3093 SD font-cache release before sleep decode: current equivalent `9f8689fbe`.
- #3153 dictionary heading breaks: current equivalent `6984dd05c`.

## Next actionable item

Parent gate: approve opening Phase 1 safety/data implementation from the Phase-0 mapping. The
first action is source/test design for #3215/#2880 plus the `PersistableStore`/Hardcover locking
audit; no firmware build until final programme delivery.

## Resume file

`.planning/UPSTREAM_PR_AUDIT.md`
