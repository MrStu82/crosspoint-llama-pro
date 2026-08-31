# State — Upstream Safety/Reader Convergence

## Active prototype — Reader top spacing (2026-08-31)

- Base: `06c974e551ffd68f8ce6c56753377957c3d98630`; branch: `ui/reader-top-spacing-20260831`.
- Scope: code-native/simulator prototype only. Top reader progress fill begins at physical `y=0`; when the top
  text lane is enabled its layout starts one clear pixel below the progress fill. Bottom status geometry,
  viewport reservation, reader behavior, themes and sleep imagery remain unchanged.
- Gate: deterministic geometry/source contract plus one clean X4 Pro simulator capture. No firmware build or release.
- Stop after prototype evidence for Stuart visual approval.

## Current position

- **Phase 1:** complete. `a9f20903` adds explicit held X4 latch sequencing and candidate/running-image chip validation; focused host gate 20/20 PASS.

- **Phase:** 0 complete (planning/audit only).
- **Frozen implementation base:** `b529db98e4c7d83d28f9ade24d747dde98120efd`.
- **Frozen app:** `crosspoint-x4pro-v1.5.0-195-gb529db98e-app-0x10000.bin`.
- **No Phase-0 source changes or firmware build.**

## Evidence

- Raw GitHub PR metadata and changed-file lists: `.planning/evidence/upstream-pr-audit/`.
- Current-fork mapping/dedupe/conflict register: `.planning/UPSTREAM_PR_AUDIT.md`; host proof: `/workspace/agent/phase1-focused-host.log`.
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

Parent gate: approve Phase 2 power/performance implementation. No firmware build until final programme delivery.

## Resume file

`.planning/UPSTREAM_PR_AUDIT.md`; host proof: `/workspace/agent/phase1-focused-host.log`
