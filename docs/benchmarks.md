# Benchmarks

MARS includes persistent mathematical benchmarks and focused binaries for symbolic,
integrator-heavy and arithmetic-core paths.

## Persistent mathematical benchmarks

`make bench-db ARGS='sync'` creates or synchronises **bench/benchmarks.db**, a plain
SQLite database beside its maintained schema, **bench/benchmarks.sql**. It survives
`make clean`. Building, installing, launching MARS and reading benchmark status
never create it. Synchronisation preserves additional catalogue cases and applies
the stored history policy.

The benchmark code and `bench/benchmarks.sql` are shared through Git. The generated
`bench/benchmarks.db` and its SQLite sidecars are ignored, so a fresh clone on
another machine starts without timing history and creates its own database on
the first explicit synchronisation or benchmark run. Subsequent Git pulls
preserve that machine's local history. The published Markdown tables remain
shared reference results, labelled with the processor that produced them.

`make bench-db ARGS='run'` measures the full catalogue sequentially. Profiles include
double-backed `number_t` arguments (53 bits), direct `qfloat_t` and `qcomplex_t`
calls (approximately 106 bits), and real and complex `number_t` calculations at
256, 512, 768, 1024, 2048 and 4096 bits. The catalogue covers arithmetic, elementary
and special functions, distributions, polynomials and integer mathematics.
Names without a corresponding direct backend API are recorded as unsupported;
complex profiles skip operations restricted to real integers. Integer-only APIs
run once with exact operands in the `exact-integer` profile; an arbitrary floating
precision would be meaningless for those algorithms.

Integer workloads use a dense 1,024-bit dividend, a dense 512-bit divisor and
exponent, and the 521-bit Mersenne prime 2⁵²¹ − 1. The dividend and divisor share
the non-trivial 128-bit GCD 2·(2¹²⁷ − 1). Bit operations address positions 700 and
701 or shift by 257 bits. Factorisation uses the 1,215-bit smooth composite
2⁵¹²·3²⁵⁶·5¹²⁸, exercising large integers without an unbounded prime search.
Factorial and Fibonacci accept native integer indices by API design: the cases
5,000! and F(10,000) produce exact 54,233-bit and 6,942-bit results. The partition
case is p(5,000). Input parsing and exact-integer result checks are outside timing.
Saved arguments distinguish these workloads from earlier small-input timings.
In generated Markdown tables, integer Inputs cells occupy up to 80 character widths,
with each argument starting on a separate line. Other backends retain their 40-character
width and inline argument separators. Values wrap without truncation. Explicit line
breaks also limit their width in readers which discard inline styling. The database
and TSV exports retain the complete, unwrapped operands; terminal grids wrap to
the available terminal width.

Each run retains UTC dates, machine, processor, operating system, compiler, build
flags and content hashes of the linked library and executable. Inputs, precision,
iterations, clock resolution, individual timed samples, median, mean, minimum,
maximum and median absolute deviation are retained. The catalogue is separate
from `tests/tests.db`: numerical correctness remains the responsibility of the
regression suites. A finite benchmark result is not a proof of numerical accuracy.

### History and storage limits

The database defaults to **32 MiB**, **20 recent runs** and a **90-day maximum
age**. Original timing samples are retained for the **newest five runs**. Older
retained runs keep their per-case means, medians, extrema, sample counts, outcomes,
machine/build provenance, comparison values and outlier flags. Runs exceeding
the age or count limit are deleted completely; summaries do not accumulate forever.
These limits apply across the database, rather than separately to each machine.

`make bench-db ARGS='retention'` displays the persisted settings without
modifying storage:

```text
Benchmark retention
  Database cap: 32 MiB
  Recent runs: 20
  Maximum age: 90 days
  Raw samples: newest 5 runs
```

The `retention` command accepts `--max-mib`, `--keep-runs`, `--max-days` and
`--raw-runs` value pairs to save a different policy and apply it immediately.
Settings reside in the singleton `benchmark_retention` table; synchronisation
never resets them. Reducing `--keep-runs` also reduces raw retention when needed,
unless an explicitly supplied `--raw-runs` value contradicts the run limit.
Zero raw runs keeps summaries without original timing batches. Other limits
must be positive; `bench-db help` lists their accepted ranges.

Cleanup runs during catalogue synchronisation and before and after measurements.
Near capacity, older samples are discarded before older summaries, and SQLite
compaction returns freed space to the filesystem. `make bench-db ARGS='prune'`
applies the current policy explicitly without executing mathematics. Settings and
logical deletions are transactional, and the benchmark writer lock excludes
concurrent cleanup. Run identifiers are never recycled after expiry.

An active or explicitly resumed run is protected during measurement. The SQLite
page cap prevents its main database file from exceeding the configured size; if
one run exhausts capacity, measurement stops with prior checkpoints intact.
Temporary transaction journals and compaction files can require additional disk
space. Browsing statistics, refreshing Markdown, ordinary builds and `make clean`
never trigger pruning. Expired runs are no longer available for reporting or
comparison; retained outlier flags are never reset to establish a new baseline.

### Measurement and reporting

Both `run` and `stats` accept `--operation NAME`, `--backend NAME` and `--bits N`.
For example, `make bench-db ARGS='stats --operation zeta' | less -R` displays only
zeta measurements from the latest run. Add `--run ID` to inspect a particular saved run;
add `--backend mpc --bits 4096` to select its 4096-bit complex measurements.
The equivalent `run --operation zeta` command measures only that function.
The top-level `make help` lists every benchmark option. Use `make help bench-db` for
benchmark help alone; it neither builds tools nor opens the database. The executable
also provides its command reference through `make bench-db ARGS='help'`.

Timing uses checked `clock_gettime(CLOCK_MONOTONIC)` calls and records
`clock_getres()`. Integer nanosecond differences avoid subtracting large floating
timestamps. A 100 ms warm-up precedes adaptive calibration towards five milliseconds of
mathematical work per batch, up to 1,048,576 calls. Live result storage stays bounded to 512 values:
larger batches add the checked elapsed times of those chunks, excluding disposal between chunks.
Slow operations require only one call per batch. Warm-up and batches amortise timer and dispatch overhead;
parsing, result checks, disposal and SQL writes are outside timed intervals.
The clock already provides the required monotonic elapsed-time facility; no
replacement timing module or processor cycle-counter assumptions are needed.

Each case has a 15-second subprocess deadline by default. Completed cases and
raw samples commit together. `make bench-db ARGS='status'` shows progress;
`make bench-db ARGS='run --resume RUN'` continues pending cases after interruption,
provided the library and executable still match. Timeouts and failures are
retained explicitly. Each run has its own identifier; older history expires under
the stored policy.

Interactive runs use a compact, replacing display with an animated fractional
progress bar, completed/total count, percentage, elapsed time, coloured outcome
counters and the current operation, backend, precision and case deadline.
The spinner and case clock continue while a worker is busy. Failed cases and
timeouts receive permanent diagnostic lines; skipped cases only advance their
counter. Captured output uses occasional plain-text checkpoints, backend/precision
changes and a final summary instead of printing every successful case. Add
`run --verbose` to retain the detailed case-by-case listing. `NO_COLOR=1` disables
colour while retaining terminal updates. Progress follows committed cases and
restores all outcome counts when resuming a saved run.

Browse saved results with `make bench-db ARGS='stats' | less -R`. Compact
timing tables are the default and include an Arguments column beside Operation.
Unsupported cases are hidden by default, including in detailed panels, Markdown
and TSV exports. Add `stats --include-skipped` to include them. Failures,
timeouts and pending calculations remain visible; stored observations are unchanged.
`stats --summary` explicitly selects the same view; `stats --details` adds
timestamps, calibration and failure explanations in detailed panels. Both views group operations by
backend and precision, with double outer borders and bold headings.
Operation names are cyan; PASS is bright green, FAIL and TIMEOUT
are bright red, and SKIPPED is bright yellow. Median times are bold yellow and
means are bold light cyan. All durations in this view are microseconds per call;
retained raw samples keep their original nanosecond values in the database.

Change percentages compare each median with its stored compatible, non-outlier
baseline. Green ▲ means faster, red ▼ means slower, and amber ▬ means within 5%.
This display band does not alter the slowdown-outlier policy. A dagger marks an
outlier; unavailable timings remain dashes. SKIPPED means the backend does not
support that case. A timed batch is a sample within one run, not a separate run.

`stats` selects the latest run, including pending cases while it is running.
Use `stats --run 4` to select an earlier run, `stats --backend qfloat` for a backend,
`stats --bits 256` for a precision, or `stats --operation sin` for one operation.
These filters may be combined. Every view includes the actual operand literals;
`--details` adds timestamps, calibration, clock resolution, dispersion and failure explanations.
Complex operands include their imaginary parts, such as `0.7+0.125i`; integer
orders stay real, for example `2, 0.7+0.125i` for Bessel J. Saved literals and
their imaginary-component mask determine this display, so historical inputs do
not change when the catalogue is edited. Generated Markdown tables show these
same actual inputs in their Inputs column.
The `clausen` benchmark measures the general C API with its explicit order,
currently three; `clausen2` measures the order-two wrapper. These are different
workloads. In MARS Lab input, the one-argument spelling `clausen(x)` instead
aliases `clausen2(x)`; use `Cl(n,x)` to choose an order.

Stored line breaks, tabs and terminal controls are shown as literal hexadecimal
escapes in reports, preserving the original messages in the database.
Operation names and arguments wrap within compact table cells without truncation.
Narrow terminals automatically use wrapped panels; `--summary` still omits diagnostic metadata.
Statistics take a read-only snapshot before writing to the pager, so
paging does not hold a transaction open or delay measurement checkpoints.

`less -R` preserves the terminal colours; `NO_COLOR=1` disables them. Explicit
`stats --format=markdown` and `stats --format=tsv` exports contain no terminal
escapes.

Use `--operation sin`, `--backend qfloat`, `--bits 256`, `--samples 7` or
`--timeout 30` after `run` to restrict or configure a measurement. Fixed backends
retain their actual fixed precision. Multiprecision labels identify requested
working precision, not a guarantee that each underlying algorithm achieves it.
Complex profiles add 0.125i to permitted operands while keeping integer orders real.
`number-double` includes the public number API's dispatch and any documented
internal fallback; it is not advertised as a direct C libm measurement.

Comparisons require matching machine, CPU, system, compiler, build flags,
operation, arguments, backend, precision and timing protocol. A slowdown exceeding both 50% and
500 ns is flagged and retained, and is excluded from the next baseline.
Inspect flagged cases before accepting a performance change; do not change the
threshold to conceal a regression. This conservative flag is a prompt to investigate,
not a statistical proof from one noisy run. Run benchmarks without competing builds.

