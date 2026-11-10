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
shared reference results, labelled with the machine and CPU that produced them.

`make bench-db ARGS='run'` measures the full catalogue sequentially. Profiles include
double-backed `number_t` arguments (53 bits), direct `qfloat_t` and `qcomplex_t`
calls (approximately 106 bits), and real and complex `number_t` calculations at
256, 512, 768, 1024, 2048 and 4096 bits. The catalogue covers arithmetic, elementary
and special functions, distributions, polynomials and integer mathematics.
Names without a corresponding direct backend API are recorded as unsupported;
complex profiles skip operations restricted to real integers. Integer-only APIs
run once with exact operands in the `exact-integer` profile; an arbitrary floating
precision would be meaningless for those algorithms.

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

Timing uses checked `clock_gettime(CLOCK_MONOTONIC)` calls and records
`clock_getres()`. Integer nanosecond differences avoid subtracting large floating
timestamps. A 20 ms warm-up precedes adaptive batch calibration, preventing a
slow startup call from prematurely fixing a tiny batch size. Warm-up and batches amortise timer and dispatch overhead;
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
Benchmark run **11**, 2026-11-10T10:25:23.939Z (UTC).

Machine: ophelia; Intel(R) N95.

System: Linux 7.0.0-27-generic x86_64.

Compiler: 15.2.0; build flags: gcc -Wall -Wextra -Werror -O2 -fPIC -D_GNU_SOURCE -pthread -DHAVE_UNISTRING -I/usr/include/sqlcipher -DSQLITE_HAS_CODEC   -I/usr/include/x86_64-linux-gnu -isystem /usr/include/mit-krb5 -isystem /usr/include/mit-krb5 -I/usr/include/p11-kit-1.

Median per call from 7 timed batches.

Timing method: CLOCK_MONOTONIC; warmup=20ms; target_batch=250us; max_batch=512.

Argument parsing, output validation, disposal and database writes are outside timing.
Inputs show the actual operands, including +0.125i for permitted complex arguments; integer orders remain real.
`number-double` includes generic number dispatch; the other fixed profiles call qfloat/qcomplex directly.
Requested multiprecision bits do not guarantee that every algorithm achieves that accuracy.
Operations and precision columns with only unsupported results are omitted; — denotes an unsupported cell.
Failures and timeouts remain visible. † marks a retained slowdown outlier.

### `number-double`

| Operation | Inputs | 53 bits |
|---|---|---:|
| E1 | 2.345678 | 7.8 µs |
| Ei | 2.345678 | 7.8 µs |
| Li | 2.345 | 7.1 µs |
| abs | 0.7 | 8.7 ns |
| acos | 0.7 | 20.4 ns |
| acosec | 2.345 | 15.8 ns |
| acosech | 0.7 | 57.1 ns |
| acosh | 2.345 | 26.4 ns |
| acot | 0.7 | 40.9 ns |
| acoth | 2.345 | 24.1 ns |
| add | 0.7, 0.7 | 17.6 ns |
| add_long | 17, 3 | 510.5 ns |
| appell_f1 | 0.5, 0.7, 0.8, 2.5, 0.2, 0.3 | 44.9 µs |
| arccovercos | 0.5 | 92.4 ns |
| arccoversin | 0.5 | 90.1 ns |
| archacovercos | 0.5 | 204.5 ns |
| archacoversin | 0.5 | 198.4 ns |
| archavercos | 0.5 | 185.2 ns |
| archaversin | 0.5 | 177.6 ns |
| arcvercos | 0.5 | 93.6 ns |
| arcversin | 0.5 | 119.6 ns |
| arg | 0.7 | 61.7 ns |
| asec | 2.345 | 19.4 ns |
| asech | 0.7 | 35.8 ns |
| asin | 0.7 | 31.7 ns |
| asinh | 0.7 | 54.8 ns |
| atan | 0.7 | 31.1 ns |
| atan2 | 0.5, -0.75 | 35.0 ns |
| atanh | 0.7 | 22.8 ns |
| bessel_i | 2, 0.7 | 34.5 µs |
| bessel_j | 2, 0.7 | 5.6 µs |
| bessel_k | 2, 0.7 | 142.3 µs |
| bessel_y | 2, 0.7 | 16.6 µs |
| beta | 2.3, 4.5 | 116.0 ns |
| beta_pdf | 0.5, 2.5, 3.5 | 142.5 ns |
| binomial | 5, 2 | 145.4 ns |
| ceil | 0.7 | 143.6 ns |
| chebyshev_t | 2, 0.7 | 745.5 ns |
| chebyshev_u | 2, 0.7 | 1.2 µs |
| circ | 0.7 | 277.0 ns |
| clausen | 3, 0.7 | 419.0 µs † |
| clausen2 | 0.7 | 5.9 µs |
| conj | 0.7 | 9.2 ns |
| cos | 0.7 | 36.3 ns |
| cosec | 0.7 | 35.7 ns |
| cosech | 0.7 | 59.3 ns |
| cosh | 0.7 | 40.9 ns |
| cot | 0.7 | 42.8 ns |
| coth | 0.7 | 67.6 ns |
| covercos | 0.7 | 128.9 ns |
| coversin | 0.7 | 124.6 ns |
| cubrt | 0.7 | 6.0 µs |
| digamma | 2.345 | 3.5 µs † |
| dilog | 0.7 | 16.0 µs |
| div | 0.7, 0.7 | 12.5 ns |
| erf | 0.7 | 74.3 ns |
| erfc | 0.7 | 73.5 ns |
| erfcinv | 0.5 | 224.1 µs |
| erfinv | 0.5 | 188.8 µs |
| exp | 1.23456789 | 40.5 ns |
| floor | 0.7 | 9.2 ns |
| gamma | 2.345 | 87.7 ns |
| gammainc_P | 2.3, 1.4 | 11.5 µs |
| gammainc_Q | 2.3, 1.4 | 19.7 µs † |
| gammainc_lower | 2.3, 1.4 | 12.3 µs |
| gammainc_upper | 2.3, 1.4 | 15.6 µs |
| gammainv | 9.5 | 110.5 µs |
| hacovercos | 0.7 | 193.8 ns |
| hacoversin | 0.7 | 196.1 ns |
| harmonic_poly | 2, 0.7 | 1.6 µs |
| havercos | 0.7 | 239.1 ns |
| haversin | 0.7 | 190.5 ns |
| hermite_h | 2, 0.7 | 1.3 µs |
| hypergeometric_pFq | 0.7, 2.5, 0.3 | 4.4 µs |
| hypot | 0.7, 1.2 | 39.9 ns |
| imag_part | 0.7 | 18.9 ns |
| inv | 0.7 | 6.9 ns |
| lambert_w0 | 0.7 | 13.5 µs |
| lambert_wm1 | -0.2 | 13.1 µs |
| lambert_wn | 1, 0.7 | 23.4 µs |
| lauricella_f | 0.5, 0.3, 2.5 | 16.2 µs |
| ldexp | 1.23456789, 3 | 33.7 ns |
| legendre_chi | 2, 0.7 | 32.6 µs |
| lerch_phi | 0.3, 2, 1.4 | 242.0 µs |
| lg | 2.345678 | 20.1 ns |
| lgamma | 2.345 | 54.4 ns |
| ln | 2.345678 | 17.5 ns |
| log | 2.345678 | 12.1 ns |
| log10 | 2.345678 | 18.2 ns |
| logbeta | 2.3, 4.5 | 103.8 ns |
| logbeta_pdf | 0.5, 2.5, 3.5 | 132.3 ns |
| lommel_s | 0.5, 0.7, 1.2 | 7.2 µs |
| mul | 0.7, 0.7 | 32.4 ns |
| mul_long | 17, 3 | 381.3 ns |
| mul_pow10 | 1.23456789, 3 | 52.5 ns |
| neg | 0.7 | 7.2 ns |
| normal_cdf | 0.7 | 59.2 ns |
| normal_logpdf | 0.7 | 13.0 ns |
| normal_pdf | 0.7 | 19.8 ns |
| polygamma | 3, 2.345 | 20.6 µs |
| polylog | 2, 0.7 | 12.8 µs |
| polylog1 | 0.7 | 94.4 ns |
| pow | 1.23456789, 3.5 | 60.0 ns |
| pow10 | 3 | 212.1 ns |
| pow_int | 1.23456789, 3 | 67.0 ns |
| productlog | 0.7 | 13.7 µs |
| qdigamma | 0.5, 2.3 | 21.8 µs |
| real_part | 0.7 | 25.2 ns |
| rect | 0.7 | 216.4 ns |
| root | 1.2345, 3 | 6.2 µs |
| round | 0.7 | 315.6 ns |
| roundn | 1.23456789, 3 | 5.3 µs † |
| rounds | 1.23456789, 3 | 6.6 µs |
| sec | 0.7 | 32.6 ns |
| sech | 0.7 | 29.4 ns |
| sgn | 0.7 | 99.1 ns |
| sin | 0.567 | 28.4 ns |
| sinc | 0.7 | 3.5 µs |
| sincos | 0.7 | 480.8 ns |
| sinh | 0.7 | 47.0 ns |
| sinhcosh | 0.7 | 92.1 ns |
| sqr | 0.7 | 7.2 ns |
| sqrt | 1.23456789 | 9.9 ns |
| step | 0.7 | 113.1 ns |
| struve_h | 2, 0.7 | 54.5 µs |
| struve_l | 2, 0.7 | 54.6 µs |
| sub | 0.7, 0.7 | 10.0 ns |
| tan | 0.7 | 4.9 µs |
| tanh | 0.7 | 52.6 ns |
| tetragamma | 2.345 | 3.5 µs |
| tri | 0.7 | 203.5 ns |
| trigamma | 2.345 | 2.3 µs |
| vercos | 0.7 | 100.9 ns |
| versin | 0.7 | 102.1 ns |
| zatahp | 2.3, 1.4 | 321.7 µs |
| zeta | 2.345 | 326.9 µs |
| zetah | 2.3, 1.4 | 337.6 µs |
| zetap | 2.345 | 261.7 µs |

