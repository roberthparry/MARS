#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
TMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/mars-bench-docs.XXXXXX")
trap 'rm -rf "$TMP_DIR"' EXIT

DOC_REPEATS=${MARS_BENCH_REPEATS:-51}
declare -a ALL_TARGETS=(mfloat number mcomplex mint mrational qfloat qcomplex)
declare -a REQUESTED_TARGETS=("$@")
declare -a UPDATED_DOCS=()

target_selected() {
  local target=$1
  local requested

  if ((${#REQUESTED_TARGETS[@]} == 0)); then
    return 0
  fi

  for requested in "${REQUESTED_TARGETS[@]}"; do
    if [[ $requested == "$target" ]]; then
      return 0
    fi
  done

  return 1
}

assert_known_targets() {
  local target
  local known
  local found

  for target in "${REQUESTED_TARGETS[@]}"; do
    found=0
    for known in "${ALL_TARGETS[@]}"; do
      if [[ $target == "$known" ]]; then
        found=1
        break
      fi
    done
    if ((found == 0)); then
      printf 'Unknown benchmark doc target: %s\n' "$target" >&2
      printf 'Known targets: %s\n' "${ALL_TARGETS[*]}" >&2
      exit 1
    fi
  done
}

build_target() {
  make "$1"
}

run_md_bench() {
  local output_path=$1
  shift

  MARS_BENCH_FORMAT=md "$@" >"$output_path"
}

replace_block() {
  local doc_path=$1
  local start_marker=$2
  local end_marker=$3
  local replacement_path=$4

  DOC_PATH=$doc_path \
  START_MARKER=$start_marker \
  END_MARKER=$end_marker \
  REPLACEMENT_PATH=$replacement_path \
  perl -0pi -e '
    use strict;
    use warnings;

    my $replacement_path = $ENV{"REPLACEMENT_PATH"};
    open my $replacement_fh, "<", $replacement_path
      or die "Unable to open replacement file $replacement_path: $!";
    local $/;
    my $replacement = <$replacement_fh>;
    close $replacement_fh;

    my $start = quotemeta($ENV{"START_MARKER"});
    my $end = quotemeta($ENV{"END_MARKER"});
    my $pattern = qr/$start.*?$end/s;

    s/$pattern/$replacement/
      or die "Failed to replace block in $ENV{DOC_PATH}\n";
  ' "$ROOT_DIR/$doc_path"
}

register_updated_doc() {
  UPDATED_DOCS+=("$1")
}

update_mfloat() {
  cat >"$TMP_DIR/mfloat_block.md" <<EOF
Current sample results from that command on this tree, measured on:

- \`Linux x86_64\`
- kernel \`6.8.0-110-generic\`
- \`Intel(R) Core(TM) i7-4510U CPU @ 2.00GHz\`
- \`4\` logical CPUs

Markdown output uses the release benchmark with \`$DOC_REPEATS\` timed batches per case,
records the sample median for each row, and keeps at least \`3\` inner
iterations even for the slowest docs-mode rows.

Results:

EOF
  run_md_bench "$TMP_DIR/mfloat_table.md" "$ROOT_DIR/build/release/bench/mfloat/bench_mfloat_maths"
  cat "$TMP_DIR/mfloat_table.md" >>"$TMP_DIR/mfloat_block.md"
  printf '\nFor broader benchmark notes, see [`docs/benchmarks.md`](benchmarks.md).\n' >>"$TMP_DIR/mfloat_block.md"

  replace_block \
    docs/mfloat.md \
    "Current sample results from that command on this tree, measured on:" \
    "For broader benchmark notes, see [\`docs/benchmarks.md\`](benchmarks.md)." \
    "$TMP_DIR/mfloat_block.md"

  register_updated_doc docs/mfloat.md
}

update_number() {
  cat >"$TMP_DIR/number_block.md" <<EOF
Current sample results from that command on this tree, measured on:

- \`Linux x86_64\`
- kernel \`6.8.0-110-generic\`
- \`Intel(R) Core(TM) i7-4510U CPU @ 2.00GHz\`
- \`4\` logical CPUs

Markdown output uses the release benchmark with \`$DOC_REPEATS\` timed batches per case,
records the sample median for each row, and keeps at least \`3\` inner
iterations even for the slowest docs-mode rows.

Results (microseconds per call):

EOF
  run_md_bench "$TMP_DIR/number_table.md" "$ROOT_DIR/build/release/bench/number/bench_number_maths"
  cat "$TMP_DIR/number_table.md" >>"$TMP_DIR/number_block.md"
  printf '\nFor broader benchmark notes, see [`docs/benchmarks.md`](benchmarks.md).\n' >>"$TMP_DIR/number_block.md"

  replace_block \
    docs/number.md \
    "Current sample results from that command on this tree, measured on:" \
    "For broader benchmark notes, see [\`docs/benchmarks.md\`](benchmarks.md)." \
    "$TMP_DIR/number_block.md"

  register_updated_doc docs/number.md
}

update_mcomplex() {
  cat >"$TMP_DIR/mcomplex_block.md" <<EOF
Current sample results from a fresh local run on this tree, measured on:

- \`Linux x86_64\`
- kernel \`6.8.0-110-generic\`
- \`Intel(R) Core(TM) i7-4510U CPU @ 2.00GHz\`
- \`4\` logical CPUs

Markdown output uses the release benchmark with \`$DOC_REPEATS\` timed batches per case,
records the sample median for each row, and keeps at least \`3\` inner
iterations even for the slowest docs-mode rows.

Results with genuinely complex inputs, limited to rows we have measured cleanly
on the current native \`mcomplex\` implementation:

EOF
  run_md_bench "$TMP_DIR/mcomplex_table.md" "$ROOT_DIR/build/release/bench/mcomplex/bench_mcomplex_maths"
  cat "$TMP_DIR/mcomplex_table.md" >>"$TMP_DIR/mcomplex_block.md"
  cat >>"$TMP_DIR/mcomplex_block.md" <<EOF

The table below compares the same \`number_t\` operations when the underlying
complex value is the private exact-preserving \`complex_t\` backend versus a
forced legacy \`mcomplex_t\` backend, measured at
\`${MARS_BENCH_COMPLEX_BITS:-256}\` bits. Ratios are
\`complex_t / mcomplex_t\`, so values above \`1.00x\` mean the private
\`complex_t\` path is slower for that row.

EOF
  MARS_BENCH_REPEATS=${MARS_BENCH_COMPLEX_COMPARE_REPEATS:-9} \
  MARS_BENCH_COMPLEX_BITS=${MARS_BENCH_COMPLEX_BITS:-256} \
      run_md_bench "$TMP_DIR/mcomplex_compare_table.md" \
      "$ROOT_DIR/build/release/bench/number/bench_number_complex_compare"
  cat "$TMP_DIR/mcomplex_compare_table.md" >>"$TMP_DIR/mcomplex_block.md"
  printf '\nFor broader benchmark notes, see [`docs/benchmarks.md`](benchmarks.md).\n' >>"$TMP_DIR/mcomplex_block.md"

  replace_block \
    docs/mcomplex.md \
    "Current sample results from a fresh local run on this tree, measured on:" \
    "For broader benchmark notes, see [\`docs/benchmarks.md\`](benchmarks.md)." \
    "$TMP_DIR/mcomplex_block.md"

  register_updated_doc docs/mcomplex.md
}

update_mint() {
  cat >"$TMP_DIR/mint_block.md" <<EOF
Rows are grouped by operation and left-operand bit band, columns by
right-operand bit band, and each cell reports a robust median across
representative exact integers in the bucket, with each representative input
measured from \`$DOC_REPEATS\` timed batches.

Recent measured timings on this tree, in nanoseconds per operation, were:

EOF
  run_md_bench "$TMP_DIR/mint_table.md" "$ROOT_DIR/build/release/bench/mint/bench_mint_arith"
  cat "$TMP_DIR/mint_table.md" >>"$TMP_DIR/mint_block.md"
  printf '\n## Internal Layout\n' >>"$TMP_DIR/mint_block.md"

  replace_block \
    docs/mint.md \
    "Rows are grouped by operation and left-operand bit band, columns by" \
    "## Internal Layout" \
    "$TMP_DIR/mint_block.md"

  register_updated_doc docs/mint.md
}

update_mrational() {
  cat >"$TMP_DIR/mrational_block.md" <<EOF
Recent measured timings, as robust medians from representative inputs measured
with \`$DOC_REPEATS\` timed batches and reported in nanoseconds per operation, were:

EOF
  run_md_bench "$TMP_DIR/mrational_table.md" "$ROOT_DIR/build/release/bench/mrational/bench_mrational_arith"
  cat "$TMP_DIR/mrational_table.md" >>"$TMP_DIR/mrational_block.md"
  printf '\nThese figures come from the current banded benchmark in\n[`bench/mrational/bench_mrational_arith.c`](../bench/mrational/bench_mrational_arith.c).\n' >>"$TMP_DIR/mrational_block.md"

  replace_block \
    docs/mrational.md \
    "Recent measured timings, as robust medians from representative inputs measured" \
    "These figures come from the current banded benchmark in
[\`bench/mrational/bench_mrational_arith.c\`](../bench/mrational/bench_mrational_arith.c)." \
    "$TMP_DIR/mrational_block.md"

  register_updated_doc docs/mrational.md
}

update_qfloat() {
  cat >"$TMP_DIR/qfloat_block.md" <<EOF
Current sample results from that command on this tree, measured on:

- \`Linux x86_64\`
- kernel \`6.8.0-110-generic\`
- \`Intel(R) Core(TM) i7-4510U CPU @ 2.00GHz\`
- \`4\` logical CPUs

Markdown output uses the release benchmark with \`$DOC_REPEATS\` timed batches per case,
records the sample median for each row, and times only the actual \`qfloat\`
math operation after argument parsing and warmup.

Results:

EOF
  run_md_bench "$TMP_DIR/qfloat_table.md" "$ROOT_DIR/build/release/bench/qfloat/bench_qfloat_gamma_maths"
  cat "$TMP_DIR/qfloat_table.md" >>"$TMP_DIR/qfloat_block.md"
  printf '\nFor a broader benchmark overview, see\n[`docs/benchmarks.md`](benchmarks.md).\n' >>"$TMP_DIR/qfloat_block.md"

  replace_block \
    docs/qfloat.md \
    "Current sample results from that command on this tree, measured on:" \
    "For a broader benchmark overview, see
[\`docs/benchmarks.md\`](benchmarks.md)." \
    "$TMP_DIR/qfloat_block.md"

  register_updated_doc docs/qfloat.md
}

update_qcomplex() {
  cat >"$TMP_DIR/qcomplex_block.md" <<EOF
Current sample results from that command on this tree, measured on:

- \`Linux x86_64\`
- kernel \`6.8.0-110-generic\`
- \`Intel(R) Core(TM) i7-4510U CPU @ 2.00GHz\`
- \`4\` logical CPUs

Markdown output uses the release benchmark with \`$DOC_REPEATS\` timed batches per case,
records the sample median for each row, and times only the actual \`qcomplex\`
math operation after argument parsing and warmup.

Results:

EOF
  run_md_bench "$TMP_DIR/qcomplex_table.md" "$ROOT_DIR/build/release/bench/qcomplex/bench_qcomplex_maths"
  cat "$TMP_DIR/qcomplex_table.md" >>"$TMP_DIR/qcomplex_block.md"
  printf '\nFor a broader benchmark overview, see\n[`docs/benchmarks.md`](benchmarks.md).\n' >>"$TMP_DIR/qcomplex_block.md"

  replace_block \
    docs/qcomplex.md \
    "Current sample results from that command on this tree, measured on:" \
    "For a broader benchmark overview, see
[\`docs/benchmarks.md\`](benchmarks.md)." \
    "$TMP_DIR/qcomplex_block.md"

  register_updated_doc docs/qcomplex.md
}

cd "$ROOT_DIR"
assert_known_targets

if target_selected mfloat; then
  build_target bench_mfloat_maths
fi
if target_selected number; then
  build_target bench_number_maths
fi
if target_selected mcomplex; then
  build_target bench_mcomplex_maths
  build_target bench_number_complex_compare
fi
if target_selected mint; then
  build_target bench_mint_arith
fi
if target_selected mrational; then
  build_target bench_mrational_arith
fi
if target_selected qfloat; then
  build_target bench_qfloat_gamma_maths
fi
if target_selected qcomplex; then
  build_target bench_qcomplex_maths
fi

if target_selected mfloat; then
  update_mfloat
fi
if target_selected number; then
  update_number
fi
if target_selected mcomplex; then
  update_mcomplex
fi
if target_selected mint; then
  update_mint
fi
if target_selected mrational; then
  update_mrational
fi
if target_selected qfloat; then
  update_qfloat
fi
if target_selected qcomplex; then
  update_qcomplex
fi

printf 'Updated benchmark tables in %s.\n' "${UPDATED_DOCS[*]}"