`make bench-db ARGS='report'` emits Markdown from the latest complete run;
an optional run number selects a specific run. `make bench-db ARGS='update-docs'`
refreshes only the marked sections of this guide and the number, qfloat and
qcomplex guides, using that same stored run. It never reruns mathematics or
mixes machines. Retained samples, failed cases and run history remain queryable in SQLite.
Published tables omit operations and precision columns with only unsupported
results. Mixed-support rows retain their timings and use a dash for unsupported
cells; failures and timeouts remain visible. The database and statistics view
retain every outcome for runs within the retention policy.
The default documentation refresh selects a complete catalogue-wide run, so a
later single-function experiment cannot accidentally replace the full tables.
An explicit run number permits publishing a selected partial run.
The timing tables below are generated from stored observations.

<!-- MARS-BENCHMARKS:BEGIN -->
Benchmark run **28**, 2026-11-11T13:11:05.705Z (UTC).

Machine: Intel(R) N95.

System: Linux 7.0.0-27-generic x86_64.

Compiler: 15.2.0; build flags: gcc -Wall -Wextra -Werror -O2 -fPIC -D_GNU_SOURCE -pthread -DHAVE_UNISTRING -I/usr/include/sqlcipher -DSQLITE_HAS_CODEC   -I/usr/include/x86_64-linux-gnu -isystem /usr/include/mit-krb5 -isystem /usr/include/mit-krb5 -I/usr/include/p11-kit-1.

Median per call from 7 timed batches.

Timing method: CLOCK_MONOTONIC; warmup=100ms; target_batch=5ms; max_batch=1048576; storage_chunk=512.

Argument parsing, output validation, disposal and database writes are outside timing.
Inputs show the actual operands, including +0.125i for permitted complex arguments; integer orders remain real.
`number-double` includes generic number dispatch; the other fixed profiles call qfloat/qcomplex directly.
Requested multiprecision bits do not guarantee that every algorithm achieves that accuracy.
Operations and precision columns with only unsupported results are omitted; — denotes an unsupported cell.
Failures and timeouts remain visible. † marks a retained slowdown outlier.

### `number-double`

| Operation | Inputs | 53 bits |
|---|---|---:|
| E1 | 2.345678 | 12.0 µs † |
| Ei | 2.345678 | 8.7 µs |
| Li | 2.345 | 7.5 µs |
| abs | 0.7 | 9.3 ns |
| acos | 0.7 | 18.4 ns |
| acosec | 2.345 | 17.6 ns |
| acosech | 0.7 | 51.7 ns |
| acosh | 2.345 | 30.5 ns |
| acot | 0.7 | 49.2 ns |
| acoth | 2.345 | 26.4 ns |
| add | 0.7, 0.7 | 10.8 ns |
| add_long | 17, 3 | 559.6 ns |
| appell_f1 | 0.5, 0.7, 0.8, 2.5, 0.2, 0.3 | 26.4 µs |
| arccovercos | 0.5 | 125.0 ns |
| arccoversin | 0.5 | 114.5 ns |
| archacovercos | 0.5 | 234.1 ns |
| archacoversin | 0.5 | 230.3 ns |
| archavercos | 0.5 | 207.7 ns |
| archaversin | 0.5 | 231.1 ns |
| arcvercos | 0.5 | 115.6 ns |
| arcversin | 0.5 | 139.9 ns |
| arg | 0.7 | 84.3 ns |
| asec | 2.345 | 20.2 ns |
| asech | 0.7 | 38.1 ns |
| asin | 0.7 | 19.6 ns |
| asinh | 0.7 | 54.9 ns |
| atan | 0.7 | 31.1 ns |
| atan2 | 0.5, -0.75 | 52.9 ns |
| atanh | 0.7 | 27.3 ns |
| bessel_i | 2, 0.7 | 43.5 µs |
| bessel_j | 2, 0.7 | 6.9 µs |
| bessel_k | 2, 0.7 | 25.2 µs |
| bessel_y | 2, 0.7 | 21.1 µs |
| beta | 2.3, 4.5 | 147.9 ns |
| beta_pdf | 0.5, 2.5, 3.5 | 180.5 ns |
| binomial | 5, 2 | 181.8 ns |
| ceil | 0.7 | 60.7 ns |
| chebyshev_t | 2, 0.7 | 811.9 ns |
| chebyshev_u | 2, 0.7 | 1.1 µs |
| circ | 0.7 | 236.3 ns |
| clausen | 3, 0.7 | 7.5 µs |
| clausen2 | 0.7 | 7.6 µs |
| conj | 0.7 | 7.6 ns |
| cos | 0.7 | 29.7 ns |
| cosec | 0.7 | 31.3 ns |
| cosech | 0.7 | 54.1 ns |
| cosh | 0.7 | 33.8 ns |
| cot | 0.7 | 37.6 ns |
| coth | 0.7 | 58.3 ns |
| covercos | 0.7 | 108.6 ns |
| coversin | 0.7 | 114.7 ns |
| cubrt | 0.7 | 6.0 µs |
| digamma | 2.345 | 2.4 µs |
| dilog | 0.7 | 13.2 µs |
| div | 0.7, 0.7 | 10.5 ns |
| erf | 0.7 | 64.2 ns |
| erfc | 0.7 | 69.8 ns |
| erfcinv | 0.5 | 211.3 µs |
| erfinv | 0.5 | 190.0 µs |
| exp | 1.23456789 | 40.5 ns |
| floor | 0.7 | 8.5 ns |
| gamma | 2.345 | 90.3 ns |
| gammainc_P | 2.3, 1.4 | 11.1 µs |
| gammainc_Q | 2.3, 1.4 | 12.1 µs |
| gammainc_lower | 2.3, 1.4 | 13.5 µs |
| gammainc_upper | 2.3, 1.4 | 13.4 µs |
| gammainv | 9.5 | 100.9 µs |
| hacovercos | 0.7 | 233.3 ns |
| hacoversin | 0.7 | 229.4 ns |
| harmonic_poly | 2, 0.7 | 1.4 µs |
| havercos | 0.7 | 213.9 ns |
| haversin | 0.7 | 228.1 ns |
| hermite_h | 2, 0.7 | 1.5 µs |
| hypergeometric_pFq | 0.7, 2.5, 0.3 | 3.6 µs |
| hypot | 0.7, 1.2 | 33.3 ns |
| imag_part | 0.7 | 23.6 ns |
| inv | 0.7 | 7.7 ns |
| lambert_w0 | 0.7 | 14.0 µs |
| lambert_wm1 | -0.2 | 14.5 µs |
| lambert_wn | 1, 0.7 | 25.6 µs |
| lauricella_f | 0.5, 0.3, 2.5 | 17.3 µs |
| ldexp | 1.23456789, 3 | 41.1 ns |
| legendre_chi | 2, 0.7 | 33.7 µs |
| lerch_phi | 0.3, 2, 1.4 | 288.2 µs |
| lg | 2.345678 | 23.5 ns |
| lgamma | 2.345 | 52.2 ns |
| ln | 2.345678 | 16.9 ns |
| log | 2.345678 | 18.1 ns |
| log10 | 2.345678 | 19.6 ns |
| logbeta | 2.3, 4.5 | 125.0 ns |
| logbeta_pdf | 0.5, 2.5, 3.5 | 137.5 ns |
| lommel_s | 0.5, 0.7, 1.2 | 6.7 µs |
| mul | 0.7, 0.7 | 10.6 ns |
| mul_long | 17, 3 | 466.4 ns |
| mul_pow10 | 1.23456789, 3 | 60.6 ns |
| neg | 0.7 | 8.3 ns |
| normal_cdf | 0.7 | 64.4 ns |
| normal_logpdf | 0.7 | 15.8 ns |
| normal_pdf | 0.7 | 24.2 ns |
| polygamma | 3, 2.345 | 22.8 µs |
| polylog | 2, 0.7 | 14.1 µs |
| polylog1 | 0.7 | 115.0 ns |
| pow | 1.23456789, 3.5 | 33.1 ns |
| pow10 | 3 | 168.0 ns |
| pow_int | 1.23456789, 3 | 61.5 ns |
| productlog | 0.7 | 14.6 µs |
| qdigamma | 0.5, 2.3 | 22.9 µs |
| real_part | 0.7 | 24.6 ns |
| rect | 0.7 | 249.5 ns |
| root | 1.2345, 3 | 7.0 µs |
| round | 0.7 | 371.5 ns |
| roundn | 1.23456789, 3 | 3.8 µs |
| rounds | 1.23456789, 3 | 6.1 µs |
| sec | 0.7 | 31.5 ns |
| sech | 0.7 | 32.3 ns |
| sgn | 0.7 | 114.8 ns |
| sin | 0.567 | 32.0 ns |
| sinc | 0.7 | 3.1 µs |
| sincos | 0.7 | 621.3 ns |
| sinh | 0.7 | 56.7 ns |
| sinhcosh | 0.7 | 110.4 ns |
| sqr | 0.7 | 8.2 ns |
| sqrt | 1.23456789 | 8.3 ns |
| step | 0.7 | 108.3 ns |
| struve_h | 2, 0.7 | 67.0 µs |
| struve_l | 2, 0.7 | 71.2 µs |
| sub | 0.7, 0.7 | 11.1 ns |
| tan | 0.7 | 6.2 µs |
| tanh | 0.7 | 55.2 ns |
| tetragamma | 2.345 | 3.0 µs |
| tri | 0.7 | 217.8 ns |
| trigamma | 2.345 | 2.5 µs |
| vercos | 0.7 | 116.6 ns |
| versin | 0.7 | 136.5 ns |
| zatahp | 2.3, 1.4 | 365.4 µs |
| zeta | 2.345 | 89.3 µs |
| zetah | 2.3, 1.4 | 338.4 µs |
| zetap | 2.345 | 141.3 µs |

### `qfloat`