### `qfloat`

| Operation | Inputs | 106 bits |
|---|---|---:|
| E1 | 2.345678 | 6.7 µs |
| Ei | 2.345678 | 8.5 µs |
| Li | 2.345 | 8.2 µs |
| abs | 0.7 | 2.1 ns |
| acos | 0.7 | 1.9 µs |
| acosec | 2.345 | 1.5 µs |
| acosech | 0.7 | 3.0 µs |
| acosh | 2.345 | 3.2 µs |
| acot | 0.7 | 1.2 µs |
| acoth | 2.345 | 2.7 µs |
| add | 0.7, 0.7 | 5.7 ns |
| appell_f1 | 0.5, 0.7, 0.8, 2.5, 0.2, 0.3 | 72.4 µs |
| arccovercos | 0.5 | 1.5 µs |
| arccoversin | 0.5 | 1.5 µs |
| archacovercos | 0.5 | 44.9 ns |
| archacoversin | 0.5 | 29.5 ns |
| archavercos | 0.5 | 46.6 ns |
| archaversin | 0.5 | 46.9 ns |
| arcvercos | 0.5 | 1.5 µs |
| arcversin | 0.5 | 1.5 µs |
| asec | 2.345 | 1.7 µs |
| asech | 0.7 | 3.3 µs |
| asin | 0.7 | 1.5 µs |
| asinh | 0.7 | 2.9 µs |
| atan | 0.7 | 1.1 µs |
| atan2 | 0.5, -0.75 | 1.5 µs |
| atanh | 0.7 | 3.5 µs |
| bessel_i | 2, 0.7 | 46.2 µs |
| bessel_j | 2, 0.7 | 5.5 µs |
| bessel_k | 2, 0.7 | 178.4 µs |
| bessel_y | 2, 0.7 | 21.6 µs |
| beta | 2.3, 4.5 | 4.3 µs |
| beta_pdf | 0.5, 2.5, 3.5 | 27.0 µs |
| binomial | 5, 2 | 26.1 µs |
| chebyshev_t | 2, 0.7 | 33.1 ns |
| chebyshev_u | 2, 0.7 | 33.6 ns |
| circ | 0.7 | 12.1 ns |
| clausen | 3, 0.7 | 266.6 µs |
| clausen2 | 0.7 | 4.6 µs |
| cos | 0.7 | 747.3 ns |
| cosec | 0.7 | 793.0 ns |
| cosech | 0.7 | 2.9 µs |
| cosh | 0.7 | 2.9 µs |
| cot | 0.7 | 1.6 µs |
| coth | 0.7 | 5.8 µs |
| covercos | 0.7 | 766.3 ns |
| coversin | 0.7 | 760.2 ns |
| cubrt | 0.7 | 4.0 µs |
| digamma | 2.345 | 2.7 µs |
| dilog | 0.7 | 12.3 µs |
| div | 0.7, 0.7 | 27.7 ns |
| erf | 0.7 | 3.3 µs |
| erfc | 0.7 | 7.0 µs † |
| erfcinv | 0.5 | 238.9 µs |
| erfinv | 0.5 | 187.7 µs |
| exp | 1.23456789 | 1.4 µs |
| floor | 0.7 | 20.3 ns |
| gamma | 2.345 | 1.4 µs |
| gammainc_P | 2.3, 1.4 | 13.8 µs |
| gammainc_Q | 2.3, 1.4 | 10.9 µs |
| gammainc_lower | 2.3, 1.4 | 12.4 µs |
| gammainc_upper | 2.3, 1.4 | 12.3 µs |
| gammainv | 9.5 | 86.7 µs |
| hacovercos | 0.7 | 777.8 ns |
| hacoversin | 0.7 | 779.7 ns |
| harmonic_poly | 2, 0.7 | 96.2 ns |
| havercos | 0.7 | 781.0 ns |
| haversin | 0.7 | 783.0 ns |
| hermite_h | 2, 0.7 | 42.7 ns |
| hypergeometric_pFq | 0.7, 2.5, 0.3 | 3.4 µs |
| hypot | 0.7, 1.2 | 394.5 ns |
| lambert_w0 | 0.7 | 12.3 µs |
| lambert_wm1 | -0.2 | 15.0 µs |
| lauricella_f | 0.5, 0.3, 2.5 | 34.6 µs † |
| ldexp | 1.23456789, 3 | 8.4 ns |
| legendre_chi | 2, 0.7 | 25.7 µs |
| lerch_phi | 0.3, 2, 1.4 | 305.7 µs |
| lg | 2.345678 | 3.3 µs |
| lgamma | 2.345 | 4.7 µs |
| ln | 2.345678 | 2.5 µs |
| log | 2.345678 | 2.6 µs |
| log10 | 2.345678 | 2.6 µs |
| logbeta | 2.3, 4.5 | 23.8 µs |
| logbeta_pdf | 0.5, 2.5, 3.5 | 25.6 µs |
| lommel_s | 0.5, 0.7, 1.2 | 6.1 µs |
| mul | 0.7, 0.7 | 6.3 ns |
| mul_pow10 | 1.23456789, 3 | 36.1 ns |
| neg | 0.7 | 1.7 ns |
| normal_cdf | 0.7 | 3.1 µs |
| normal_logpdf | 0.7 | 24.6 ns |
| normal_pdf | 0.7 | 1.5 µs |
| polygamma | 3, 2.345 | 4.2 µs |
| polylog | 2, 0.7 | 14.2 µs |
| polylog1 | 0.7 | 3.2 µs |
| pow | 1.23456789, 3.5 | 5.0 µs |
| pow10 | 3 | 23.0 ns |
| pow_int | 1.23456789, 3 | 46.3 ns |
| productlog | 0.7 | 14.4 µs |
| qdigamma | 0.5, 2.3 | 27.4 µs |
| rect | 0.7 | 15.3 ns |
| root | 1.2345, 3 | 4.6 µs |
| round | 0.7 | 31.0 ns |
| roundn | 1.23456789, 3 | 4.5 µs |
| rounds | 1.23456789, 3 | 13.3 µs |
| sec | 0.7 | 1.3 µs † |
| sech | 0.7 | 3.7 µs |
| sgn | 0.7 | 8.0 ns |
| sin | 0.567 | 746.2 ns |
| sinc | 0.7 | 817.2 ns |
| sinh | 0.7 | 3.0 µs |
| sqr | 0.7 | 4.1 ns |
| sqrt | 1.23456789 | 300.0 ns |
| step | 0.7 | 7.1 ns |
| struve_h | 2, 0.7 | 70.6 µs |
| struve_l | 2, 0.7 | 82.3 µs |
| sub | 0.7, 0.7 | 7.5 ns |
| tan | 0.7 | 1.6 µs |
| tanh | 0.7 | 1.5 µs |
| tetragamma | 2.345 | 2.7 µs |
| tri | 0.7 | 10.7 ns |
| trigamma | 2.345 | 2.8 µs |
| vercos | 0.7 | 1.1 µs |
| versin | 0.7 | 960.1 ns |
| zatahp | 2.3, 1.4 | 271.7 µs |
| zeta | 2.345 | 326.6 µs |
| zetah | 2.3, 1.4 | 267.1 µs |
| zetap | 2.345 | 265.6 µs |

### `qcomplex`

