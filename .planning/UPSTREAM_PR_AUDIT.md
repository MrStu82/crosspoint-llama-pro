# Phase 0 — selected upstream PR audit

**Method.** GitHub metadata and `/files` payloads were captured on 2026-08-28 under
`.planning/evidence/upstream-pr-audit/` (29/29 responses). Each row names the current-fork
integration surface and the deterministic test surface. `Present` means a semantic equivalent
was verified as an ancestor of frozen HEAD, not merely a matching PR number.

| PR | Selected intent | Current-fork files / tests | Phase-0 disposition and semantic conflict |
|---|---|---|---|
| #3215 | X4 deep-sleep power latch | `lib/hal/HalPowerManager.cpp`; add `test/hal_power_manager` | Missing/board-specific. Must inspect `BoardConfig` latch pins; never copy generic GPIO assumptions. |
| #2962 | resume rollback | `src/activities/reader/EpubReaderActivity.{cpp,h}`; reader-position tests | **Present** as `cb4d6ce72`; retain, no duplicate patch. |
| #3034 | file render-task race | `src/activities/home/FileBrowserActivity.cpp`; add focused race harness | **Present** as `f69dbd3c5`; verify fork touch/Home changes did not reopen it. |
| #2880 | cross-chip flash guard | `src/network/{FirmwareFlasher,OtaUpdater}.{cpp,h}`, OTA/SD update activities; add image-header test | Missing. Conflict: fork X4 USB/partition policy; validate running image and candidate header without changing updater UX blindly. |
| #2959 | image decode bounds | `lib/Epub/Epub*`, `converters/*`, `EpubReaderActivity.cpp`; existing image fixtures | **Present** as `961629fc4`; only add fixture if its exact overflow invariant lacks coverage. |
| #3001 | EPUB metadata | `ContentOpfParser.cpp`, `XmlParserUtils.h`, `test/xml_parser_utils` | **Present** as `7e5c98c6e`; preserve fork library metadata reader. |
| #2834 | credential thread safety | `lib/Serialization/{PersistableStore,CredentialIntegrity,ObfuscationUtils}*`, `CrossPointSettings`, Wi-Fi/Web; `test/credential_integrity` | **Present** as `0437c5183`. Phase 1 must additionally audit `src/network/HardcoverRating*` atomic store for compatible lock ordering. |
| #3233 | KOReader Wi-Fi awake | `KOReader{Sync,Auth}Activity.cpp`; add activity/mock Wi-Fi test | Missing. Upstream unrelated font test is excluded. Scope wake lifetime only. |
| #3144 | 323KB built-in font compression | `lib/EpdFont/*`, generated `builtinFonts/*`, conversion scripts; `test/differential_rounding` | Missing/generated. Isolate after renderer baseline hash/flash accounting; never hand-edit generated font headers. |
| #3191 | X4 wake detection | `lib/hal/HalGPIO.*`, `src/main.cpp`, `platformio.ini`, FreeInk board config; add GPIO mock test | Missing/board-specific. Conflicts with fork GT911/Home gesture and boot policy. |
| #3223 | USB fallback detection | `lib/hal/HalGPIO.cpp`, FreeInk battery interface; same GPIO test target | Missing/board-specific. Must not infer USB from charging on unsupported hardware. |
| #3005 | low-memory CSS retry | `lib/Epub/Epub*`, `Section.cpp`, `css/CssParser*`; add `test/css_parser` | Missing. Rebase into fork advanced parser; do not replace parser/cache API. |
| #3222 | style-aware font prewarm | `lib/GfxRenderer/FontCacheManager*`; add `test/font_cache_manager` | Missing. Existing CJK/SD cache changes are a semantic conflict to preserve. |
| #2937 | transparent sleep base | `CrossPointSettings/State`, `SleepActivity`, Home/File/bitmap viewer; add sleep state tests | Missing. Conflicts with InkPoint Home retained-frame work; import as a bundle only. |
| #2974 | PNG transparent sleep | image decoder, `SleepActivity`, Reader, Viewer, `UITheme`; sleep/image fixtures | Missing. Depends on #2937; preserve current decode bounds (#2959). |
| #2943 | wake release swallow | `src/main.cpp`; add input-sequence test | Missing. Must not consume an ordinary in-app power release. |
| #3009 | clear sleep image after wake | `ActivityManager`, Home, `main.cpp`; retained-frame test | **Present** as `3f3aa504e`; no duplicate patch. |
| #3119 | non-alpha sleep dither | `SleepActivity.cpp`; BMP fixture | Missing. Depends on transparent-sleep base. |
| #3093 | release SD font cache before overlay | `SleepActivity.cpp`; cache-order test | **Present** as `9f8689fbe`; retain. |
| #2696 | StarDict synonyms | `src/util/Dictionary*`; add synonym EPUB/fixture test | Missing. Keep existing lookup normalisation (`#2877`) and sidecar freshness. |
| #2836 | HTML dictionary layout | Dictionary definition/select activities, `DictHtmlPages*`, `Dictionary*`; add dictionary HTML fixture | Missing. Bound memory use against current reader parser. |
| #3154 | dictionary styled-definition first page | `DictHtmlPages.cpp`; dictionary page test | Missing. Must land after #2836 and retain current `#3153` heading fix. |
| #3153 | dictionary heading breaks | `HtmlToPlainText.cpp`; `test/html_to_plain_text` | **Present** as `6984dd05c`; retain. |
| #2654 | EPUB tables | `Epub/Section.cpp`, `ChapterHtmlSlimParser*`; parser table fixtures | Missing. Conflict: fork advanced parser/redaction/ruby changes; port table invariant, not upstream file wholesale. |
| #3156 | touch control centre | Frontlight panel, themes, renderer, reader/activity manager, FreeInk; touch-layout tests | Deferred behind prototype approval. Existing fork has no current control-centre implementation to dedupe. |
| #3115 | lower X4 frontlight | `freeink-sdk` board implementation; hardware/mock range test | Deferred UI/power phase. Never advance the SDK wholesale. |
| #3037 | Extra Wide spacing | `CrossPointSettings*`, `SettingsList`, Text settings, all i18n; settings enum test | Missing. Existing reader layout must accept the new enum without reflow regression. |
| #3080 | password reveal | Wi-Fi/KOReader/OPDS settings activities; keyboard state test | Deferred UI gate. Reveal must be transient, no changed storage/logging semantics. |
| #2989 | skip boot | `src/main.cpp`; boot retained-frame test | Missing. Bundle with sleep/wake to avoid blank/intermediate panel paint. |

## Bounded phase order

1. **Safety/data:** #3215/#2962/#3034/#2880/#2834.
2. **Power/performance:** #3233/#3191/#3223/#3144/#3222/#3115/#3005.
3. **Reader:** #2959/#3001/#2696/#2836/#3154/#3153/#2654/#3037.
4. **Sleep bundle:** #2937/#2974/#2943/#3009/#3119/#3093/#2989.
5. **Parent UI-prototype approval gate.**
6. **Visual/settings implementation:** #3156/#3115/#3080 only after that approval.

No firmware build is authorised before the final post-approval delivery build.
