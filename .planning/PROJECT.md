# CrossPoint X4 Pro — Upstream Safety/Reader Convergence

## Current mandate

Integrate only the selected upstream fixes into the frozen X4 Pro fork base. This is
**selective source-level adoption**, never a wholesale upstream rebase.

## Frozen base

- Source: `b529db98e4c7d83d28f9ade24d747dde98120efd`
- Release app: `crosspoint-x4pro-v1.5.0-195-gb529db98e-app-0x10000.bin`
- This base includes the approved Hardcover NestingLimit/touch work and Home solid-star/
  quote-centre correction. Those behaviours are regression constraints for every phase.

## Goals

1. Eliminate safety/data-loss/cross-device risks before feature work.
2. Improve power, wake, USB and font/memory performance without silently changing board policy.
3. Add reader/dictionary functionality through bounded, testable imports.
4. Stop before visual control-centre/frontlight work for a parent UI-prototype approval gate.
5. Run host tests for each implementation phase; build **one** final X4 Pro app only after that gate.

## Non-goals

- No wholesale rebase, SDK bump, generated-font refresh, or firmware build in Phase 0.
- No duplicate implementation where the exact semantic fix already exists in this fork.
- No change to Hardcover matching, storage, confirmation or touch flow except the dedicated
  credential-concurrency audit in Phase 1.

## Integration rules

- Preserve fork-local APIs and tests; translate upstream intent into the current file shape.
- Each imported behaviour gets a deterministic regression test before it is considered done.
- Generated assets (`freeink-sdk`, built-in fonts) are isolated and hash/size-accounted.
- A conflicting upstream patch is not cherry-picked blindly; its invariant is re-proven against
  the fork's implementation first.
