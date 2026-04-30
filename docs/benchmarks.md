# Benchmarks

MARS includes a small set of focused benchmark binaries for symbolic and
integrator-heavy paths.

The sample results in this document were collected on an
`Intel(R) Core(TM) i7-4510U CPU @ 2.00GHz` laptop, with `4` logical CPUs
(`2` cores, `2` threads per core). Treat them as illustrative rather than as a
strict cross-machine baseline.

## Running Benchmarks

From the repository root:

```sh
make bench_integrator
make bench_matrix_dval
```

As with the test suites, prefer running benchmarks sequentially for now. The
current codebase and build products are not yet fully thread-safe for
overlapping runs.

## Output Units

Current benchmark output reports average wall-clock time per call in both
microseconds and milliseconds:

- `avg_µs=561.060 avg_ms=0.561` means about `561 µs`
- `avg_µs=47154.829 avg_ms=47.155` means about `47.15 ms`

## Integrator Benchmark

`bench_integrator` measures symbolic fast paths in the adaptive integrator.

It reports:

- matched symbolic shortcut families
- nearby fallback cases that look similar but miss the exact matcher path
- average time per integral call
- interval count used on the warmup run

Sample output:

```text
iters=200

Matched shortcut families
affine_exp             intervals=1    avg_µs=     7.367 avg_ms=     0.007
affine_square          intervals=1    avg_µs=     1.238 avg_ms=     0.001
affine_quartic         intervals=1    avg_µs=     1.408 avg_ms=     0.001
affine_times_exp       intervals=1    avg_µs=    19.045 avg_ms=     0.019
affine_times_sin       intervals=1    avg_µs=    27.537 avg_ms=     0.028
affine_times_sinh      intervals=1    avg_µs=    37.119 avg_ms=     0.037

Near misses (generic path)
near_miss_square       intervals=1    avg_µs=   179.185 avg_ms=     0.179
near_miss_quartic      intervals=1    avg_µs=  5976.778 avg_ms=     5.977
near_miss_exp          intervals=2    avg_µs= 15188.762 avg_ms=    15.189
near_miss_sin          intervals=2    avg_µs= 34710.213 avg_ms=    34.710
near_miss_sinh         intervals=2    avg_µs= 34462.050 avg_ms=    34.462
```

The useful comparison is usually “matched case versus near miss,” since that
shows how much work the symbolic shortcut is saving.

## Symbolic Matrix Benchmark

`bench_matrix_dval` measures dense symbolic `MAT_TYPE_DVAL` linear algebra on
representative exact cases.

It currently covers:

- dense symbolic solve on `3x3` and `6x6` systems
- dense symbolic inverse on `4x4` and `6x6` systems

Sample output:

```text
iters=40

Symbolic dval solve
solve_dense3x3_rhs2      avg_µs=   561.060 avg_ms=     0.561
solve_dense6x6_rhs2      avg_µs= 47154.829 avg_ms=    47.155

Symbolic dval inverse
inverse_dense4x4         avg_µs=  2525.437 avg_ms=     2.525
inverse_dense6x6         avg_µs= 66841.376 avg_ms=    66.841
```

These numbers are intended as a rough baseline for the current
fraction-free symbolic elimination path rather than as a strict performance
contract.
