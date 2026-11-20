#!/bin/sh
set -eu
umask 077

if [ "$#" -ne 2 ]; then
    echo "Usage: sh tools/provision-config.sh /private/path/config.json /dev/cu.usbmodemXXXX" >&2
    exit 2
fi
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
config=$(CDPATH= cd -- "$(dirname -- "$1")" && pwd)/$(basename -- "$1")
port=$2
test -f "$config"
if [ ! -x "$root/build-desktop/weather-config-check" ]; then
    echo "Run make desktop before provisioning." >&2
    exit 1
fi
"$root/build-desktop/weather-config-check" "$config"
generator="$root/.tools/mklittlefs-native"
if [ ! -x "$generator/mklittlefs" ]; then
    mkdir -p "$root/.tools"
    git clone --depth 1 --branch 4.0.1 --recurse-submodules --shallow-submodules \
        https://github.com/earlephilhower/mklittlefs.git "$generator"
    test "$(git -C "$generator" rev-parse HEAD)" = cc32c94f300c2c9c2a27f86296ee0958c27774a0
    test "$(git -C "$generator/littlefs" rev-parse HEAD)" = d01280e64934a09ba16cac60cf9d3a37e228bb66
    git -C "$generator" apply "$root/tools/mklittlefs-native.patch"
    make -C "$generator" CC=clang CXX=clang++ LITTLEFS_VERSION=d01280e
fi
staging=$(mktemp -d "${TMPDIR:-/tmp}/weather-config.XXXXXX")
trap 'rm -f "$staging/files/config.json" "$staging/config.bin"; rmdir "$staging/files" "$staging"' EXIT HUP INT TERM
mkdir "$staging/files"
cp "$config" "$staging/files/config.json"
"$generator/mklittlefs" -c "$staging/files" -b 4096 -s 1048576 "$staging/config.bin"
core=${PLATFORMIO_CORE_DIR:-"$HOME/.platformio"}
"$core/penv/bin/python" "$core/packages/tool-esptoolpy/esptool.py" \
    --chip esp32s3 --port "$port" --baud 460800 write_flash 0x610000 "$staging/config.bin"
