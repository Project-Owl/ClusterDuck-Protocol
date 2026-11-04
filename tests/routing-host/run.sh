#!/bin/bash
# Build and run the routing host tests, once with remote commands off (the
# default) and once with CDPCFG_REMOTE_COMMANDS=1. See README.md.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
cdp="$(cd "$here/../.." && pwd)"
deps="${CDP_LIBDEPS:-$cdp/.pio/libdeps/local_lilygo_t_beam_sx1262}"
json="${ARDUINOJSON_SRC:-$deps/ArduinoJson/src}"
crc="${CRC32_SRC:-$deps/CRC32/src}"
if [[ ! -d "$json" || ! -d "$crc" ]]; then
    echo "ArduinoJson or CRC32 sources not found. Build any example with" >&2
    echo "'pio run -e local_lilygo_t_beam_sx1262' first, or set ARDUINOJSON_SRC and CRC32_SRC." >&2
    exit 1
fi
out="${TMPDIR:-/tmp}/cdp-routing-host"
flags=(-std=c++20 -g -O1 -Wno-deprecated-declarations -Wno-c++11-narrowing -Wno-inconsistent-missing-override
       -fsanitize=address,undefined -fno-omit-frame-pointer)
for commands in 0 1; do
    echo "== CDPCFG_REMOTE_COMMANDS=$commands"
    "${CXX:-clang++}" "${flags[@]}" -DCDPCFG_REMOTE_COMMANDS=$commands \
        -include "$here/stubs/common.h" -I "$here/stubs" -I "$cdp/src" -I "$json" -I "$crc" \
        "$here/mesh.cpp" "$cdp/src/CdpPacket.cpp" "$cdp/src/utils/DuckUtils.cpp" \
        "$cdp/src/routing/DuckRouter.cpp" "$cdp/src/routing/bloomfilter.cpp" "$crc/CRC32.cpp" \
        -o "$out-$commands"
    "$out-$commands"
done
