#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/../.." && pwd)"
cd "${repo_root}"

ref="${1:-HEAD}"
tmpdir="$(mktemp -d /tmp/mars_mfloat_maths_compare.XXXXXX)"
cleanup() {
    rm -rf "$tmpdir"
}
trap cleanup EXIT

echo "Preparing baseline workspace from ${ref}..."
cp -a . "$tmpdir/repo"

mkdir -p "$tmpdir/repo/src/mfloat" "$tmpdir/repo/include" "$tmpdir/repo/bench/mfloat"

for f in include/mfloat.h \
         src/mfloat/mfloat_core.c \
         src/mfloat/mfloat_arith.c \
         src/mfloat/mfloat_maths.c \
         src/mfloat/mfloat_string.c \
         src/mfloat/mfloat_print.c \
         src/mfloat/mfloat_internal.h; do
    if ! git -C "$tmpdir/repo" cat-file -e "${ref}:${f}" 2>/dev/null; then
        echo "Reference ${ref} does not contain ${f}."
        echo "Pick a later ref that already includes the mfloat subsystem."
        exit 1
    fi
    git -C "$tmpdir/repo" show "${ref}:${f}" > "$tmpdir/repo/${f}"
done

# Keep using the current benchmark driver so results stay like-for-like.
cp "bench/mfloat/bench_mfloat_maths.c" "$tmpdir/repo/bench/mfloat/bench_mfloat_maths.c"

echo
echo "Current tree"
make bench_mfloat_maths >/dev/null
build/release/bench/bench_mfloat_maths

echo
echo "Baseline (${ref})"
make -C "$tmpdir/repo" bench_mfloat_maths >/dev/null
"$tmpdir/repo/build/release/bench/bench_mfloat_maths"