| Operation | Inputs | 106 bits |
|---|---|---:|
| E1 | 2.345678 | 8.6 µs |
| Ei | 2.345678 | 9.3 µs |
| Li | 2.345 | 10.7 µs |
| abs | 0.7 | 3.0 ns |
| acos | 0.7 | 1.6 µs |
| acosec | 2.345 | 1.6 µs |
| acosech | 0.7 | 3.6 µs |
| acosh | 2.345 | 3.7 µs |
| acot | 0.7 | 1.2 µs |
| acoth | 2.345 | 3.1 µs |
| add | 0.7, 0.7 | 6.2 ns |
| appell_f1 | 0.5, 0.7, 0.8, 2.5, 0.2, 0.3 | 80.2 µs |
| arccovercos | 0.5 | 1.5 µs |
| arccoversin | 0.5 | 1.6 µs |
| archacovercos | 0.5 | 26.9 ns |
| archacoversin | 0.5 | 26.9 ns |
| archavercos | 0.5 | 55.4 ns |
| archaversin | 0.5 | 48.8 ns |
| arcvercos | 0.5 | 1.6 µs |
| arcversin | 0.5 | 1.6 µs |
| asec | 2.345 | 1.7 µs |
| asech | 0.7 | 3.5 µs |
| asin | 0.7 | 1.5 µs |
| asinh | 0.7 | 3.2 µs |
| atan | 0.7 | 1.2 µs |
| atan2 | 0.5, -0.75 | 1.2 µs |
| atanh | 0.7 | 3.0 µs |
| bessel_i | 2, 0.7 | 54.1 µs |
| bessel_j | 2, 0.7 | 6.0 µs |
| bessel_k | 2, 0.7 | 32.9 µs |
| bessel_y | 2, 0.7 | 23.2 µs |
| beta | 2.3, 4.5 | 4.6 µs |
| beta_pdf | 0.5, 2.5, 3.5 | 30.9 µs |
| binomial | 5, 2 | 31.1 µs |
| chebyshev_t | 2, 0.7 | 42.6 ns |
| chebyshev_u | 2, 0.7 | 42.7 ns |
| circ | 0.7 | 17.1 ns |
| clausen | 3, 0.7 | 365.4 µs |
| clausen2 | 0.7 | 5.6 µs |
| cos | 0.7 | 826.9 ns |
| cosec | 0.7 | 868.4 ns |
| cosech | 0.7 | 3.1 µs |
| cosh | 0.7 | 3.2 µs |
| cot | 0.7 | 1.6 µs |
| coth | 0.7 | 6.6 µs |
| covercos | 0.7 | 788.2 ns |
| coversin | 0.7 | 803.8 ns |
| cubrt | 0.7 | 4.8 µs |
| digamma | 2.345 | 2.7 µs |
| dilog | 0.7 | 12.5 µs |
| div | 0.7, 0.7 | 31.0 ns |
| erf | 0.7 | 3.9 µs |
| erfc | 0.7 | 3.9 µs |
| erfcinv | 0.5 | 238.7 µs |
| erfinv | 0.5 | 216.3 µs |
| exp | 1.23456789 | 1.8 µs |
| floor | 0.7 | 22.3 ns |
| gamma | 2.345 | 1.6 µs |
| gammainc_P | 2.3, 1.4 | 12.2 µs |
| gammainc_Q | 2.3, 1.4 | 11.6 µs |
| gammainc_lower | 2.3, 1.4 | 14.7 µs |
| gammainc_upper | 2.3, 1.4 | 14.8 µs |
| gammainv | 9.5 | 101.3 µs |
| hacovercos | 0.7 | 898.3 ns |
| hacoversin | 0.7 | 926.0 ns |
| harmonic_poly | 2, 0.7 | 109.2 ns |
| havercos | 0.7 | 922.9 ns |
| haversin | 0.7 | 985.4 ns |
| hermite_h | 2, 0.7 | 53.9 ns |
| hypergeometric_pFq | 0.7, 2.5, 0.3 | 3.7 µs |
| hypot | 0.7, 1.2 | 435.1 ns |
| lambert_w0 | 0.7 | 13.1 µs |
| lambert_wm1 | -0.2 | 16.4 µs |
| lauricella_f | 0.5, 0.3, 2.5 | 19.9 µs |
| ldexp | 1.23456789, 3 | 9.6 ns |
| legendre_chi | 2, 0.7 | 26.7 µs |
| lerch_phi | 0.3, 2, 1.4 | 281.2 µs |
| lg | 2.345678 | 2.8 µs |
| lgamma | 2.345 | 5.9 µs |
| ln | 2.345678 | 2.8 µs |
| log | 2.345678 | 2.8 µs |
| log10 | 2.345678 | 2.9 µs |
| logbeta | 2.3, 4.5 | 26.7 µs |
| logbeta_pdf | 0.5, 2.5, 3.5 | 28.2 µs |
| lommel_s | 0.5, 0.7, 1.2 | 7.3 µs |
| mul | 0.7, 0.7 | 7.0 ns |
| mul_pow10 | 1.23456789, 3 | 43.9 ns |
| neg | 0.7 | 2.0 ns |
| normal_cdf | 0.7 | 3.4 µs |
| normal_logpdf | 0.7 | 26.9 ns |
| normal_pdf | 0.7 | 1.6 µs |
| polygamma | 3, 2.345 | 4.5 µs |
| polylog | 2, 0.7 | 13.8 µs |
| polylog1 | 0.7 | 2.9 µs |
| pow | 1.23456789, 3.5 | 4.2 µs |
| pow10 | 3 | 25.7 ns |
| pow_int | 1.23456789, 3 | 50.7 ns |
| productlog | 0.7 | 14.5 µs |
| qdigamma | 0.5, 2.3 | 24.0 µs |
| rect | 0.7 | 16.1 ns |
| root | 1.2345, 3 | 4.9 µs |
| round | 0.7 | 34.3 ns |
| roundn | 1.23456789, 3 | 6.0 µs |
| rounds | 1.23456789, 3 | 16.4 µs |
| sec | 0.7 | 807.3 ns |
| sech | 0.7 | 3.2 µs |
| sgn | 0.7 | 7.0 ns |
| sin | 0.567 | 760.1 ns |
| sinc | 0.7 | 827.9 ns |
| sinh | 0.7 | 3.2 µs |
| sqr | 0.7 | 4.6 ns |
| sqrt | 1.23456789 | 328.7 ns |
| step | 0.7 | 7.2 ns |
| struve_h | 2, 0.7 | 80.0 µs |
| struve_l | 2, 0.7 | 82.6 µs |
| sub | 0.7, 0.7 | 7.0 ns |
| tan | 0.7 | 1.8 µs |
| tanh | 0.7 | 1.6 µs |
| tetragamma | 2.345 | 3.0 µs |
| tri | 0.7 | 12.6 ns |
| trigamma | 2.345 | 2.6 µs |
| vercos | 0.7 | 812.2 ns |
| versin | 0.7 | 796.4 ns |
| zatahp | 2.3, 1.4 | 318.9 µs |
| zeta | 2.345 | 293.0 µs |
| zetah | 2.3, 1.4 | 278.5 µs |
| zetap | 2.345 | 282.7 µs |

### `qcomplex`

| Operation | Inputs | 106 bits |
|---|---|---:|
| E1 | 2.345678+0.125i | 43.0 µs |
| Ei | 2.345678+0.125i | 45.9 µs |
| Li | 2.345+0.125i | 41.4 µs |
| abs | 0.7+0.125i | 452.2 ns |
| acos | 0.7+0.125i | 7.8 µs |
| acosec | 2.345+0.125i | 8.7 µs |
| acosech | 0.7+0.125i | 8.6 µs |
| acosh | 2.345+0.125i | 11.4 µs |
| acot | 0.7+0.125i | 9.0 µs |
| acoth | 2.345+0.125i | 5.2 µs |
| add | 0.7+0.125i, 0.7+0.125i | 15.2 ns |
| appell_f1 | <span style="display:inline-block;width:40ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">0.5+0.125i, 0.7+0.125i, 0.8+0.125i, 2.5+<br/>0.125i, 0.2+0.125i, 0.3+0.125i</span> | 532.8 µs |
| arccovercos | 0.5+0.125i | 8.2 µs |
| arccoversin | 0.5+0.125i | 8.3 µs |
| archacovercos | 0.5+0.125i | 5.2 µs |
| archacoversin | 0.5+0.125i | 5.0 µs |
| archavercos | 0.5+0.125i | 5.2 µs |
| archaversin | 0.5+0.125i | 5.8 µs |
| arcvercos | 0.5+0.125i | 9.0 µs |
| arcversin | 0.5+0.125i | 8.2 µs |
| arg | 0.7+0.125i | 1.3 µs |
| asec | 2.345+0.125i | 8.3 µs |
| asech | 0.7+0.125i | 11.3 µs |
| asin | 0.7+0.125i | 8.6 µs |
| asinh | 0.7+0.125i | 8.5 µs |
| atan | 0.7+0.125i | 8.7 µs |
| atan2 | 0.5+0.125i, -0.75+0.125i | 10.7 µs |
| atanh | 0.7+0.125i | 5.2 µs |
| bessel_i | 2, 0.7+0.125i | 96.1 µs |
| bessel_j | 2, 0.7+0.125i | 107.4 µs |
| bessel_k | 2, 0.7+0.125i | 76.3 µs |
| bessel_y | 2, 0.7+0.125i | 84.1 µs |
| beta | 2.3+0.125i, 4.5+0.125i | 40.2 µs |
| beta_pdf | 0.5+0.125i, 2.5+0.125i, 3.5+0.125i | 55.3 µs |
| binomial | 5+0.125i, 2+0.125i | 26.3 µs |
| chebyshev_t | 2, 0.7+0.125i | 158.5 ns |
| chebyshev_u | 2, 0.7+0.125i | 155.5 ns |
| circ | 0.7 | 19.2 ns |
| clausen | 3, 0.7+0.125i | 337.6 µs |
| clausen2 | 0.7+0.125i | 27.2 µs |
| conj | 0.7+0.125i | 3.9 ns |
| cos | 0.7+0.125i | 7.8 µs |
| cosec | 0.7+0.125i | 8.5 µs |
| cosech | 0.7+0.125i | 8.4 µs |
| cosh | 0.7+0.125i | 8.2 µs |
| cot | 0.7+0.125i | 16.8 µs |
| coth | 0.7+0.125i | 16.2 µs |
| covercos | 0.7+0.125i | 7.9 µs |
| coversin | 0.7+0.125i | 7.9 µs |
| cubrt | 0.7+0.125i | 8.0 µs |
| digamma | 2.345+0.125i | 11.2 µs |
| dilog | 0.7+0.125i | 85.3 µs |
| div | 0.7+0.125i, 0.7+0.125i | 160.4 ns |
| erf | 0.7+0.125i | 46.3 µs |
| erfc | 0.7+0.125i | 43.4 µs |
| erfcinv | 0.5+0.125i | 338.1 µs |
| erfinv | 0.5+0.125i | 336.3 µs |
| exp | 1.23456789+0.125i | 3.2 µs |
| floor | 0.7+0.125i | 39.8 ns |
| gamma | 2.345+0.125i | 13.1 µs |
| gammainc_P | 2.3+0.125i, 1.4+0.125i | 68.6 µs |
| gammainc_Q | 2.3+0.125i, 1.4+0.125i | 61.3 µs |
| gammainc_lower | 2.3+0.125i, 1.4+0.125i | 43.0 µs |
| gammainc_upper | 2.3+0.125i, 1.4+0.125i | 56.3 µs |
| gammainv | 9.5+0.125i | 135.2 µs |
| hacovercos | 0.7+0.125i | 8.4 µs |
| hacoversin | 0.7+0.125i | 8.8 µs |
| harmonic_poly | 2, 0.7+0.125i | 443.1 ns |
| havercos | 0.7+0.125i | 8.1 µs |
| haversin | 0.7+0.125i | 7.8 µs |
| hermite_h | 2, 0.7+0.125i | 193.3 ns |
| hypergeometric_pFq | 0.7+0.125i, 2.5+0.125i, 0.3+0.125i | 29.1 µs |
| hypot | 0.7+0.125i, 1.2+0.125i | 1.4 µs |
| imag_part | 0.7+0.125i | 2.5 ns |
| inv | 0.7+0.125i | 179.0 ns |
| lambert_w0 | 0.7+0.125i | 30.1 µs |
| lambert_wm1 | -0.2+0.125i | 29.8 µs |
| lambert_wn | 1, 0.7+0.125i | 26.3 µs |
| lauricella_f | 0.5+0.125i, 0.3+0.125i, 2.5+0.125i | 145.7 µs |
| ldexp | 1.23456789+0.125i, 3 | 18.3 ns |
| legendre_chi | 2, 0.7+0.125i | 352.7 µs |
| lerch_phi | 0.3+0.125i, 2+0.125i, 1.4+0.125i | 587.7 µs |
| lg | 2.345678+0.125i | 4.2 µs |
| lgamma | 2.345+0.125i | 10.1 µs |
| ln | 2.345678+0.125i | 4.8 µs |
| log | 2.345678+0.125i | 4.3 µs |
| log10 | 2.345678+0.125i | 4.9 µs |
| logbeta | 2.3+0.125i, 4.5+0.125i | 34.1 µs |
| logbeta_pdf | 0.5+0.125i, 2.5+0.125i, 3.5+0.125i | 42.1 µs |
| lommel_s | 0.5+0.125i, 0.7+0.125i, 1.2+0.125i | 108.4 µs |
| mul | 0.7+0.125i, 0.7+0.125i | 45.0 ns |
| neg | 0.7+0.125i | 5.4 ns |
| normal_cdf | 0.7+0.125i | 42.7 µs |
| normal_logpdf | 0.7+0.125i | 137.7 ns |
| normal_pdf | 0.7+0.125i | 3.4 µs |
| polygamma | 3, 2.345+0.125i | 14.6 µs |
| polylog | 2, 0.7+0.125i | 86.4 µs |
| polylog1 | 0.7+0.125i | 4.7 µs |
| pow | 1.23456789+0.125i, 3.5+0.125i | 8.3 µs |
| productlog | 0.7+0.125i | 28.9 µs |
| qdigamma | 0.5+0.125i, 2.3+0.125i | 151.0 µs |
| real_part | 0.7+0.125i | 2.3 ns |
| rect | 0.7 | 20.4 ns |
| root | 1.2345+0.125i, 3 | 7.6 µs |
| round | 0.7+0.125i | 77.1 ns |
| roundn | 1.23456789+0.125i, 3 | 11.1 µs |
| rounds | 1.23456789+0.125i, 3 | 30.2 µs |
| sec | 0.7+0.125i | 9.6 µs |
| sech | 0.7+0.125i | 8.8 µs |
| sgn | 0.7 | 10.0 ns |
| sin | 0.567+0.125i | 7.6 µs |
| sinc | 0.7+0.125i | 8.3 µs |
| sinh | 0.7+0.125i | 8.2 µs |
| sqr | 0.7+0.125i | 55.5 ns |
| sqrt | 1.23456789+0.125i | 3.7 µs |
| step | 0.7 | 10.4 ns |
| struve_h | 2, 0.7+0.125i | 127.9 µs |
| struve_l | 2, 0.7+0.125i | 134.8 µs |
| sub | 0.7+0.125i, 0.7+0.125i | 16.8 ns |
| tan | 0.7+0.125i | 17.3 µs |
| tanh | 0.7+0.125i | 16.1 µs |
| tetragamma | 2.345+0.125i | 4.1 µs |
| tri | 0.7 | 17.1 ns |
| trigamma | 2.345+0.125i | 3.3 µs |
| vercos | 0.7+0.125i | 7.7 µs |
| versin | 0.7+0.125i | 7.9 µs |
| zatahp | 2.3+0.125i, 1.4+0.125i | 557.6 µs |
| zeta | 2.345+0.125i | 412.9 µs |
| zetah | 2.3+0.125i, 1.4+0.125i | 525.6 µs |
| zetap | 2.345+0.125i | 393.8 µs |

