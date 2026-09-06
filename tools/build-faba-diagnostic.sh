#!/bin/sh
set -eu
: "${NRF52_SDK_ROOT:?Set NRF52_SDK_ROOT}"
: "${GNU_INSTALL_ROOT:?Set GNU_INSTALL_ROOT}"
[ "$#" -eq 1 ] || { echo 'Usage: build-faba-diagnostic.sh 2.16.1-fabaN' >&2; exit 2; }
FW_VERSION=$1
export FW_VERSION
cd "$(dirname "$0")/.."
# Generate metadata before make checks version2.c dependencies. Parallel generation
# in the upstream default goal can otherwise link yesterday's version object.
make -C firmware/pixl-faba/fw/application version BOARD=KEYPAD RELEASE=1 APP_VERSION=21601
make -C firmware/pixl-faba/fw/application -j8 BOARD=KEYPAD RELEASE=1 APP_VERSION=21601 OUTPUT_DIRECTORY=../_build_faba
python3 - "$FW_VERSION" <<'PY'
import sys
from pathlib import Path
assert sys.argv[1].encode() + b'\0' in Path('firmware/pixl-faba/fw/_build_faba/pixljs.bin').read_bytes(), 'Wrong firmware label'
print('Expected version label verified in compiled image')
PY
