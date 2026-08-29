# CrossPoint Reader

[![Fund contributors](https://img.shields.io/badge/%F0%9F%91%91_Fund_contributors-royalty.dev-BB953A?style=for-the-badge&labelColor=1a1a1a)](https://app.royalty.dev/crosspoint-reader/crosspoint-reader)

## This fork: `crosspoint-llama-pro` status

This fork targets the **Xteink X4 Pro**, an ESP32-S3 device distinct from the
ESP32-C3 X3 and X4 supported by upstream CrossPoint. The active
[`feature/reader-convergence-20260825`](https://github.com/MrStu82/crosspoint-llama-pro/tree/feature/reader-convergence-20260825)
branch selectively adopts reviewed CrossPoint 1.6 work; it is **not** a wholesale
1.6 rebase and does not claim full 1.6 feature parity. The selection and conflict
rules are recorded in the [upstream PR audit](./.planning/UPSTREAM_PR_AUDIT.md).

The integrated source is
[`b2036ff9`](https://github.com/MrStu82/crosspoint-llama-pro/commit/b2036ff973ab0cc718bf8b47e9fdeb0a1b7b4af6).
Its final production image identifies itself as `crosspoint-llama-pro` /
`v1.5.0-212-gb2036ff`. That source passed the complete 274-test host suite, a
clean X4 Pro simulator build, and deterministic touch and framebuffer gates.
Those results validate source and simulated behaviour; **they do not claim that
the exact v212 image has been tested on a real X4 Pro**.

### Selected integration

- **Safety and data:** X4 Pro power-latch sequencing, running/candidate image chip
  guards, stable wake detection, and board-aware USB fallback, while retaining
  the fork's resume, credential, cache, GT911 Home-key, and battery semantics.
- **Power and performance:** KOReader Wi-Fi lifetime locking, lossless built-in
  font compression, style-aware prewarming, one bounded low-memory CSS retry,
  and the lower X4 Pro frontlight range with separate cool/warm calibration.
- **Reader and UI:** StarDict synonyms and styled definitions, first-page and
  heading fixes, bounded EPUB table columns, Extra Wide spacing, transparent
  sleep overlays and retained-frame wake, a touch control centre, and transient
  password reveal. Home now has a true clipped half-star, a confidence-gated
  whole-book ETA beside chapter ETA, and a double-thickness progress bar directly
  below the cover without changing the surrounding visual hierarchy.

CrossPoint is open-source e-reader firmware - community-built, fully hackable, free forever. It's maintained by a growing community of developers and readers who believe your device should do what you want - not what a manufacturer decided for you.

**Device targets:** upstream CrossPoint runs on ESP32-C3-based Xteink [X4](https://www.xteink.com/products/xteink-x4) and [X3](https://www.xteink.com/products/xteink-x3); this fork additionally targets the ESP32-S3-based X4 Pro.

![CrossPoint Reader running on Xteink device](./docs/images/cover.jpg)

> If you're planning to buy an Xteink device, consider purchasing an **X3/X4 Developer Edition** through https://crosspointreader.com. CrossPoint receives a small share of each sale, helping fund development costs.

## What can CrossPoint do?

- **Reader engine**: EPUB 2/3 rendering with embedded-style option, image handling, hyphenation, kerning, chapter navigation, footnotes, bookmarks, dictionary lookups ([StarDict](docs/dictionary.md)), go-to-percent, auto page turn, orientation control, focus reading, KOReader progress sync and more.

- **Various formats**: native handling for `.epub`, `.xtc/.xtch`, `.txt`, and `.bmp`.

- **Screenshots.**

- **Custom fonts**: install your favorite fonts on the SD card.

- **Tilt page turn (X3 only)**.

- **Library workflow**: folder browser, hidden-file toggle, long-press delete, recent books, SD-cache management.

- **Wireless workflows**:

  - File transfer web UI
  - EPUB Optimizer
  - Web settings UI/API (edit many device settings from browser)
  - WebSocket fast uploads
  - WebDAV handler
  - AP mode (hotspot) and STA mode (join existing Wi-Fi), both with QR helpers
  - Calibre wireless connect flow
  - OPDS browser with saved servers (up to 8), search, pagination, and direct download
  - OTA update checks and installs from GitHub releases

- **Customization**: multiple themes (Classic, Lyra, Lyra Extended, RoundedRaff), sleep screen modes including transparent overlays, front/side button remapping, status bar controls, power-button behavior, refresh cadence, and more.

- **Localization**: 31 UI languages and counting, with RTL support.

### Coming soon:

- More themes.

- Much more! stay tuned.

---

## USB-locked devices (Xteink Unlocker)

> This upstream guidance applies to the ESP32-C3 X3 and X4. Do not treat an X3/X4 web-flasher selection as an X4 Pro target.

Some Xteink units purchased from third-party stores (e.g. AliExpress) ship with USB flashing locked from the factory.
If your device is locked, you will need to use the **Xteink Unlocker** tool available at
https://crosspointreader.com/#unlock-tool before you can flash CrossPoint.

**You do not need this tool if you bought your device directly from xteink.com.** Those units are not locked.

**Not sure if your device is locked?** Power it on, connect the USB-C cable, and try flashing via the web flasher first (see
[Install firmware](#install-firmware) below). If the browser's serial device picker does not show your device, try a different
USB port or browser before assuming the device is locked. Only reach for the unlocker if the device still doesn't appear.

> ### ⚠️ WARNING: READ THIS BEFORE USING THE UNLOCKER ⚠️
>
> **The only officially supported firmwares in the unlock tool are CrossPoint and CrossInk.**
>
> Flashing any other firmware on a USB-locked device may **permanently brick the device** or leave it **permanently
> stuck on that firmware with no recovery path**. Once USB flashing is re-locked, your only way back is via OTA, and if
> the firmware you flashed doesn't support OTA, **there is no way out**.

## Install firmware

### Web installer (upstream X3/X4, recommended)

1. Connect your ESP32-C3 X3 or X4 to your computer via USB-C and wake/unlock the device.
2. Go to https://crosspointreader.com/#flash-tools, select X3 or X4, and choose an official CrossPoint release.

### Web installer (upstream X3/X4, specific version)

1. Connect your device to your computer via USB-C and wake/unlock the device
2. Download a `firmware.bin` from [Releases](https://github.com/crosspoint-reader/crosspoint-reader/releases), local build, or continuous integration artifact.
3. Go to https://crosspointreader.com/#flash-tools, select device (X3 or X4), click "Custom .bin" and upload a `firmware.bin`.

### Revert an upstream X3/X4 to official firmware

To revert an X3 or X4 to the official firmware, you can also flash the latest official firmware using https://crosspointreader.com/#flash-tools.

### X4 Pro fork (ESP32-S3)

The web installer's X3 and X4 choices are ESP32-C3 targets and must not be used
for an X4 Pro image. Build the X4 Pro fork with its production environment:

```bash
git clone --recursive --branch feature/reader-convergence-20260825 \
  https://github.com/MrStu82/crosspoint-llama-pro.git
cd crosspoint-llama-pro
git checkout b2036ff973ab0cc718bf8b47e9fdeb0a1b7b4af6
git submodule update --init --recursive
pio run -e x4pro
```

The single application image is `.pio/build/x4pro/firmware.bin`. It targets an
ESP32-S3 and belongs at the app0 offset `0x10000`:

```bash
esptool --chip esp32s3 --port /dev/ttyACM0 --baud 921600 \
  write-flash 0x10000 .pio/build/x4pro/firmware.bin
```

Adjust the port for your system. This is an application image, not a merged
factory image; do not flash it at offset zero.

### Command line (upstream X3/X4)

1. Install [`esptool`](https://github.com/espressif/esptool):

```bash
pip install esptool
```

2. Download `firmware.bin` from the [releases page](https://github.com/crosspoint-reader/crosspoint-reader/releases).
3. Connect your device via USB-C.
4. Find the device port. On Linux, run `dmesg` after connecting. On macOS:

```bash
log stream --predicate 'subsystem == "com.apple.iokit"' --info
```

5. Flash:

```bash
esptool.py --chip esp32c3 --port /dev/ttyACM0 --baud 921600 write_flash 0x10000 /path/to/firmware.bin
```

Adjust `/dev/ttyACM0` to match your system.

### Manual

See [Development quick start](#development-quick-start) below.

---

## Custom SD-card fonts

Convert your own TTF/OTF files into `.cpfont` files that load from the SD card. No firmware reflash is needed.

1. Go to https://crosspointreader.com/fonts and open the "SD-card font builder" form.
2. Upload up to four styles (regular, bold, italic, bold-italic), set the family name, point sizes, and Unicode range.
3. Download the generated `.cpfont` files.
4. Copy them to your SD card under `/fonts/YourFont/` (or `/.fonts/YourFont/` to hide the folder).
5. Select the font on the device from the font settings.

Conversion runs the firmware repo's `lib/EpdFont/scripts/fontconvert_sdcard.py` script unmodified, so output matches a local host build.

---

## Documentation

- [User Guide](./USER_GUIDE.md)
- [Web server usage](./docs/webserver.md)
- [Web server endpoints](./docs/webserver-endpoints.md)
- [Project scope](./SCOPE.md)
- [Contributing docs](./docs/contributing/README.md)
- [Touch and UI development](./docs/contributing/touch-and-ui.md) - FreeInkUI components for new screens, the touch bridge for existing ones, and build envs for the non-Xteink touch devices
- [Selected upstream PR audit](./.planning/UPSTREAM_PR_AUDIT.md) - the review, deduplication, and conflict rules used for the selective 1.6 adoption

---

## Development quick start

### Prerequisites

- [pioarduino](https://github.com/pioarduino/pioarduino) or VS Code + pioarduino plugin
- Python 3.8+
- `clang-format` 21
- USB-C cable supporting data transfer

### Setup

```bash
git clone --recursive https://github.com/crosspoint-reader/crosspoint-reader
cd crosspoint-reader

# if cloned without --recursive:
git submodule update --init --recursive
```

### Nix/NixOS

Nix/NixOS users can enter the development shell with either `nix develop` (flakes) or `nix-shell`:

```bash
nix develop -f nix
# or
nix-shell nix
```

To flash a connected ESP32-C3 device, enable PlatformIO's udev rules in your NixOS configuration:

```nix
services.udev.packages = with pkgs; [ platformio-core.udev ];
```

After rebuilding the system configuration, reconnect the device or reload udev rules.

### Build / flash / monitor

```bash
# Upstream ESP32-C3 X3/X4 default environment
pio run --target upload

# X4 Pro fork production environment
pio run -e x4pro
# Output: .pio/build/x4pro/firmware.bin (ESP32-S3 app image at 0x10000)
```

Use the [X4 Pro flashing command](#x4-pro-fork-esp32-s3) above rather than an
upstream X3/X4 target selection.

### Contributor pre-PR checks

```bash
./bin/clang-format-fix
pio check -e default
pio run -e default
```

### Debugging

After flashing the new features, it’s recommended to capture detailed logs from the serial port.

First, make sure all required Python packages are installed:

```python
python3 -m pip install pyserial colorama matplotlib
```

After that run the script:

```sh
# For Linux
# This was tested on Debian and should work on most Linux systems.
python3 scripts/debugging_monitor.py

# For macOS
python3 scripts/debugging_monitor.py /dev/cu.usbmodem2101
```

Minor adjustments may be required for Windows.

---

## Internals

CrossPoint Reader is pretty aggressive about caching data down to the SD card to minimise RAM usage. The upstream ESP32-C3 X3/X4 targets have only ~380KB of usable RAM, so many design decisions are based on that constraint. The ESP32-S3 X4 Pro fork retains the same bounded cache and persistence behaviour even though its hardware profile differs.

### Data caching

The first time chapters of a book are loaded, they are cached to the SD card. Subsequent loads are served from the
cache. This cache directory exists at `.crosspoint` on the SD card. The structure is as follows:

```text
.crosspoint/
├── epub_<hash>/         # one directory per book, named by content hash
│   ├── progress.bin     # reading position (chapter, page, etc.)
│   ├── cover.bmp        # generated cover image
│   ├── book.bin         # metadata: title, author, spine, TOC
│   ├── css_rules.cache  # parsed CSS rule cache
│   ├── img_*            # rendered image cache files
│   └── sections/        # per-chapter layout cache
│       ├── 0.bin
│       ├── 1.bin
│       └── ...
├── settings.json        # device settings
├── state.json           # resume/runtime state
└── recent.json          # recent books list
```

Removing `/.crosspoint` clears all cached metadata and forces a full regeneration on next open. Book deletes, overwrites, and moves done through the firmware or web UI clear or re-key matching caches; manual SD-card edits may leave stale cache directories behind.

For more details on the internal file structures, see the [file formats document](./docs/file-formats.md).

---

## Contributing

Contributions are welcome. If you're new to the codebase, start with the [contributing docs](./docs/contributing/README.md). For things to work on, check the [ideas discussion board](https://github.com/crosspoint-reader/crosspoint-reader/discussions/categories/ideas) — leave a comment before starting so we don't duplicate effort.

Everyone here is a volunteer, so please be respectful and patient. For governance and community expectations, see [GOVERNANCE.md](./GOVERNANCE.md).

---

## Community forks

One of the best things about open source is that anyone can take the code in a different direction. If you need something outside CrossPoint's [scope](./SCOPE.md), check out the community forks:

- [CrossInk](https://github.com/uxjulia/CrossInk) — Typography and reading tracking: Bionic Reading (bolds word stems to create fixation points), guide dots between words, improved paragraph indents, and replaces the default fonts with ChareInk/Lexend/Bitter.

- [papyrix-reader](https://github.com/bigbag/papyrix-reader) — Adds FB2 and MD format support. Actively maintained with Arabic script support. Custom themes via SD card.

- ~~[crosspet](https://github.com/trilwu/crosspet) — A Vietnamese fork that adds a Tamagotchi-style virtual chicken that grows based on your reading milestones (pages read, streaks, care). Also: Flashcards, Weather, Pomodoro timer, and mini-games.~~ (Unmaintained)

- [crosspoint-reader-cjk](https://github.com/aBER0724/crosspoint-reader-cjk) — Purpose-built for Chinese, Japanese, and Korean reading.

- [inx](https://github.com/obijuankenobiii/inx) — Completely reimagines the user interface with tabbed navigation.

- ~~[PlusPoint](https://github.com/ngxson/pluspoint-reader) — custom JS apps support.~~ (Unmaintained)

- [crosspoint-reader-papers3](https://github.com/juicecultus/crosspoint-reader-papers3) — Crosspoint port for M5Stack Paper S3.

- [t5s3-reader](https://github.com/ShallowGreen123/t5s3-reader) — Crosspoint port for LilyGo T5 ePaper S3 / T5S3 4.7-inch e-paper device.

**Note:** Many of these features will make their way into CrossPoint over time. We maintain a slower pace to ensure rock-solid stability and squash bugs before they reach your device.

Want to build your own device? Be sure to check out the [de-link](https://github.com/iandchasse/de-link) project.

---

CrossPoint Reader is **not affiliated with Xteink or any device manufacturer**.

Huge shoutout to [diy-esp32-epub-reader](https://github.com/atomic14/diy-esp32-epub-reader), which inspired this project.
