# Build and maintain the firmware

Installing the supplied release does not require a firmware build. The exact binary is preserved in `evidence/faba10/`. A new build contains new metadata and need not have the same binary hash.

## Source snapshot

`firmware/pixl-faba/` contains the patched working source and vendored dependencies. It is not a nested Git repository and needs no `git submodule update`. [Provenance](../evidence/source-provenance.json) records upstream revisions. The [application patch](../evidence/faba10/application.patch) includes all new files; the [Chameleon patch](../evidence/faba10/chameleon.patch) applies inside the upstream Chameleon dependency. These are archived release patches, already applied in this snapshot.

Private key files and upstream GitHub workflows were excluded from the publication. The application source, dependency licence notices, linker script and release patches are retained. A clean upstream checkout without these patches will not reproduce Faba functionality.

## Host tests

With Python 3 and a C compiler with AddressSanitizer and UndefinedBehaviorSanitizer:

```sh
sh tools/test-faba-catalog-release.sh
python3 tools/verify-release.py
```

Run this suite for every firmware change. It exercises actual firmware functions through host boundaries, including catalogue/menu/player lifetime, NFC replay, bounded event queue and favourites persistence. Host tests do not establish physical interrupt timing, RF coupling or power-cut behaviour. The Mac setup script separately builds the portable BLE tools and runs Nordic package parsing and signature verification.

## Toolchain

The delivered build used Nordic nRF5 SDK 17.1.0 and xPack Arm GNU 12.3.1-1.2 for macOS ARM64. The compiler download SHA-256 was `507926ba1e37e6fcae2a7499559cffd6da015b93145ff7657aafca9ef097d683`.

The SDK and packaging environment came from this pinned container:

```text
solosky/nrf52-sdk@sha256:ef337de9a0504dc3cf0f94a0a8d09f06193b318b3cc9b1bc5bd0b92a7bc1ea16
```

Obtain the compiler from [xPack's release archive](https://github.com/xpack-dev-tools/arm-none-eabi-gcc-xpack/releases/tag/v12.3.1-1.2) and SDK 17.1.0 from Nordic or the pinned container. The complete outer SDK and compiler are external prerequisites, not included in this snapshot. Do not confuse the Chameleon dependency's bundled SDK files with the outer application's required SDK. Inspect the pinned container to locate the SDK before copying it; do not assume a temporary path from the original workstation still exists.

Set absolute paths to the extracted dependencies. Include the trailing slash on the compiler `bin/` path:

```sh
NRF52_SDK_ROOT=/absolute/path/to/sdk \
GNU_INSTALL_ROOT=/absolute/path/to/xpack-arm-none-eabi-gcc-12.3.1-1.2/bin/ \
sh tools/build-faba-diagnostic.sh 2.16.1-faba11
```

Use a new display version for changes. The build script orders version generation before parallel compilation and checks the embedded version label. Preserve `BOARD=KEYPAD RELEASE=1 APP_VERSION=21601`. The numeric Nordic version and the displayed version serve different purposes.

The faba10 application occupies 369540 of 372736 available flash bytes, leaving 3196 bytes. Its map leaves only 96 bytes between the existing heap/stack reservations. Keep the 18 KB UI pool, 4 KB C-library heap, 8 KB stack and player memory reserve. Link success alone does not establish sufficient runtime memory.

## Signing a new application

The supplied faba10 ZIP is already signed. No private key is required to install it. A new package needs a signing key matching the public key in the device's existing bootloader. This repository deliberately excludes private key files. Resolve compatible signing credentials through the upstream project or your own bootloader setup; do not commit them here.

With a matching key supplied as `/absolute/path/to/signing.pem`, package only the application:

```sh
docker run --rm --platform linux/amd64 \
  -v "$PWD/firmware/pixl-faba:/work" \
  -v /absolute/path/to/signing.pem:/signing.pem:ro \
  solosky/nrf52-sdk@sha256:ef337de9a0504dc3cf0f94a0a8d09f06193b318b3cc9b1bc5bd0b92a7bc1ea16 \
  nrfutil pkg generate \
  --application /work/fw/_build_faba/pixljs.hex \
  --application-version 21601 --hw-version 52 --sd-req 0x0103 \
  --key-file /signing.pem /work/fw/_build_faba/new-release.zip
```

Validate into a new directory:

```sh
python3 tools/verify-faba-package.py \
  firmware/pixl-faba/fw/_build_faba/new-release.zip \
  firmware/pixl-faba evidence/new-release 2.16.1-faba11
.build/bin/verify-faba-signature evidence/new-release
.build/bin/PixlDFU validate firmware/pixl-faba/fw/_build_faba/new-release.zip
```

Keep the matching `.bin`, `.hex`, `.out` ELF and `.map`, complete source changes, checksums, build/test logs and separate signature evidence before building again. The package verifier writes a pending signature status; a successful CryptoKit check is separate evidence. Back up device configuration, every saved slot and both favourite banks before flashing any new build.

## Catalogue changes

The published catalogue is pinned, not fetched at runtime. Update `catalog/TAGS.md` and `catalog/source.json` deliberately, then run `python3 tools/generate-faba-catalog.py` and the host suite. This regenerates the catalogue files and the firmware's catalogue metadata header. Review record lengths, group order, IDs and the 257-entry favourites capacity before increasing catalogue size.
