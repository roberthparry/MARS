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
run once with exact operands in the **number / unrestricted integer** (`exact-integer`)
profile; an arbitrary floating precision would be meaningless for those algorithms.
This profile also measures addition, subtraction, multiplication, exact division,
negation, absolute value, squaring and positive integer powers. The
**number / unrestricted fraction** (`exact-fraction`) profile measures the same
arithmetic and reciprocals using exact rational operands. These profiles have zero
precision bits in storage because their results are exact, rather than rounded to a
floating-point precision.

Integer workloads use a dense 1,024-bit dividend, a dense 512-bit divisor and
exponent, and the 521-bit Mersenne prime 2⁵²¹ − 1. The dividend and divisor share
the non-trivial 128-bit GCD 2·(2¹²⁷ − 1). Bit operations address positions 700 and
701 or shift by 257 bits. Factorisation uses the 1,215-bit smooth composite
2⁵¹²·3²⁵⁶·5¹²⁸, exercising large integers without an unbounded prime search.
Factorial and Fibonacci accept native integer indices by API design: the cases
5,000! and F(10,000) produce exact 54,233-bit and 6,942-bit results. The partition
case is p(5,000). Input parsing and exact-integer result checks are outside timing.
Saved arguments distinguish these workloads from earlier small-input timings.
Integer division uses the dividend's 128-bit common divisor so its result remains
an integer. The fraction operands are the 1,024-bit dividend over the 521-bit prime
and the 512-bit divisor over the 1,024-bit dividend; canonical reduction precedes
timing. The rational working-size budget is 16,384 bits, sufficient for these
operands and their fifth powers. Every measured result is checked to retain the
profile's exact integer or fraction representation.

Arithmetic workloads are authored in `bench/number/bench_number.sql` and merged
into `benchmark_workload`, indexed by operation and backend. Their actual argument
literals are copied into each run and passed to its isolated workers; resuming a
run retains its saved operands. Synchronisation preserves other workloads and
previous observations.

In generated Markdown tables, unrestricted integer and fraction Inputs cells occupy up to 80 character widths,
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
Benchmark run **29**, 2026-11-11T21:56:14.598Z (UTC).

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
| E1 | 2.345678 | 9.1 µs |
| Ei | 2.345678 | 10.7 µs |
| Li | 2.345 | 20.4 µs † |
| abs | 0.7 | 9.6 ns |
| acos | 0.7 | 24.2 ns |
| acosec | 2.345 | 23.6 ns |
| acosech | 0.7 | 70.8 ns |
| acosh | 2.345 | 49.0 ns |
| acot | 0.7 | 65.4 ns |
| acoth | 2.345 | 40.9 ns |
| add | 0.7, 0.7 | 17.4 ns |
| add_long | 17, 3 | 597.3 ns |
| appell_f1 | 0.5, 0.7, 0.8, 2.5, 0.2, 0.3 | 34.8 µs |
| arccovercos | 0.5 | 142.6 ns |
| arccoversin | 0.5 | 145.7 ns |
| archacovercos | 0.5 | 279.4 ns |
| archacoversin | 0.5 | 279.5 ns |
| archavercos | 0.5 | 269.5 ns |
| archaversin | 0.5 | 313.7 ns |
| arcvercos | 0.5 | 166.7 ns |
| arcversin | 0.5 | 169.3 ns |
| arg | 0.7 | 101.1 ns |
| asec | 2.345 | 28.9 ns |
| asech | 0.7 | 48.6 ns |
| asin | 0.7 | 26.8 ns |
| asinh | 0.7 | 68.0 ns |
| atan | 0.7 | 46.5 ns |
| atan2 | 0.5, -0.75 | 54.5 ns |
| atanh | 0.7 | 37.5 ns |
| bessel_i | 2, 0.7 | 53.6 µs |
| bessel_j | 2, 0.7 | 7.7 µs |
| bessel_k | 2, 0.7 | 32.8 µs |
| bessel_y | 2, 0.7 | 25.9 µs |
| beta | 2.3, 4.5 | 206.4 ns |
| beta_pdf | 0.5, 2.5, 3.5 | 240.1 ns |
| binomial | 5, 2 | 216.5 ns |
| ceil | 0.7 | 77.9 ns |
| chebyshev_t | 2, 0.7 | 906.6 ns |
| chebyshev_u | 2, 0.7 | 1.7 µs † |
| circ | 0.7 | 373.0 ns |
| clausen | 3, 0.7 | 10.4 µs |
| clausen2 | 0.7 | 9.5 µs |
| conj | 0.7 | 11.3 ns |
| cos | 0.7 | 51.7 ns |
| cosec | 0.7 | 46.1 ns |
| cosech | 0.7 | 63.3 ns |
| cosh | 0.7 | 46.9 ns |
| cot | 0.7 | 56.5 ns |
| coth | 0.7 | 77.9 ns |
| covercos | 0.7 | 158.8 ns |
| coversin | 0.7 | 185.1 ns |
| cubrt | 0.7 | 10.5 µs † |
| digamma | 2.345 | 3.1 µs |
| dilog | 0.7 | 19.4 µs |
| div | 0.7, 0.7 | 15.6 ns |
| erf | 0.7 | 90.0 ns |
| erfc | 0.7 | 101.1 ns |
| erfcinv | 0.5 | 314.7 µs |
| erfinv | 0.5 | 249.8 µs |
| exp | 1.23456789 | 57.2 ns |
| floor | 0.7 | 12.7 ns |
| gamma | 2.345 | 136.8 ns |
| gammainc_P | 2.3, 1.4 | 15.7 µs |
| gammainc_Q | 2.3, 1.4 | 16.3 µs |
| gammainc_lower | 2.3, 1.4 | 16.5 µs |
| gammainc_upper | 2.3, 1.4 | 20.3 µs † |
| gammainv | 9.5 | 117.7 µs |
| hacovercos | 0.7 | 333.4 ns |
| hacoversin | 0.7 | 333.4 ns |
| harmonic_poly | 2, 0.7 | 2.0 µs |
| havercos | 0.7 | 315.2 ns |
| haversin | 0.7 | 340.8 ns |
| hermite_h | 2, 0.7 | 2.4 µs † |
| hypergeometric_pFq | 0.7, 2.5, 0.3 | 5.7 µs † |
| hypot | 0.7, 1.2 | 34.3 ns |
| imag_part | 0.7 | 29.6 ns |
| inv | 0.7 | 13.6 ns |
| lambert_w0 | 0.7 | 22.0 µs † |
| lambert_wm1 | -0.2 | 18.8 µs |
| lambert_wn | 1, 0.7 | 38.4 µs † |
| lauricella_f | 0.5, 0.3, 2.5 | 26.7 µs † |
| ldexp | 1.23456789, 3 | 70.5 ns |
| legendre_chi | 2, 0.7 | 35.7 µs |
| lerch_phi | 0.3, 2, 1.4 | 340.3 µs |
| lg | 2.345678 | 30.3 ns |
| lgamma | 2.345 | 62.2 ns |
| ln | 2.345678 | 22.1 ns |
| log | 2.345678 | 17.9 ns |
| log10 | 2.345678 | 32.3 ns |
| logbeta | 2.3, 4.5 | 162.6 ns |
| logbeta_pdf | 0.5, 2.5, 3.5 | 230.9 ns |
| lommel_s | 0.5, 0.7, 1.2 | 9.9 µs |
| mul | 0.7, 0.7 | 16.9 ns |
| mul_long | 17, 3 | 642.3 ns |
| mul_pow10 | 1.23456789, 3 | 94.5 ns |
| neg | 0.7 | 12.7 ns |
| normal_cdf | 0.7 | 119.8 ns |
| normal_logpdf | 0.7 | 19.7 ns |
| normal_pdf | 0.7 | 30.1 ns |
| polygamma | 3, 2.345 | 31.9 µs |
| polylog | 2, 0.7 | 16.6 µs |
| polylog1 | 0.7 | 201.3 ns |
| pow | 1.23456789, 3.5 | 49.9 ns |
| pow10 | 3 | 215.2 ns |
| pow_int | 1.23456789, 3 | 95.4 ns |
| productlog | 0.7 | 19.8 µs |
| qdigamma | 0.5, 2.3 | 37.1 µs † |
| real_part | 0.7 | 37.7 ns |
| rect | 0.7 | 422.7 ns |
| root | 1.2345, 3 | 8.4 µs |
| round | 0.7 | 625.9 ns |
| roundn | 1.23456789, 3 | 5.4 µs |
| rounds | 1.23456789, 3 | 9.5 µs † |
| sec | 0.7 | 39.5 ns |
| sech | 0.7 | 58.9 ns |
| sgn | 0.7 | 180.5 ns |
| sin | 0.567 | 57.1 ns |
| sinc | 0.7 | 5.2 µs † |
| sincos | 0.7 | 1.2 µs † |
| sinh | 0.7 | 69.4 ns |
| sinhcosh | 0.7 | 172.7 ns |
| sqr | 0.7 | 11.7 ns |
| sqrt | 1.23456789 | 12.2 ns |
| step | 0.7 | 196.5 ns |
| struve_h | 2, 0.7 | 99.8 µs |
| struve_l | 2, 0.7 | 88.5 µs |
| sub | 0.7, 0.7 | 18.9 ns |
| tan | 0.7 | 8.3 µs |
| tanh | 0.7 | 78.1 ns |
| tetragamma | 2.345 | 5.0 µs † |
| tri | 0.7 | 336.9 ns |
| trigamma | 2.345 | 3.7 µs |
| vercos | 0.7 | 188.2 ns |
| versin | 0.7 | 195.2 ns |
| zatahp | 2.3, 1.4 | 486.6 µs |
| zeta | 2.345 | 89.9 µs |
| zetah | 2.3, 1.4 | 404.4 µs |
| zetap | 2.345 | 180.1 µs |

