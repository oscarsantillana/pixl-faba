# Install faba10

Run the commands below from the repository root. This guide uses the native macOS tools. It does not require compiling the firmware.

## 1. Check the device

The tested device is a four-button KEYPAD Pixl / AmiiboTool with an SH1106 OLED display and back button on P0.20. It already ran Pixl firmware with **BLE File Transfer** and **Settings → Firmware Update**. The speaker is an original Faba.

This release requires Nordic hardware version 52, SoftDevice requirement `0x0103`, and a bootloader accepting the upstream Pixl signing public key. The package updates only the application. It cannot initialize a bare board or correct an incompatible bootloader. Check your device's hardware and vendor firmware before proceeding. Save its original firmware recovery package.

Use a charged battery and keep the device near the Mac. Close other phone or browser connections to it.

## 2. Download and check

Install Apple's command-line tools if needed with `xcode-select --install`. You also need Python 3, Git, macOS 13 or later, and internet access for the pinned Nordic library and ZIPFoundation dependency.

```sh
git clone https://github.com/oscarsantillana/pixl-faba.git
cd pixl-faba
python3 tools/verify-release.py
sh tools/setup-macos.sh
```

Setup compiles the BLE tools, validates the package with Nordic's parser, and verifies its signature. It does not connect to a device. Give your terminal Bluetooth permission when macOS requests it during later device commands.

The firmware ZIP SHA-256 must be:

```text
abc0ddd43b30b7ce89cea4ae8c4054bd5548a3a9222049c6ffe1f3e73d7b5270
```

## 3. Identify your application UUID

Open **BLE File Transfer** on Pixl and run:

```sh
.build/bin/pixl-scan
```

The scanner reports advertisements without connecting. The application advertises Nordic UART service `6E400001-B5A3-F393-E0A9-E50E24DCCA9E`. Confirm the device by switching your Pixl's BLE mode off and on between scans and checking which advertisement disappears and returns. Do not select an unrelated UART device.

Set the UUID printed for your device:

```sh
export PIXL_UUID='REPLACE-WITH-YOUR-APPLICATION-UUID'
```

These UUIDs are specific to the Mac. The application's UUID and the DFU bootloader's UUID are usually different. The tools reject a missing or malformed UUID and connect only to the exact UUID you selected.

## 4. Back up before writing

Record the firmware version shown under Settings → Version. Inventory your saved card slots and other files using the device's file manager or your existing file-transfer client. Close that client's connection before running the native tools.

Set the slot inventory to the actual saved filenames. This example covers configuration and three saved slots only; add every other saved slot on your device. Filenames are hexadecimal, such as `0A.bin`.

```sh
export PIXL_SLOT_FILES='config.bin,00.bin,01.bin,02.bin'
mkdir -p backups
.build/bin/pixl-read-slots backups/pre-slots-01
.build/bin/pixl-read-favorites backups/pre-favorites-01
```

Read the results and require successful completion. These tools do not back up the whole filesystem. Preserve any other personal files with your file-transfer client. If the device has no saved slots or configuration yet, establish and record that empty state rather than treating a failed read as an empty backup.

Always preserve both `E:/faba/pref0.bin` and `E:/faba/pref1.bin` if present. Both may be absent on a first install. Only the explicit missing-file response counts as absence; a timeout, access error or failed connection does not. Keep each attempt in a fresh directory. Never reuse a failed or previous backup directory.

## 5. Discover the DFU UUID

First start the scanner:

```sh
.build/bin/pixl-scan
```

While it runs, open **Settings → Firmware Update** on Pixl. Note the UUID of your `pixldfu` advertisement with service `FE59`. Identify it by the timing of your device entering update mode, not by the name alone. The tested bootloader advertises for roughly 20 seconds. Let this discovery session finish and return the device to its normal menu if needed.

## 6. Flash the firmware

Replace the DFU placeholder below with the UUID from the preceding step. Start the updater first:

```sh
.build/bin/PixlDFU flash \
  release/pixl-faba-2.16.1-faba10-preferidos.zip \
  'REPLACE-WITH-YOUR-DFU-UUID'
```

While it listens, open **Settings → Firmware Update** on Pixl again. Alternatively, with BLE File Transfer open, run this in another terminal using the exact freshly observed version:

```sh
export PIXL_UUID='REPLACE-WITH-YOUR-APPLICATION-UUID'
.build/bin/pixl-enter-dfu 'REPLACE-WITH-YOUR-CURRENT-VERSION'
```

The helper refuses a version mismatch. Keep the updater running until it reports completed and exits successfully. A previous transfer took about 51 seconds for 369540 bytes. Progress or elapsed time alone is not completion.

If no device is found, restart the listener and enter Firmware Update again. Do not substitute another nearby UUID. If the bootloader rejects the package, check hardware, SoftDevice and signing compatibility. Do not erase the device or replace its bootloader to bypass that check.

## 7. Install the catalogue

After Pixl restarts, check Settings → Version for `2.16.1-faba10`, then reopen **BLE File Transfer**. If it advertises a new application UUID, identify it again and update `PIXL_UUID`.

```sh
.build/bin/pixl-upload-catalog out/faba-catalog backups/catalog-01
```

The uploader checks the seven manifest hashes, creates `E:/faba/` if absent, backs up existing differing catalogue files, uploads only `cat0.bin` through `cat6.bin`, and reads each file back. Require `CATALOG VERIFIED`. It does not upload `manifest.json` or modify favourite banks. All seven files total 41232 bytes and cover 257 entries.

If uploading manually with a compatible client, create `E:/faba/`, copy the seven `.bin` files directly into it, then download them and compare their SHA-256 hashes with `out/faba-catalog/manifest.json`. Do not place them in an extra nested directory.

## 8. Verify your installation

Reopen BLE File Transfer and read into fresh directories:

```sh
.build/bin/pixl-read-slots backups/post-slots-01
.build/bin/pixl-read-favorites backups/post-favorites-01
```

Compare saved files against the pre-install backups before playing or editing favourites. Note any explained counter changes separately. If your initial device state had no slot configuration, repeat the inventory check instead. Record a timeout as an incomplete check.

Leave BLE mode. Open Faba, choose a language and a story already available on your speaker, then short-press centre. Keep Pixl very close to the reader with buttons clear. Hearing playback is the relevant check; waking the display or reading the tag with a phone is not enough.

Try multiple selections and back navigation. Hold centre for about one second on a title to star it. Confirm it appears in Preferidos, restart Pixl and confirm it remains there. Make a fresh backup of both favourite banks after creating your own favourites.

## Troubleshooting and recovery

| Symptom | Next check |
| --- | --- |
| Faba menu missing | Check the running version; transfer success is separate from running-version verification. |
| Titles missing or catalogue error | Check all seven files in `E:/faba/` and repeat upload with a fresh backup directory. |
| No sound but phone reads work | Use an available story and move Pixl closer. The speaker and phone exercise different NFC behaviour. |
| Wrong display or buttons | Recheck KEYPAD/SH1106 hardware compatibility before another update. |
| Bluetooth timeout | Reopen the relevant BLE mode, disconnect other clients and repeat in a new directory. |
| Favourites unavailable | Back up both banks and inspect them; do not delete or overwrite unreadable state blindly. |
| Playback resets | Capture the retained diagnostic before restarting or selecting another story; see [technical notes](TECHNICAL.md). |

For recovery, use your hardware vendor's matching application-only recovery package and the same verified-backup/DFU procedure. Arbitrary downgrade paths have not been tested. This repository does not prescribe a mass erase or a bootloader/SoftDevice replacement. Restore personal data deliberately per file, preserving both favourite banks even when using older firmware that cannot display them.

Phone and web DFU routes exist upstream, but the complete procedure verified during development used the native Mac updater. This repository does not claim a tested Windows, Linux, iOS or Android end-to-end install.
