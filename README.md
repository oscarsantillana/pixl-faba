# Faba on Pixl

Firmware for the **four-button KEYPAD Pixl / AmiiboTool with an SH1106 display**, used with the **original Faba speaker**. The current release is **2.16.1-faba10**.

Faba appears first in the menu. Browse 257 titles in seven language groups, play a title, or hold the centre button to save it in **Preferidos**. Favourites survive a restart on the tested device.

## Install and use

Start with the [installation guide](docs/INSTALL.md). It covers identifying compatible hardware, backing up your files, installing the signed firmware, uploading the catalogue, and checking playback.

You need both of these:

1. [Firmware ZIP](release/pixl-faba-2.16.1-faba10-preferidos.zip). This is the actual application-only Nordic DFU package. Give it to the updater without extracting it.
2. [Seven catalogue files](out/faba-catalog). Upload `cat0.bin` through `cat6.bin` into `E:/faba/` after flashing. The firmware ZIP does not contain these files.

Downloadable bundles and checksums are also on the [faba10 release page](https://github.com/oscarsantillana/pixl-faba/releases/tag/v2.16.1-faba10).

The complete documented installation path uses a Mac with macOS 13 or later and Bluetooth. Other Pixl variants, bare boards without a compatible bootloader, and Faba+ are outside the tested setup. The catalogue supplies story identifiers; it does not include audio or install audio on the speaker. Use a story already available on your Faba for the first playback check.

## Controls

- Open **Faba**, then a language folder or **Preferidos**.
- Use left/right to move through the list and its page controls.
- Short-press the centre button to play.
- Hold centre for about one second on a title to add or remove a favourite.
- Use the back button to leave playback before browsing again.
- Keep Pixl close to the speaker's NFC reader. Screen-down placement worked on the tested hardware; keep the buttons clear.

## What is included

| Path | Contents |
| --- | --- |
| [docs/INSTALL.md](docs/INSTALL.md) | First installation, backup, catalogue upload and recovery |
| [docs/BUILD.md](docs/BUILD.md) | Firmware build, tests and signing requirements |
| [docs/TECHNICAL.md](docs/TECHNICAL.md) | Compatibility fix, memory ownership, favourites format and diagnostics |
| [docs/VALIDATION.md](docs/VALIDATION.md) | Confirmed results and remaining limits |
| [firmware/pixl-faba](firmware/pixl-faba) | Source snapshot with dependencies vendored, including the patched Chameleon code |
| [tools](tools) | Mac BLE tools, build helpers and host regression tests |
| [catalog](catalog) | Pinned catalogue source and parsed entries |
| [evidence/faba10](evidence/faba10) | Release patches, binary, ELF, map, test logs and signature evidence |

No submodule initialization is needed. The source includes new files that were untracked in the original development workspace. Private signing keys and personal device backups are excluded.

## Verification status

The faba10 build, host tests, package checks and signature verification passed. DFU completed. The device owner confirmed working playback and Preferidos, including favourites surviving restart. The final faba10 BLE readback timed out, so a post-install version readback and slot comparison are not claimed. See [validation details](docs/VALIDATION.md).

This is an independent project. FABA branding belongs to its owner. See [credits and licensing](THIRD_PARTY.md); upstream licence notices remain with their source.