### `qfloat`

| Operation | Inputs | 106 bits |
|---|---|---:|
| E1 | 2.345678 | 11.7 µs |
| Ei | 2.345678 | 11.0 µs |
| Li | 2.345 | 18.7 µs † |
| abs | 0.7 | 3.2 ns |
| acos | 0.7 | 2.5 µs † |
| acosec | 2.345 | 2.6 µs † |
| acosech | 0.7 | 4.1 µs |
| acosh | 2.345 | 4.5 µs |
| acot | 0.7 | 1.9 µs † |
| acoth | 2.345 | 4.3 µs |
| add | 0.7, 0.7 | 9.5 ns |
| appell_f1 | 0.5, 0.7, 0.8, 2.5, 0.2, 0.3 | 122.2 µs † |
| arccovercos | 0.5 | 2.6 µs † |
| arccoversin | 0.5 | 2.4 µs |
| archacovercos | 0.5 | 36.3 ns |
| archacoversin | 0.5 | 45.0 ns |
| archavercos | 0.5 | 84.7 ns |
| archaversin | 0.5 | 86.6 ns |
| arcvercos | 0.5 | 2.4 µs † |
| arcversin | 0.5 | 2.6 µs † |
| asec | 2.345 | 2.6 µs † |
| asech | 0.7 | 4.9 µs |
| asin | 0.7 | 2.5 µs † |
| asinh | 0.7 | 5.3 µs † |
| atan | 0.7 | 1.8 µs |
| atan2 | 0.5, -0.75 | 1.8 µs |
| atanh | 0.7 | 4.7 µs † |
| bessel_i | 2, 0.7 | 82.7 µs † |
| bessel_j | 2, 0.7 | 9.9 µs † |
| bessel_k | 2, 0.7 | 38.4 µs |
| bessel_y | 2, 0.7 | 39.3 µs † |
| beta | 2.3, 4.5 | 5.7 µs |
| beta_pdf | 0.5, 2.5, 3.5 | 38.4 µs |
| binomial | 5, 2 | 50.8 µs † |
| chebyshev_t | 2, 0.7 | 56.2 ns |
| chebyshev_u | 2, 0.7 | 56.2 ns |
| circ | 0.7 | 19.4 ns |
| clausen | 3, 0.7 | 402.4 µs |
| clausen2 | 0.7 | 7.8 µs |
| cos | 0.7 | 1.0 µs |
| cosec | 0.7 | 1.0 µs |
| cosech | 0.7 | 5.3 µs † |
| cosh | 0.7 | 4.6 µs |
| cot | 0.7 | 2.3 µs |
| coth | 0.7 | 8.8 µs |
| covercos | 0.7 | 1.2 µs |
| coversin | 0.7 | 1.0 µs |
| cubrt | 0.7 | 6.6 µs |
| digamma | 2.345 | 3.8 µs |
| dilog | 0.7 | 33.8 µs † |
| div | 0.7, 0.7 | 59.0 ns |
| erf | 0.7 | 5.4 µs |
| erfc | 0.7 | 5.4 µs |
| erfcinv | 0.5 | 257.4 µs |
| erfinv | 0.5 | 326.0 µs † |
| exp | 1.23456789 | 2.2 µs |
| floor | 0.7 | 29.4 ns |
| gamma | 2.345 | 2.4 µs † |
| gammainc_P | 2.3, 1.4 | 18.8 µs † |
| gammainc_Q | 2.3, 1.4 | 20.8 µs † |
| gammainc_lower | 2.3, 1.4 | 18.9 µs |
| gammainc_upper | 2.3, 1.4 | 22.4 µs † |
| gammainv | 9.5 | 161.0 µs † |
| hacovercos | 0.7 | 1.3 µs |
| hacoversin | 0.7 | 1.3 µs |
| harmonic_poly | 2, 0.7 | 149.7 ns |
| havercos | 0.7 | 1.3 µs |
| haversin | 0.7 | 1.4 µs |
| hermite_h | 2, 0.7 | 64.5 ns |
| hypergeometric_pFq | 0.7, 2.5, 0.3 | 5.0 µs |
| hypot | 0.7, 1.2 | 617.5 ns |
| lambert_w0 | 0.7 | 18.2 µs |
| lambert_wm1 | -0.2 | 25.1 µs † |
| lauricella_f | 0.5, 0.3, 2.5 | 25.7 µs |
| ldexp | 1.23456789, 3 | 11.9 ns |
| legendre_chi | 2, 0.7 | 35.7 µs |
| lerch_phi | 0.3, 2, 1.4 | 472.5 µs † |
| lg | 2.345678 | 5.1 µs † |
| lgamma | 2.345 | 7.7 µs |
| ln | 2.345678 | 3.3 µs |
| log | 2.345678 | 4.4 µs † |
| log10 | 2.345678 | 3.9 µs |
| logbeta | 2.3, 4.5 | 37.4 µs |
| logbeta_pdf | 0.5, 2.5, 3.5 | 35.9 µs |
| lommel_s | 0.5, 0.7, 1.2 | 8.7 µs |
| mul | 0.7, 0.7 | 8.7 ns |
| mul_pow10 | 1.23456789, 3 | 49.6 ns |
| neg | 0.7 | 2.9 ns |
| normal_cdf | 0.7 | 4.1 µs |
| normal_logpdf | 0.7 | 37.8 ns |
| normal_pdf | 0.7 | 2.2 µs |
| polygamma | 3, 2.345 | 11.7 µs † |
| polylog | 2, 0.7 | 21.0 µs † |
| polylog1 | 0.7 | 4.3 µs |
| pow | 1.23456789, 3.5 | 7.3 µs † |
| pow10 | 3 | 29.4 ns |
| pow_int | 1.23456789, 3 | 57.0 ns |
| productlog | 0.7 | 20.4 µs |
| qdigamma | 0.5, 2.3 | 32.0 µs |
| rect | 0.7 | 17.9 ns |
| root | 1.2345, 3 | 6.2 µs |
| round | 0.7 | 50.2 ns |
| roundn | 1.23456789, 3 | 9.2 µs † |
| rounds | 1.23456789, 3 | 23.7 µs |
| sec | 0.7 | 1.3 µs † |
| sech | 0.7 | 5.2 µs † |
| sgn | 0.7 | 9.7 ns |
| sin | 0.567 | 1.1 µs |
| sinc | 0.7 | 1.7 µs † |
| sinh | 0.7 | 3.7 µs |
| sqr | 0.7 | 6.4 ns |
| sqrt | 1.23456789 | 466.4 ns |
| step | 0.7 | 11.3 ns |
| struve_h | 2, 0.7 | 98.3 µs |
| struve_l | 2, 0.7 | 99.6 µs |
| sub | 0.7, 0.7 | 9.5 ns |
| tan | 0.7 | 2.7 µs |
| tanh | 0.7 | 2.0 µs |
| tetragamma | 2.345 | 4.6 µs † |
| tri | 0.7 | 19.4 ns |
| trigamma | 2.345 | 3.6 µs |
| vercos | 0.7 | 1.1 µs |
| versin | 0.7 | 1.1 µs |
| zatahp | 2.3, 1.4 | 401.6 µs |
| zeta | 2.345 | 469.2 µs † |
| zetah | 2.3, 1.4 | 417.4 µs |
| zetap | 2.345 | 448.9 µs † |

### `qcomplex`

