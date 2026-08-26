# Two version identities, one of them undiagnostic

**Status:** open, deliberately not fixed. Raised 2026-08-26 while fixing the
`esp_app_desc_t` stamping defect. Do this on a build we are not in the middle of
delivering.

## The problem

This firmware reports its identity in two independent places, and they disagree.

**`CROSSPOINT_VERSION`** — the string on the device that Stuart reads off the
screen when he reports a fault. `scripts/git_branch.py` is a `pre:` script in
`[base]` that computes `<base>-dev-<branch>-<short-sha>` from git and injects it
as a `CPPDEFINES` entry. But it defers to any env that sets the macro itself in
`build_flags`, and `[env:x4pro]` does exactly that:

    -DCROSSPOINT_VERSION=\"${crosspoint.version}-llmp\"

`${crosspoint.version}` is the literal `1.5.0` from the `[crosspoint]` section,
hand-edited and last moved a long time ago. So every X4 Pro build ever made
reports `1.5.0-llmp`. It identifies the product line and nothing else — not the
commit, not the branch, not the day. When Stuart says "it's broken on 1.5.0-llmp"
that narrows the candidate builds to all of them.

**`esp_app_desc_t`** — the descriptor at file offset 0x20 of the app image, read
by `esptool image-info` as *App version* / *Project name*. Since
`scripts/stamp_app_desc.py` this is `git describe --always --dirty` and is
genuinely precise: `v1.5.0-185-g204b4ae1a` / `crosspoint-llama-pro`. It is also
asserted in the release gate, so it cannot silently regress again.

The precise identity is the one only we can see, over USB, with the binary in
hand. The vague one is the only identity the person actually reporting bugs can
read. That is exactly backwards.

## Recommended reconciliation

Make the two strings the same value from one source, and let the device show it.

1. Delete the `-DCROSSPOINT_VERSION` line from `[env:x4pro]`'s `build_flags`.
   That alone un-bypasses `git_branch.py`, which already does the right thing and
   is already wired into `[base]`.
2. Have `git_branch.py` and `stamp_app_desc.py` read the same value rather than
   each deriving their own. `stamp_app_desc.py` already prefers `version.txt`
   with a `git describe` fallback; give `git_branch.py` the same precedence and
   the two agree by construction, on the host (where the tarball has no `.git`
   and `version.txt` is written by the runner) and in a working clone alike.
3. Keep `-llmp` if the product-line marker is wanted — as a suffix on the real
   version, not as a replacement for it.

Do not solve this by teaching the runner script to write the string into two
places. That is the shape of defect this whole exercise removed.

## Cost of not doing it

Every device-side fault report has to be paired with a manual descriptor read
over USB before we know which commit Stuart is looking at. Cheap once, expensive
every time, and it silently fails the day nobody thinks to check.
