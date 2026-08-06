# Llama mods port — items needing Stuart's real device to validate

Running list, updated as each mod lands. Nothing here has been checked on hardware yet —
all verification so far is build/static-analysis only (`pio run`, `pio check`, clang-format).

## Item 1 — chapter progress bar
- No changes were needed (already present in Pro). Nothing to validate.

## Item 2 — cover progress indicator (commit 3cd566e)
- Visual placement/sizing of the progress overlay on the book cover thumbnail.

## Item 3 — reader stats (commits ddafdf2, be8f944)
- Stats screen layout: cover thumbnail render, book/chapter progress bars, Today/All-Time/
  All-Items trio/duo stat rows — never seen rendered, only compiled.
- Menu entry ("Stats") appears correctly in the Home menu list, in the right position, with
  the right icon.
- Reading-time tracking accuracy: open a book, read for a known duration, confirm the
  Today/All-Time reading-time stat matches (validates `sessionStartTime`/`addReadingTimeSeconds`
  wiring in all three readers).
- Pages-read counter increments correctly on real page turns (Epub, Txt, Xtc).
- Books-opened counter increments once per book open, not per re-entry glitch.
- Books-finished counter increments exactly once when a book is read to the end (Epub via
  spine-index crossing, Xtc via page-count bounds check) — not double-counted on repeated
  EOB screen renders.
- Total-books-on-device count (`countEpubsRecursively`) matches the real SD card contents.

## Item 4 — Deep Mines (commit 024a00f, on `main`)
Full 9-file port (dungeon generator, renderer, save/state, menu, title, gameplay
loop). Compiled and static-analysis clean, never run — same caveat as items 1-3,
but this feature also depends on hardware pieces (buttons, redraw speed) that were
themselves only ever a hypothesis until Stuart's device confirms them (see the
[Pro hardware surface](#pro-hardware-surface) section below). Nothing below can be
skipped by assuming a clean compile means it works.

- **Reach the feature at all.** From Home, confirm a "Deep Mines" (or equivalent)
  menu entry appears, opens the glyph title screen, and the title screen advances
  to the gameplay loop. If nothing appears, or it hangs on title, this is a
  complete port failure, not a cosmetic one — report back before testing anything
  else below.
- **Input mapping — the Pro's two nav buttons only.** The legacy fork this was
  ported from had a different button layout. On the Pro, gameplay (movement,
  menu selection, confirm/back) has to work using only Left (GPIO0) and Right
  (GPIO7), plus the GT911 Home key — there is no D-pad, no four-way ladder. Play
  through a few dungeon moves and confirm every action the game expects (move in
  more than one direction, select a menu item, confirm, back out) is actually
  reachable with just those three inputs. If a legacy control scheme assumed more
  buttons than the Pro has, some action may be silently unreachable.
- **Save persistence across a real power cycle.** Start a run, make some progress
  (move around, pick up at least one item, ideally go down a level), then power
  the device fully off (not sleep) and back on. Confirm the save is still there
  and resumes correctly — not just "a save file exists" but that position, items,
  and dungeon state come back matching what you left. This exercises `GameSave`
  actually round-tripping through a real reboot, which a compile can't prove.
- **Full-screen dungeon redraw — is it bearable on this panel?** This is an
  e-ink panel doing full-screen or near-full-screen redraws on every player move
  in a real-time-ish game loop — very different from a reader's occasional page
  turn. Play for a couple of minutes and judge: does each move visibly ghost or
  smear, is the refresh fast enough to feel like a game rather than a slideshow,
  does it stay readable? If it's unbearable, that's a real finding to report, not
  something to push through — there's no fix that doesn't touch the renderer.

## Pro hardware surface
This entire section exists because the Pro port is the actual point of this whole
project — the mods above are content, this is the thing that makes Stuart's actual
device work at all. None of it has met a bench. A green compile only proves the
code links against these pin numbers; it proves nothing about whether the numbers
are right.

- **Frontlight — HYPOTHESIS, not confirmed.** The dual warm/cool LEDC PWM pin
  mapping (cool/white on GPIO8, warm on GPIO9, both active-high, 10kHz/10-bit) came
  out of an OEM firmware dump, reverse-engineered — it has never been run on a real
  board. Test: turn the frontlight on, confirm any light comes on at all before
  judging anything else. Then check brightness actually ramps smoothly end-to-end
  (not stuck at one level, not inverted). Then check color-temperature: does the
  warm/cool mix genuinely shift the tint, or does adjusting it visibly do nothing /
  do the wrong thing? If GPIO8/GPIO9 are swapped, or either pin is wrong entirely,
  the most likely symptom is "nothing lights, or only one channel ever responds" —
  treat that as a pin-mapping bug to report, not a dimmer-logic bug.
  **Do not treat this as confirmed working anywhere else this feature is
  referenced** (release notes, README, chat) until Stuart has physically watched it
  light up and ramp.
- **Nav buttons — Left (GPIO0) and Right (GPIO7).** Digital, active-low,
  `INPUT_PULLUP`. Confirm both register reliably in the reader (page turn) and
  wherever else they're mapped (Deep Mines above). GPIO0 is a boot-strap pin — if
  it's ever held down through a power-on, watch for boot mode weirdness (this
  shouldn't happen in normal use, but worth knowing if the device ever misbehaves
  right after a cold boot with a button held).
- **GT911 touch + capacitive Home key.** Confirm taps register accurately across
  the screen (not offset, not mirrored) — the touch controller is mounted portrait
  on a landscape panel and relies on an axis swap to line up, so a mapping error
  here would show up as taps landing in the wrong place, worse near the edges. Also
  confirm the Home key (a capacitive bit read off the touch controller, not a
  physical button) reliably returns Home from wherever you are, including from
  inside Deep Mines.
- **Battery / power reporting**, if not already covered elsewhere in this project's
  validation history — not the focus of this round, but if anything looks
  obviously wrong (percentage stuck, charging never detected) while doing the
  above, note it; it's on the same I²C bus as the touch controller and RTC.
