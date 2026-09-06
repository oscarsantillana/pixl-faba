#!/usr/bin/env bash
set -eo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
REPO_ROOT="$(cd "$ROOT_DIR/.." && pwd)"
BOARD="${1:?BOARD required}"
OUT_DIR="$REPO_ROOT/${2:?output directory required (relative to repo root)}"
APP_VERSION="${APP_VERSION:-21601}"
FW_VERSION="${FW_VERSION:-2.16.1}"
WORKFLOW_BRANCH_OR_TAG="${WORKFLOW_BRANCH_OR_TAG:-$(git -C "$ROOT_DIR/.." rev-parse --abbrev-ref HEAD)}"

export NRF52_SDK_ROOT="${NRF52_SDK_ROOT:-$ROOT_DIR/.sdk/nRF5_SDK_17.1.0_ddde560}"
export GNU_INSTALL_ROOT="${GNU_INSTALL_ROOT:-$ROOT_DIR/.toolchain/arm-gnu-toolchain-12.2.rel1-darwin-x86_64-arm-none-eabi/bin/}"
export PATH="$GNU_INSTALL_ROOT:$ROOT_DIR/.tools/bin:$ROOT_DIR/.venv311/bin:$PATH"
export FW_VERSION APP_VERSION WORKFLOW_BRANCH_OR_TAG ROOT_DIR

mkdir -p "$OUT_DIR"
cd "$ROOT_DIR"

make clean
make -C application version RELEASE=1 APP_VERSION="$APP_VERSION" BOARD="$BOARD"

BL_ARGS=(bl RELEASE=1 BOARD="$BOARD")
if [ "$BOARD" = "OLED" ] || [ "$BOARD" = "KEYPAD" ]; then
  BL_ARGS+=(OLED_TYPE=sh1106)
fi

make -j"$(sysctl -n hw.ncpu 2>/dev/null || nproc)" "${BL_ARGS[@]}"
make -j"$(sysctl -n hw.ncpu 2>/dev/null || nproc)" -C application pixljs RELEASE=1 APP_VERSION="$APP_VERSION" BOARD="$BOARD"

nrfutil settings generate \
  --family NRF52 \
  --application "$ROOT_DIR/_build/pixljs.hex" \
  --application-version "$APP_VERSION" \
  --bootloader-version 1 \
  --bl-settings-version 1 \
  "$ROOT_DIR/_build/settings.hex"

python3 - <<'PY'
from intelhex import IntelHex
from pathlib import Path
import os

root = Path(os.environ["ROOT_DIR"])
build = root / "_build"
sdk = Path(os.environ["NRF52_SDK_ROOT"])
files = [
    build / "settings.hex",
    build / "bootloader.hex",
    sdk / "components/softdevice/s112/hex/s112_nrf52_7.2.0_softdevice.hex",
    build / "pixljs.hex",
]
merged = IntelHex()
for path in files:
    merged.merge(IntelHex(str(path)), overlap="replace")
merged.write_hex_file(str(build / "pixljs_all.hex"))
PY

nrfutil pkg generate \
  --application "$ROOT_DIR/_build/pixljs.hex" \
  --application-version "$APP_VERSION" \
  --hw-version 52 \
  --sd-req 0x0103 \
  --key-file "$ROOT_DIR/bootloader/priv.pem" \
  "$ROOT_DIR/_build/pixjs_ota_v${APP_VERSION}.zip"

cp "$ROOT_DIR/_build/bootloader.hex" "$OUT_DIR/"
cp "$ROOT_DIR/_build/pixljs.hex" "$OUT_DIR/"
cp "$ROOT_DIR/_build/pixljs_all.hex" "$OUT_DIR/"
cp "$ROOT_DIR/_build/pixjs_ota_v${APP_VERSION}.zip" "$OUT_DIR/pixjs_ota_v${FW_VERSION}.zip"
cp "$ROOT_DIR/docs/fw_readme.txt" "$OUT_DIR/"
cp "$ROOT_DIR/scripts/fw_update.bat" "$OUT_DIR/"

echo "Packaged $BOARD firmware v$FW_VERSION into $OUT_DIR"
