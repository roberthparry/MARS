#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/../.." && pwd)"
cd "${repo_root}"

ref="${1:-HEAD}"
tmpdir="$(mktemp -d /tmp/mars_mint_div_compare.XXXXXX)"
cleanup() {
    rm -rf "$tmpdir"
}
trap cleanup EXIT

echo "Preparing baseline workspace from ${ref}..."
cp -a . "$tmpdir/repo"

git -C "$tmpdir/repo" show "${ref}:src/mint/mint.c" > "$tmpdir/repo/src/mint/mint.c"
git -C "$tmpdir/repo" show "${ref}:include/mint.h" > "$tmpdir/repo/include/mint.h"

# Keep using the current benchmark driver so results stay like-for-like.
mkdir -p "$tmpdir/repo/bench/mint"
cp "bench/mint/bench_mint_div.c" "$tmpdir/repo/bench/mint/bench_mint_div.c"

echo
echo "Current tree"
make bench_mint_div >/dev/null
build/release/bench/bench_mint_div

echo
echo "Baseline (${ref})"
make -C "$tmpdir/repo" bench_mint_div >/dev/null
"$tmpdir/repo/build/release/bench/bench_mint_div"