### `mpfr`

| Operation | Inputs | 256 bits | 512 bits | 768 bits | 1024 bits | 2048 bits | 4096 bits |
|---|---|---:|---:|---:|---:|---:|---:|
| E1 | 2.345678 | 37.5 µs | 70.3 µs | 149.4 µs | 172.3 µs | 601.8 µs | 1.803 ms |
| Ei | 2.345678 | 39.0 µs | 62.5 µs | 116.6 µs | 145.1 µs | 535.4 µs | 2.103 ms |
| Li | 2.345 | 35.8 µs | 67.1 µs | 101.4 µs | 148.0 µs | 457.7 µs | 1.757 ms |
| abs | 0.7 | 80.4 ns | 79.6 ns | 79.0 ns | 86.4 ns | 84.4 ns | 103.5 ns |
| acos | 0.7 | 14.4 µs | 25.6 µs | 35.9 µs | 51.8 µs | 96.5 µs | 345.7 µs |
| acosec | 2.345 | 17.2 µs | 24.9 µs | 35.0 µs | 54.4 µs | 121.9 µs | 358.5 µs |
| acosech | 0.7 | 5.7 µs | 9.2 µs | 12.0 µs | 16.0 µs | 39.9 µs | 116.5 µs |
| acosh | 2.345 | 6.0 µs | 9.9 µs | 13.2 µs | 17.1 µs | 42.2 µs | 116.8 µs |
| acot | 0.7 | 13.5 µs | 25.2 µs | 29.1 µs | 48.9 µs | 121.5 µs | 267.0 µs |
| acoth | 2.345 | 5.6 µs | 10.4 µs | 14.3 µs | 17.6 µs | 59.0 µs | 103.4 µs |
| add | 0.7, 0.7 | 93.5 ns | 106.8 ns | 114.6 ns | 130.7 ns | 208.8 ns | 284.5 ns |
| add_long | 17, 3 | 657.2 ns | 589.1 ns | 766.9 ns | 963.3 ns | 1.1 µs | 744.8 ns |
| appell_f1 | 0.5, 0.7, 0.8, 2.5, 0.2, 0.3 | 114.4 µs | 343.7 µs | 635.4 µs | 966.3 µs | 5.430 ms | 25.813 ms |
| arccovercos | 0.5 | 14.0 µs | 23.0 µs | 33.8 µs | 45.8 µs | 106.0 µs | 356.1 µs |
| arccoversin | 0.5 | 13.7 µs | 23.1 µs | 32.5 µs | 43.7 µs | 120.5 µs | 259.3 µs |
| archacovercos | 0.5 | 1.3 µs | 1.5 µs | 1.6 µs | 1.4 µs | 1.6 µs | 1.5 µs |
| archacoversin | 0.5 | 1.3 µs | 1.4 µs | 1.9 µs | 1.6 µs | 1.7 µs | 1.5 µs |
| archavercos | 0.5 | 1.4 µs | 1.4 µs | 2.0 µs | 1.5 µs | 2.0 µs | 1.8 µs |
| archaversin | 0.5 | 1.4 µs | 1.4 µs | 1.8 µs | 1.3 µs | 1.7 µs | 1.9 µs |
| arcvercos | 0.5 | 14.2 µs | 22.4 µs | 37.4 µs | 44.0 µs | 115.1 µs | 268.7 µs |
| arcversin | 0.5 | 13.4 µs | 23.0 µs | 33.7 µs | 45.5 µs | 136.1 µs | 300.6 µs |
| arg | 0.7 | 509.9 ns | 490.7 ns | 639.4 ns | 532.1 ns | 638.9 ns | 614.1 ns |
| asec | 2.345 | 18.8 µs | 25.5 µs | 42.7 µs | 54.7 µs | 126.5 µs | 442.2 µs |
| asech | 0.7 | 5.7 µs | 9.3 µs | 14.8 µs | 16.6 µs | 48.8 µs | 135.8 µs |
| asin | 0.7 | 15.9 µs | 26.0 µs | 39.4 µs | 51.2 µs | 108.8 µs | 414.8 µs |
| asinh | 0.7 | 4.8 µs | 8.0 µs | 13.4 µs | 15.6 µs | 49.4 µs | 123.5 µs |
| atan | 0.7 | 12.4 µs | 21.5 µs | 35.5 µs | 39.7 µs | 114.7 µs | 399.6 µs |
| atan2 | 0.5, -0.75 | 13.1 µs | 22.2 µs | 33.5 µs | 41.6 µs | 114.4 µs | 365.7 µs |
| atanh | 0.7 | 5.9 µs | 9.6 µs | 15.3 µs | 16.3 µs | 53.3 µs | 160.3 µs † |
| bessel_i | 2, 0.7 | 91.9 µs | 184.4 µs | 351.4 µs | 539.1 µs | 6.163 ms † | 8.662 ms |
| bessel_j | 2, 0.7 | 6.1 µs | 14.6 µs | 26.6 µs | 41.5 µs | 224.3 µs | 886.4 µs |
| bessel_k | 2, 0.7 | 49.0 µs | 103.6 µs | 195.5 µs | 242.1 µs | 1.263 ms | 4.903 ms |
| bessel_y | 2, 0.7 | 45.6 µs | 93.4 µs | 161.6 µs | 199.4 µs | 1.254 ms † | 4.129 ms |
| beta | 2.3, 4.5 | 93.7 µs | 253.9 µs | 526.7 µs | 837.1 µs | 8.299 ms † | 13.394 ms |
| beta_pdf | 0.5, 2.5, 3.5 | 97.7 µs | 199.1 µs | 428.3 µs | 504.6 µs | 2.831 ms | 8.768 ms |
| binomial | 5, 2 | 93.9 µs | 245.8 µs | 612.9 µs † | 840.1 µs | 2.915 ms | 12.575 ms |
| ceil | 0.7 | 453.3 ns | 470.9 ns | 522.3 ns | 492.7 ns | 480.3 ns | 575.7 ns |
| chebyshev_t | 2, 0.7 | 2.3 µs | 2.9 µs | 3.6 µs | 3.0 µs | 3.6 µs | 6.7 µs |
| chebyshev_u | 2, 0.7 | 3.1 µs | 3.3 µs | 4.1 µs | 3.8 µs | 4.0 µs | 6.8 µs |
| circ | 0.7 | 890.9 ns | 851.7 ns | 1.0 µs | 851.2 ns | 951.5 ns | 1.1 µs |
| clausen | 3, 0.7 | 24.1 µs | 58.8 µs | 117.6 µs | 208.4 µs | 892.7 µs | 4.838 ms |
| clausen2 | 0.7 | 23.9 µs | 64.7 µs | 133.9 µs | 187.1 µs | 942.0 µs | 5.530 ms |
| conj | 0.7 | 83.4 ns | 78.7 ns | 82.6 ns | 77.3 ns | 83.5 ns | 100.9 ns |
| cos | 0.7 | 2.0 µs | 4.3 µs | 7.3 µs | 12.0 µs | 37.8 µs | 182.6 µs |
| cosec | 0.7 | 3.3 µs | 6.2 µs | 8.3 µs | 13.3 µs | 47.5 µs | 194.0 µs |
| cosech | 0.7 | 2.6 µs | 6.2 µs | 10.5 µs | 14.7 µs | 60.9 µs | 187.2 µs |
| cosh | 0.7 | 2.7 µs | 6.0 µs | 9.1 µs | 14.4 µs | 42.1 µs | 214.9 µs |
| cot | 0.7 | 5.5 µs | 9.0 µs | 18.8 µs | 26.8 µs | 75.7 µs | 337.5 µs |
| coth | 0.7 | 5.7 µs | 12.4 µs | 18.5 µs | 28.3 µs | 86.2 µs | 369.7 µs |
| covercos | 0.7 | 3.5 µs | 6.0 µs | 9.2 µs | 13.5 µs | 37.9 µs | 168.9 µs |
| coversin | 0.7 | 3.5 µs | 6.0 µs | 9.4 µs | 15.1 µs | 46.3 µs | 137.1 µs |
| cubrt | 0.7 | 12.3 µs | 17.6 µs | 26.2 µs | 34.1 µs | 105.3 µs | 267.6 µs |
| digamma | 2.345 | 95.9 µs | 266.2 µs | 487.8 µs | 927.9 µs | 3.738 ms | 18.930 ms |
| dilog | 0.7 | 94.9 µs | 223.9 µs | 440.2 µs | 773.9 µs | 4.002 ms | 19.602 ms |
| div | 0.7, 0.7 | 332.0 ns | 535.8 ns | 746.6 ns | 1.3 µs | 3.0 µs | 9.7 µs |
| erf | 0.7 | 16.0 µs | 35.8 µs | 70.2 µs | 122.9 µs | 494.3 µs | 2.188 ms |
| erfc | 0.7 | 17.2 µs | 41.7 µs | 64.4 µs | 145.1 µs | 518.4 µs | 2.025 ms |
| erfcinv | 0.5 | 320.3 µs | 561.0 µs | 689.9 µs | 1.372 ms | 3.065 ms | 11.329 ms |
| erfinv | 0.5 | 337.5 µs | 489.3 µs | 583.4 µs | 929.7 µs | 3.176 ms | 10.561 ms |
| exp | 1.23456789 | 2.5 µs | 5.2 µs | 9.4 µs | 16.6 µs | 48.1 µs | 166.1 µs |
| floor | 0.7 | 81.6 ns | 80.1 ns | 85.3 ns | 85.1 ns | 87.3 ns | 100.9 ns |
| gamma | 2.345 | 30.9 µs | 86.2 µs | 150.5 µs | 233.8 µs | 903.2 µs | 4.037 ms |
| gammainc_P | 2.3, 1.4 | 106.8 µs | 252.9 µs | 466.2 µs | 864.6 µs | 2.597 ms | 12.845 ms |
| gammainc_Q | 2.3, 1.4 | 97.9 µs | 225.1 µs | 707.7 µs | 1.483 ms | 4.568 ms | 27.013 ms |
| gammainc_lower | 2.3, 1.4 | 69.5 µs | 155.9 µs | 233.6 µs | 509.5 µs | 1.596 ms | 11.143 ms |
| gammainc_upper | 2.3, 1.4 | 93.6 µs | 262.2 µs | 717.9 µs | 1.133 ms | 5.315 ms | 27.112 ms |
| gammainv | 9.5 | 645.4 µs | 1.425 ms | 2.319 ms | 4.202 ms | 12.887 ms | 60.558 ms |
| hacovercos | 0.7 | 4.0 µs | 7.0 µs | 12.0 µs | 15.4 µs | 42.8 µs | 155.0 µs |
| hacoversin | 0.7 | 4.3 µs | 7.9 µs | 10.6 µs | 17.3 µs | 51.9 µs | 180.9 µs |
| harmonic_poly | 2, 0.7 | 3.8 µs | 3.8 µs | 5.1 µs | 6.8 µs | 12.0 µs | 27.5 µs |
| havercos | 0.7 | 3.5 µs | 5.9 µs | 8.9 µs | 14.9 µs | 43.4 µs | 138.8 µs |
| haversin | 0.7 | 3.4 µs | 5.8 µs | 10.6 µs | 15.5 µs | 43.7 µs | 167.4 µs |
| hermite_h | 2, 0.7 | 3.2 µs | 3.8 µs | 3.7 µs | 4.0 µs | 5.0 µs | 6.2 µs |
| hypergeometric_pFq | 0.7, 2.5, 0.3 | 4.3 µs | 4.0 µs | 4.6 µs | 4.5 µs | 4.6 µs | 4.3 µs |
| hypot | 0.7, 1.2 | 721.0 ns | 861.7 ns | 1.1 µs | 1.8 µs | 2.7 µs | 8.5 µs |
| imag_part | 0.7 | 69.5 ns | 70.9 ns | 73.5 ns | 107.9 ns | 76.1 ns | 78.3 ns |
| inv | 0.7 | 199.6 ns | 264.6 ns | 362.7 ns | 643.8 ns | 1.3 µs | 4.4 µs |
| lambert_w0 | 0.7 | 55.8 µs | 86.3 µs | 158.7 µs | 255.3 µs | 756.4 µs | 3.869 ms |
| lambert_wm1 | -0.2 | 52.4 µs | 108.6 µs | 166.0 µs | 314.5 µs | 886.6 µs | 3.624 ms |
| lambert_wn | 1, 0.7 | 25.9 µs | 26.1 µs | 30.6 µs | 29.9 µs | 29.7 µs | 27.1 µs |
| lauricella_f | 0.5, 0.3, 2.5 | 18.4 µs | 21.7 µs | 21.0 µs | 19.5 µs | 21.1 µs | 47.1 µs † |
| ldexp | 1.23456789, 3 | 109.4 ns | 110.1 ns | 115.0 ns | 118.7 ns | 130.8 ns | 141.0 ns |
| legendre_chi | 2, 0.7 | 54.4 µs | 162.5 µs | 407.6 µs | 494.3 µs | 2.663 ms | 12.256 ms |
| lerch_phi | 0.3, 2, 1.4 | 301.7 µs | 283.3 µs | 285.2 µs | 314.0 µs | 320.9 µs | 347.2 µs |
| lg | 2.345678 | 10.3 µs | 14.6 µs | 22.5 µs | 33.2 µs | 86.7 µs | 221.9 µs |
| lgamma | 2.345 | 33.9 µs | 94.4 µs | 159.7 µs | 230.9 µs | 968.4 µs | 4.099 ms |
| ln | 2.345678 | 4.9 µs | 7.3 µs | 12.3 µs | 16.7 µs | 40.2 µs | 96.4 µs |
| log | 2.345678 | 4.8 µs | 8.2 µs | 11.4 µs | 14.6 µs | 41.7 µs | 131.0 µs |
| log10 | 2.345678 | 9.5 µs | 17.7 µs | 26.1 µs | 34.1 µs | 107.3 µs | 265.3 µs |
| logbeta | 2.3, 4.5 | 100.3 µs | 235.0 µs | 425.6 µs | 734.8 µs | 3.543 ms | 12.991 ms |
| logbeta_pdf | 0.5, 2.5, 3.5 | 81.6 µs | 199.5 µs | 382.1 µs | 562.5 µs | 2.113 ms | 8.268 ms |
| lommel_s | 0.5, 0.7, 1.2 | 28.3 µs | 63.2 µs | 107.4 µs | 216.7 µs | 746.5 µs | 2.829 ms |
| mul | 0.7, 0.7 | 112.6 ns | 168.8 ns | 244.5 ns | 349.2 ns | 847.4 ns | 2.7 µs |
| mul_long | 17, 3 | 647.3 ns | 919.0 ns | 823.0 ns | 696.4 ns | 770.5 ns | 876.6 ns |
| mul_pow10 | 1.23456789, 3 | 312.4 ns | 431.9 ns | 563.9 ns | 602.4 ns | 467.9 ns | 606.1 ns |
| neg | 0.7 | 82.7 ns | 86.3 ns | 87.6 ns | 101.0 ns | 94.8 ns | 102.9 ns |
| normal_cdf | 0.7 | 13.9 µs | 69.5 µs | 62.4 µs | 103.8 µs | 377.1 µs | 1.665 ms |
| normal_logpdf | 0.7 | 5.1 µs | 8.3 µs | 12.6 µs | 14.9 µs | 40.3 µs | 111.6 µs |
| normal_pdf | 0.7 | 3.7 µs | 7.1 µs | 11.2 µs | 16.6 µs | 55.3 µs | 188.8 µs |
| polygamma | 3, 2.345 | 45.0 µs | 109.7 µs | 237.9 µs | 343.7 µs | 1.741 ms | 16.714 ms † |
| polylog | 2, 0.7 | 93.4 µs | 265.2 µs | 488.1 µs | 719.8 µs | 3.718 ms | 19.287 ms |
| polylog1 | 0.7 | 6.0 µs | 7.6 µs | 11.7 µs | 18.4 µs | 35.2 µs | 109.1 µs |
| pow | 1.23456789, 3.5 | 7.8 µs | 15.6 µs | 21.0 µs | 29.3 µs | 85.5 µs | 303.3 µs |
| pow10 | 3 | 230.4 ns | 292.6 ns | 392.9 ns | 525.5 ns | 379.6 ns | 443.4 ns |
| pow_int | 1.23456789, 3 | 269.9 ns | 408.8 ns | 597.5 ns | 863.1 ns | 2.3 µs | 7.5 µs |
| productlog | 0.7 | 47.4 µs | 87.7 µs | 161.0 µs | 238.6 µs | 945.7 µs | 3.033 ms |
| qdigamma | 0.5, 2.3 | 27.1 µs | 26.6 µs | 25.1 µs | 26.0 µs | 24.8 µs | 28.8 µs |
| real_part | 0.7 | 81.9 ns | 84.9 ns | 76.3 ns | 95.6 ns | 158.4 ns | 125.6 ns |
| rect | 0.7 | 857.9 ns | 864.5 ns | 951.8 ns | 895.9 ns | 1.2 µs | 1.1 µs |
| root | 1.2345, 3 | 14.1 µs | 19.7 µs | 24.9 µs | 41.3 µs | 123.0 µs | 247.3 µs |
| round | 0.7 | 2.2 µs | 2.3 µs | 2.4 µs | 2.7 µs | 3.3 µs | 2.4 µs |
| roundn | 1.23456789, 3 | 5.3 µs | 5.1 µs | 5.5 µs | 6.1 µs | 8.5 µs | 9.8 µs |
| rounds | 1.23456789, 3 | 17.3 µs | 24.0 µs | 31.0 µs | 44.3 µs | 94.6 µs | 211.9 µs |
| sec | 0.7 | 2.3 µs | 5.0 µs | 9.0 µs | 12.5 µs | 42.6 µs | 133.8 µs |
| sech | 0.7 | 2.7 µs | 5.6 µs | 8.9 µs | 14.4 µs | 49.6 µs | 178.0 µs |
| sgn | 0.7 | 214.2 ns | 228.4 ns | 239.2 ns | 224.3 ns | 240.4 ns | 200.5 ns |
| sin | 0.567 | 2.8 µs | 5.3 µs | 8.5 µs | 14.6 µs | 42.9 µs | 200.4 µs |
| sinc | 0.7 | 4.6 µs | 7.5 µs | 16.4 µs | 19.3 µs | 53.0 µs | 168.3 µs |
| sincos | 0.7 | 10.0 µs | 12.2 µs | 19.7 µs | 19.6 µs | 49.7 µs | 184.8 µs |
| sinh | 0.7 | 3.0 µs | 6.0 µs | 11.8 µs | 15.6 µs | 41.8 µs | 148.5 µs |
| sinhcosh | 0.7 | 2.8 µs | 6.2 µs | 24.1 µs † | 14.0 µs | 48.6 µs | 175.2 µs |
| sqr | 0.7 | 130.4 ns | 192.4 ns | 367.1 ns | 409.9 ns | 638.4 ns | 1.3 µs |
| sqrt | 1.23456789 | 295.2 ns | 428.3 ns | 629.7 ns | 630.1 ns | 1.2 µs | 2.8 µs |
| step | 0.7 | 222.8 ns | 224.7 ns | 230.0 ns | 248.6 ns | 234.7 ns | 220.0 ns |
| struve_h | 2, 0.7 | 135.1 µs | 289.7 µs | 516.0 µs | 772.8 µs | 2.860 ms | 11.819 ms |
| struve_l | 2, 0.7 | 149.2 µs | 344.3 µs | 527.3 µs | 832.0 µs | 2.542 ms | 11.000 ms |
| sub | 0.7, 0.7 | 74.3 ns | 77.9 ns | 92.4 ns | 91.0 ns | 154.6 ns | 263.1 ns |
| tan | 0.7 | 12.4 µs | 12.8 µs | 19.6 µs | 20.8 µs | 50.5 µs | 168.4 µs |
| tanh | 0.7 | 2.9 µs | 5.8 µs | 9.6 µs | 13.9 µs | 43.9 µs | 158.2 µs |
| tetragamma | 2.345 | 43.9 µs | 123.5 µs | 257.3 µs | 432.8 µs | 1.899 ms | 11.580 ms |
| tri | 0.7 | 814.7 ns | 887.5 ns | 840.1 ns | 962.2 ns | 939.1 ns | 1.1 µs |
| trigamma | 2.345 | 28.7 µs | 81.8 µs | 192.1 µs | 292.0 µs | 1.596 ms | 7.217 ms |
| vercos | 0.7 | 2.6 µs | 5.0 µs | 8.9 µs | 11.3 µs | 43.1 µs | 146.2 µs |
| versin | 0.7 | 2.6 µs | 4.8 µs | 7.2 µs | 12.2 µs | 40.2 µs | 160.6 µs |
| zatahp | 2.3, 1.4 | 575.3 µs | 1.121 ms | 2.523 ms | 3.975 ms | 19.597 ms | 124.965 ms |
| zeta | 2.345 | 834.1 µs | 4.976 ms | 9.341 ms | 1.131 ms | 4.491 ms | 23.811 ms |
| zetah | 2.3, 1.4 | 545.8 µs | 1.241 ms | 2.034 ms | 4.174 ms | 16.917 ms | 102.185 ms |
| zetap | 2.345 | 212.4 µs | 445.1 µs | 774.1 µs | 1.830 ms | 6.282 ms | 31.897 ms |

