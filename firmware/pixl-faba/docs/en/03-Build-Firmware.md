# Build

## Build with Github Actions

You could download the latest develop build from Github Actions

https://github.com/solosky/pixl.js/actions


## Build with customized Docker image

You could build the firmware using customized Docker image. 

```
# create containers
docker run -it --rm solosky/nrf52-sdk:latest

# init repository
root@b10d54636088:/builds# git clone https://github.com/solosky/pixl.js
root@b10d54636088:/builds# cd pixl.js
root@b10d54636088:/builds/pixl.js# git submodule update --init --recursive

# build LCD version
root@b10d54636088:/builds/pixl.js# cd fw && make all BOARD=LCD RELEASE=1

# build OLED version
root@b10d54636088:/builds/pixl.js# cd fw && make all BOARD=OLED RELEASE=1

# build OLED keypad version (4 buttons, back on P0.20)
root@b10d54636088:/builds/pixl.js# cd fw && make all BOARD=KEYPAD RELEASE=1

```

The firmware is fw/_build/pixjs_all.hex，ota package is fw/_build/pixjs_ota_vXXXX.zip

## AmiiboTool / iNFC four-button devices

AmiiboTool, iNFC, and similar rebranded hardware with a dedicated back button (P0.20) **must** use `BOARD=KEYPAD` firmware. Do **not** use the standard three-button `BOARD=OLED` package.

- GitHub Actions: download the **`amiibotool-fw-keypad`** artifact from the `amiibotool-fw` workflow on the `amiibotool` branch
- Local build: `make all BOARD=KEYPAD RELEASE=1` (OLED/KEYPAD boards auto-enable SH1106 in the bootloader)
- After first install or a board-type change, flash **`pixljs_all.hex`** via wired method (includes bootloader); do not rely on OTA alone
- Verify branch and build info under **Settings → Version**
- KEYPAD firmware disables **Player** and **Game** to fit FLASH; tag database and emulator apps remain enabled

## OTA signing key

OTA packages are signed with the key selected by `DFU_PRIVATE_KEY`. The default value is `../bootloader/priv.pem`, which is intentionally public in this project so compatible-device owners and firmware developers can build and update custom firmware.

If you build a custom bootloader with a different public key, pass the matching private key when generating OTA packages:

```
cd fw && make ota DFU_PRIVATE_KEY=/path/to/custom-private-key.pem
```

Devices will only accept OTA packages signed by the private key that matches the public key compiled into their bootloader. Do not commit personal or device-specific private keys.
