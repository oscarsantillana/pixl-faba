#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
python3 tools/test-faba-catalog.py
python3 tools/test-faba-player.py
python3 tools/test-faba-menu-memory.py
python3 tools/test-faba-app-lifecycle.py
python3 tools/replay-faba-state.py
python3 tools/test-decode-faba-trace.py
check_dir=$(mktemp -d)
trap 'rm -rf "$check_dir"' EXIT
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
 -Ifirmware/pixl-faba/fw/application/src/mod tools/test-faba-trace.c \
 firmware/pixl-faba/fw/application/src/mod/faba_trace.c -o "$check_dir/trace"
"$check_dir/trace" "$check_dir/sample.trace"
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
 -Ifirmware/pixl-faba/fw/application/src/mod -Itools/faba-test-support \
 -Ifirmware/pixl-faba/fw/application/src/mod/vfs tools/test-faba-trace-store.c \
 firmware/pixl-faba/fw/application/src/mod/faba_trace.c \
 firmware/pixl-faba/fw/application/src/mod/faba_trace_store.c -o "$check_dir/store"
"$check_dir/store"
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
 -Ifirmware/pixl-faba/fw/application/src/mod -Itools/faba-timing-test-support \
 tools/test-faba-trace-timing.c firmware/pixl-faba/fw/application/src/mod/faba_trace_timing.c \
 -o "$check_dir/timing"
"$check_dir/timing"

python3 tools/test-mui-event-queue.py
python3 tools/test-faba-diag.py

cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
 -Ifirmware/pixl-faba/fw/application/src/mod -Ifirmware/pixl-faba/fw/application/src/mod/vfs \
 -Itools/faba-test-support tools/test-faba-favorites.c -o "$check_dir/favorites"
"$check_dir/favorites"