### `mpc`

| Operation | Inputs | 256 bits | 512 bits | 768 bits | 1024 bits | 2048 bits | 4096 bits |
|---|---|---:|---:|---:|---:|---:|---:|
| E1 | 2.345678+0.125i | 87.3 µs | 184.2 µs | 369.4 µs | 570.0 µs | 1.814 ms | 7.771 ms |
| Ei | 2.345678+0.125i | 74.7 µs | 160.6 µs | 403.7 µs | 508.1 µs | 1.813 ms | 8.889 ms |
| Li | 2.345+0.125i | 94.2 µs | 163.2 µs | 406.3 µs | 529.5 µs | 1.812 ms | 8.940 ms |
| abs | 0.7+0.125i | 891.4 ns | 1.2 µs | 1.5 µs | 1.6 µs | 2.5 µs | 6.8 µs |
| acos | 0.7+0.125i | 56.3 µs | 85.7 µs | 104.1 µs | 80.3 µs | 176.8 µs | 416.3 µs |
| acosec | 2.345+0.125i | 251.0 µs | 543.0 µs | 1.140 ms | 1.957 ms | 9.310 ms | 49.268 ms |
| acosech | 0.7+0.125i | 50.0 µs | 75.8 µs | 111.4 µs | 124.1 µs | 294.6 µs | 990.5 µs |
| acosh | 2.345+0.125i | 45.9 µs | 70.8 µs | 90.2 µs | 71.8 µs | 302.5 µs | 369.6 µs |
| acot | 0.7+0.125i | 45.1 µs | 76.5 µs | 88.9 µs | 129.7 µs | 287.4 µs | 1.045 ms |
| acoth | 2.345+0.125i | 33.4 µs | 57.8 µs | 67.9 µs | 105.7 µs | 270.3 µs | 711.7 µs |
| add | 0.7+0.125i, 0.7+0.125i | 357.7 ns | 409.3 ns | 453.3 ns | 408.1 ns | 502.7 ns | 553.1 ns |
| add_long | 17+0.125i, 3 | 2.5 µs | 2.8 µs | 2.7 µs | 2.5 µs | 2.7 µs | 2.7 µs |
| appell_f1 | <span style="display:inline-block;width:40ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">0.5+0.125i, 0.7+0.125i, 0.8+0.125i, 2.5+<br/>0.125i, 0.2+0.125i, 0.3+0.125i</span> | 439.4 µs | 1.589 ms | 2.928 ms | 5.232 ms | 22.536 ms | 124.256 ms |
| arccovercos | 0.5+0.125i | 50.3 µs | 86.9 µs | 54.6 µs | 138.1 µs | 169.7 µs | 389.4 µs |
| arccoversin | 0.5+0.125i | 50.8 µs | 48.0 µs | 46.0 µs | 129.9 µs | 161.6 µs | 462.4 µs |
| archacovercos | 0.5+0.125i | 15.0 µs | 17.2 µs | 21.1 µs | 23.7 µs | 48.1 µs | 129.0 µs |
| archacoversin | 0.5+0.125i | 34.5 µs † | 18.1 µs | 20.1 µs | 21.7 µs | 55.0 µs | 119.0 µs |
| archavercos | 0.5+0.125i | 16.8 µs | 17.7 µs | 22.0 µs | 20.7 µs | 47.8 µs | 132.0 µs |
| archaversin | 0.5+0.125i | 20.6 µs † | 16.0 µs | 18.9 µs | 26.0 µs | 44.8 µs | 107.3 µs |
| arcvercos | 0.5+0.125i | 62.3 µs | 46.2 µs | 98.5 µs | 82.1 µs | 167.9 µs | 445.8 µs |
| arcversin | 0.5+0.125i | 28.6 µs | 42.4 µs | 101.8 µs | 74.4 µs | 161.9 µs | 415.5 µs |
| arg | 0.7+0.125i | 12.6 µs | 24.0 µs | 32.2 µs | 41.2 µs | 115.4 µs | 301.0 µs |
| asec | 2.345+0.125i | 221.4 µs | 506.6 µs | 1.052 ms | 2.142 ms | 10.458 ms | 44.522 ms |
| asech | 0.7+0.125i | 24.6 µs | 46.0 µs | 58.6 µs | 69.4 µs | 160.7 µs | 469.7 µs |
| asin | 0.7+0.125i | 51.3 µs | 81.2 µs | 97.1 µs | 140.7 µs | 346.3 µs | 483.1 µs |
| asinh | 0.7+0.125i | 46.5 µs | 80.3 µs | 91.7 µs | 139.9 µs | 340.3 µs | 836.9 µs |
| atan | 0.7+0.125i | 44.3 µs | 65.1 µs | 87.9 µs | 124.0 µs | 325.9 µs | 858.9 µs |
| atan2 | 0.5+0.125i, -0.75+0.125i | 10.6 µs | 9.6 µs | 10.6 µs | 11.7 µs | 11.8 µs | 11.1 µs |
| atanh | 0.7+0.125i | 38.0 µs | 61.1 µs | 82.9 µs | 125.2 µs | 274.3 µs | 760.2 µs |
| bessel_i | 2, 0.7+0.125i | 188.8 µs | 432.6 µs | 643.9 µs | 1.103 ms | 3.880 ms | 16.831 ms |
| bessel_j | 2, 0.7+0.125i | 186.5 µs | 346.3 µs | 681.3 µs | 927.7 µs | 4.045 ms | 15.440 ms |
| bessel_k | 2, 0.7+0.125i | 106.6 µs | 221.2 µs | 372.8 µs | 697.3 µs | 2.940 ms | 9.566 ms |
| bessel_y | 2, 0.7+0.125i | 167.1 µs | 332.5 µs | 685.2 µs | 980.7 µs | 3.945 ms | 16.382 ms |
| beta | 2.3+0.125i, 4.5+0.125i | 35.5 µs | 39.1 µs | 40.5 µs | 40.2 µs | 37.7 µs | 41.5 µs |
| beta_pdf | 0.5+0.125i, 2.5+0.125i, 3.5+0.125i | 48.5 µs | 71.1 µs | 56.5 µs | 60.7 µs | 62.9 µs | 60.3 µs |
| binomial | 5+0.125i, 2+0.125i | 27.2 µs | 38.7 µs | 29.7 µs | 26.1 µs | 27.4 µs | 27.5 µs |
| ceil | 0.7+0.125i | 1.6 µs | 1.7 µs | 1.6 µs | 1.4 µs | 1.4 µs | 1.7 µs |
| chebyshev_t | 2, 0.7+0.125i | 9.0 µs | 10.8 µs | 11.5 µs | 10.0 µs | 11.5 µs | 15.7 µs |
| chebyshev_u | 2, 0.7+0.125i | 17.5 µs | 11.7 µs | 13.9 µs | 13.3 µs | 14.7 µs | 21.0 µs |
| circ | 0.7 | 2.8 µs | 2.2 µs | 2.5 µs | 3.0 µs | 4.2 µs | 7.9 µs |
| clausen | 3, 0.7+0.125i | 153.0 µs | 361.0 µs | 678.7 µs | 1.494 ms | 4.604 ms | 23.685 ms |
| clausen2 | 0.7+0.125i | 127.2 µs | 318.6 µs | 596.0 µs | 1.420 ms | 4.482 ms | 28.917 ms |
| conj | 0.7+0.125i | 496.1 ns | 280.0 ns | 260.3 ns | 310.9 ns | 271.1 ns | 351.2 ns |
| cos | 0.7+0.125i | 22.2 µs | 14.2 µs | 23.2 µs | 41.6 µs | 93.1 µs | 333.4 µs |
| cosec | 0.7+0.125i | 25.9 µs | 18.0 µs | 26.8 µs | 47.9 µs | 112.1 µs | 338.8 µs |
| cosech | 0.7+0.125i | 32.8 µs † | 16.9 µs | 26.9 µs | 88.2 µs | 81.9 µs | 253.2 µs |
| cosh | 0.7+0.125i | 30.9 µs † | 15.1 µs | 19.8 µs | 81.4 µs | 79.8 µs | 292.5 µs |
| cot | 0.7+0.125i | 45.5 µs † | 34.2 µs | 49.8 µs | 75.5 µs | 199.2 µs | 677.1 µs |
| coth | 0.7+0.125i | 25.9 µs | 27.3 µs | 44.3 µs | 149.3 µs | 170.9 µs | 601.8 µs |
| covercos | 0.7+0.125i | 25.1 µs | 17.9 µs | 23.8 µs | 38.2 µs | 101.5 µs | 319.1 µs |
| coversin | 0.7+0.125i | 24.3 µs | 16.3 µs | 25.0 µs | 36.6 µs | 104.3 µs | 348.6 µs |
| cubrt | 0.7+0.125i | 45.4 µs | 70.4 µs | 84.5 µs | 140.9 µs | 386.7 µs | 1.358 ms |
| digamma | 2.345+0.125i | 226.1 µs | 500.6 µs | 784.5 µs | 1.382 ms | 4.275 ms | 21.954 ms |
| dilog | 0.7+0.125i | 180.9 µs | 419.6 µs | 819.6 µs | 1.452 ms | 5.954 ms | 25.303 ms |
| div | 0.7+0.125i, 0.7+0.125i | 8.2 µs | 11.2 µs | 15.3 µs | 20.4 µs | 39.5 µs | 115.1 µs |
| erf | 0.7+0.125i | 87.1 µs | 200.4 µs | 316.3 µs | 474.8 µs | 2.041 ms | 8.131 ms |
| erfc | 0.7+0.125i | 85.1 µs | 170.2 µs | 281.8 µs | 522.3 µs | 2.033 ms | 8.449 ms |
| erfcinv | 0.5+0.125i | 927.7 µs | 1.487 ms | 2.069 ms | 4.143 ms | 11.672 ms | 49.067 ms |
| erfinv | 0.5+0.125i | 799.4 µs | 1.393 ms | 1.999 ms | 3.964 ms | 11.354 ms | 55.857 ms |
| exp | 1.23456789+0.125i | 10.5 µs | 21.4 µs | 33.5 µs | 47.6 µs | 174.8 µs | 549.4 µs |
| floor | 0.7+0.125i | 266.3 ns | 268.1 ns | 276.1 ns | 298.2 ns | 280.9 ns | 308.0 ns |
| gamma | 2.345+0.125i | 13.4 µs | 13.7 µs | 14.1 µs | 15.7 µs | 14.6 µs | 16.6 µs |
| gammainc_P | 2.3+0.125i, 1.4+0.125i | 71.7 µs | 67.4 µs | 77.8 µs | 74.2 µs | 60.6 µs | 61.2 µs |
| gammainc_Q | 2.3+0.125i, 1.4+0.125i | 63.8 µs | 60.3 µs | 70.1 µs | 59.7 µs | 64.6 µs | 65.8 µs |
| gammainc_lower | 2.3+0.125i, 1.4+0.125i | 46.6 µs | 55.2 µs | 56.3 µs | 45.7 µs | 49.5 µs | 52.3 µs |
| gammainc_upper | 2.3+0.125i, 1.4+0.125i | 62.1 µs | 69.6 µs | 64.7 µs | 64.9 µs | 60.2 µs | 62.1 µs |
| gammainv | 9.5+0.125i | 129.6 µs | 139.2 µs | 138.4 µs | 164.9 µs | 140.6 µs | 164.6 µs |
| hacovercos | 0.7+0.125i | 23.0 µs | 21.3 µs | 25.9 µs | 42.4 µs | 105.9 µs | 371.6 µs |
| hacoversin | 0.7+0.125i | 23.5 µs | 19.9 µs | 32.3 µs | 44.8 µs | 117.4 µs | 410.0 µs |
| harmonic_poly | 2, 0.7+0.125i | 14.3 µs | 18.1 µs | 19.2 µs | 21.1 µs | 30.5 µs | 81.1 µs |
| havercos | 0.7+0.125i | 25.4 µs | 22.6 µs | 29.4 µs | 44.8 µs | 111.8 µs | 350.9 µs |
| haversin | 0.7+0.125i | 23.8 µs | 21.4 µs | 31.3 µs | 45.9 µs | 109.1 µs | 383.4 µs |
| hermite_h | 2, 0.7+0.125i | 11.8 µs | 12.9 µs | 13.0 µs | 14.0 µs | 17.1 µs | 19.4 µs |
| hypergeometric_pFq | 0.7+0.125i, 2.5+0.125i, 0.3+0.125i | 38.0 µs | 36.0 µs | 35.4 µs | 33.7 µs | 36.2 µs | 35.2 µs |
| hypot | 0.7+0.125i, 1.2+0.125i | 2.2 µs | 2.0 µs | 2.2 µs | 2.2 µs | 2.2 µs | 2.4 µs |
| imag_part | 0.7+0.125i | 117.1 ns | 148.7 ns | 121.2 ns | 140.2 ns | 132.0 ns | 165.6 ns |
| inv | 0.7+0.125i | 1.2 µs | 1.5 µs | 1.9 µs | 2.2 µs | 4.5 µs | 12.2 µs |
| lambert_w0 | 0.7+0.125i | 53.7 µs | 63.9 µs | 82.3 µs | 106.0 µs | 300.6 µs | 955.5 µs |
| lambert_wm1 | -0.2+0.125i | 50.6 µs | 65.4 µs | 82.4 µs | 114.4 µs | 297.0 µs | 977.0 µs |
| lambert_wn | 1, 0.7+0.125i | 37.4 µs | 33.3 µs | 33.8 µs | 29.3 µs | 31.3 µs | 34.3 µs |
| lauricella_f | 0.5+0.125i, 0.3+0.125i, 2.5+0.125i | 153.7 µs | 181.3 µs | 153.8 µs | 166.1 µs | 171.1 µs | 219.5 µs |
| ldexp | 1.23456789+0.125i, 3 | 387.3 ns | 379.7 ns | 334.1 ns | 332.0 ns | 337.8 ns | 498.3 ns |
| legendre_chi | 2, 0.7+0.125i | 323.9 µs | 615.5 µs | 1.260 ms | 2.579 ms | 10.872 ms | 72.436 ms |
| lerch_phi | 0.3+0.125i, 2+0.125i, 1.4+0.125i | 808.9 µs | 1.063 ms † | 621.3 µs | 579.9 µs | 600.0 µs | 849.8 µs † |
| lg | 2.345678+0.125i | 33.5 µs | 49.6 µs | 60.3 µs | 67.9 µs | 191.1 µs | 640.5 µs |
| lgamma | 2.345+0.125i | 19.5 µs † | 12.7 µs | 13.3 µs | 12.5 µs | 12.7 µs | 15.4 µs |
| ln | 2.345678+0.125i | 23.5 µs | 33.9 µs | 43.2 µs | 49.5 µs | 149.3 µs | 483.4 µs |
| log | 2.345678+0.125i | 19.6 µs | 30.1 µs | 38.9 µs | 59.7 µs | 146.2 µs | 425.4 µs |
| log10 | 2.345678+0.125i | 24.8 µs | 42.7 µs | 48.5 µs | 68.3 µs | 163.5 µs | 567.1 µs |
| logbeta | 2.3+0.125i, 4.5+0.125i | 38.1 µs | 47.2 µs | 34.5 µs | 36.2 µs | 37.2 µs | 41.1 µs |
| logbeta_pdf | 0.5+0.125i, 2.5+0.125i, 3.5+0.125i | 48.6 µs | 57.1 µs | 42.9 µs | 42.0 µs | 41.7 µs | 57.5 µs |
| lommel_s | 0.5+0.125i, 0.7+0.125i, 1.2+0.125i | 210.1 µs | 595.9 µs | 696.4 µs | 1.344 ms | 4.645 ms | 22.537 ms |
| mul | 0.7+0.125i, 0.7+0.125i | 1.7 µs | 1.7 µs | 2.2 µs | 2.9 µs | 4.1 µs | 9.0 µs |
| mul_long | 17+0.125i, 3 | 3.4 µs | 3.3 µs | 3.7 µs | 3.2 µs | 3.6 µs | 4.2 µs |
| mul_pow10 | 1.23456789+0.125i, 3 | 3.2 µs | 2.8 µs | 3.6 µs | 3.0 µs | 2.9 µs | 4.0 µs |
| neg | 0.7+0.125i | 277.9 ns | 282.6 ns | 308.7 ns | 284.9 ns | 333.5 ns | 386.4 ns |
| normal_cdf | 0.7+0.125i | 89.1 µs | 194.8 µs | 330.3 µs | 508.7 µs | 2.310 ms | 9.186 ms |
| normal_logpdf | 0.7+0.125i | 7.4 µs | 12.1 µs | 15.4 µs | 19.3 µs | 55.6 µs | 141.5 µs |
| normal_pdf | 0.7+0.125i | 20.7 µs | 32.6 µs | 49.2 µs | 82.0 µs | 327.1 µs † | 1.088 ms |
| polygamma | 3, 2.345+0.125i | 334.4 µs | 736.3 µs | 1.117 ms | 1.619 ms | 5.894 ms | 42.202 ms |
| polylog | 2, 0.7+0.125i | 233.2 µs | 434.5 µs | 855.3 µs | 1.476 ms | 6.465 ms | 29.221 ms |
| polylog1 | 0.7+0.125i | 33.5 µs | 39.0 µs | 47.6 µs | 83.3 µs | 151.7 µs | 488.7 µs |
| pow | 1.23456789+0.125i, 3.5+0.125i | 42.1 µs | 56.2 µs | 102.3 µs | 129.6 µs | 393.5 µs | 1.415 ms |
| pow10 | 3 | 11.1 µs | 15.2 µs | 26.3 µs | 33.3 µs | 63.7 µs | 186.1 µs |
| pow_int | 1.23456789+0.125i, 3 | 6.5 µs | 7.6 µs | 8.2 µs | 8.5 µs | 13.4 µs | 24.0 µs |
| productlog | 0.7+0.125i | 53.8 µs | 59.2 µs | 84.7 µs | 123.3 µs | 285.5 µs | 1.113 ms |
| qdigamma | 0.5+0.125i, 2.3+0.125i | 199.9 µs | 167.3 µs | 172.1 µs | 164.2 µs | 151.5 µs | 194.5 µs |
| real_part | 0.7+0.125i | 163.0 ns | 136.1 ns | 138.2 ns | 121.9 ns | 133.0 ns | 166.1 ns |
| rect | 0.7 | 2.7 µs | 2.7 µs | 2.4 µs | 3.4 µs | 4.3 µs | 8.5 µs |
| root | 1.2345+0.125i, 3 | 40.8 µs | 72.1 µs | 169.5 µs | 247.5 µs | 375.2 µs | 1.410 ms |
| round | 0.7+0.125i | 9.3 µs | 9.1 µs | 9.6 µs | 9.5 µs | 9.4 µs | 10.0 µs |
| roundn | 1.23456789+0.125i, 3 | 15.4 µs | 18.1 µs | 17.1 µs | 17.1 µs | 20.7 µs | 33.6 µs |
| rounds | 1.23456789+0.125i, 3 | 46.5 µs | 64.9 µs | 79.6 µs | 96.1 µs | 191.8 µs | 629.8 µs |
| sec | 0.7+0.125i | 16.5 µs | 17.8 µs | 25.9 µs | 34.7 µs | 101.1 µs | 341.1 µs |
| sech | 0.7+0.125i | 12.7 µs | 16.8 µs | 23.0 µs | 80.9 µs | 83.9 µs | 367.0 µs |
| sgn | 0.7 | 743.0 ns | 788.4 ns | 809.3 ns | 814.2 ns | 937.6 ns | 862.8 ns |
| sin | 0.567+0.125i | 16.4 µs | 14.2 µs | 22.1 µs | 34.9 µs | 94.3 µs | 457.2 µs |
| sinc | 0.7+0.125i | 19.0 µs | 28.7 µs | 38.0 µs | 47.7 µs | 128.3 µs | 496.7 µs |
| sinh | 0.7+0.125i | 7.8 µs | 14.1 µs | 20.4 µs | 86.6 µs | 68.8 µs | 338.1 µs |
| sqr | 0.7+0.125i | 1.5 µs | 1.8 µs | 2.4 µs | 2.8 µs | 4.3 µs | 7.7 µs |
| sqrt | 1.23456789+0.125i | 7.4 µs † | 2.9 µs | 3.4 µs | 3.6 µs | 6.8 µs | 17.1 µs |
| step | 0.7 | 1.3 µs † | 807.3 ns | 759.7 ns | 798.0 ns | 1.0 µs | 1.1 µs |
| struve_h | 2, 0.7+0.125i | 239.3 µs | 484.6 µs | 889.7 µs | 1.362 ms | 4.855 ms | 24.368 ms |
| struve_l | 2, 0.7+0.125i | 256.0 µs | 546.4 µs | 833.7 µs | 1.228 ms | 4.959 ms | 23.051 ms |
| sub | 0.7+0.125i, 0.7+0.125i | 344.1 ns | 335.6 ns | 390.6 ns | 384.2 ns | 436.8 ns | 648.9 ns |
| tan | 0.7+0.125i | 11.3 µs | 19.5 µs | 28.8 µs | 37.3 µs | 101.4 µs | 418.0 µs |
| tanh | 0.7+0.125i | 28.5 µs | 16.2 µs | 63.8 µs | 41.1 µs | 97.7 µs | 383.0 µs |
| tetragamma | 2.345+0.125i | 334.2 µs | 692.7 µs | 1.281 ms | 2.070 ms | 6.995 ms | 39.360 ms |
| tri | 0.7 | 2.3 µs | 2.4 µs | 2.8 µs | 3.1 µs | 4.6 µs | 10.4 µs |
| trigamma | 2.345+0.125i | 466.8 µs † | 583.0 µs | 1.112 ms | 1.469 ms | 5.144 ms | 25.445 ms |
| vercos | 0.7+0.125i | 18.2 µs | 17.7 µs | 28.2 µs | 34.5 µs | 101.7 µs | 343.9 µs |
| versin | 0.7+0.125i | 21.4 µs | 17.8 µs | 29.5 µs | 34.8 µs | 108.9 µs | 319.7 µs |
| zatahp | 2.3+0.125i, 1.4+0.125i | 2.211 ms | 4.462 ms | 8.566 ms | 16.518 ms | 106.725 ms | 472.295 ms |
| zeta | 2.345+0.125i | 587.5 µs | 1.168 ms | 2.690 ms | 3.825 ms | 21.074 ms | 114.240 ms |
| zetah | 2.3+0.125i, 1.4+0.125i | 2.064 ms | 3.771 ms | 9.478 ms | 14.282 ms | 78.827 ms | 531.778 ms |
| zetap | 2.345+0.125i | 623.6 µs | 1.313 ms | 3.191 ms | 5.783 ms | 22.846 ms | 128.182 ms |

