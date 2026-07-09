#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
OUT="$ROOT/build/fat-tests"
BIN="$ROOT/build/fat-host-test"
mkdir -p "$OUT"
rm -f "$OUT"/*.img "$BIN"

cc -std=gnu99 -Wall -Wextra -Werror -I"$ROOT/include" \
    "$ROOT/tools/tests/fat_host_test.c" "$ROOT/kernel/fs/fat.c" \
    "$ROOT/kernel/drivers/storage/block_cache.c" "$ROOT/kernel/lib/string.c" -o "$BIN"

make_image() {
    type=$1
    size=$2
    image="$OUT/fat$type-clean.img"
    truncate -s "$size" "$image"
    mkfs.fat -F "$type" "$image" >/dev/null
    "$BIN" "$image" "$type" exercise
    fsck.fat -vn "$image" >/dev/null

    corrupt_image="$OUT/fat$type-corrupt.img"
    cp "$image" "$corrupt_image"
    "$BIN" "$corrupt_image" "$type" corrupt
    if fsck.fat -vn "$corrupt_image" >/dev/null 2>&1; then
        echo "FAIL FAT$type corruption was not detected by fsck.fat" >&2
        exit 1
    fi
    echo "PASS FAT$type external corruption detection"
}

make_image 12 2M
make_image 16 32M
make_image 32 128M

echo "All FAT boundary, multi-cluster, reuse, corruption, and directory tests passed."
