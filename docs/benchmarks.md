# Benchmarks

MARS includes a small set of focused benchmark binaries for symbolic,
integrator-heavy, and arithmetic-core paths.

The sample results in this document were collected on an
`Intel(R) Core(TM) i7-4510U CPU @ 2.00GHz` laptop, with `4` logical CPUs
(`2` cores, `2` threads per core). Treat them as illustrative rather than as a
strict cross-machine baseline.

## Running Benchmarks

From the repository root:

```sh
make bench_integrator
make bench_matrix_dval
make bench_mint_mul
make bench_mint_div
make bench_mrational_arith
make bench_mfloat_maths
make bench_mcomplex_maths
make bench_number_maths
```

As with the test suites, prefer running benchmarks sequentially for now. The
current codebase and build products are not yet fully thread-safe for
overlapping runs.

To refresh the benchmark tables in the numeric docs from the release benches,
run:

```sh
tools/bench-docs/update_bench_docs.sh
```

To refresh only a subset of those docs, pass one or more target names:

```sh
tools/bench-docs/update_bench_docs.sh qfloat qcomplex
```

## Output Units

Current benchmark output reports robust per-call timing estimates rather than a
single average:

- plain-text scalar benches print the sample median, MAD, and a bootstrap 95%
  confidence interval
- markdown tables print the median in a compact display unit, keeping `1dp` in
  the base unit and promoting to the next unit with `3dp` once the value
  reaches `1000`

## Integrator Benchmark

`bench_integrator` measures symbolic fast paths in the adaptive integrator.

It reports:

- matched symbolic shortcut families
- nearby fallback cases that look similar but miss the exact matcher path
- average time per integral call
- interval count used on the warmup run

Sample output:

```text
iters=10

Matched shortcut families
affine_exp             intervals=1    avg_µs=   218.240 avg_ms=     0.218
affine_square          intervals=1    avg_µs=   233.723 avg_ms=     0.234
affine_quartic         intervals=1    avg_µs=    61.877 avg_ms=     0.062
affine_times_exp       intervals=1    avg_µs=    77.678 avg_ms=     0.078
affine_cube_exp        intervals=1    avg_µs=    87.244 avg_ms=     0.087
affine_times_sin       intervals=1    avg_µs=    82.199 avg_ms=     0.082
affine_cube_sin        intervals=1    avg_µs=   106.263 avg_ms=     0.106
affine_times_sinh      intervals=1    avg_µs=   147.103 avg_ms=     0.147
affine_cube_sinh       intervals=1    avg_µs=   123.834 avg_ms=     0.124

Near misses (generic path)
near_miss_square       intervals=1    avg_µs=  1647.171 avg_ms=     1.647
near_miss_quartic      intervals=1    avg_µs= 17767.062 avg_ms=    17.767
near_miss_exp          intervals=2    avg_µs= 49084.036 avg_ms=    49.084
near_miss_sin          intervals=2    avg_µs= 40497.935 avg_ms=    40.498
near_miss_sinh         intervals=2    avg_µs= 32043.325 avg_ms=    32.043
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
iters=3

Symbolic dval solve
solve_dense3x3_rhs2      avg_µs=  6000.421 avg_ms=     6.000
solve_dense6x6_rhs2      avg_µs=566623.682 avg_ms=   566.624

Symbolic dval inverse
inverse_dense4x4         avg_µs= 40635.324 avg_ms=    40.635
inverse_dense6x6         avg_µs=876341.423 avg_ms=   876.341
```

These numbers are intended as a rough baseline for the current
fraction-free symbolic elimination path rather than as a strict performance
contract.

## Integer and Multiprecision Benchmarks

The repository also includes focused arithmetic benchmarks for the newer
numeric cores.

### `mint`

Available benchmark targets include:

```sh
make bench_mint_arith
make bench_mint_add
make bench_mint_mul
make bench_mint_div
make bench_mint_gcd
make bench_mint_combinatorics
```

These cover the main optimisation areas in the arbitrary-precision integer
subsystem: small/native-word fast paths, wider multiply/divide behaviour,
`gcd`/`lcm`/`modinv`, combinatorics helpers, and a banded operand-size matrix
for the core binary arithmetic paths.

### `mrational`

Available benchmark target:

```sh
make bench_mrational_arith
```

This benchmark reports one combined timing table for:

- `mr_add`
- `mr_sub`
- `mr_mul`
- `mr_div`
- `mr_inv`

Rows are grouped by operation and numerator-bit band, columns by
denominator-bit band, and each cell reports the average time per call.

### `mfloat`

The native multiprecision floating-point layer now has:

```sh
make bench_mfloat_maths
```

This benchmark reports direct timings for native `mfloat` math paths such as:

- `exp`
- `log`
- `sin`
- `cos`
- `atan`
- `pow`
- `pi`, `e`, and Euler–Mascheroni construction at higher precision

There is also a compare helper:

```sh
bench/mfloat/compare_mfloat_maths.sh <git-ref>
```

Use a reference that already contains the `mfloat` subsystem.

### `number`

Available benchmark target:

```sh
make bench_number_maths
```

There is also a scope-focused benchmark target:

```sh
make bench_number_scope
```

This benchmark mirrors the `mfloat` maths coverage through the generic
`number_t` API, including:

- elementary functions such as `exp`, `log`, `sqrt`, `sin`, `cos`, `atan`,
  `sinh`, `tanh`, and `pow`
- paired output functions such as `sincos` and `sinhcosh`
- special functions such as `gamma`, `lgamma`, `digamma`, `trigamma`,
  `tetragamma`, `erf`, `erfc`, `gammainv`, `lambert_w0`, `lambert_wm1`,
  `beta`, `logbeta`, `binomial`, `normal_pdf`, `normal_cdf`, `ei`, and `e1`
- named constants such as `pi`, `e`, and Euler-Mascheroni

Because `number_t` accepts exact integer and rational inputs as well as
floating-point values, this benchmark is useful for measuring the combined cost
of generic promotion, backend dispatch, and the underlying maths implementation
on the same workloads used by `bench_mfloat_maths`.

The scope benchmark focuses specifically on temporary-lifetime management. It
compares:

- fully manual ownership with explicit `num_destroy(...)`
- broad whole-scope temporary accumulation
- the preferred pattern of scoped short-lived intermediates plus an ordinary
  owned rolling result

Current sample timings on the benchmark machine for that scope benchmark:

```text
mfloat chain             manual= 129.082 ms  scoped= 125.772 ms  ratio= 1.026x
mfloat scoped+roll       manual= 129.082 ms  scoped= 110.554 ms  ratio= 1.168x
mcomplex chain           manual= 221.584 ms  scoped= 225.807 ms  ratio= 0.981x
mcomplex scoped+roll     manual= 221.584 ms  scoped= 198.489 ms  ratio= 1.116x
```

Current sample timings on the benchmark machine:

Use `MARS_BENCH_FORMAT=md` with the benchmark binaries when you want
docs-ready tables instead of the ordinary line-by-line terminal output. The
module-specific docs for [`mint`](mint.md), [`mrational`](mrational.md),
[`mfloat`](mfloat.md), [`number`](number.md), and [`mcomplex`](mcomplex.md)
include current sample tables captured that way.

### `qfloat`

Available benchmark target:

```sh
make bench_qfloat_gamma_maths
```

This benchmark currently covers a broader `qfloat` maths slice:

- `qf_exp(1)` and `qf_log(10)`
- `qf_erf(0.5)` and `qf_erfc(0.5)`
- `qf_gamma`, `qf_lgamma`, `qf_digamma`, `qf_trigamma`, and `qf_tetragamma`
- `qf_gammainv`
- `qf_lambert_w0` and `qf_lambert_wm1`
- `qf_ei` and `qf_e1`
- `qf_beta` and `qf_logbeta`

Current sample timings with `MARS_BENCH_SCALE=5` on the benchmark machine:

```text
exp_1                 bits=256  avg_µs=   1.370 avg_ms=     0.001
log_10                bits=256  avg_µs=   2.471 avg_ms=     0.002
gamma_2_3             bits=256  avg_µs=   1.246 avg_ms=     0.001
lgamma_2_3            bits=256  avg_µs=   3.797 avg_ms=     0.004
gammainv_9_5          bits=256  avg_µs=  82.548 avg_ms=     0.083
lambert_wm1_-0_1      bits=256  avg_µs=  58.889 avg_ms=     0.059
logbeta_2_3_4_5       bits=256  avg_µs=  17.206 avg_ms=     0.017
```