### `exact-integer`

| Operation | Inputs | Exact |
|---|---|---:|
| bit_and | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 206.0 ns |
| bit_length | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776</span> | 167.4 ns |
| bit_not | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776</span> | 298.3 ns |
| bit_or | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 188.2 ns |
| bit_xor | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 180.4 ns |
| clear_bit | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>701</span> | 206.2 ns |
| divmod | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 325.7 ns |
| factorial | 5000 | 111.2 µs |
| factors | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">54772119019154636339867862041163448790974484131459861520687327809209914339836012<br/>52275954799541486775711126005967382600118182797233708133775335788335658037065081<br/>31569062299976033041302189833215742992653063887339218309467131548996199881113600<br/>00000000000000000000000000000000000000000000000000000000000000000000000000000000<br/>0000000000000000000000000000000000000000000000</span> | 77.1 µs |
| fibonacci | 10000 | 5.0 µs |
| gcd | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 2.7 µs |
| gcdext | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 3.1 µs |
| is_prime | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">68647976601306097149819007990813932172694353001433054093944634591855431833976560<br/>52122559640661454554977296311391480858037121987999716643812574028291115057151</span> | 422.7 µs |
| isqrt | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776</span> | 367.6 ns |
| lcm | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 3.2 µs |
| mod | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 246.8 ns |
| modinv | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>68647976601306097149819007990813932172694353001433054093944634591855431833976560<br/>52122559640661454554977296311391480858037121987999716643812574028291115057151</span> | 3.3 µs |
| next_prime | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 3.750 ms |
| partition | 5000 | 9.383 ms † |
| powmod | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662, <br/>68647976601306097149819007990813932172694353001433054093944634591855431833976560<br/>52122559640661454554977296311391480858037121987999716643812574028291115057151</span> | 311.4 µs † |
| prev_prime | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 6.642 ms |
| prove_prime | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">68647976601306097149819007990813932172694353001433054093944634591855431833976560<br/>52122559640661454554977296311391480858037121987999716643812574028291115057151</span> | 3.235 ms |
| set_bit | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>700</span> | 231.7 ns |
| shl | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>257</span> | 331.3 ns |
| shr | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>257</span> | 250.3 ns |
| test_bit | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>701</span> | 243.4 ns |

