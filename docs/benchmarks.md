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
make bench_mfloat_maths
make bench_mcomplex_maths
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

## Integer and Multiprecision Benchmarks

The repository also includes focused arithmetic benchmarks for the newer
numeric cores.

### `mint`

Available benchmark targets include:

```sh
make bench_mint_add
make bench_mint_mul
make bench_mint_div
make bench_mint_gcd
make bench_mint_combinatorics
```

These cover the main optimisation areas in the arbitrary-precision integer
subsystem: small/native-word fast paths, wider multiply/divide behaviour,
`gcd`/`lcm`/`modinv`, and combinatorics helpers.

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
exp_1                 bits=256  avg_µs=   1.146 avg_ms=     0.001
log_10                bits=256  avg_µs=   2.222 avg_ms=     0.002
gamma_2_3             bits=256  avg_µs=   1.107 avg_ms=     0.001
lgamma_2_3            bits=256  avg_µs=   3.562 avg_ms=     0.004
gammainv_9_5          bits=256  avg_µs=  83.440 avg_ms=     0.083
lambert_wm1_-0_1      bits=256  avg_µs=  55.965 avg_ms=     0.056
logbeta_2_3_4_5       bits=256  avg_µs=  17.997 avg_ms=     0.018
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
exp_1_plus_1i                avg_µs=     3.582 avg_ms=     0.004
log_1_plus_1i                avg_µs=     5.405 avg_ms=     0.005
erf_0_5_plus_0_5i            avg_µs=    10.404 avg_ms=     0.010
erfc_0_5_plus_0_5i           avg_µs=     9.357 avg_ms=     0.009
gamma_1_5_plus_0_7i          avg_µs=    14.554 avg_ms=     0.015
lgamma_1_5_plus_0_7i         avg_µs=    13.515 avg_ms=     0.014
digamma_2_plus_1i            avg_µs=    14.643 avg_ms=     0.015
trigamma_2_plus_0_5i         avg_µs=     4.596 avg_ms=     0.005
tetragamma_2_plus_0_5i       avg_µs=     6.256 avg_ms=     0.006
gammainv_gamma_2_5           avg_µs=    95.411 avg_ms=     0.095
gammainv_gamma_2_5_0_3i      avg_µs=   219.491 avg_ms=     0.219
productlog_1_plus_1i         avg_µs=    30.014 avg_ms=     0.030
lambert_wm1_-0_2_-0_1i       avg_µs=    27.295 avg_ms=     0.027
ei_1_plus_1i                 avg_µs=    44.938 avg_ms=     0.045
e1_1_plus_1i                 avg_µs=    41.641 avg_ms=     0.042
beta_1_5_0_5__2_-0_3         avg_µs=    38.087 avg_ms=     0.038
logbeta_1_5_0_5__2_-0_3      avg_µs=    37.286 avg_ms=     0.037
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
exp_1_plus_1i_512            bits=512  avg_µs=    12.014 avg_ms=     0.012
log_1_plus_1i_512            bits=512  avg_µs=    17.694 avg_ms=     0.018
productlog_1_plus_1i_512     bits=512  avg_µs= 92528.000 avg_ms=    92.528
ei_1_plus_1i_512             bits=512  avg_µs=    59.575 avg_ms=     0.060
e1_1_plus_1i_512             bits=512  avg_µs=    56.693 avg_ms=     0.057
```

The current `mcomplex` implementation still has remaining wrapper-era paths, so
these numbers should be read as active optimization checkpoints rather than
final end-state timings. The bench target still covers the pure-real gamma and
lgamma cases; this snapshot highlights the complex hot paths we tuned in this
phase.