| Operation | Inputs | 106 bits |
|---|---|---:|
| E1 | 2.345678+0.125i | 53.4 µs |
| Ei | 2.345678+0.125i | 42.2 µs |
| Li | 2.345+0.125i | 36.7 µs |
| abs | 0.7+0.125i | 502.2 ns |
| acos | 0.7+0.125i | 7.6 µs |
| acosec | 2.345+0.125i | 7.7 µs |
| acosech | 0.7+0.125i | 9.6 µs |
| acosh | 2.345+0.125i | 13.6 µs |
| acot | 0.7+0.125i | 8.4 µs |
| acoth | 2.345+0.125i | 4.4 µs |
| add | 0.7+0.125i, 0.7+0.125i | 12.7 ns |
| appell_f1 | 0.5+0.125i, 0.7+0.125i, 0.8+0.125i, 2.5+0.125i, 0.2+0.125i, 0.3+0.125i | 493.3 µs |
| arccovercos | 0.5+0.125i | 7.6 µs |
| arccoversin | 0.5+0.125i | 7.6 µs |
| archacovercos | 0.5+0.125i | 4.6 µs |
| archacoversin | 0.5+0.125i | 4.6 µs |
| archavercos | 0.5+0.125i | 4.6 µs |
| archaversin | 0.5+0.125i | 4.6 µs |
| arcvercos | 0.5+0.125i | 7.7 µs |
| arcversin | 0.5+0.125i | 7.6 µs |
| arg | 0.7+0.125i | 1.0 µs |
| asec | 2.345+0.125i | 7.8 µs |
| asech | 0.7+0.125i | 11.1 µs |
| asin | 0.7+0.125i | 7.6 µs |
| asinh | 0.7+0.125i | 7.5 µs |
| atan | 0.7+0.125i | 8.3 µs |
| atan2 | 0.5+0.125i, -0.75+0.125i | 8.4 µs |
| atanh | 0.7+0.125i | 4.3 µs |
| bessel_i | 2, 0.7+0.125i | 86.6 µs |
| bessel_j | 2, 0.7+0.125i | 87.5 µs |
| bessel_k | 2, 0.7+0.125i | 999.4 µs |
| bessel_y | 2, 0.7+0.125i | 91.4 µs |
| beta | 2.3+0.125i, 4.5+0.125i | 32.3 µs |
| beta_pdf | 0.5+0.125i, 2.5+0.125i, 3.5+0.125i | 46.4 µs |
| binomial | 5+0.125i, 2+0.125i | 23.6 µs |
| chebyshev_t | 2, 0.7+0.125i | 133.0 ns |
| chebyshev_u | 2, 0.7+0.125i | 132.0 ns |
| circ | 0.7 | 15.9 ns |
| clausen | 3, 0.7+0.125i | 289.5 µs |
| clausen2 | 0.7+0.125i | 21.6 µs |
| conj | 0.7+0.125i | 4.1 ns |
| cos | 0.7+0.125i | 7.3 µs |
| cosec | 0.7+0.125i | 7.7 µs |
| cosech | 0.7+0.125i | 7.5 µs |
| cosh | 0.7+0.125i | 9.2 µs |
| cot | 0.7+0.125i | 18.6 µs |
| coth | 0.7+0.125i | 18.6 µs |
| covercos | 0.7+0.125i | 9.2 µs |
| coversin | 0.7+0.125i | 9.2 µs |
| cubrt | 0.7+0.125i | 7.3 µs |
| digamma | 2.345+0.125i | 10.2 µs |
| dilog | 0.7+0.125i | 99.2 µs |
| div | 0.7+0.125i, 0.7+0.125i | 138.7 ns |
| erf | 0.7+0.125i | 68.5 µs † |
| erfc | 0.7+0.125i | 69.7 µs † |
| erfcinv | 0.5+0.125i | 425.0 µs |
| erfinv | 0.5+0.125i | 536.6 µs † |
| exp | 1.23456789+0.125i | 3.0 µs |
| floor | 0.7+0.125i | 30.7 ns |
| gamma | 2.345+0.125i | 11.6 µs |
| gammainc_P | 2.3+0.125i, 1.4+0.125i | 54.1 µs |
| gammainc_Q | 2.3+0.125i, 1.4+0.125i | 54.3 µs |
| gammainc_lower | 2.3+0.125i, 1.4+0.125i | 42.6 µs |
| gammainc_upper | 2.3+0.125i, 1.4+0.125i | 54.0 µs |
| gammainv | 9.5+0.125i | 120.1 µs |
| hacovercos | 0.7+0.125i | 7.3 µs |
| hacoversin | 0.7+0.125i | 7.3 µs |
| harmonic_poly | 2, 0.7+0.125i | 373.7 ns |
| havercos | 0.7+0.125i | 7.3 µs |
| haversin | 0.7+0.125i | 8.2 µs |
| hermite_h | 2, 0.7+0.125i | 154.6 ns |
| hypergeometric_pFq | 0.7+0.125i, 2.5+0.125i, 0.3+0.125i | 28.0 µs |
| hypot | 0.7+0.125i, 1.2+0.125i | 1.2 µs |
| imag_part | 0.7+0.125i | 2.1 ns |
| inv | 0.7+0.125i | 141.2 ns |
| lambert_w0 | 0.7+0.125i | 28.1 µs |
| lambert_wm1 | -0.2+0.125i | 25.5 µs |
| lambert_wn | 1, 0.7+0.125i | 30.6 µs |
| lauricella_f | 0.5+0.125i, 0.3+0.125i, 2.5+0.125i | 172.1 µs |
| ldexp | 1.23456789+0.125i, 3 | 21.8 ns |
| legendre_chi | 2, 0.7+0.125i | 373.2 µs |
| lerch_phi | 0.3+0.125i, 2+0.125i, 1.4+0.125i | 639.9 µs |
| lg | 2.345678+0.125i | 5.1 µs |
| lgamma | 2.345+0.125i | 12.2 µs |
| ln | 2.345678+0.125i | 5.1 µs |
| log | 2.345678+0.125i | 5.1 µs |
| log10 | 2.345678+0.125i | 4.2 µs |
| logbeta | 2.3+0.125i, 4.5+0.125i | 36.8 µs |
| logbeta_pdf | 0.5+0.125i, 2.5+0.125i, 3.5+0.125i | 37.3 µs |
| lommel_s | 0.5+0.125i, 0.7+0.125i, 1.2+0.125i | 144.4 µs |
| mul | 0.7+0.125i, 0.7+0.125i | 42.5 ns |
| neg | 0.7+0.125i | 4.5 ns |
| normal_cdf | 0.7+0.125i | 64.9 µs † |
| normal_logpdf | 0.7+0.125i | 120.3 ns |
| normal_pdf | 0.7+0.125i | 3.1 µs |
| polygamma | 3, 2.345+0.125i | 11.5 µs |
| polylog | 2, 0.7+0.125i | 78.9 µs |
| polylog1 | 0.7+0.125i | 4.1 µs |
| pow | 1.23456789+0.125i, 3.5+0.125i | 7.1 µs |
| productlog | 0.7+0.125i | 28.0 µs |
| qdigamma | 0.5+0.125i, 2.3+0.125i | 141.4 µs |
| real_part | 0.7+0.125i | 2.7 ns |
| rect | 0.7 | 15.7 ns |
| root | 1.2345+0.125i, 3 | 7.1 µs |
| round | 0.7+0.125i | 78.7 ns |
| roundn | 1.23456789+0.125i, 3 | 8.0 µs |
| rounds | 1.23456789+0.125i, 3 | 28.2 µs |
| sec | 0.7+0.125i | 7.4 µs |
| sech | 0.7+0.125i | 7.4 µs |
| sgn | 0.7 | 8.6 ns |
| sin | 0.567+0.125i | 7.3 µs |
| sinc | 0.7+0.125i | 7.5 µs |
| sinh | 0.7+0.125i | 16.5 µs † |
| sqr | 0.7+0.125i | 43.4 ns |
| sqrt | 1.23456789+0.125i | 3.3 µs |
| step | 0.7 | 8.6 ns |
| struve_h | 2, 0.7+0.125i | 110.8 µs |
| struve_l | 2, 0.7+0.125i | 112.4 µs |
| sub | 0.7+0.125i, 0.7+0.125i | 13.0 ns |
| tan | 0.7+0.125i | 14.9 µs |
| tanh | 0.7+0.125i | 14.8 µs |
| tetragamma | 2.345+0.125i | 3.6 µs |
| tri | 0.7 | 14.4 ns |
| trigamma | 2.345+0.125i | 2.8 µs |
| vercos | 0.7+0.125i | 7.3 µs |
| versin | 0.7+0.125i | 7.6 µs |
| zatahp | 2.3+0.125i, 1.4+0.125i | 484.7 µs |
| zeta | 2.345+0.125i | 367.5 µs |
| zetah | 2.3+0.125i, 1.4+0.125i | 473.8 µs |
| zetap | 2.345+0.125i | 379.0 µs |

### `mpfr`

