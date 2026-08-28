# Roadmap — Upstream Safety/Reader Convergence

## Phase 0 — audit and bounded plan (complete; no code/build)

Evidence captured in `.planning/evidence/upstream-pr-audit/` and the exact mapping in
`.planning/UPSTREAM_PR_AUDIT.md`. The frozen base is v195 / `b529db98`.

## Phase 1 — safety and data integrity

Selected upstream: #3215, #2962, #3034, #2880, #2834.

- Dedupe the already-present semantic equivalents for resume rollback (#2962), file-list render
  race (#3034), and credential-store threading (#2834); retain their current regression tests.
- Audit `PersistableStore` locking together with fork-local `HardcoverRating` atomic storage;
  prove no lock-order or write-window regression.
- Port only missing X4 latch and cross-chip image validation invariants after board-specific
  inspection. Host tests: mocked latch sequencing, image header/chip rejection, concurrent
  credential/Hardcover store operations.

## Phase 2 — power, wake, USB and memory performance

Selected upstream: #3233, #3191, #3223, #3144, #3222, #3115, #3005.

- Treat FreeInk SDK changes (#3115/#3191/#3223) as board-policy adaptations, not submodule
  updates. Test X4 wake/USB detection with mocks before any hardware build.
- Rebase low-memory CSS retry (#3005) into the fork's advanced CSS parser; do not replace it.
- Add style-aware font prewarm (#3222) with its host target; isolate #3144 generated-font work,
  measuring flash delta and preserving renderer outputs.
- Keep low-frontlight minimum and KOReader Wi-Fi awake behaviour bounded to their existing
  settings/sync paths.

## Phase 3 — reader, EPUB and dictionary correctness

Selected upstream: #2959, #3001, #2696, #2836, #3154, #3153, #2654, #3037.

- Retain already-present #2959, #3001 and #3153 equivalents; add missing fixtures only where
  the existing test does not prove the selected upstream invariant.
- Integrate image decoding, EPUB metadata, CSS retry, table layout, synonyms and HTML dictionary
  rendering as small changes against current parser/reader APIs.
- Add Extra Wide line spacing only through the existing text-settings enum/i18n path.

## Phase 4 — transparent sleep/boot bundle

Selected upstream: #2937, #2974, #2943, #3009, #3119, #3093, #2989.

- Preserve fork-local Home retained-frame semantics and current #3009/#3093 equivalents.
- Implement overlay formats, wake-release swallowing and boot suppression together only where
  their repaint/lifetime invariants require it. Regression tests cover retained image, alpha,
  cache release and wake release.

## Gate — UI prototype approval (parent required)

No control-centre or visual implementation proceeds until a touch-first X4 Pro prototype is
reviewed and explicitly approved. No firmware build is produced for the prototype phase.

## Phase 5 — approved visual/settings work

Selected upstream: #3156, #3080, #3115 (UI aspect).

- Control centre, password reveal and frontlight minimum proceed only from the approved prototype.
- Password reveal must be transient UI state only: no plaintext persistence/logging.
- Deterministic touch/layout tests are required.

## Final verification/delivery

After Phase 5 approval and all phase host gates: one clean X4 Pro app build, descriptor stamp,
esptool validation, artifact hash/size, and simulator evidence. No interim firmware images.
