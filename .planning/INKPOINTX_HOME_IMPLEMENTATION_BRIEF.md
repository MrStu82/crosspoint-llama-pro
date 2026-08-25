# InkPointX Home implementation brief

**Status:** implementation authorised. Provider locked to Hardcover beta GraphQL catalog search.

**Visual benchmark:** `00af7165a57d22140fd2c520a04111906f488bccd19b2dce0882a1b79bb1b348`
(480×800 prototype; Pixel PASS and parent inspection). The footer crop is
`952fba09911a39c1bfdc99246d628908738998708b42d213cc16d1d30d074ba4`.

## Scope boundary

Implement the approved InkPointX-derived Home / Now Reading screen only. Keep
the existing partition table, both OTA slots, rollback, SPIFFS and the existing
SD/downloadable-font path. Do not redesign reader pages or sweep unrelated
menus. The benchmark is the acceptance contract, not a suggestion.

## Shared implementation shape

1. **One Home model.** Enrich the existing most-recent `RecentBook` entry with
   cover availability, progress, per-book reading totals/ETA confidence, and an
   optional cached rating snapshot. Rendering consumes this bounded value
   object; it never performs network or storage I/O.
2. **One portrait layout.** Add an X4 Pro 480×800 Home layout in the existing
   Home/theme path (`HomeActivity` + theme renderer), with geometry constants
   shared by draw and hit-test. Preserve cover-buffer reuse and current book
   selection semantics.
3. **One heading token.** Register the InkPointX Caveat face once and expose a
   shared screen-heading style. Caveat is used only for screen headings, the
   approved quote, and approved stat numerals; body, metadata, errors, controls
   and attribution remain in the selected readable UI face.
4. **One persistent footer component.** Six 72×60 direct targets at x
   14..86/90..162/166..238/242..314/318..390/394..466: Home, Library, Files,
   Games, Transfer, Settings. Current destination alone is inverted. Icons are
   strict 1-bit line art; Settings uses the approved continuous eight-tooth
   outline cog. Recent stays inside Library; Sleep stays in power/settings.
5. **No fabricated state.** Cover progress comes from the existing recent-book
   badge. A failed/missing cover renders a typographic title/author card. Missing
   stats or rating omit/degrade only their own field; Home never blanks.

## Approved visual/data requirements

- Fixed 18px status lane, distinct 42px Caveat heading lane beneath it, no
  divider.
- Native-aspect dithered cover is the sole resume target, with a thin attached
  progress bar. Text/icons remain crisp 1-bit.
- Right column: 36px condensed uppercase title whose ink top is not above the
  cover ink top; author/year; four filled vector stars plus a visibly partial
  fifth for 4.44, with no numeric rating or font-star dependency; tightly packed
  Time read / Chapter left / Book left groups.
- Quote block remains vertically centred in y=551..719 with its established
  internal spacing and attribution. Quote pack follows the separately accepted
  deterministic shuffled-deck plan and its 96 KiB gate.
- All touch targets are at least 44×44; physical navigation remains complete;
  swipes are optional only.

## Per-book stats contract

- Canonical identity: ISBN-13 when available; otherwise an audited alias record
  for format/provider identifiers. Title+author matching may propose an alias
  but never silently merges books.
- Persist accumulated foreground reading seconds and observed page samples per
  canonical book.
- Derive chapter/book ETA from that book's observed seconds/page only after the
  accepted confidence threshold. Before confidence, show `—` (or the explicitly
  approved global fallback); never present invented precision.
- Update atomically and keep last-known-good data on corruption or write error.

## Hardcover rating provider and credential audit

- Endpoint: `POST https://api.hardcover.app/v1/graphql`; catalog `search`
  requires only the dedicated `read:catalog:search` PAT scope. The token is
  provisioned at runtime. Transfer accepts a one-time `/hardcover.token` file,
  validates the `hc_pat_` prefix, bounded length and safe character set,
  atomically persists the device-bound/obfuscated value at
  `/.crosspoint/hardcover.json`, and removes the import file only after the
  save succeeds. Settings exposes explicit import/replace and confirmed forget
  actions. The web password field remains write-only/redacted (`hasValue`,
  never the token). The token is never compiled or logged.
- Match ISBN first when EPUB metadata supplies one; on missing/no-match, query
  title+author and accept only an exact normalised title plus exact author.
- Provider is isolated behind `RatingSnapshot` because Hardcover documents the
  API as beta. GraphQL errors, missing fields, network loss and auth failure all
  leave the atomic per-book last-good cache untouched. Unresolved books render
  no rating. No Goodreads or HTML scraping exists.
- Refresh is opportunistic only when Wi-Fi is already connected, at most once
  per 24 hours per current book.

## Implementation phases

1. **Data contracts and deterministic tests** — Home model, per-book stats,
   quote deck, rating adapter selected above; corruption/stale/missing tests.
2. **Renderer and input** — benchmark geometry, cover/fallback, vector stars,
   shared heading token and six-tab draw/hit-test component.
3. **Simulator gate** — deterministic `simulator_x4_pro` fixtures for cover,
   coverless, missing/stale rating, confident/unconfident ETA, all six touch
   targets and physical focus; compare 480×800 framebuffer to the benchmark.
4. **Single release build** — unchanged partition bytes, program-size gate,
   host suite, valid X4 Pro bare app, then one Pixel screenshot and one Gauge
   functional gate. A material failure stops for parent decision; no autonomous
   rebuild churn.

## Acceptance proof

- Host tests pin draw/hit geometry, canonical book mapping, time accumulation,
  ETA threshold, rating cache atomicity/fallback, quote selection and no-repeat.
- Simulator proves the actual screen and all navigation paths without Stuart.
- Release report includes exact commit, version, byte size, SHA-256, esptool
  image info, unchanged partition-table hash and headroom.
- The single release build occurs only after host and simulator gates pass.