| Operation | Inputs | 256 bits | 512 bits | 768 bits | 1024 bits | 2048 bits | 4096 bits |
|---|---|---:|---:|---:|---:|---:|---:|
| E1 | 2.345678 | 31.3 µs | 59.2 µs | 125.8 µs | 176.0 µs | 445.3 µs | 1.725 ms |
| Ei | 2.345678 | 32.3 µs | 58.8 µs | 112.9 µs | 135.4 µs | 425.3 µs | 1.713 ms |
| Li | 2.345 | 79.7 µs † | 55.5 µs | 111.5 µs | 124.4 µs | 409.0 µs | 1.564 ms |
| abs | 0.7 | 69.8 ns | 67.8 ns | 69.1 ns | 70.9 ns | 71.0 ns | 78.5 ns |
| acos | 0.7 | 13.9 µs | 21.5 µs | 28.6 µs | 42.2 µs | 124.4 µs | 263.8 µs |
| acosec | 2.345 | 14.5 µs | 22.0 µs | 38.1 µs | 42.8 µs | 119.2 µs | 349.8 µs |
| acosech | 0.7 | 4.8 µs | 7.7 µs | 13.6 µs | 14.4 µs | 48.4 µs | 130.6 µs |
| acosh | 2.345 | 5.1 µs | 7.8 µs | 15.0 µs | 17.8 µs | 46.5 µs | 97.5 µs |
| acot | 0.7 | 11.6 µs | 24.6 µs | 32.2 µs | 39.0 µs | 122.3 µs | 259.3 µs |
| acoth | 2.345 | 5.2 µs | 8.2 µs | 15.4 µs | 15.4 µs | 44.7 µs | 108.5 µs |
| add | 0.7, 0.7 | 78.8 ns | 80.9 ns | 109.8 ns | 100.1 ns | 170.4 ns | 651.7 ns |
| add_long | 17, 3 | 498.7 ns | 513.5 ns | 700.7 ns | 527.9 ns | 720.5 ns | 629.0 ns |
| appell_f1 | 0.5, 0.7, 0.8, 2.5, 0.2, 0.3 | 202.8 µs | 517.7 µs | 1.066 ms | 1.383 ms | 7.481 ms | 35.083 ms |
| arccovercos | 0.5 | 15.2 µs | 20.1 µs | 25.8 µs | 37.2 µs | 106.3 µs | 243.7 µs |
| arccoversin | 0.5 | 12.1 µs | 20.0 µs | 25.0 µs | 37.0 µs | 105.3 µs | 307.4 µs |
| archacovercos | 0.5 | 1.1 µs | 1.2 µs | 1.6 µs | 1.1 µs | 1.2 µs | 1.3 µs |
| archacoversin | 0.5 | 1.1 µs | 1.1 µs | 2.9 µs † | 1.1 µs | 1.4 µs | 1.6 µs |
| archavercos | 0.5 | 1.1 µs | 1.2 µs | 1.7 µs | 1.5 µs | 1.2 µs | 1.7 µs |
| archaversin | 0.5 | 1.1 µs | 1.2 µs | 1.9 µs | 1.2 µs | 1.3 µs | 2.1 µs † |
| arcvercos | 0.5 | 12.2 µs | 20.2 µs | 33.1 µs | 36.7 µs | 93.8 µs | 308.2 µs |
| arcversin | 0.5 | 12.3 µs | 19.9 µs | 38.2 µs | 37.1 µs | 81.6 µs | 303.0 µs |
| arg | 0.7 | 399.4 ns | 405.7 ns | 512.2 ns | 415.2 ns | 525.4 ns | 441.0 ns |
| asec | 2.345 | 15.1 µs | 22.2 µs | 30.3 µs | 43.8 µs | 90.9 µs | 270.6 µs |
| asech | 0.7 | 5.3 µs | 8.2 µs | 14.8 µs | 19.0 µs | 35.5 µs | 105.8 µs |
| asin | 0.7 | 23.0 µs † | 27.1 µs | 36.3 µs | 44.2 µs | 245.3 µs † | 263.8 µs |
| asinh | 0.7 | 6.3 µs | 7.5 µs | 10.6 µs | 18.0 µs | 59.4 µs † | 149.5 µs † |
| atan | 0.7 | 10.8 µs | 26.4 µs | 30.5 µs | 35.3 µs | 87.1 µs | 317.2 µs |
| atan2 | 0.5, -0.75 | 14.5 µs | 24.2 µs | 28.0 µs | 96.2 µs † | 92.0 µs | 329.8 µs |
| atanh | 0.7 | 6.4 µs | 10.5 µs | 11.1 µs | 19.2 µs | 36.1 µs | 285.7 µs † |
| bessel_i | 2, 0.7 | 79.7 µs | 224.2 µs | 348.1 µs | 562.9 µs | 1.542 ms | 7.514 ms |
| bessel_j | 2, 0.7 | 5.8 µs | 18.4 µs | 21.6 µs | 34.2 µs | 142.5 µs | 873.2 µs |
| bessel_k | 2, 0.7 | 298.2 µs | 761.4 µs | 882.8 µs | 1.192 ms | 3.405 ms | 13.203 ms |
| bessel_y | 2, 0.7 | 48.3 µs | 169.0 µs † | 118.0 µs | 467.8 µs † | 851.4 µs | 4.011 ms |
| beta | 2.3, 4.5 | 89.5 µs | 300.2 µs | 385.5 µs | 622.5 µs | 2.415 ms | 13.004 ms |
| beta_pdf | 0.5, 2.5, 3.5 | 89.1 µs | 171.7 µs | 384.8 µs | 467.2 µs | 1.710 ms | 10.748 ms |
| binomial | 5, 2 | 115.7 µs | 280.3 µs | 481.8 µs | 645.0 µs | 2.971 ms | 12.000 ms |
| ceil | 0.7 | 486.8 ns | 467.4 ns | 431.2 ns | 381.3 ns | 410.8 ns | 560.3 ns |
| chebyshev_t | 2, 0.7 | 2.6 µs | 6.3 µs † | 3.2 µs | 2.3 µs | 2.8 µs | 6.1 µs |
| chebyshev_u | 2, 0.7 | 3.0 µs | 2.8 µs | 2.9 µs | 2.8 µs | 4.2 µs | 6.1 µs |
| circ | 0.7 | 859.7 ns | 1.9 µs † | 677.6 ns | 722.1 ns | 709.1 ns | 1.1 µs |
| clausen | 3, 0.7 | 1.331 ms | 5.257 ms | 10.510 ms | 22.032 ms | 145.463 ms | 986.164 ms |
| clausen2 | 0.7 | 1.346 ms | 5.149 ms | 10.507 ms | 22.351 ms | 151.191 ms | 1.002 s |
| conj | 0.7 | 81.6 ns | 94.1 ns | 93.2 ns | 66.1 ns | 65.8 ns | 126.5 ns |
| cos | 0.7 | 2.4 µs | 4.9 µs | 7.6 µs | 11.8 µs | 40.5 µs | 126.0 µs |
| cosec | 0.7 | 3.3 µs | 6.6 µs | 10.1 µs | 12.2 µs | 36.5 µs | 137.4 µs |
| cosech | 0.7 | 3.1 µs | 5.0 µs | 10.9 µs | 15.4 µs | 40.6 µs | 371.3 µs † |
| cosh | 0.7 | 3.0 µs | 4.7 µs | 10.7 µs | 12.3 µs | 39.4 µs | 180.5 µs |
| cot | 0.7 | 4.5 µs | 11.0 µs | 18.9 µs | 29.7 µs | 71.8 µs | 263.9 µs |
| coth | 0.7 | 6.2 µs | 12.1 µs | 20.9 µs | 31.6 µs | 106.7 µs | 288.1 µs |
| covercos | 0.7 | 3.9 µs | 6.9 µs | 22.3 µs † | 15.7 µs | 44.9 µs | 133.5 µs |
| coversin | 0.7 | 3.9 µs | 5.4 µs | 9.9 µs | 16.2 µs | 47.0 µs | 133.4 µs |
| cubrt | 0.7 | 28.6 µs † | 15.7 µs | 28.8 µs | 31.3 µs | 110.1 µs | 642.7 µs † |
| digamma | 2.345 | 119.4 µs | 285.6 µs | 658.7 µs | 797.0 µs | 4.862 ms | 17.217 ms |
| dilog | 0.7 | 164.8 µs † | 220.2 µs | 544.2 µs | 681.5 µs | 3.757 ms | 16.204 ms |
| div | 0.7, 0.7 | 339.2 ns | 559.9 ns | 882.4 ns | 1.0 µs | 2.8 µs | 8.6 µs |
| erf | 0.7 | 13.7 µs | 32.7 µs | 71.3 µs | 90.4 µs | 575.4 µs | 2.025 ms |
| erfc | 0.7 | 18.0 µs | 35.0 µs | 61.2 µs | 119.1 µs | 396.9 µs | 2.017 ms |
| erfcinv | 0.5 | 271.0 µs | 521.0 µs † | 600.3 µs † | 980.5 µs † | 2.728 ms † | 11.389 ms † |
| erfinv | 0.5 | 277.6 µs | 400.4 µs † | 834.2 µs † | 737.6 µs † | 2.670 ms † | 11.957 ms † |
| exp | 1.23456789 | 2.9 µs | 4.5 µs | 8.1 µs | 13.0 µs | 43.2 µs | 190.7 µs |
| floor | 0.7 | 92.5 ns | 67.5 ns | 70.4 ns | 69.0 ns | 71.8 ns | 103.1 ns |
| gamma | 2.345 | 35.3 µs | 70.8 µs | 126.3 µs | 269.3 µs | 860.0 µs | 4.271 ms |
| gammainc_P | 2.3, 1.4 | 111.0 µs | 204.2 µs | 361.2 µs | 778.3 µs | 2.681 ms | 14.079 ms |
| gammainc_Q | 2.3, 1.4 | 114.7 µs | 205.5 µs | 657.9 µs | 1.217 ms | 5.682 ms | 25.823 ms |
| gammainc_lower | 2.3, 1.4 | 61.8 µs | 163.6 µs | 226.0 µs | 469.5 µs | 2.365 ms | 8.397 ms |
| gammainc_upper | 2.3, 1.4 | 85.0 µs | 205.5 µs | 758.3 µs | 1.330 ms | 5.549 ms | 23.756 ms |
| gammainv | 9.5 | 1.145 ms † | 2.469 ms | 4.746 ms | 11.973 ms | 57.555 ms | 298.933 ms |
| hacovercos | 0.7 | 3.7 µs | 6.4 µs | 9.4 µs | 23.2 µs † | 50.7 µs | 179.8 µs |
| hacoversin | 0.7 | 3.7 µs | 6.2 µs | 9.4 µs | 17.7 µs | 50.0 µs | 166.5 µs |
| harmonic_poly | 2, 0.7 | 3.0 µs | 3.7 µs | 5.3 µs | 6.2 µs | 12.1 µs | 32.6 µs |
| havercos | 0.7 | 3.0 µs | 5.1 µs | 8.0 µs | 15.2 µs | 45.8 µs | 177.8 µs |
| haversin | 0.7 | 3.7 µs | 7.1 µs | 8.0 µs | 32.0 µs † | 44.3 µs | 179.1 µs |
| hermite_h | 2, 0.7 | 2.7 µs | 3.1 µs | 3.1 µs | 7.6 µs † | 5.3 µs | 7.1 µs |
| hypergeometric_pFq | 0.7, 2.5, 0.3 | 3.8 µs | 5.0 µs | 3.8 µs | 4.8 µs | 5.0 µs | 5.0 µs |
| hypot | 0.7, 1.2 | 534.2 ns | 945.7 ns | 945.2 ns | 1.6 µs | 3.3 µs | 7.2 µs |
| imag_part | 0.7 | 63.2 ns | 82.3 ns | 61.6 ns | 85.2 ns | 90.9 ns | 62.6 ns |
| inv | 0.7 | 169.7 ns | 249.1 ns | 321.7 ns | 561.5 ns | 1.4 µs | 3.3 µs |
| lambert_w0 | 0.7 | 130.7 µs † | 102.3 µs | 160.9 µs | 224.7 µs | 894.7 µs | 3.252 ms |
| lambert_wm1 | -0.2 | 49.9 µs | 102.0 µs | 155.8 µs | 449.1 µs † | 1.034 ms | 2.708 ms |
| lambert_wn | 1, 0.7 | 24.1 µs | 30.8 µs | 24.1 µs | 24.4 µs | 32.1 µs | 24.8 µs |
| lauricella_f | 0.5, 0.3, 2.5 | 32.2 µs † | 17.6 µs | 16.5 µs | 16.8 µs | 19.1 µs | 16.7 µs |
| ldexp | 1.23456789, 3 | 98.6 ns | 121.0 ns | 95.5 ns | 129.1 ns | 226.9 ns | 101.7 ns |
| legendre_chi | 2, 0.7 | 183.8 µs | 574.3 µs | 864.4 µs | 2.055 ms | 6.697 ms | 36.818 ms |
| lerch_phi | 0.3, 2, 1.4 | 242.5 µs | 308.7 µs | 243.2 µs | 312.9 µs | 306.1 µs | 306.4 µs |
| lg | 2.345678 | 12.4 µs | 18.1 µs | 20.7 µs | 34.1 µs | 89.0 µs | 241.2 µs |
| lgamma | 2.345 | 29.0 µs | 93.6 µs | 159.3 µs | 268.7 µs | 879.3 µs | 3.737 ms |
| ln | 2.345678 | 4.1 µs | 6.8 µs | 9.3 µs | 17.9 µs | 42.8 µs | 110.7 µs |
| log | 2.345678 | 5.1 µs | 8.5 µs | 9.3 µs | 17.9 µs | 43.8 µs | 93.5 µs |
| log10 | 2.345678 | 8.9 µs | 14.3 µs | 20.3 µs | 36.0 µs | 83.9 µs | 190.1 µs |
| logbeta | 2.3, 4.5 | 118.7 µs | 237.2 µs | 397.2 µs | 826.1 µs | 3.271 ms | 14.394 ms |
| logbeta_pdf | 0.5, 2.5, 3.5 | 73.8 µs | 167.8 µs | 312.7 µs | 469.6 µs | 2.093 ms | 9.682 ms |
| lommel_s | 0.5, 0.7, 1.2 | 23.6 µs | 55.5 µs | 101.1 µs | 174.6 µs | 812.5 µs | 3.589 ms |
| mul | 0.7, 0.7 | 94.5 ns | 181.0 ns | 224.1 ns | 352.8 ns | 1.0 µs | 2.8 µs |
| mul_long | 17, 3 | 597.8 ns | 608.5 ns | 904.5 ns | 830.6 ns | 576.3 ns | 652.5 ns |
| mul_pow10 | 1.23456789, 3 | 278.4 ns | 354.2 ns | 508.5 ns | 652.6 ns | 363.7 ns | 691.2 ns |
| neg | 0.7 | 72.4 ns | 82.5 ns | 71.6 ns | 93.3 ns | 75.0 ns | 102.2 ns |
| normal_cdf | 0.7 | 12.9 µs | 66.1 µs | 51.8 µs | 109.1 µs | 381.0 µs | 1.701 ms |
| normal_logpdf | 0.7 | 4.0 µs | 9.2 µs | 10.0 µs | 17.3 µs | 70.8 µs † | 105.7 µs |
| normal_pdf | 0.7 | 2.9 µs | 7.4 µs | 9.2 µs | 19.4 µs | 47.1 µs | 169.1 µs |
| polygamma | 3, 2.345 | 141.3 µs | 322.3 µs | 687.9 µs | 1.184 ms | 7.759 ms | 100.409 ms |
| polylog | 2, 0.7 | 85.2 µs | 213.1 µs | 400.7 µs | 884.3 µs | 3.749 ms | 17.572 ms |
| polylog1 | 0.7 | 4.6 µs | 7.4 µs | 10.1 µs | 15.2 µs | 42.9 µs | 126.3 µs |
| pow | 1.23456789, 3.5 | 7.3 µs | 12.5 µs | 19.1 µs | 29.2 µs | 93.2 µs | 311.6 µs |
| pow10 | 3 | 192.0 ns | 240.0 ns | 301.8 ns | 1.1 µs † | 347.4 ns | 462.4 ns |
| pow_int | 1.23456789, 3 | 223.7 ns | 325.5 ns | 461.2 ns | 762.4 ns | 2.6 µs | 7.5 µs |
| productlog | 0.7 | 42.6 µs | 81.2 µs | 137.9 µs | 278.5 µs | 913.2 µs | 2.866 ms |
| qdigamma | 0.5, 2.3 | 22.0 µs | 22.2 µs | 22.0 µs | 27.9 µs | 34.4 µs † | 28.2 µs |
| real_part | 0.7 | 65.2 ns | 63.3 ns | 65.1 ns | 86.0 ns | 86.0 ns | 94.5 ns |
| rect | 0.7 | 714.3 ns | 757.7 ns | 769.1 ns | 961.1 ns | 1.0 µs | 1.2 µs |
| root | 1.2345, 3 | 11.4 µs | 16.4 µs | 22.8 µs | 40.2 µs | 201.5 µs † | 307.5 µs |
| round | 0.7 | 1.9 µs | 1.9 µs | 1.8 µs | 1.9 µs | 2.4 µs | 2.6 µs |
| roundn | 1.23456789, 3 | 4.6 µs | 5.2 µs | 4.1 µs | 4.7 µs | 5.3 µs | 10.2 µs |
| rounds | 1.23456789, 3 | 19.9 µs | 21.4 µs | 76.2 µs † | 34.5 µs | 75.1 µs | 201.0 µs |
| sec | 0.7 | 2.0 µs | 5.2 µs | 6.7 µs | 26.1 µs † | 33.2 µs | 163.2 µs |
| sech | 0.7 | 2.4 µs | 4.9 µs | 8.4 µs | 12.8 µs | 112.0 µs † | 160.1 µs |
| sgn | 0.7 | 179.6 ns | 176.3 ns | 444.3 ns | 178.1 ns | 178.1 ns | 229.9 ns |
| sin | 0.567 | 3.3 µs | 4.7 µs | 7.6 µs | 11.8 µs | 88.5 µs † | 145.5 µs |
| sinc | 0.7 | 4.1 µs | 6.7 µs | 9.9 µs | 14.5 µs | 52.0 µs | 186.9 µs |
| sincos | 0.7 | 7.9 µs | 10.1 µs | 13.2 µs | 17.1 µs | 41.4 µs | 188.0 µs |
| sinh | 0.7 | 2.4 µs | 4.8 µs | 19.7 µs † | 12.2 µs | 50.2 µs | 180.9 µs |
| sinhcosh | 0.7 | 2.5 µs | 5.0 µs | 8.4 µs | 12.4 µs | 51.8 µs | 179.9 µs |
| sqr | 0.7 | 173.4 ns | 218.3 ns | 248.1 ns | 364.8 ns | 634.6 ns | 1.5 µs |
| sqrt | 1.23456789 | 259.9 ns | 364.1 ns | 593.2 ns | 565.2 ns | 1.1 µs | 2.4 µs |
| step | 0.7 | 185.5 ns | 202.4 ns | 179.3 ns | 330.0 ns | 177.0 ns | 243.3 ns |
| struve_h | 2, 0.7 | 124.3 µs | 270.9 µs | 430.0 µs | 691.9 µs | 3.042 ms | 12.529 ms |
| struve_l | 2, 0.7 | 124.2 µs | 270.0 µs | 422.7 µs | 655.7 µs | 2.509 ms | 12.749 ms |
| sub | 0.7, 0.7 | 74.6 ns | 67.6 ns | 83.0 ns | 202.9 ns | 113.7 ns | 213.6 ns |
| tan | 0.7 | 9.1 µs | 11.4 µs | 14.6 µs | 19.0 µs | 47.3 µs | 189.7 µs |
| tanh | 0.7 | 2.4 µs | 13.3 µs † | 16.2 µs † | 12.5 µs | 43.9 µs | 187.1 µs |
| tetragamma | 2.345 | 110.1 µs | 426.1 µs † | 947.3 µs † | 1.305 ms † | 7.346 ms † | 108.400 ms † |
| tri | 0.7 | 705.4 ns | 1.1 µs | 712.9 ns | 911.2 ns | 946.7 ns | 1.1 µs |
| trigamma | 2.345 | 75.8 µs | 224.7 µs | 453.1 µs † | 848.1 µs † | 4.811 ms † | 82.123 ms † |
| vercos | 0.7 | 2.9 µs | 4.1 µs | 7.0 µs | 10.5 µs | 42.2 µs | 149.8 µs |
| versin | 0.7 | 2.2 µs | 4.0 µs | 6.8 µs | 10.5 µs | 32.6 µs | 126.7 µs |
| zatahp | 2.3, 1.4 | 1.359 ms | 2.557 ms | 5.027 ms | 9.361 ms | 88.971 ms | 1.010 s |
| zeta | 2.345 | 739.9 µs | 3.248 ms | 7.931 ms | 19.826 ms | 117.768 ms | 691.499 ms |
| zetah | 2.3, 1.4 | 1.300 ms | 2.623 ms | 4.826 ms | 8.846 ms | 85.575 ms | 872.560 ms |
| zetap | 2.345 | 2.322 ms | 5.231 ms | 8.900 ms | 15.359 ms | 110.676 ms | 1.170 s |