| Operation | Inputs | 106 bits |
|---|---|---:|
| E1 | 2.345678+0.125i | 66.9 µs † |
| Ei | 2.345678+0.125i | 64.0 µs |
| Li | 2.345+0.125i | 50.4 µs |
| abs | 0.7+0.125i | 667.6 ns |
| acos | 0.7+0.125i | 9.7 µs |
| acosec | 2.345+0.125i | 11.0 µs |
| acosech | 0.7+0.125i | 11.1 µs |
| acosh | 2.345+0.125i | 16.1 µs |
| acot | 0.7+0.125i | 13.1 µs |
| acoth | 2.345+0.125i | 7.1 µs |
| add | 0.7+0.125i, 0.7+0.125i | 21.3 ns |
| appell_f1 | <span style="display:inline-block;width:40ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">0.5+0.125i, 0.7+0.125i, 0.8+0.125i, 2.5+<br/>0.125i, 0.2+0.125i, 0.3+0.125i</span> | 718.8 µs |
| arccovercos | 0.5+0.125i | 11.9 µs |
| arccoversin | 0.5+0.125i | 12.8 µs † |
| archacovercos | 0.5+0.125i | 6.8 µs |
| archacoversin | 0.5+0.125i | 7.7 µs † |
| archavercos | 0.5+0.125i | 6.8 µs |
| archaversin | 0.5+0.125i | 6.8 µs |
| arcvercos | 0.5+0.125i | 11.9 µs |
| arcversin | 0.5+0.125i | 12.0 µs |
| arg | 0.7+0.125i | 1.8 µs |
| asec | 2.345+0.125i | 11.2 µs |
| asech | 0.7+0.125i | 19.3 µs † |
| asin | 0.7+0.125i | 11.2 µs |
| asinh | 0.7+0.125i | 10.9 µs |
| atan | 0.7+0.125i | 16.2 µs † |
| atan2 | 0.5+0.125i, -0.75+0.125i | 13.6 µs |
| atanh | 0.7+0.125i | 6.8 µs |
| bessel_i | 2, 0.7+0.125i | 158.3 µs † |
| bessel_j | 2, 0.7+0.125i | 161.3 µs † |
| bessel_k | 2, 0.7+0.125i | 111.3 µs |
| bessel_y | 2, 0.7+0.125i | 147.7 µs † |
| beta | 2.3+0.125i, 4.5+0.125i | 55.0 µs |
| beta_pdf | 0.5+0.125i, 2.5+0.125i, 3.5+0.125i | 78.3 µs |
| binomial | 5+0.125i, 2+0.125i | 41.1 µs † |
| chebyshev_t | 2, 0.7+0.125i | 217.0 ns |
| chebyshev_u | 2, 0.7+0.125i | 230.1 ns |
| circ | 0.7 | 23.6 ns |
| clausen | 3, 0.7+0.125i | 458.3 µs |
| clausen2 | 0.7+0.125i | 35.5 µs |
| conj | 0.7+0.125i | 4.7 ns |
| cos | 0.7+0.125i | 10.4 µs |
| cosec | 0.7+0.125i | 12.9 µs † |
| cosech | 0.7+0.125i | 10.6 µs |
| cosh | 0.7+0.125i | 11.7 µs |
| cot | 0.7+0.125i | 20.5 µs |
| coth | 0.7+0.125i | 24.0 µs |
| covercos | 0.7+0.125i | 11.6 µs |
| coversin | 0.7+0.125i | 9.7 µs |
| cubrt | 0.7+0.125i | 9.9 µs |
| digamma | 2.345+0.125i | 17.4 µs † |
| dilog | 0.7+0.125i | 114.2 µs |
| div | 0.7+0.125i, 0.7+0.125i | 201.5 ns |
| erf | 0.7+0.125i | 54.6 µs |
| erfc | 0.7+0.125i | 55.5 µs |
| erfcinv | 0.5+0.125i | 400.0 µs |
| erfinv | 0.5+0.125i | 410.0 µs |
| exp | 1.23456789+0.125i | 4.7 µs |
| floor | 0.7+0.125i | 47.4 ns |
| gamma | 2.345+0.125i | 17.4 µs |
| gammainc_P | 2.3+0.125i, 1.4+0.125i | 80.6 µs |
| gammainc_Q | 2.3+0.125i, 1.4+0.125i | 82.6 µs |
| gammainc_lower | 2.3+0.125i, 1.4+0.125i | 65.3 µs † |
| gammainc_upper | 2.3+0.125i, 1.4+0.125i | 88.1 µs † |
| gammainv | 9.5+0.125i | 198.2 µs |
| hacovercos | 0.7+0.125i | 10.1 µs |
| hacoversin | 0.7+0.125i | 11.4 µs |
| harmonic_poly | 2, 0.7+0.125i | 655.1 ns |
| havercos | 0.7+0.125i | 11.8 µs |
| haversin | 0.7+0.125i | 10.4 µs |
| hermite_h | 2, 0.7+0.125i | 251.6 ns |
| hypergeometric_pFq | 0.7+0.125i, 2.5+0.125i, 0.3+0.125i | 44.5 µs † |
| hypot | 0.7+0.125i, 1.2+0.125i | 1.8 µs |
| imag_part | 0.7+0.125i | 3.1 ns |
| inv | 0.7+0.125i | 226.9 ns |
| lambert_w0 | 0.7+0.125i | 46.3 µs † |
| lambert_wm1 | -0.2+0.125i | 37.4 µs |
| lambert_wn | 1, 0.7+0.125i | 36.0 µs |
| lauricella_f | 0.5+0.125i, 0.3+0.125i, 2.5+0.125i | 179.6 µs |
| ldexp | 1.23456789+0.125i, 3 | 32.0 ns |
| legendre_chi | 2, 0.7+0.125i | 454.0 µs |
| lerch_phi | 0.3+0.125i, 2+0.125i, 1.4+0.125i | 789.9 µs |
| lg | 2.345678+0.125i | 7.0 µs † |
| lgamma | 2.345+0.125i | 14.2 µs |
| ln | 2.345678+0.125i | 6.5 µs |
| log | 2.345678+0.125i | 6.1 µs |
| log10 | 2.345678+0.125i | 6.7 µs |
| logbeta | 2.3+0.125i, 4.5+0.125i | 43.4 µs |
| logbeta_pdf | 0.5+0.125i, 2.5+0.125i, 3.5+0.125i | 51.2 µs |
| lommel_s | 0.5+0.125i, 0.7+0.125i, 1.2+0.125i | 169.3 µs † |
| mul | 0.7+0.125i, 0.7+0.125i | 68.6 ns |
| neg | 0.7+0.125i | 8.1 ns |
| normal_cdf | 0.7+0.125i | 60.5 µs |
| normal_logpdf | 0.7+0.125i | 186.9 ns |
| normal_pdf | 0.7+0.125i | 5.5 µs † |
| polygamma | 3, 2.345+0.125i | 20.0 µs |
| polylog | 2, 0.7+0.125i | 129.4 µs |
| polylog1 | 0.7+0.125i | 5.7 µs |
| pow | 1.23456789+0.125i, 3.5+0.125i | 11.3 µs |
| productlog | 0.7+0.125i | 45.1 µs † |
| qdigamma | 0.5+0.125i, 2.3+0.125i | 242.2 µs † |
| real_part | 0.7+0.125i | 3.5 ns |
| rect | 0.7 | 23.5 ns |
| root | 1.2345+0.125i, 3 | 10.0 µs |
| round | 0.7+0.125i | 96.1 ns |
| roundn | 1.23456789+0.125i, 3 | 13.8 µs |
| rounds | 1.23456789+0.125i, 3 | 50.2 µs † |
| sec | 0.7+0.125i | 13.9 µs |
| sech | 0.7+0.125i | 15.2 µs † |
| sgn | 0.7 | 16.9 ns |
| sin | 0.567+0.125i | 10.2 µs |
| sinc | 0.7+0.125i | 12.2 µs |
| sinh | 0.7+0.125i | 12.3 µs |
| sqr | 0.7+0.125i | 65.4 ns |
| sqrt | 1.23456789+0.125i | 5.2 µs |
| step | 0.7 | 14.0 ns |
| struve_h | 2, 0.7+0.125i | 179.4 µs |
| struve_l | 2, 0.7+0.125i | 180.1 µs |
| sub | 0.7+0.125i, 0.7+0.125i | 20.1 ns |
| tan | 0.7+0.125i | 21.9 µs |
| tanh | 0.7+0.125i | 22.9 µs |
| tetragamma | 2.345+0.125i | 6.3 µs † |
| tri | 0.7 | 20.7 ns |
| trigamma | 2.345+0.125i | 4.7 µs |
| vercos | 0.7+0.125i | 11.1 µs |
| versin | 0.7+0.125i | 12.0 µs † |
| zatahp | 2.3+0.125i, 1.4+0.125i | 827.8 µs |
| zeta | 2.345+0.125i | 599.7 µs |
| zetah | 2.3+0.125i, 1.4+0.125i | 776.2 µs |
| zetap | 2.345+0.125i | 514.5 µs |

### `mpfr`