One implementation note matters here: on the current x86_64 machine, `qfloat`
release tests stay correct under `-O2` and `-O2 -flto`, but `-march=native
-mtune=native` caused `Ei`/`E1` regressions. Because of that, the user-facing
`qfloat` release targets are built with a safer release profile than the
project-wide native-tuned default.

### `qcomplex`

Available benchmark target:

```sh
make bench_qcomplex_maths
```

This benchmark covers a representative complex-valued slice:

- `qc_exp(1+i)` and `qc_log(1+i)`
- `qc_erf(0.5+0.5i)` and `qc_erfc(0.5+0.5i)`
- complex gamma-family calls including `qc_gamma`, `qc_lgamma`, `qc_digamma`,
  `qc_trigamma`, and `qc_tetragamma`
- real and genuinely complex `qc_gammainv` cases
- `qc_productlog` and `qc_lambert_wm1`
- `qc_ei`, `qc_e1`, `qc_beta`, and `qc_logbeta`

Current sample timings on the benchmark machine:

```text
exp_1_plus_1i                avg_µs=     2.513 avg_ms=     0.003
log_1_plus_1i                avg_µs=     3.503 avg_ms=     0.004
erf_0_5_plus_0_5i            avg_µs=     6.665 avg_ms=     0.007
erfc_0_5_plus_0_5i           avg_µs=     6.299 avg_ms=     0.006
gamma_1_5_plus_0_7i          avg_µs=     9.753 avg_ms=     0.010
lgamma_1_5_plus_0_7i         avg_µs=     8.058 avg_ms=     0.008
digamma_2_plus_1i            avg_µs=     9.108 avg_ms=     0.009
trigamma_2_plus_0_5i         avg_µs=     2.895 avg_ms=     0.003
tetragamma_2_plus_0_5i       avg_µs=     3.598 avg_ms=     0.004
gammainv_gamma_2_5           avg_µs=    55.586 avg_ms=     0.056
gammainv_gamma_2_5_0_3i      avg_µs=   145.943 avg_ms=     0.146
productlog_1_plus_1i         avg_µs=    19.744 avg_ms=     0.020
lambert_wm1_-0_2_-0_1i       avg_µs=    19.262 avg_ms=     0.019
ei_1_plus_1i                 avg_µs=    29.968 avg_ms=     0.030
e1_1_plus_1i                 avg_µs=    30.136 avg_ms=     0.030
beta_1_5_0_5__2_-0_3         avg_µs=    26.146 avg_ms=     0.026
logbeta_1_5_0_5__2_-0_3      avg_µs=    24.514 avg_ms=     0.025
```

### `mcomplex`

Available benchmark target:

```sh
make bench_mcomplex_maths
```

This benchmark mirrors the current `qcomplex` coverage and tracks the native
`mcomplex` replacement work:

- `mc_exp(1+i)` and `mc_log(1+i)`
- `mc_erf(0.5+0.5i)` and `mc_erfc(0.5+0.5i)`
- complex gamma-family calls including `mc_gamma`, `mc_lgamma`, `mc_digamma`,
  `mc_trigamma`, and `mc_tetragamma`
- pure-real `mc_gamma(2.3 + 0i)` and `mc_lgamma(2.3 + 0i)` for apples-to-apples
  comparison with `mfloat`
- real and genuinely complex `mc_gammainv` cases
- `mc_productlog` and `mc_lambert_wm1`
- `mc_ei`, `mc_e1`, `mc_beta`, and `mc_logbeta`

Recent sample timings on this tree:

```text
exp_0_567_plus_0_321i_512    bits=512  avg_µs=    21.237 avg_ms=     0.021
log_0_567_plus_0_321i_512    bits=512  avg_µs=    36.909 avg_ms=     0.037
productlog_1_plus_1i_512     bits=512  avg_µs=   144.221 avg_ms=     0.144
ei_1_plus_1i_512             bits=512  avg_µs=    31.214 avg_ms=     0.031
e1_1_plus_1i_512             bits=512  avg_µs=    31.427 avg_ms=     0.031
```

The current `mcomplex` implementation still has remaining wrapper-era paths, so
these numbers should be read as active optimisation checkpoints rather than
final end-state timings. The bench target still covers the pure-real gamma and
lgamma cases; this snapshot highlights the complex hot paths we tuned in this
phase.
