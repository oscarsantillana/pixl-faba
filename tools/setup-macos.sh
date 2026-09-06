#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
root=$(pwd)
command -v swiftc >/dev/null
command -v git >/dev/null
mkdir -p .deps .build/bin .build/module-cache
nordic="$root/.deps/IOS-DFU-Library"
revision=9c87e9fbce1980487373c2330f066612dad5ba82
if [ ! -e "$nordic" ]; then
 git clone --branch 4.17.0 --depth 1 https://github.com/NordicSemiconductor/IOS-DFU-Library.git "$nordic"
fi
[ "$(git -C "$nordic" rev-parse HEAD)" = "$revision" ] || { echo 'Unexpected Nordic revision; use a fresh checkout.' >&2; exit 1; }
if git -C "$nordic" apply --check "$root/docs/toolchain/nordic-swift59.patch" 2>/dev/null; then
 git -C "$nordic" apply "$root/docs/toolchain/nordic-swift59.patch"
else
 git -C "$nordic" apply --reverse --check "$root/docs/toolchain/nordic-swift59.patch"
fi
for name in pixl-scan pixl-read-slots pixl-read-favorites pixl-upload-catalog pixl-enter-dfu pixl-read-faba-diag pixl-read-trace; do
 swiftc -module-cache-path "$root/.build/module-cache" "tools/$name.swift" \
  -Xlinker -sectcreate -Xlinker __TEXT -Xlinker __info_plist \
  -Xlinker "$root/docs/toolchain/Info.plist" -o ".build/bin/$name"
done
swiftc tools/verify-faba-signature.swift -o .build/bin/verify-faba-signature
swift build -c release --package-path tools/pixl-dfu
cp tools/pixl-dfu/.build/release/PixlDFU .build/bin/PixlDFU
.build/bin/PixlDFU validate release/pixl-faba-2.16.1-faba10-preferidos.zip
.build/bin/verify-faba-signature evidence/faba10
printf 'Tools ready in %s/.build/bin. No Bluetooth connection or firmware write was performed.\n' "$root"