| Operation | Inputs | 256 bits | 512 bits | 768 bits | 1024 bits | 2048 bits | 4096 bits |
|---|---|---:|---:|---:|---:|---:|---:|
| E1 | 2.345678 | 55.3 µs | 88.7 µs | 155.7 µs | 209.6 µs | 576.2 µs | 2.538 ms |
| Ei | 2.345678 | 55.6 µs | 93.0 µs | 142.8 µs | 216.9 µs | 647.2 µs | 2.598 ms |
| Li | 2.345 | 54.0 µs † | 84.4 µs | 147.5 µs | 202.0 µs | 538.1 µs | 2.218 ms |
| abs | 0.7 | 106.8 ns | 145.8 ns | 106.2 ns | 134.0 ns | 131.1 ns | 122.2 ns |
| acos | 0.7 | 21.7 µs † | 42.5 µs † | 51.9 µs | 70.4 µs | 120.4 µs | 515.6 µs |
| acosec | 2.345 | 21.4 µs | 36.9 µs | 57.6 µs † | 69.8 µs | 163.1 µs | 426.1 µs |
| acosech | 0.7 | 8.2 µs | 11.1 µs | 16.5 µs | 24.7 µs † | 46.6 µs | 155.3 µs |
| acosh | 2.345 | 8.2 µs | 14.3 µs | 16.9 µs | 24.8 µs | 56.4 µs | 165.3 µs |
| acot | 0.7 | 20.2 µs | 32.5 µs | 37.7 µs | 60.5 µs | 146.5 µs | 391.9 µs |
| acoth | 2.345 | 8.4 µs † | 12.4 µs | 15.9 µs | 24.0 µs | 55.9 µs | 138.4 µs |
| add | 0.7, 0.7 | 122.6 ns | 127.3 ns | 153.5 ns | 134.6 ns | 213.0 ns | 347.4 ns |
| add_long | 17, 3 | 826.7 ns | 991.5 ns | 880.5 ns | 1.1 µs | 979.3 ns | 922.6 ns |
| appell_f1 | 0.5, 0.7, 0.8, 2.5, 0.2, 0.3 | 185.0 µs † | 412.8 µs | 714.5 µs | 1.353 ms | 6.215 ms | 30.577 ms |
| arccovercos | 0.5 | 24.1 µs † | 30.0 µs | 35.8 µs | 54.4 µs | 126.0 µs | 337.3 µs |
| arccoversin | 0.5 | 21.5 µs † | 31.9 µs | 43.1 µs | 74.9 µs † | 119.0 µs | 335.2 µs |
| archacovercos | 0.5 | 1.5 µs | 1.7 µs | 2.4 µs | 1.9 µs | 1.9 µs | 2.3 µs † |
| archacoversin | 0.5 | 1.9 µs | 1.9 µs | 1.9 µs | 2.2 µs | 1.7 µs | 2.4 µs † |
| archavercos | 0.5 | 1.7 µs | 1.8 µs | 2.2 µs | 2.1 µs | 2.3 µs | 1.8 µs |
| archaversin | 0.5 | 1.7 µs | 2.1 µs | 2.1 µs | 2.1 µs † | 1.7 µs | 2.6 µs |
| arcvercos | 0.5 | 20.7 µs | 31.5 µs | 37.2 µs | 60.4 µs | 148.0 µs | 410.4 µs † |
| arcversin | 0.5 | 18.4 µs | 35.4 µs † | 55.6 µs † | 61.0 µs | 125.7 µs | 351.8 µs |
| arg | 0.7 | 716.6 ns | 682.8 ns | 646.2 ns | 888.4 ns | 695.2 ns | 653.6 ns |
| asec | 2.345 | 22.0 µs | 41.6 µs † | 49.0 µs | 70.7 µs | 150.5 µs | 444.0 µs |
| asech | 0.7 | 9.1 µs † | 13.4 µs | 17.6 µs | 21.0 µs | 51.9 µs | 157.0 µs |
| asin | 0.7 | 22.5 µs | 38.0 µs | 41.0 µs | 59.5 µs | 137.7 µs | 394.8 µs |
| asinh | 0.7 | 8.0 µs † | 11.9 µs | 17.6 µs | 23.2 µs | 49.4 µs | 139.7 µs |
| atan | 0.7 | 16.4 µs | 29.0 µs | 44.1 µs | 57.7 µs | 117.9 µs | 399.9 µs |
| atan2 | 0.5, -0.75 | 19.8 µs † | 34.7 µs † | 43.6 µs | 54.6 µs | 128.3 µs | 407.6 µs |
| atanh | 0.7 | 7.1 µs | 13.5 µs | 17.6 µs | 22.6 µs | 55.4 µs | 149.5 µs |
| bessel_i | 2, 0.7 | 135.2 µs | 276.4 µs | 409.8 µs | 664.2 µs | 2.132 ms | 11.387 ms |
| bessel_j | 2, 0.7 | 8.4 µs | 20.6 µs | 36.7 µs | 47.9 µs | 232.1 µs | 965.3 µs |
| bessel_k | 2, 0.7 | 62.4 µs | 132.0 µs | 214.9 µs | 311.1 µs | 1.043 ms | 5.183 ms |
| bessel_y | 2, 0.7 | 60.1 µs | 120.1 µs | 188.9 µs | 231.0 µs | 1.334 ms † | 4.699 ms |
| beta | 2.3, 4.5 | 156.6 µs † | 365.6 µs | 630.0 µs | 1.076 ms | 4.311 ms | 17.012 ms |
| beta_pdf | 0.5, 2.5, 3.5 | 141.3 µs | 245.4 µs | 566.3 µs | 654.5 µs | 3.050 ms | 11.572 ms |
| binomial | 5, 2 | 132.3 µs | 310.2 µs | 627.6 µs † | 997.4 µs | 4.150 ms | 14.480 ms |
| ceil | 0.7 | 730.3 ns | 618.8 ns | 675.6 ns | 789.4 ns | 614.6 ns | 738.8 ns |
| chebyshev_t | 2, 0.7 | 3.7 µs † | 4.2 µs | 4.4 µs | 3.4 µs | 5.2 µs | 8.5 µs |
| chebyshev_u | 2, 0.7 | 3.8 µs | 4.7 µs | 4.8 µs | 4.3 µs | 5.6 µs | 8.6 µs |
| circ | 0.7 | 1.1 µs | 1.1 µs | 1.1 µs | 1.1 µs | 1.2 µs | 1.5 µs |
| clausen | 3, 0.7 | 35.0 µs | 95.9 µs † | 190.2 µs † | 284.7 µs | 1.161 ms | 6.679 ms |
| clausen2 | 0.7 | 33.3 µs | 87.0 µs | 197.2 µs | 345.3 µs † | 1.009 ms | 5.709 ms |
| conj | 0.7 | 103.2 ns | 100.5 ns | 144.5 ns | 105.0 ns | 120.7 ns | 134.2 ns |
| cos | 0.7 | 2.8 µs | 5.5 µs | 11.6 µs † | 15.3 µs | 67.1 µs † | 191.4 µs |
| cosec | 0.7 | 4.4 µs | 8.2 µs | 11.5 µs | 17.0 µs | 51.7 µs | 185.0 µs |
| cosech | 0.7 | 4.1 µs † | 7.1 µs | 14.3 µs | 20.7 µs | 61.3 µs | 221.4 µs |
| cosh | 0.7 | 4.1 µs † | 7.6 µs | 12.4 µs | 18.9 µs | 62.5 µs | 247.9 µs |
| cot | 0.7 | 7.0 µs | 12.0 µs | 23.2 µs | 29.6 µs | 118.4 µs † | 439.1 µs |
| coth | 0.7 | 6.9 µs | 17.6 µs | 25.7 µs | 33.8 µs | 136.4 µs † | 534.0 µs |
| covercos | 0.7 | 5.3 µs † | 10.3 µs † | 15.6 µs † | 19.5 µs | 48.4 µs | 212.4 µs |
| coversin | 0.7 | 4.9 µs | 9.6 µs † | 12.3 µs | 16.8 µs | 48.8 µs | 217.4 µs † |
| cubrt | 0.7 | 18.7 µs † | 29.4 µs † | 33.7 µs | 51.4 µs † | 139.2 µs | 415.3 µs † |
| digamma | 2.345 | 148.6 µs † | 326.9 µs | 634.2 µs | 1.252 ms | 5.025 ms | 22.763 ms |
| dilog | 0.7 | 132.8 µs | 305.6 µs | 666.7 µs † | 1.077 ms | 4.724 ms | 23.967 ms |
| div | 0.7, 0.7 | 443.7 ns | 743.2 ns | 982.3 ns | 3.5 µs † | 3.9 µs | 13.3 µs |
| erf | 0.7 | 21.8 µs | 53.4 µs | 92.3 µs | 295.5 µs † | 596.7 µs | 2.556 ms |
| erfc | 0.7 | 22.6 µs | 56.4 µs | 77.2 µs | 403.8 µs † | 608.6 µs | 2.562 ms |
| erfcinv | 0.5 | 529.3 µs † | 702.1 µs | 882.5 µs | 1.540 ms | 4.086 ms | 15.788 ms |
| erfinv | 0.5 | 525.5 µs † | 628.4 µs | 751.8 µs | 1.605 ms † | 3.616 ms | 14.362 ms |
| exp | 1.23456789 | 10.0 µs † | 7.7 µs | 11.9 µs | 23.9 µs | 65.1 µs | 210.2 µs |
| floor | 0.7 | 145.0 ns | 110.5 ns | 111.1 ns | 138.1 ns | 114.2 ns | 140.2 ns |
| gamma | 2.345 | 45.9 µs | 99.0 µs | 179.5 µs | 351.4 µs † | 1.309 ms | 6.765 ms † |
| gammainc_P | 2.3, 1.4 | 160.7 µs † | 351.8 µs | 605.6 µs | 844.5 µs | 3.848 ms | 16.183 ms |
| gammainc_Q | 2.3, 1.4 | 127.5 µs | 305.6 µs | 971.9 µs | 1.475 ms | 7.408 ms † | 30.428 ms |
| gammainc_lower | 2.3, 1.4 | 93.6 µs | 213.0 µs | 369.8 µs † | 599.9 µs | 2.559 ms † | 11.170 ms |
| gammainc_upper | 2.3, 1.4 | 133.5 µs | 326.9 µs | 1.066 ms | 1.810 ms † | 6.654 ms | 31.626 ms |
| gammainv | 9.5 | 930.6 µs | 2.378 ms † | 3.290 ms | 5.735 ms | 17.145 ms | 77.310 ms |
| hacovercos | 0.7 | 6.2 µs † | 11.2 µs † | 18.4 µs † | 25.5 µs † | 58.3 µs | 246.0 µs † |
| hacoversin | 0.7 | 7.6 µs † | 9.5 µs | 14.4 µs | 20.2 µs | 68.4 µs | 194.9 µs |
| harmonic_poly | 2, 0.7 | 6.4 µs † | 5.8 µs † | 7.2 µs | 8.7 µs | 14.1 µs | 40.9 µs |
| havercos | 0.7 | 5.8 µs † | 8.7 µs | 14.0 µs † | 18.9 µs | 54.4 µs | 187.7 µs |
| haversin | 0.7 | 4.9 µs | 7.2 µs | 12.3 µs | 22.9 µs | 48.9 µs | 248.3 µs |
| hermite_h | 2, 0.7 | 3.9 µs | 5.1 µs | 5.5 µs | 4.8 µs | 6.4 µs | 8.3 µs |
| hypergeometric_pFq | 0.7, 2.5, 0.3 | 6.7 µs † | 5.8 µs | 8.3 µs † | 5.5 µs | 6.6 µs | 5.9 µs |
| hypot | 0.7, 1.2 | 1.0 µs | 1.2 µs | 1.5 µs | 2.0 µs | 3.4 µs | 10.5 µs |
| imag_part | 0.7 | 93.2 ns | 95.0 ns | 121.2 ns | 99.2 ns | 115.5 ns | 109.2 ns |
| inv | 0.7 | 264.3 ns | 415.0 ns | 504.8 ns | 607.0 ns | 1.7 µs | 5.2 µs |
| lambert_w0 | 0.7 | 73.4 µs | 136.4 µs † | 228.1 µs | 413.1 µs † | 1.163 ms † | 3.429 ms |
| lambert_wm1 | -0.2 | 81.4 µs † | 135.0 µs | 236.9 µs | 404.2 µs | 1.010 ms | 4.051 ms |
| lambert_wn | 1, 0.7 | 35.4 µs | 40.8 µs † | 39.1 µs | 35.9 µs | 42.2 µs | 40.1 µs |
| lauricella_f | 0.5, 0.3, 2.5 | 30.1 µs † | 24.4 µs | 23.9 µs | 31.6 µs † | 23.6 µs | 25.8 µs |
| ldexp | 1.23456789, 3 | 139.7 ns | 163.3 ns | 181.4 ns | 159.5 ns | 149.6 ns | 199.0 ns |
| legendre_chi | 2, 0.7 | 84.1 µs † | 226.7 µs | 451.4 µs | 620.9 µs | 3.121 ms | 14.439 ms |
| lerch_phi | 0.3, 2, 1.4 | 365.0 µs | 338.6 µs | 401.8 µs | 348.2 µs | 331.9 µs | 360.4 µs |
| lg | 2.345678 | 15.1 µs | 19.5 µs | 37.1 µs † | 42.0 µs | 88.8 µs | 307.6 µs |
| lgamma | 2.345 | 60.9 µs † | 108.0 µs | 207.6 µs | 351.8 µs † | 1.209 ms | 6.306 ms † |
| ln | 2.345678 | 6.0 µs | 11.6 µs † | 13.6 µs | 23.4 µs | 49.9 µs | 145.9 µs † |
| log | 2.345678 | 7.5 µs † | 10.4 µs | 16.0 µs | 22.1 µs † | 56.3 µs | 151.0 µs |
| log10 | 2.345678 | 13.5 µs | 21.5 µs | 30.9 µs | 55.7 µs † | 89.9 µs | 333.4 µs |
| logbeta | 2.3, 4.5 | 170.9 µs † | 366.5 µs † | 703.5 µs † | 1.287 ms † | 3.921 ms | 19.787 ms † |
| logbeta_pdf | 0.5, 2.5, 3.5 | 107.8 µs | 274.6 µs | 439.3 µs | 799.1 µs | 2.587 ms | 13.236 ms † |
| lommel_s | 0.5, 0.7, 1.2 | 49.9 µs † | 94.6 µs | 171.6 µs † | 720.0 µs † | 906.9 µs | 4.267 ms † |
| mul | 0.7, 0.7 | 149.1 ns | 248.3 ns | 348.8 ns | 509.4 ns | 1.3 µs | 3.9 µs |
| mul_long | 17, 3 | 906.6 ns | 1.1 µs | 1.2 µs | 900.1 ns | 1.1 µs | 1.1 µs |
| mul_pow10 | 1.23456789, 3 | 427.8 ns | 496.0 ns | 839.1 ns | 791.5 ns | 649.8 ns | 904.6 ns |
| neg | 0.7 | 104.4 ns | 144.9 ns | 111.3 ns | 130.7 ns | 110.9 ns | 139.9 ns |
| normal_cdf | 0.7 | 17.5 µs | 103.6 µs | 73.7 µs | 125.1 µs | 561.3 µs | 2.624 ms † |
| normal_logpdf | 0.7 | 5.9 µs | 11.6 µs | 16.8 µs | 21.8 µs | 57.7 µs | 163.8 µs |
| normal_pdf | 0.7 | 4.9 µs | 7.9 µs | 14.7 µs | 23.2 µs | 71.2 µs | 220.2 µs |
| polygamma | 3, 2.345 | 54.2 µs | 153.1 µs | 314.3 µs | 441.7 µs | 2.246 ms | 12.257 ms |
| polylog | 2, 0.7 | 120.9 µs | 374.0 µs | 692.4 µs | 1.040 ms | 4.452 ms | 25.598 ms |
| polylog1 | 0.7 | 7.2 µs | 12.8 µs † | 17.1 µs | 24.9 µs | 53.3 µs † | 136.0 µs |
| pow | 1.23456789, 3.5 | 12.5 µs † | 18.0 µs | 34.2 µs † | 49.6 µs † | 132.2 µs † | 333.8 µs |
| pow10 | 3 | 331.5 ns | 461.3 ns | 507.0 ns | 739.7 ns | 526.8 ns | 521.8 ns |
| pow_int | 1.23456789, 3 | 462.5 ns | 590.5 ns | 741.7 ns | 904.5 ns | 2.9 µs | 9.3 µs |
| productlog | 0.7 | 66.6 µs | 152.7 µs † | 297.5 µs † | 292.9 µs | 1.073 ms | 3.355 ms |
| qdigamma | 0.5, 2.3 | 30.1 µs | 39.1 µs | 40.8 µs † | 31.8 µs | 32.6 µs | 34.2 µs |
| real_part | 0.7 | 121.7 ns | 101.0 ns | 118.9 ns | 127.3 ns | 101.8 ns | 118.7 ns |
| rect | 0.7 | 1.2 µs | 1.6 µs † | 1.2 µs | 1.2 µs | 1.3 µs | 1.6 µs † |
| root | 1.2345, 3 | 17.0 µs | 27.3 µs | 34.0 µs | 45.2 µs | 120.9 µs | 378.7 µs † |
| round | 0.7 | 3.0 µs | 3.3 µs | 3.6 µs | 3.7 µs | 3.4 µs | 3.7 µs † |
| roundn | 1.23456789, 3 | 6.3 µs | 7.1 µs | 7.8 µs | 7.7 µs | 9.4 µs | 12.2 µs |
| rounds | 1.23456789, 3 | 27.5 µs † | 34.8 µs | 46.4 µs | 66.8 µs † | 135.2 µs | 350.3 µs † |
| sec | 0.7 | 3.6 µs † | 5.9 µs | 10.6 µs | 13.9 µs | 64.6 µs † | 191.9 µs |
| sech | 0.7 | 4.2 µs † | 8.2 µs | 16.4 µs † | 20.1 µs | 80.3 µs † | 243.3 µs |
| sgn | 0.7 | 267.1 ns | 294.1 ns | 274.6 ns | 341.7 ns | 282.1 ns | 292.2 ns |
| sin | 0.567 | 3.9 µs | 7.4 µs | 12.3 µs | 20.2 µs | 54.5 µs | 169.6 µs |
| sinc | 0.7 | 6.1 µs | 11.0 µs | 13.3 µs | 19.3 µs | 59.6 µs | 217.2 µs |
| sincos | 0.7 | 11.5 µs | 15.5 µs | 23.4 µs | 27.1 µs | 66.3 µs | 191.9 µs |
| sinh | 0.7 | 3.7 µs | 8.2 µs | 12.5 µs | 18.9 µs | 62.7 µs | 245.4 µs † |
| sinhcosh | 0.7 | 3.8 µs | 7.2 µs | 13.9 µs | 17.6 µs | 59.6 µs | 195.8 µs |
| sqr | 0.7 | 189.1 ns | 285.2 ns | 410.2 ns | 765.1 ns | 677.9 ns | 1.8 µs |
| sqrt | 1.23456789 | 413.7 ns | 546.1 ns | 771.4 ns | 1.1 µs | 1.6 µs | 4.3 µs † |
| step | 0.7 | 351.9 ns | 353.7 ns | 300.3 ns | 351.5 ns | 299.6 ns | 275.9 ns |
| struve_h | 2, 0.7 | 186.9 µs | 454.5 µs † | 676.8 µs | 1.146 ms | 3.853 ms | 16.412 ms |
| struve_l | 2, 0.7 | 207.1 µs | 484.5 µs | 719.5 µs | 1.060 ms | 3.869 ms † | 14.652 ms |
| sub | 0.7, 0.7 | 117.1 ns | 105.4 ns | 111.9 ns | 120.7 ns | 202.0 ns | 349.6 ns |
| tan | 0.7 | 16.8 µs | 20.0 µs † | 26.1 µs | 29.6 µs | 72.5 µs | 220.5 µs |
| tanh | 0.7 | 4.6 µs † | 7.7 µs | 13.4 µs | 17.2 µs | 51.5 µs | 243.5 µs † |
| tetragamma | 2.345 | 56.2 µs | 177.2 µs | 318.9 µs | 457.9 µs | 2.330 ms | 14.178 ms |
| tri | 0.7 | 1.3 µs | 1.1 µs | 1.1 µs | 1.5 µs † | 1.4 µs | 1.3 µs |
| trigamma | 2.345 | 37.5 µs | 99.6 µs | 232.9 µs | 326.1 µs | 1.505 ms | 8.480 ms |
| vercos | 0.7 | 4.1 µs † | 5.9 µs | 9.8 µs | 16.7 µs | 55.9 µs | 164.4 µs |
| versin | 0.7 | 3.3 µs | 6.6 µs | 10.8 µs | 16.8 µs | 43.0 µs | 197.9 µs |
| zatahp | 2.3, 1.4 | 798.6 µs | 1.708 ms † | 3.223 ms | 4.665 ms | 26.063 ms | 143.500 ms |
| zeta | 2.345 | 1.215 ms | 5.124 ms | 13.201 ms | 1.981 ms † | 5.687 ms | 30.787 ms |
| zetah | 2.3, 1.4 | 763.7 µs | 1.416 ms | 2.940 ms | 4.549 ms | 25.189 ms | 136.666 ms |
| zetap | 2.345 | 338.4 µs † | 529.5 µs | 1.239 ms † | 1.805 ms | 6.944 ms | 55.089 ms † |