<!-- MARS-BENCHMARKS:END -->

The sample results in this document were collected on an
`Intel(R) Core(TM) i7-4510U CPU @ 2.00GHz` laptop, with `4` logical CPUs
(`2` cores, `2` threads per core). Treat them as illustrative rather than as a
strict cross-machine baseline.

## Running Benchmarks

From the repository root:

```sh
make bench_integrator
make bench_matrix_expr
make bench_number_maths
make bench_number_scope
make bench_qfloat_gamma_maths
make bench_qcomplex_maths
```

As with the test suites, prefer running benchmarks sequentially for now. The
current codebase and build products are not yet fully thread-safe for
overlapping runs.

The focused binaries below remain available for specialist workloads. Their
terminal output is independent of the persistent catalogue. Use `make bench-db`
for retained results and database-derived documentation tables.

## Expression Parser Benchmark

`bench/expression/bench_expr_parse.c` measures the public parser, including
construction and disposal of the resulting expression. It reports seven-sample
median, minimum and maximum nanoseconds per parse for ASCII calls, Unicode
aliases, non-function expressions and function-power notation. Run variants
sequentially on the same machine; allocator state, processor frequency and
other workloads can affect the measurements.

The following README example builds the benchmark without running timings,
then verifies that every workload parses:

```sh
make -j1 build/release/bench/expression/bench_expr_parse
build/release/bench/expression/bench_expr_parse --check
```