### `mpc`

| Operation | Inputs | 256 bits | 512 bits | 768 bits | 1024 bits | 2048 bits | 4096 bits |
|---|---|---:|---:|---:|---:|---:|---:|
| E1 | 2.345678+0.125i | 175.3 µs | 379.4 µs | 546.8 µs | 1.292 ms | 4.276 ms | 16.771 ms |
| Ei | 2.345678+0.125i | 162.5 µs | 379.1 µs | 697.4 µs | 1.171 ms | 4.449 ms | 16.740 ms |
| Li | 2.345+0.125i | 115.6 µs | 463.3 µs † | 465.7 µs | 1.050 ms | 3.545 ms | 17.322 ms |
| abs | 0.7+0.125i | 842.1 ns | 1.3 µs | 1.1 µs | 1.6 µs | 2.8 µs | 6.6 µs |
| acos | 0.7+0.125i | 58.3 µs | 91.9 µs | 89.6 µs | 83.0 µs | 183.5 µs | 496.5 µs |
| acosec | 2.345+0.125i | 210.1 µs | 1.383 ms † | 935.8 µs | 2.452 ms | 10.564 ms | 46.571 ms |
| acosech | 0.7+0.125i | 39.6 µs | 84.7 µs | 199.7 µs † | 153.8 µs | 326.2 µs | 950.5 µs |
| acosh | 2.345+0.125i | 46.2 µs | 87.1 µs | 92.6 µs | 77.0 µs | 254.2 µs | 458.5 µs |
| acot | 0.7+0.125i | 50.5 µs | 85.9 µs | 97.9 µs | 150.3 µs | 278.4 µs | 962.1 µs |
| acoth | 2.345+0.125i | 35.1 µs | 64.4 µs | 76.1 µs | 116.8 µs | 210.9 µs | 774.6 µs |
| add | 0.7+0.125i, 0.7+0.125i | 363.6 ns | 424.6 ns | 391.3 ns | 310.4 ns | 340.7 ns | 432.5 ns |
| add_long | 17+0.125i, 3 | 2.5 µs | 5.4 µs † | 2.2 µs | 3.5 µs | 2.0 µs | 2.1 µs |
| appell_f1 | 0.5+0.125i, 0.7+0.125i, 0.8+0.125i, 2.5+0.125i, 0.2+0.125i, 0.3+0.125i | 1.189 ms | 3.656 ms | 6.156 ms | 11.562 ms | 40.844 ms | 193.985 ms |
| arccovercos | 0.5+0.125i | 56.0 µs | 81.6 µs | 54.0 µs | 146.4 µs | 321.4 µs † | 355.9 µs |
| arccoversin | 0.5+0.125i | 42.7 µs | 35.3 µs | 49.3 µs | 156.1 µs | 136.5 µs | 353.2 µs |
| archacovercos | 0.5+0.125i | 12.6 µs | 27.5 µs † | 21.0 µs | 24.4 µs | 46.0 µs | 101.7 µs |
| archacoversin | 0.5+0.125i | 26.8 µs † | 16.4 µs | 16.4 µs | 23.7 µs | 44.8 µs | 102.2 µs |
| archavercos | 0.5+0.125i | 10.0 µs | 16.4 µs | 16.4 µs | 24.8 µs | 42.0 µs | 101.8 µs |
| archaversin | 0.5+0.125i | 12.4 µs | 20.2 µs † | 16.3 µs | 25.0 µs | 50.9 µs | 129.7 µs |
| arcvercos | 0.5+0.125i | 55.8 µs | 37.9 µs | 87.4 µs | 74.8 µs | 169.0 µs | 463.7 µs |
| arcversin | 0.5+0.125i | 25.1 µs | 43.8 µs | 107.3 µs | 81.9 µs | 169.3 µs | 437.8 µs |
| arg | 0.7+0.125i | 13.8 µs | 29.7 µs | 37.3 µs | 37.0 µs | 112.2 µs | 312.5 µs |
| asec | 2.345+0.125i | 270.2 µs † | 603.2 µs | 1.234 ms | 2.223 ms | 9.771 ms | 47.318 ms |
| asech | 0.7+0.125i | 62.4 µs † | 35.9 µs | 54.2 µs | 63.4 µs | 184.3 µs | 412.8 µs |
| asin | 0.7+0.125i | 78.2 µs † | 192.7 µs † | 97.6 µs | 156.1 µs | 351.1 µs | 435.3 µs |
| asinh | 0.7+0.125i | 37.3 µs | 158.7 µs † | 99.3 µs | 110.9 µs | 338.6 µs | 764.6 µs |
| atan | 0.7+0.125i | 47.2 µs | 77.9 µs | 77.9 µs | 107.8 µs | 327.0 µs | 745.8 µs |
| atan2 | 0.5+0.125i, -0.75+0.125i | 11.2 µs | 9.1 µs | 10.4 µs | 9.1 µs | 11.6 µs | 9.0 µs |
| atanh | 0.7+0.125i | 45.8 µs | 67.5 µs | 72.5 µs | 126.2 µs | 252.3 µs | 803.6 µs |
| bessel_i | 2, 0.7+0.125i | 238.7 µs † | 458.5 µs | 708.6 µs | 1.126 ms | 3.625 ms | 18.437 ms |
| bessel_j | 2, 0.7+0.125i | 246.1 µs † | 395.5 µs | 785.4 µs | 1.006 ms | 4.086 ms | 17.274 ms |
| bessel_k | 2, 0.7+0.125i | 2.743 ms | 5.110 ms | 7.501 ms | 9.752 ms | 30.079 ms | 106.762 ms |
| bessel_y | 2, 0.7+0.125i | 219.8 µs † | 376.7 µs | 676.5 µs | 1.015 ms | 4.449 ms | 17.442 ms |
| beta | 2.3+0.125i, 4.5+0.125i | 49.9 µs † | 32.4 µs | 32.4 µs | 34.2 µs | 41.0 µs | 32.6 µs |
| beta_pdf | 0.5+0.125i, 2.5+0.125i, 3.5+0.125i | 60.8 µs | 116.4 µs † | 59.7 µs | 48.6 µs | 60.1 µs | 47.4 µs |
| binomial | 5+0.125i, 2+0.125i | 30.8 µs | 31.2 µs | 31.8 µs | 28.3 µs | 30.8 µs | 24.2 µs |
| ceil | 0.7+0.125i | 2.1 µs † | 1.4 µs | 1.6 µs | 1.1 µs | 1.2 µs | 1.1 µs |
| chebyshev_t | 2, 0.7+0.125i | 7.4 µs | 9.5 µs | 9.1 µs | 8.3 µs | 12.5 µs | 13.0 µs |
| chebyshev_u | 2, 0.7+0.125i | 13.6 µs | 28.2 µs † | 14.0 µs | 10.5 µs | 15.8 µs | 15.1 µs |
| circ | 0.7 | 1.9 µs | 1.8 µs | 2.6 µs | 2.1 µs | 4.2 µs | 7.9 µs |
| clausen | 3, 0.7+0.125i | 1.318 ms | 5.436 ms | 13.188 ms | 25.447 ms | 159.600 ms | 1.001 s |
| clausen2 | 0.7+0.125i | 1.490 ms | 5.670 ms | 12.402 ms | 24.261 ms | 159.577 ms | 1.001 s |
| conj | 0.7+0.125i | 228.0 ns | 302.0 ns | 451.4 ns | 222.1 ns | 219.6 ns | 503.3 ns |
| cos | 0.7+0.125i | 17.0 µs | 14.2 µs | 26.9 µs | 36.6 µs | 83.1 µs | 394.3 µs |
| cosec | 0.7+0.125i | 21.2 µs | 38.3 µs † | 29.7 µs | 40.7 µs | 88.6 µs | 416.3 µs |
| cosech | 0.7+0.125i | 12.1 µs | 17.7 µs | 30.8 µs † | 89.0 µs | 70.6 µs | 300.3 µs |
| cosh | 0.7+0.125i | 9.3 µs | 14.3 µs | 23.2 µs | 69.8 µs | 65.6 µs | 290.8 µs |
| cot | 0.7+0.125i | 47.9 µs † | 75.9 µs † | 54.8 µs | 76.8 µs | 173.9 µs | 772.4 µs |
| coth | 0.7+0.125i | 19.1 µs | 25.1 µs | 79.2 µs † | 151.0 µs | 138.6 µs | 592.4 µs |
| covercos | 0.7+0.125i | 39.3 µs † | 18.4 µs | 25.0 µs | 32.4 µs | 85.7 µs | 381.7 µs |
| coversin | 0.7+0.125i | 20.2 µs | 19.6 µs | 57.4 µs † | 41.3 µs | 85.6 µs | 354.7 µs |
| cubrt | 0.7+0.125i | 88.9 µs † | 68.2 µs | 99.5 µs | 138.6 µs | 311.0 µs | 1.322 ms |
| digamma | 2.345+0.125i | 718.2 µs | 1.529 ms | 3.072 ms | 4.764 ms | 12.199 ms | 127.862 ms |
| dilog | 0.7+0.125i | 343.3 µs | 1.232 ms | 2.431 ms | 4.081 ms | 16.600 ms | 93.743 ms |
| div | 0.7+0.125i, 0.7+0.125i | 11.0 µs † | 26.8 µs † | 17.0 µs | 18.3 µs | 78.7 µs † | 112.9 µs |
| erf | 0.7+0.125i | 303.4 µs † | 358.4 µs † | 734.1 µs † | 834.4 µs † | 3.032 ms † | 14.910 ms † |
| erfc | 0.7+0.125i | 206.6 µs † | 292.1 µs † | 614.7 µs † | 959.2 µs † | 3.080 ms † | 14.572 ms † |
| erfcinv | 0.5+0.125i | 1.200 ms | 2.882 ms | 4.403 ms | 7.270 ms | 26.087 ms | 139.874 ms |
| erfinv | 0.5+0.125i | 1.333 ms † | 2.807 ms † | 4.125 ms † | 7.524 ms † | 23.547 ms † | 134.729 ms † |
| exp | 1.23456789+0.125i | 11.7 µs | 42.1 µs † | 28.9 µs | 54.2 µs | 178.2 µs | 485.0 µs |
| floor | 0.7+0.125i | 678.7 ns | 298.0 ns | 273.2 ns | 213.1 ns | 227.9 ns | 229.4 ns |
| gamma | 2.345+0.125i | 15.6 µs | 17.4 µs | 15.0 µs | 14.8 µs | 12.0 µs | 15.1 µs |
| gammainc_P | 2.3+0.125i, 1.4+0.125i | 69.3 µs | 136.5 µs † | 73.2 µs | 70.0 µs | 63.3 µs | 55.0 µs |
| gammainc_Q | 2.3+0.125i, 1.4+0.125i | 119.5 µs † | 70.5 µs | 71.5 µs | 68.8 µs | 54.9 µs | 55.2 µs |
| gammainc_lower | 2.3+0.125i, 1.4+0.125i | 64.6 µs † | 50.9 µs | 45.0 µs | 58.0 µs | 54.2 µs | 54.1 µs |
| gammainc_upper | 2.3+0.125i, 1.4+0.125i | 152.0 µs † | 56.4 µs | 69.5 µs | 67.6 µs | 57.4 µs | 68.9 µs |
| gammainv | 9.5+0.125i | 153.4 µs | 152.3 µs | 300.9 µs † | 121.0 µs | 122.0 µs | 120.9 µs |
| hacovercos | 0.7+0.125i | 24.0 µs | 50.3 µs † | 26.5 µs | 35.1 µs | 118.8 µs | 413.7 µs |
| hacoversin | 0.7+0.125i | 25.5 µs | 17.7 µs | 32.8 µs | 43.6 µs | 120.7 µs | 412.3 µs |
| harmonic_poly | 2, 0.7+0.125i | 16.0 µs | 16.3 µs | 16.0 µs | 16.5 µs | 27.4 µs | 63.2 µs |
| havercos | 0.7+0.125i | 25.4 µs | 17.8 µs | 25.0 µs | 35.4 µs | 96.2 µs | 366.2 µs |
| haversin | 0.7+0.125i | 25.5 µs | 22.9 µs | 25.0 µs | 38.9 µs | 99.2 µs | 401.0 µs |
| hermite_h | 2, 0.7+0.125i | 15.5 µs † | 12.7 µs | 13.5 µs | 10.9 µs | 12.7 µs | 22.3 µs |
| hypergeometric_pFq | 0.7+0.125i, 2.5+0.125i, 0.3+0.125i | 40.6 µs | 36.5 µs | 36.7 µs | 35.1 µs | 28.8 µs | 32.7 µs |
| hypot | 0.7+0.125i, 1.2+0.125i | 2.8 µs † | 3.8 µs † | 1.7 µs | 1.8 µs | 2.0 µs | 2.7 µs |
| imag_part | 0.7+0.125i | 110.3 ns | 142.9 ns | 125.7 ns | 96.4 ns | 100.4 ns | 139.1 ns |
| inv | 0.7+0.125i | 1.2 µs | 1.9 µs † | 1.9 µs | 1.9 µs | 3.8 µs | 9.5 µs |
| lambert_w0 | 0.7+0.125i | 55.0 µs | 67.9 µs | 91.8 µs | 94.5 µs | 243.4 µs | 994.1 µs |
| lambert_wm1 | -0.2+0.125i | 52.0 µs | 67.5 µs | 137.0 µs † | 110.3 µs | 239.6 µs | 783.5 µs |
| lambert_wn | 1, 0.7+0.125i | 28.3 µs | 26.9 µs | 27.3 µs | 25.7 µs | 25.8 µs | 25.9 µs |
| lauricella_f | 0.5+0.125i, 0.3+0.125i, 2.5+0.125i | 174.4 µs | 137.3 µs | 176.7 µs | 174.9 µs | 217.0 µs † | 145.4 µs |
| ldexp | 1.23456789+0.125i, 3 | 563.8 ns | 313.4 ns | 240.1 ns | 250.6 ns | 305.5 ns | 257.1 ns |
| legendre_chi | 2, 0.7+0.125i | 1.880 ms | 4.883 ms | 8.592 ms | 13.877 ms | 65.145 ms | 336.717 ms |
| lerch_phi | 0.3+0.125i, 2+0.125i, 1.4+0.125i | 1.103 ms † | 645.7 µs | 654.8 µs | 527.2 µs | 639.0 µs | 507.7 µs |
| lg | 2.345678+0.125i | 26.6 µs | 44.8 µs | 60.1 µs | 61.9 µs | 191.9 µs | 441.1 µs |
| lgamma | 2.345+0.125i | 26.0 µs † | 24.5 µs † | 12.9 µs | 12.7 µs | 12.6 µs | 12.0 µs |
| ln | 2.345678+0.125i | 19.9 µs | 25.5 µs | 31.3 µs | 46.1 µs | 122.9 µs | 357.6 µs |
| log | 2.345678+0.125i | 20.7 µs | 25.3 µs | 33.7 µs | 45.8 µs | 136.5 µs | 334.8 µs |
| log10 | 2.345678+0.125i | 55.5 µs † | 33.8 µs | 58.3 µs | 79.0 µs | 190.5 µs | 498.7 µs |
| logbeta | 2.3+0.125i, 4.5+0.125i | 37.5 µs | 29.7 µs | 29.6 µs | 37.4 µs | 30.4 µs | 37.3 µs |
| logbeta_pdf | 0.5+0.125i, 2.5+0.125i, 3.5+0.125i | 48.4 µs | 40.5 µs | 97.2 µs † | 47.9 µs | 40.7 µs | 48.5 µs |
| lommel_s | 0.5+0.125i, 0.7+0.125i, 1.2+0.125i | 257.8 µs | 402.7 µs | 630.7 µs | 1.295 ms | 3.999 ms | 18.828 ms |
| mul | 0.7+0.125i, 0.7+0.125i | 1.5 µs | 1.3 µs | 1.7 µs | 2.8 µs | 3.7 µs | 7.0 µs |
| mul_long | 17+0.125i, 3 | 3.3 µs | 2.5 µs | 3.1 µs | 2.8 µs | 2.5 µs | 2.7 µs |
| mul_pow10 | 1.23456789+0.125i, 3 | 2.9 µs | 2.4 µs | 2.6 µs | 2.4 µs | 2.3 µs | 2.6 µs |
| neg | 0.7+0.125i | 295.7 ns | 224.2 ns | 221.8 ns | 248.8 ns | 223.3 ns | 291.4 ns |
| normal_cdf | 0.7+0.125i | 168.3 µs † | 278.5 µs † | 639.8 µs † | 800.3 µs † | 2.907 ms † | 17.297 ms † |
| normal_logpdf | 0.7+0.125i | 8.7 µs † | 9.5 µs † | 15.6 µs † | 15.5 µs † | 35.3 µs † | 123.7 µs † |
| normal_pdf | 0.7+0.125i | 23.4 µs † | 34.6 µs † | 63.2 µs † | 66.7 µs † | 205.3 µs † | 959.7 µs † |
| polygamma | 3, 2.345+0.125i | 1.302 ms | 2.404 ms | 4.764 ms | 6.060 ms | 23.295 ms | 176.331 ms |
| polylog | 2, 0.7+0.125i | 430.2 µs | 1.570 ms † | 2.137 ms | 3.340 ms | 16.910 ms | 91.256 ms |
| polylog1 | 0.7+0.125i | 29.2 µs | 46.2 µs | 79.7 µs | 58.6 µs | 132.6 µs | 473.1 µs |
| pow | 1.23456789+0.125i, 3.5+0.125i | 64.9 µs † | 62.7 µs | 97.1 µs | 105.2 µs | 392.9 µs | 1.033 ms |
| pow10 | 3 | 11.3 µs | 18.5 µs | 22.9 µs | 30.1 µs | 53.5 µs | 152.2 µs |
| pow_int | 1.23456789+0.125i, 3 | 17.0 µs † | 7.8 µs | 6.1 µs | 6.9 µs | 9.7 µs | 19.4 µs |
| productlog | 0.7+0.125i | 111.1 µs † | 67.8 µs | 71.1 µs | 95.0 µs | 327.5 µs | 895.1 µs |
| qdigamma | 0.5+0.125i, 2.3+0.125i | 141.6 µs | 154.2 µs | 181.0 µs | 151.5 µs | 182.6 µs | 171.2 µs |
| real_part | 0.7+0.125i | 127.1 ns | 284.0 ns | 101.0 ns | 95.9 ns | 101.7 ns | 146.9 ns |
| rect | 0.7 | 5.4 µs † | 2.5 µs | 3.5 µs † | 2.4 µs | 4.2 µs | 9.0 µs |
| root | 1.2345+0.125i, 3 | 33.0 µs | 118.9 µs † | 203.4 µs | 284.7 µs | 392.9 µs | 1.483 ms |
| round | 0.7+0.125i | 9.4 µs | 7.3 µs | 21.8 µs † | 7.1 µs | 7.9 µs | 10.6 µs |
| roundn | 1.23456789+0.125i, 3 | 36.4 µs † | 12.0 µs | 17.5 µs | 12.6 µs | 15.8 µs | 31.6 µs |
| rounds | 1.23456789+0.125i, 3 | 74.5 µs † | 62.4 µs | 89.7 µs | 95.8 µs | 159.4 µs | 536.0 µs |
| sec | 0.7+0.125i | 18.8 µs | 39.1 µs † | 29.0 µs | 32.3 µs | 89.6 µs | 390.0 µs |
| sech | 0.7+0.125i | 12.1 µs | 17.7 µs | 26.3 µs | 90.3 µs | 70.9 µs | 302.2 µs |
| sgn | 0.7 | 750.1 ns | 1.3 µs † | 888.7 ns | 623.9 ns | 635.6 ns | 669.0 ns |
| sin | 0.567+0.125i | 20.5 µs † | 13.2 µs | 19.2 µs | 28.8 µs | 106.1 µs | 293.8 µs |
| sinc | 0.7+0.125i | 21.0 µs | 21.1 µs | 75.5 µs † | 51.7 µs | 102.2 µs | 336.8 µs |
| sinh | 0.7+0.125i | 9.3 µs | 25.7 µs † | 38.6 µs † | 68.8 µs | 65.6 µs | 225.2 µs |
| sqr | 0.7+0.125i | 1.5 µs | 1.7 µs | 2.1 µs | 2.2 µs | 3.3 µs | 5.6 µs |
| sqrt | 1.23456789+0.125i | 2.7 µs | 2.2 µs | 3.3 µs | 3.2 µs | 5.0 µs | 14.5 µs |
| step | 0.7 | 948.7 ns | 777.4 ns | 1.5 µs † | 625.6 ns | 625.6 ns | 628.0 ns |
| struve_h | 2, 0.7+0.125i | 659.5 µs † | 592.7 µs | 1.079 ms | 1.147 ms | 4.362 ms | 18.497 ms |
| struve_l | 2, 0.7+0.125i | 268.8 µs | 559.2 µs | 955.0 µs | 1.170 ms | 4.086 ms | 21.965 ms |
| sub | 0.7+0.125i, 0.7+0.125i | 427.4 ns | 371.8 ns | 304.4 ns | 284.2 ns | 313.4 ns | 578.8 ns |
| tan | 0.7+0.125i | 14.7 µs | 51.6 µs † | 23.8 µs | 34.7 µs | 100.2 µs | 412.0 µs |
| tanh | 0.7+0.125i | 30.6 µs | 20.6 µs | 94.9 µs † | 33.4 µs | 77.3 µs | 321.7 µs |
| tetragamma | 2.345+0.125i | 1.204 ms † | 2.962 ms † | 4.341 ms † | 7.242 ms † | 25.777 ms † | 179.401 ms † |
| tri | 0.7 | 2.3 µs | 2.4 µs | 2.2 µs | 3.0 µs | 3.4 µs | 6.3 µs |
| trigamma | 2.345+0.125i | 945.0 µs † | 2.400 ms † | 3.702 ms † | 4.796 ms † | 19.422 ms † | 137.650 ms † |
| vercos | 0.7+0.125i | 22.8 µs | 19.3 µs | 21.3 µs | 82.8 µs † | 85.9 µs | 298.1 µs |
| versin | 0.7+0.125i | 20.5 µs | 19.8 µs | 27.3 µs | 39.0 µs | 95.1 µs | 299.1 µs |
| zatahp | 2.3+0.125i, 1.4+0.125i | 6.193 ms | 10.345 ms | 18.520 ms | 32.955 ms | 174.526 ms | 1.346 s |
| zeta | 2.345+0.125i | 4.857 ms | 8.309 ms | 18.160 ms | 28.883 ms | 150.910 ms | 1.387 s |
| zetah | 2.3+0.125i, 1.4+0.125i | 5.194 ms | 9.356 ms | 14.706 ms | 28.410 ms | 152.053 ms | 1.510 s |
| zetap | 2.345+0.125i | 4.655 ms | 7.432 ms | 19.859 ms | 34.716 ms | 164.699 ms | 1.449 s |