### `mpc`

| Operation | Inputs | 256 bits | 512 bits | 768 bits | 1024 bits | 2048 bits | 4096 bits |
|---|---|---:|---:|---:|---:|---:|---:|
| E1 | 2.345678+0.125i | 104.1 µs | 246.7 µs | 438.8 µs | 658.7 µs | 2.377 ms | 8.771 ms |
| Ei | 2.345678+0.125i | 106.7 µs | 235.3 µs | 447.1 µs | 633.0 µs | 2.084 ms | 10.078 ms |
| Li | 2.345+0.125i | 106.4 µs | 232.9 µs | 397.8 µs | 765.6 µs | 2.687 ms | 11.376 ms |
| abs | 0.7+0.125i | 1.2 µs | 1.3 µs | 2.0 µs | 2.0 µs | 3.7 µs | 9.0 µs |
| acos | 0.7+0.125i | 67.2 µs | 116.7 µs | 136.6 µs | 89.6 µs | 253.8 µs | 640.3 µs † |
| acosec | 2.345+0.125i | 277.8 µs | 800.5 µs | 1.591 ms | 2.719 ms | 11.422 ms | 61.287 ms |
| acosech | 0.7+0.125i | 67.9 µs | 89.4 µs | 131.4 µs | 200.5 µs † | 396.5 µs | 1.168 ms |
| acosh | 2.345+0.125i | 50.6 µs | 90.7 µs | 127.2 µs | 87.0 µs | 393.2 µs | 533.0 µs |
| acot | 0.7+0.125i | 60.7 µs | 97.8 µs | 118.9 µs | 157.5 µs | 389.4 µs | 1.243 ms |
| acoth | 2.345+0.125i | 55.0 µs † | 81.4 µs | 90.4 µs | 135.5 µs | 338.6 µs | 937.9 µs |
| add | 0.7+0.125i, 0.7+0.125i | 464.4 ns | 453.9 ns | 556.7 ns | 587.2 ns | 540.4 ns | 834.2 ns |
| add_long | 17+0.125i, 3 | 3.9 µs † | 3.6 µs | 3.7 µs | 3.8 µs † | 3.9 µs | 4.1 µs † |
| appell_f1 | <span style="display:inline-block;width:40ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">0.5+0.125i, 0.7+0.125i, 0.8+0.125i, 2.5+<br/>0.125i, 0.2+0.125i, 0.3+0.125i</span> | 585.3 µs | 1.616 ms | 3.451 ms | 6.809 ms | 26.972 ms | 145.105 ms |
| arccovercos | 0.5+0.125i | 72.2 µs | 96.4 µs | 76.5 µs | 189.9 µs | 242.5 µs | 536.3 µs |
| arccoversin | 0.5+0.125i | 66.9 µs | 56.3 µs | 76.7 µs † | 187.3 µs | 210.7 µs | 585.5 µs |
| archacovercos | 0.5+0.125i | 18.1 µs | 21.2 µs | 28.7 µs | 28.1 µs | 57.4 µs | 160.7 µs |
| archacoversin | 0.5+0.125i | 13.8 µs | 19.1 µs | 26.1 µs | 33.0 µs † | 65.8 µs | 155.8 µs |
| archavercos | 0.5+0.125i | 14.4 µs | 23.7 µs | 27.6 µs | 31.2 µs † | 67.0 µs | 163.2 µs |
| archaversin | 0.5+0.125i | 17.2 µs | 21.1 µs | 25.9 µs | 31.6 µs | 59.2 µs | 154.2 µs |
| arcvercos | 0.5+0.125i | 66.1 µs | 72.3 µs † | 138.7 µs | 99.9 µs | 185.0 µs | 571.4 µs |
| arcversin | 0.5+0.125i | 37.5 µs | 61.5 µs | 138.8 µs | 108.7 µs | 193.7 µs | 512.1 µs |
| arg | 0.7+0.125i | 16.2 µs | 33.0 µs | 42.0 µs | 60.4 µs | 125.3 µs | 428.4 µs |
| asec | 2.345+0.125i | 262.6 µs | 743.2 µs | 1.539 ms | 2.562 ms | 14.080 ms | 60.247 ms |
| asech | 0.7+0.125i | 34.4 µs | 68.0 µs | 74.2 µs | 88.0 µs | 239.1 µs | 527.4 µs |
| asin | 0.7+0.125i | 64.3 µs | 139.2 µs † | 148.1 µs † | 186.6 µs | 459.1 µs | 661.9 µs |
| asinh | 0.7+0.125i | 66.9 µs | 96.3 µs | 122.9 µs | 175.7 µs | 473.0 µs | 1.228 ms |
| atan | 0.7+0.125i | 62.1 µs | 78.5 µs | 120.4 µs | 225.3 µs † | 388.8 µs | 1.170 ms |
| atan2 | 0.5+0.125i, -0.75+0.125i | 14.8 µs | 14.3 µs | 13.2 µs | 13.3 µs | 15.5 µs | 16.1 µs |
| atanh | 0.7+0.125i | 62.0 µs † | 86.3 µs | 120.4 µs | 168.1 µs | 403.2 µs | 920.4 µs |
| bessel_i | 2, 0.7+0.125i | 267.2 µs | 502.5 µs | 906.3 µs | 1.494 ms | 5.485 ms | 24.644 ms |
| bessel_j | 2, 0.7+0.125i | 262.1 µs | 566.0 µs † | 883.4 µs | 1.438 ms † | 4.684 ms | 18.778 ms |
| bessel_k | 2, 0.7+0.125i | 178.3 µs † | 336.9 µs † | 462.9 µs | 924.8 µs | 3.416 ms | 14.387 ms † |
| bessel_y | 2, 0.7+0.125i | 198.2 µs | 516.2 µs † | 911.1 µs | 1.271 ms | 4.647 ms | 23.270 ms |
| beta | 2.3+0.125i, 4.5+0.125i | 44.6 µs | 52.4 µs | 55.5 µs | 47.2 µs | 55.7 µs | 48.3 µs |
| beta_pdf | 0.5+0.125i, 2.5+0.125i, 3.5+0.125i | 63.1 µs | 82.6 µs | 71.0 µs | 63.5 µs | 67.0 µs | 72.4 µs |
| binomial | 5+0.125i, 2+0.125i | 36.8 µs | 34.9 µs | 33.4 µs | 37.8 µs | 40.1 µs | 37.8 µs |
| ceil | 0.7+0.125i | 1.9 µs | 2.3 µs | 2.0 µs | 1.7 µs | 3.1 µs † | 2.3 µs |
| chebyshev_t | 2, 0.7+0.125i | 12.5 µs | 13.7 µs | 16.6 µs | 15.2 µs † | 19.3 µs † | 22.7 µs |
| chebyshev_u | 2, 0.7+0.125i | 18.4 µs | 21.4 µs † | 22.1 µs † | 18.7 µs | 25.1 µs † | 28.9 µs |
| circ | 0.7 | 3.0 µs | 3.8 µs † | 3.4 µs | 3.9 µs | 5.3 µs | 11.1 µs |
| clausen | 3, 0.7+0.125i | 174.9 µs | 480.6 µs | 769.4 µs | 1.470 ms | 7.250 ms † | 32.291 ms |
| clausen2 | 0.7+0.125i | 201.4 µs † | 568.1 µs † | 940.8 µs † | 1.746 ms | 5.606 ms | 32.418 ms |
| conj | 0.7+0.125i | 401.4 ns | 351.2 ns | 322.8 ns | 410.7 ns | 353.2 ns | 382.7 ns |
| cos | 0.7+0.125i | 21.2 µs | 23.5 µs † | 30.3 µs | 39.3 µs | 131.2 µs | 531.8 µs † |
| cosec | 0.7+0.125i | 24.5 µs | 23.0 µs | 31.2 µs | 53.5 µs | 121.5 µs | 396.1 µs |
| cosech | 0.7+0.125i | 14.2 µs | 17.7 µs | 31.6 µs | 110.1 µs | 121.1 µs | 418.7 µs † |
| cosh | 0.7+0.125i | 11.3 µs | 16.5 µs | 27.9 µs | 104.3 µs | 103.7 µs | 351.2 µs |
| cot | 0.7+0.125i | 47.5 µs † | 38.5 µs | 64.4 µs | 91.4 µs | 331.0 µs † | 983.2 µs |
| coth | 0.7+0.125i | 27.1 µs | 40.5 µs | 49.8 µs | 184.3 µs | 209.8 µs | 690.4 µs |
| covercos | 0.7+0.125i | 23.4 µs | 29.7 µs † | 37.6 µs † | 51.1 µs | 146.4 µs | 431.7 µs |
| coversin | 0.7+0.125i | 26.5 µs | 24.1 µs | 29.5 µs | 45.3 µs | 201.9 µs † | 464.3 µs |
| cubrt | 0.7+0.125i | 50.4 µs | 97.6 µs | 160.7 µs † | 180.5 µs | 570.1 µs | 1.580 ms |
| digamma | 2.345+0.125i | 357.8 µs † | 634.0 µs | 1.138 ms | 1.687 ms | 7.332 ms † | 26.975 ms |
| dilog | 0.7+0.125i | 227.3 µs | 565.7 µs | 989.7 µs | 1.902 ms | 7.282 ms | 35.774 ms |
| div | 0.7+0.125i, 0.7+0.125i | 8.8 µs | 14.0 µs | 20.6 µs | 23.4 µs | 57.6 µs | 144.8 µs |
| erf | 0.7+0.125i | 109.3 µs | 291.4 µs | 445.6 µs | 697.4 µs | 2.807 ms | 11.092 ms |
| erfc | 0.7+0.125i | 106.2 µs | 260.5 µs † | 373.0 µs | 700.4 µs | 2.809 ms | 9.354 ms |
| erfcinv | 0.5+0.125i | 1.041 ms | 2.491 ms † | 2.595 ms | 5.860 ms | 14.565 ms | 66.532 ms |
| erfinv | 0.5+0.125i | 961.6 µs | 2.426 ms † | 2.917 ms | 4.153 ms | 18.271 ms † | 58.316 ms |
| exp | 1.23456789+0.125i | 14.3 µs | 29.0 µs | 40.4 µs | 60.5 µs | 188.6 µs | 691.4 µs |
| floor | 0.7+0.125i | 399.2 ns | 361.6 ns | 334.2 ns | 384.4 ns | 311.7 ns | 526.8 ns |
| gamma | 2.345+0.125i | 20.3 µs † | 21.4 µs † | 18.7 µs | 21.0 µs | 18.4 µs | 20.3 µs |
| gammainc_P | 2.3+0.125i, 1.4+0.125i | 84.8 µs | 96.9 µs | 75.7 µs | 102.2 µs | 78.7 µs | 75.2 µs |
| gammainc_Q | 2.3+0.125i, 1.4+0.125i | 77.5 µs | 86.3 µs | 88.5 µs | 95.2 µs † | 85.6 µs | 73.0 µs |
| gammainc_lower | 2.3+0.125i, 1.4+0.125i | 66.3 µs | 70.1 µs | 66.2 µs | 67.6 µs | 70.8 µs | 65.6 µs |
| gammainc_upper | 2.3+0.125i, 1.4+0.125i | 84.9 µs | 82.9 µs | 95.1 µs | 97.4 µs † | 90.1 µs | 85.0 µs |
| gammainv | 9.5+0.125i | 211.8 µs † | 199.4 µs | 163.6 µs | 199.9 µs | 206.8 µs | 166.3 µs |
| hacovercos | 0.7+0.125i | 35.4 µs † | 31.0 µs | 43.3 µs † | 53.1 µs | 129.8 µs | 494.6 µs |
| hacoversin | 0.7+0.125i | 36.0 µs † | 34.0 µs † | 39.6 µs | 45.6 µs | 131.0 µs | 410.3 µs |
| harmonic_poly | 2, 0.7+0.125i | 24.1 µs † | 26.1 µs | 23.7 µs | 25.7 µs | 42.4 µs | 83.5 µs |
| havercos | 0.7+0.125i | 33.9 µs | 79.2 µs † | 39.7 µs | 56.8 µs | 149.6 µs | 554.6 µs † |
| haversin | 0.7+0.125i | 28.6 µs | 33.2 µs † | 37.6 µs | 62.8 µs | 133.1 µs | 494.3 µs |
| hermite_h | 2, 0.7+0.125i | 18.8 µs † | 19.5 µs † | 21.1 µs † | 16.1 µs | 23.3 µs | 27.0 µs |
| hypergeometric_pFq | 0.7+0.125i, 2.5+0.125i, 0.3+0.125i | 45.3 µs | 52.6 µs | 44.7 µs | 47.2 µs | 40.3 µs | 42.6 µs |
| hypot | 0.7+0.125i, 1.2+0.125i | 3.3 µs | 3.0 µs | 2.9 µs | 2.5 µs | 3.2 µs | 2.5 µs |
| imag_part | 0.7+0.125i | 166.3 ns | 182.0 ns | 190.6 ns | 163.2 ns | 188.2 ns | 268.4 ns |
| inv | 0.7+0.125i | 1.8 µs | 1.7 µs | 3.0 µs † | 2.7 µs | 6.6 µs | 14.7 µs |
| lambert_w0 | 0.7+0.125i | 72.7 µs | 89.6 µs | 103.8 µs | 152.5 µs | 393.7 µs | 1.201 ms |
| lambert_wm1 | -0.2+0.125i | 60.3 µs | 75.9 µs | 96.3 µs | 167.4 µs | 401.2 µs | 1.030 ms |
| lambert_wn | 1, 0.7+0.125i | 44.0 µs | 47.9 µs | 43.2 µs | 37.0 µs | 43.7 µs | 39.5 µs |
| lauricella_f | 0.5+0.125i, 0.3+0.125i, 2.5+0.125i | 190.4 µs | 210.0 µs | 189.2 µs | 226.0 µs | 221.1 µs | 232.5 µs |
| ldexp | 1.23456789+0.125i, 3 | 462.9 ns | 460.3 ns | 379.3 ns | 506.0 ns | 657.4 ns | 422.9 ns |
| legendre_chi | 2, 0.7+0.125i | 317.0 µs | 884.3 µs | 1.732 ms | 3.247 ms | 13.524 ms | 74.096 ms |
| lerch_phi | 0.3+0.125i, 2+0.125i, 1.4+0.125i | 779.6 µs | 800.5 µs | 700.4 µs | 725.8 µs | 792.3 µs | 754.2 µs |
| lg | 2.345678+0.125i | 34.0 µs | 58.7 µs | 71.6 µs | 93.4 µs | 248.2 µs | 621.5 µs |
| lgamma | 2.345+0.125i | 17.6 µs | 17.2 µs | 16.4 µs | 16.8 µs | 19.6 µs † | 18.1 µs |
| ln | 2.345678+0.125i | 23.5 µs | 35.1 µs | 43.7 µs | 67.4 µs | 168.0 µs | 585.3 µs |
| log | 2.345678+0.125i | 20.5 µs | 39.7 µs | 44.9 µs | 81.4 µs | 190.6 µs | 625.1 µs |
| log10 | 2.345678+0.125i | 33.0 µs | 58.4 µs | 63.8 µs | 99.5 µs | 223.7 µs | 738.8 µs |
| logbeta | 2.3+0.125i, 4.5+0.125i | 42.7 µs | 46.7 µs | 52.1 µs † | 47.6 µs | 46.1 µs | 50.3 µs |
| logbeta_pdf | 0.5+0.125i, 2.5+0.125i, 3.5+0.125i | 56.3 µs | 68.3 µs | 76.5 µs † | 58.7 µs | 56.9 µs | 62.5 µs |
| lommel_s | 0.5+0.125i, 0.7+0.125i, 1.2+0.125i | 294.1 µs | 627.7 µs | 1.010 ms | 1.773 ms | 6.353 ms | 22.144 ms |
| mul | 0.7+0.125i, 0.7+0.125i | 2.1 µs | 2.6 µs † | 3.0 µs | 3.5 µs | 4.7 µs | 8.2 µs |
| mul_long | 17+0.125i, 3 | 4.0 µs | 4.8 µs | 5.3 µs | 4.4 µs | 4.4 µs | 4.4 µs |
| mul_pow10 | 1.23456789+0.125i, 3 | 4.5 µs | 3.6 µs | 4.0 µs | 3.6 µs | 3.3 µs | 4.5 µs |
| neg | 0.7+0.125i | 359.0 ns | 446.3 ns | 361.4 ns | 422.5 ns | 401.8 ns | 429.7 ns |
| normal_cdf | 0.7+0.125i | 105.3 µs | 218.3 µs | 424.0 µs | 671.2 µs | 2.910 ms | 10.100 ms |
| normal_logpdf | 0.7+0.125i | 9.9 µs | 17.1 µs | 21.2 µs | 23.9 µs | 47.5 µs | 152.7 µs |
| normal_pdf | 0.7+0.125i | 22.3 µs | 41.7 µs | 79.0 µs † | 112.1 µs | 352.1 µs † | 1.119 ms |
| polygamma | 3, 2.345+0.125i | 1.024 ms † | 943.9 µs | 1.653 ms | 2.006 ms | 7.211 ms | 36.548 ms |
| polylog | 2, 0.7+0.125i | 214.3 µs | 561.3 µs | 1.071 ms | 2.079 ms | 8.759 ms | 34.933 ms |
| polylog1 | 0.7+0.125i | 31.7 µs | 46.7 µs | 70.5 µs | 101.5 µs | 177.8 µs | 632.5 µs |
| pow | 1.23456789+0.125i, 3.5+0.125i | 48.3 µs | 82.5 µs | 144.2 µs | 201.6 µs † | 502.1 µs | 1.551 ms |
| pow10 | 3 | 13.4 µs | 22.6 µs | 34.3 µs | 46.8 µs | 92.2 µs | 231.9 µs |
| pow_int | 1.23456789+0.125i, 3 | 9.8 µs | 10.6 µs | 10.5 µs | 11.3 µs | 20.1 µs | 30.0 µs |
| productlog | 0.7+0.125i | 84.1 µs † | 80.8 µs | 132.3 µs † | 167.8 µs | 427.4 µs | 1.331 ms |
| qdigamma | 0.5+0.125i, 2.3+0.125i | 224.6 µs | 220.9 µs | 241.2 µs | 190.7 µs | 198.2 µs | 223.8 µs |
| real_part | 0.7+0.125i | 162.0 ns | 187.9 ns | 187.2 ns | 162.6 ns | 183.5 ns | 246.9 ns |
| rect | 0.7 | 2.9 µs | 2.9 µs | 3.2 µs | 3.8 µs | 4.9 µs | 10.7 µs |
| root | 1.2345+0.125i, 3 | 68.0 µs † | 76.1 µs | 253.4 µs | 371.4 µs † | 436.7 µs | 1.524 ms |
| round | 0.7+0.125i | 16.3 µs † | 11.7 µs | 10.7 µs | 13.2 µs | 13.9 µs | 12.5 µs |
| roundn | 1.23456789+0.125i, 3 | 26.3 µs † | 20.7 µs | 21.4 µs | 21.9 µs | 27.0 µs | 42.8 µs |
| rounds | 1.23456789+0.125i, 3 | 56.2 µs | 73.2 µs | 101.5 µs | 117.5 µs | 304.7 µs † | 671.8 µs |
| sec | 0.7+0.125i | 23.7 µs | 23.9 µs | 35.5 µs | 46.9 µs | 151.0 µs | 438.3 µs |
| sech | 0.7+0.125i | 13.9 µs | 22.8 µs | 31.8 µs | 113.4 µs | 118.1 µs | 399.2 µs |
| sgn | 0.7 | 1.0 µs | 1.2 µs | 1.4 µs † | 1.1 µs | 1.2 µs | 1.0 µs |
| sin | 0.567+0.125i | 19.7 µs | 25.8 µs † | 29.8 µs | 43.1 µs | 121.3 µs | 458.6 µs |
| sinc | 0.7+0.125i | 31.2 µs † | 34.8 µs | 45.0 µs | 64.2 µs | 155.0 µs | 567.3 µs |
| sinh | 0.7+0.125i | 12.1 µs † | 19.9 µs | 27.5 µs | 120.1 µs | 108.7 µs † | 445.4 µs |
| sqr | 0.7+0.125i | 1.9 µs | 2.7 µs | 2.7 µs | 4.0 µs | 5.2 µs | 9.3 µs |
| sqrt | 1.23456789+0.125i | 3.0 µs | 3.5 µs | 4.2 µs | 5.4 µs | 8.1 µs | 19.7 µs |
| step | 0.7 | 1.1 µs | 976.2 ns | 973.5 ns | 925.1 ns | 1.0 µs | 1.1 µs |
| struve_h | 2, 0.7+0.125i | 265.5 µs | 684.9 µs | 1.039 ms | 1.856 ms | 5.964 ms | 25.378 ms |
| struve_l | 2, 0.7+0.125i | 320.3 µs | 695.0 µs | 1.306 ms † | 1.918 ms † | 6.529 ms | 24.869 ms |
| sub | 0.7+0.125i, 0.7+0.125i | 537.9 ns | 546.0 ns | 453.6 ns | 565.9 ns | 649.6 ns | 650.0 ns |
| tan | 0.7+0.125i | 15.8 µs | 28.7 µs | 39.9 µs | 52.4 µs | 141.8 µs | 512.7 µs |
| tanh | 0.7+0.125i | 29.3 µs | 20.6 µs | 75.4 µs | 45.0 µs | 111.4 µs | 386.5 µs |
| tetragamma | 2.345+0.125i | 399.8 µs | 941.5 µs | 1.613 ms | 3.193 ms † | 10.999 ms † | 40.998 ms |
| tri | 0.7 | 3.2 µs | 3.5 µs | 3.7 µs | 3.6 µs | 5.4 µs | 9.0 µs |
| trigamma | 2.345+0.125i | 389.5 µs † | 652.4 µs | 1.188 ms | 1.866 ms | 6.197 ms | 33.256 ms |
| vercos | 0.7+0.125i | 25.7 µs | 24.2 µs | 30.1 µs | 47.2 µs | 136.5 µs | 433.9 µs |
| versin | 0.7+0.125i | 21.0 µs | 22.1 µs | 31.2 µs | 48.0 µs | 142.3 µs | 469.1 µs |
| zatahp | 2.3+0.125i, 1.4+0.125i | 2.703 ms | 5.927 ms | 11.463 ms | 18.037 ms | 115.908 ms | 633.655 ms |
| zeta | 2.345+0.125i | 889.6 µs † | 1.647 ms | 3.118 ms | 5.649 ms | 28.006 ms | 123.420 ms |
| zetah | 2.3+0.125i, 1.4+0.125i | 2.891 ms | 5.315 ms | 9.635 ms | 19.537 ms | 104.191 ms | 557.533 ms |
| zetap | 2.345+0.125i | 822.6 µs | 1.925 ms | 3.664 ms | 5.910 ms | 32.710 ms | 162.008 ms |