Output after the build messages:

```text
parser benchmark: 24 inputs verified
```

Omitting `--check` measures the workloads with 1,000 iterations per sample;
an optional positive integer selects the iteration count instead.

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

`bench_matrix_expr` measures dense symbolic `MAT_TYPE_EXPR` linear algebra on
representative exact cases.

It currently covers:

- dense symbolic solve on `3x3` and `6x6` systems
- dense symbolic inverse on `4x4` and `6x6` systems

Sample output:

```text
iters=3

Symbolic expr solve
solve_dense3x3_rhs2      avg_µs=  6000.421 avg_ms=     6.000
solve_dense6x6_rhs2      avg_µs=566623.682 avg_ms=   566.624

Symbolic expr inverse
inverse_dense4x4         avg_µs= 40635.324 avg_ms=    40.635
inverse_dense6x6         avg_µs=876341.423 avg_ms=   876.341
```

These numbers are intended as a rough baseline for the current
fraction-free symbolic elimination path rather than as a strict performance
contract.

## Numeric Benchmarks

### `number`

Available benchmark target:

```sh
make bench_number_maths
```

There is also a scope-focused benchmark target:

```sh
make bench_number_scope
```

This benchmark exercises the generic `number_t` API, including:

- elementary functions such as `exp`, `ln`, `lg`, `sqrt`, `sin`, `cos`, `atan`,
  `sinh`, `tanh`, and `pow`
- paired output functions such as `sincos` and `sinhcosh`
- special functions such as `gamma`, `lgamma`, `digamma`, `trigamma`,
  `tetragamma`, `erf`, `erfc`, `gammainv`, `lambert_w0`, `lambert_wm1`,
  `beta`, `logbeta`, `binomial`, `normal_pdf`, `normal_cdf`, `Ei`, and `E1`
- named constants such as `pi`, `e`, and Euler-Mascheroni

Because `number_t` accepts exact integer and rational inputs as well as
floating-point values, this benchmark is useful for measuring the combined cost
of generic promotion, backend dispatch, and the underlying maths
implementation.

The scope benchmark focuses specifically on temporary-lifetime management. It
compares:

- fully manual ownership with explicit `num_destroy(...)`
- broad whole-scope temporary accumulation
- the preferred pattern of scoped short-lived intermediates plus an ordinary
  owned rolling result

Current sample timings on the benchmark machine for that scope benchmark:

```text
real chain               manual= 129.082 ms  scoped= 125.772 ms  ratio= 1.026x
real scoped+roll         manual= 129.082 ms  scoped= 110.554 ms  ratio= 1.168x
complex chain            manual= 221.584 ms  scoped= 225.807 ms  ratio= 0.981x
complex scoped+roll      manual= 221.584 ms  scoped= 198.489 ms  ratio= 1.116x
```

Use `MARS_BENCH_FORMAT=md` with benchmark binaries when you want docs-ready
tables instead of the ordinary line-by-line terminal output.

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
- `qf_Ei` and `qf_E1`
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
- `qc_Ei`, `qc_E1`, `qc_beta`, and `qc_logbeta`

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