### `exact-integer`

| Operation | Inputs | Exact |
|---|---|---:|
| bit_and | 17, 5 | 209.5 ns |
| bit_length | 17 | 139.0 ns |
| bit_not | 17 | 191.8 ns |
| bit_or | 17, 5 | 192.8 ns |
| bit_xor | 17, 5 | 213.3 ns |
| clear_bit | 17, 3 | 263.4 ns |
| divmod | 17, 5 | 123.6 ns |
| factorial | 17 | 222.5 ns |
| factors | 17 | 255.8 ns |
| fibonacci | 17 | 173.1 ns |
| gcd | 17, 5 | 76.4 ns |
| gcdext | 17, 5 | 490.2 ns |
| is_prime | 17 | 121.3 ns |
| isqrt | 17 | 101.5 ns |
| lcm | 17, 5 | 69.9 ns |
| mod | 17, 5 | 81.7 ns |
| modinv | 17, 5 | 236.9 ns |
| next_prime | 17 | 123.2 ns |
| partition | 17 | 1.3 µs |
| powmod | 7, 5, 13 | 152.2 ns |
| prev_prime | 17 | 223.5 ns |
| prove_prime | 17 | 111.4 ns |
| set_bit | 17, 3 | 238.5 ns |
| shl | 17, 3 | 186.8 ns |
| shr | 17, 3 | 252.3 ns |
| test_bit | 17, 3 | 266.9 ns |

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