### number / unrestricted integer

| Operation | Inputs | Exact |
|---|---|---:|
| abs | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">-9712853409152031141207811585727780469484705121206232579607434236969009533618825<br/>97201918971499157056640605432269691427807258223374967278846596194834524005116336<br/>17514882293623272276764857474830401626415841035971458657442133370777607803464071<br/>101158385124186229995073045427448918227959893082282919863982307657776</span> | 122.4 ns |
| add | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 109.1 ns |
| bit_and | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 315.9 ns |
| bit_length | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776</span> | 187.9 ns |
| bit_not | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776</span> | 322.6 ns |
| bit_or | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 294.8 ns |
| bit_xor | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 278.7 ns |
| clear_bit | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>701</span> | 358.5 ns |
| div | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>340282366920938463463374607431768211454</span> | 1.7 µs |
| divmod | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 375.3 ns |
| factorial | 5000 | 127.7 µs |
| factors | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">54772119019154636339867862041163448790974484131459861520687327809209914339836012<br/>52275954799541486775711126005967382600118182797233708133775335788335658037065081<br/>31569062299976033041302189833215742992653063887339218309467131548996199881113600<br/>00000000000000000000000000000000000000000000000000000000000000000000000000000000<br/>0000000000000000000000000000000000000000000000</span> | 103.6 µs |
| fibonacci | 10000 | 6.2 µs |
| gcd | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 3.8 µs |
| gcdext | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 4.3 µs |
| is_prime | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">68647976601306097149819007990813932172694353001433054093944634591855431833976560<br/>52122559640661454554977296311391480858037121987999716643812574028291115057151</span> | 559.5 µs |
| isqrt | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776</span> | 439.5 ns |
| lcm | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 3.3 µs |
| mod | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 340.3 ns |
| modinv | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>68647976601306097149819007990813932172694353001433054093944634591855431833976560<br/>52122559640661454554977296311391480858037121987999716643812574028291115057151</span> | 4.8 µs |
| mul | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 288.1 ns |
| neg | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776</span> | 82.1 ns |
| next_prime | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 4.507 ms |
| partition | 5000 | 7.163 ms |
| pow | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>5</span> | 4.5 µs |
| pow_int | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>5</span> | 3.5 µs |
| powmod | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662, <br/>68647976601306097149819007990813932172694353001433054093944634591855431833976560<br/>52122559640661454554977296311391480858037121987999716643812574028291115057151</span> | 120.6 µs |
| prev_prime | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 6.950 ms |
| prove_prime | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">68647976601306097149819007990813932172694353001433054093944634591855431833976560<br/>52122559640661454554977296311391480858037121987999716643812574028291115057151</span> | 3.704 ms |
| set_bit | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>700</span> | 458.4 ns |
| shl | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>257</span> | 625.6 ns |
| shr | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>257</span> | 400.3 ns |
| sqr | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776</span> | 299.9 ns |
| sub | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 128.4 ns |
| test_bit | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>701</span> | 509.5 ns |

### number / unrestricted fraction

| Operation | Inputs | Exact |
|---|---|---:|
| abs | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">-9712853409152031141207811585727780469484705121206232579607434236969009533618825<br/>97201918971499157056640605432269691427807258223374967278846596194834524005116336<br/>17514882293623272276764857474830401626415841035971458657442133370777607803464071<br/>101158385124186229995073045427448918227959893082282919863982307657776/6864797660<br/>13060971498190079908139321726943530014330540939446345918554318339765605212255964<br/>0661454554977296311391480858037121987999716643812574028291115057151</span> | 242.0 ns |
| add | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776/68647976601<br/>30609714981900799081393217269435300143305409394463459185543183397656052122559640<br/>661454554977296311391480858037121987999716643812574028291115057151, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662/9712<br/>85340915203114120781158572778046948470512120623257960743423696900953361882597201<br/>91897149915705664060543226969142780725822337496727884659619483452400511633617514<br/>88229362327227676485747483040162641584103597145865744213337077760780346407110115<br/>8385124186229995073045427448918227959893082282919863982307657776</span> | 4.8 µs |
| div | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776/68647976601<br/>30609714981900799081393217269435300143305409394463459185543183397656052122559640<br/>661454554977296311391480858037121987999716643812574028291115057151, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662/9712<br/>85340915203114120781158572778046948470512120623257960743423696900953361882597201<br/>91897149915705664060543226969142780725822337496727884659619483452400511633617514<br/>88229362327227676485747483040162641584103597145865744213337077760780346407110115<br/>8385124186229995073045427448918227959893082282919863982307657776</span> | 7.3 µs |
| inv | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776/68647976601<br/>30609714981900799081393217269435300143305409394463459185543183397656052122559640<br/>661454554977296311391480858037121987999716643812574028291115057151</span> | 166.3 ns |
| mul | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776/68647976601<br/>30609714981900799081393217269435300143305409394463459185543183397656052122559640<br/>661454554977296311391480858037121987999716643812574028291115057151, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662/9712<br/>85340915203114120781158572778046948470512120623257960743423696900953361882597201<br/>91897149915705664060543226969142780725822337496727884659619483452400511633617514<br/>88229362327227676485747483040162641584103597145865744213337077760780346407110115<br/>8385124186229995073045427448918227959893082282919863982307657776</span> | 3.2 µs |
| neg | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776/68647976601<br/>30609714981900799081393217269435300143305409394463459185543183397656052122559640<br/>661454554977296311391480858037121987999716643812574028291115057151</span> | 189.6 ns |
| pow | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776/68647976601<br/>30609714981900799081393217269435300143305409394463459185543183397656052122559640<br/>661454554977296311391480858037121987999716643812574028291115057151, <br/>5</span> | 34.2 µs |
| pow_int | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776/68647976601<br/>30609714981900799081393217269435300143305409394463459185543183397656052122559640<br/>661454554977296311391480858037121987999716643812574028291115057151, <br/>5</span> | 37.0 µs |
| sqr | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776/68647976601<br/>30609714981900799081393217269435300143305409394463459185543183397656052122559640<br/>661454554977296311391480858037121987999716643812574028291115057151</span> | 539.0 ns |
| sub | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776/68647976601<br/>30609714981900799081393217269435300143305409394463459185543183397656052122559640<br/>661454554977296311391480858037121987999716643812574028291115057151, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662/9712<br/>85340915203114120781158572778046948470512120623257960743423696900953361882597201<br/>91897149915705664060543226969142780725822337496727884659619483452400511633617514<br/>88229362327227676485747483040162641584103597145865744213337077760780346407110115<br/>8385124186229995073045427448918227959893082282919863982307657776</span> | 5.0 µs |

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
