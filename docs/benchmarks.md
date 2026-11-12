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
so are floating results whose representation has fewer bits than requested.
This prevents a fixed-precision fallback from masquerading as a fast high-precision measurement.
Complex profiles skip operations restricted to real integers. Integer-only APIs
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

Database connections allow up to five seconds for a short-lived SQLite lock to clear.
This waiting happens outside numerical timing intervals. If a lock persists or storage
fails, the command reports the database operation and SQL error, preserves committed
checkpoints and prints the resume command. A storage failure is reported as stopped;
an interrupt signal is reported as interrupted.

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
Unrestricted fractions use superscript numerator digits and subscript denominator
digits in the terminal grid. Markdown tables use TeX fractions with a horizontal
fraction bar, composed through the TeX module's RPN API. Numerator and denominator
digits wrap independently into rows of at most 80 characters, without rounding
or removing authored signs or leading zeroes. Separate operands start on separate
lines, labelled **Input 1**, **Input 2** and so on when there are several operands.
A math-capable Markdown reader renders the fractions. Formulae are outside
HTML containers so readers such as VS Code recognise their math delimiters; the
embedded preview stylesheet leaves vertical clearance around each fraction. Terminal pagers such as
`less` retain the Unicode display.
Terminals and SQLite do not control individual cell font sizes. The database's
`benchmark_workload.arguments_display` and `benchmark_result.arguments_display`
columns retain the Unicode display alongside the original, parseable `arguments`.
Synchronisation fills existing history without changing measurements or outlier
flags. TSV exports keep the original literal spelling for machine processing.
Catalogue synchronisation applies polynomial API renames to their saved observations
and samples together, preserving timings and outlier flags. If both names already
identify separate histories, neither history is merged or discarded.
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
Benchmark run **31**, 2026-11-12T01:25:53.508Z (UTC).

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
| E1 | 2.345678 | 7.1 µs |
| Ei | 2.345678 | 8.6 µs |
| Li | 2.345 | 7.1 µs |
| abs | 0.7 | 7.5 ns |
| acos | 0.7 | 18.7 ns |
| acosec | 2.345 | 16.6 ns |
| acosech | 0.7 | 61.3 ns |
| acosh | 2.345 | 28.8 ns |
| acot | 0.7 | 47.1 ns |
| acoth | 2.345 | 29.6 ns |
| add | 0.7, 0.7 | 11.1 ns |
| add_long | 17, 3 | 584.7 ns |
| appell_f1 | 0.5, 0.7, 0.8, 2.5, 0.2, 0.3 | 23.1 µs |
| arccovercos | 0.5 | 113.3 ns |
| arccoversin | 0.5 | 116.1 ns |
| archacovercos | 0.5 | 218.2 ns |
| archacoversin | 0.5 | 232.8 ns |
| archavercos | 0.5 | 226.8 ns |
| archaversin | 0.5 | 216.7 ns |
| arcvercos | 0.5 | 118.0 ns |
| arcversin | 0.5 | 112.2 ns |
| arg | 0.7 | 74.6 ns |
| asec | 2.345 | 18.0 ns |
| asech | 0.7 | 31.3 ns |
| asin | 0.7 | 17.0 ns |
| asinh | 0.7 | 53.7 ns |
| atan | 0.7 | 29.6 ns |
| atan2 | 0.5, -0.75 | 41.2 ns |
| atanh | 0.7 | 25.8 ns |
| bessel_i | 2, 0.7 | 45.4 µs |
| bessel_j | 2, 0.7 | 7.3 µs |
| bessel_k | 2, 0.7 | 26.7 µs |
| bessel_y | 2, 0.7 | 17.9 µs |
| beta | 2.3, 4.5 | 131.6 ns |
| beta_pdf | 0.5, 2.5, 3.5 | 183.8 ns |
| binomial | 5, 2 | 381.7 ns |
| ceil | 0.7 | 70.8 ns |
| chebyshev_Tn | 2, 0.7 | 768.5 ns |
| chebyshev_Un | 2, 0.7 | 1.1 µs |
| circ | 0.7 | 241.0 ns |
| clausen | 3, 0.7 | 7.1 µs |
| clausen2 | 0.7 | 7.7 µs |
| conj | 0.7 | 8.0 ns |
| cos | 0.7 | 31.5 ns |
| cosec | 0.7 | 34.2 ns |
| cosech | 0.7 | 57.6 ns |
| cosh | 0.7 | 39.5 ns |
| cot | 0.7 | 36.7 ns |
| coth | 0.7 | 58.2 ns |
| covercos | 0.7 | 127.9 ns |
| coversin | 0.7 | 116.8 ns |
| cubrt | 0.7 | 5.7 µs |
| digamma | 2.345 | 2.5 µs |
| dilog | 0.7 | 15.7 µs |
| div | 0.7, 0.7 | 19.3 ns |
| erf | 0.7 | 68.3 ns |
| erfc | 0.7 | 74.2 ns |
| erfcinv | 0.5 | 343.2 ns |
| erfinv | 0.5 | 297.2 ns |
| exp | 1.23456789 | 44.0 ns |
| floor | 0.7 | 8.3 ns |
| gamma | 2.345 | 121.1 ns |
| gammainc_P | 2.3, 1.4 | 13.4 µs |
| gammainc_Q | 2.3, 1.4 | 14.0 µs |
| gammainc_lower | 2.3, 1.4 | 15.3 µs |
| gammainc_upper | 2.3, 1.4 | 17.2 µs |
| gammainv | 9.5 | 98.2 µs |
| hacovercos | 0.7 | 323.1 ns |
| hacoversin | 0.7 | 248.7 ns |
| harmonic_poly | 2, 0.7 | 1.3 µs |
| havercos | 0.7 | 248.1 ns |
| haversin | 0.7 | 219.7 ns |
| hermite_Hn | 2, 0.7 | 1.7 µs |
| hypergeometric_pFq | 0.7, 2.5, 0.3 | 12.5 µs † |
| hypot | 0.7, 1.2 | 22.4 ns |
| imag_part | 0.7 | 26.0 ns |
| inv | 0.7 | 8.5 ns |
| lambert_w0 | 0.7 | 18.5 µs |
| lambert_wm1 | -0.2 | 15.1 µs |
| lambert_wn | 1, 0.7 | 31.1 µs |
| lauricella_f | 0.5, 0.3, 2.5 | 19.9 µs |
| ldexp | 1.23456789, 3 | 48.6 ns |
| legendre_chi | 2, 0.7 | 30.4 µs |
| lerch_phi | 0.3, 2, 1.4 | 84.1 µs |
| lg | 2.345678 | 26.4 ns |
| lgamma | 2.345 | 69.9 ns |
| ln | 2.345678 | 18.0 ns |
| log | 2.345678 | 15.8 ns |
| log10 | 2.345678 | 22.3 ns |
| logbeta | 2.3, 4.5 | 129.6 ns |
| logbeta_pdf | 0.5, 2.5, 3.5 | 167.5 ns |
| lommel_s | 0.5, 0.7, 1.2 | 7.1 µs |
| mul | 0.7, 0.7 | 12.3 ns |
| mul_long | 17, 3 | 546.4 ns |
| mul_pow10 | 1.23456789, 3 | 71.6 ns |
| neg | 0.7 | 8.7 ns |
| normal_cdf | 0.7 | 66.6 ns |
| normal_logpdf | 0.7 | 15.8 ns |
| normal_pdf | 0.7 | 25.7 ns |
| polygamma | 3, 2.345 | 24.3 µs |
| polylog | 2, 0.7 | 16.1 µs |
| polylog1 | 0.7 | 170.2 ns |
| pow | 1.23456789, 3.5 | 35.3 ns |
| pow10 | 3 | 177.7 ns |
| pow_int | 1.23456789, 3 | 59.9 ns |
| productlog | 0.7 | 16.6 µs |
| qdigamma | 0.5, 2.3 | 27.1 µs |
| real_part | 0.7 | 30.2 ns |
| rect | 0.7 | 315.1 ns |
| root | 1.2345, 3 | 7.5 µs |
| round | 0.7 | 379.4 ns |
| roundn | 1.23456789, 3 | 4.3 µs |
| rounds | 1.23456789, 3 | 6.0 µs |
| sec | 0.7 | 30.2 ns |
| sech | 0.7 | 32.4 ns |
| sgn | 0.7 | 130.9 ns |
| sin | 0.567 | 32.8 ns |
| sinc | 0.7 | 5.0 µs |
| sincos | 0.7 | 814.6 ns |
| sinh | 0.7 | 62.4 ns |
| sinhcosh | 0.7 | 155.4 ns |
| sqr | 0.7 | 9.4 ns |
| sqrt | 1.23456789 | 9.8 ns |
| step | 0.7 | 124.0 ns |
| struve_h | 2, 0.7 | 62.4 µs |
| struve_l | 2, 0.7 | 70.7 µs |
| sub | 0.7, 0.7 | 32.6 ns |
| tan | 0.7 | 12.9 µs † |
| tanh | 0.7 | 304.3 ns |
| tetragamma | 2.345 | 6.7 µs † |
| tri | 0.7 | 226.3 ns |
| trigamma | 2.345 | 2.9 µs |
| vercos | 0.7 | 161.2 ns |
| versin | 0.7 | 139.7 ns |
| zatahp | 2.3, 1.4 | 314.4 µs |
| zeta | 2.345 | 68.3 µs |
| zetah | 2.3, 1.4 | 360.4 µs |
| zetap | 2.345 | 134.6 µs |

### `qfloat`

| Operation | Inputs | 106 bits |
|---|---|---:|
| E1 | 2.345678 | 8.6 µs |
| Ei | 2.345678 | 8.3 µs |
| Li | 2.345 | 9.5 µs |
| abs | 0.7 | 3.3 ns |
| acos | 0.7 | 1.8 µs |
| acosec | 2.345 | 1.9 µs |
| acosech | 0.7 | 3.6 µs |
| acosh | 2.345 | 4.1 µs |
| acot | 0.7 | 1.5 µs |
| acoth | 2.345 | 3.6 µs |
| add | 0.7, 0.7 | 7.9 ns |
| appell_f1 | 0.5, 0.7, 0.8, 2.5, 0.2, 0.3 | 96.0 µs |
| arccovercos | 0.5 | 1.7 µs |
| arccoversin | 0.5 | 1.5 µs |
| archacovercos | 0.5 | 31.0 ns |
| archacoversin | 0.5 | 31.5 ns |
| archavercos | 0.5 | 49.5 ns |
| archaversin | 0.5 | 61.4 ns |
| arcvercos | 0.5 | 1.9 µs |
| arcversin | 0.5 | 1.7 µs |
| asec | 2.345 | 1.7 µs |
| asech | 0.7 | 4.1 µs |
| asin | 0.7 | 1.6 µs |
| asinh | 0.7 | 3.2 µs |
| atan | 0.7 | 1.3 µs |
| atan2 | 0.5, -0.75 | 1.3 µs |
| atanh | 0.7 | 3.4 µs |
| bessel_i | 2, 0.7 | 64.3 µs |
| bessel_j | 2, 0.7 | 6.8 µs |
| bessel_k | 2, 0.7 | 34.2 µs |
| bessel_y | 2, 0.7 | 30.1 µs |
| beta | 2.3, 4.5 | 4.9 µs |
| beta_pdf | 0.5, 2.5, 3.5 | 30.8 µs |
| binomial | 5, 2 | 30.4 µs |
| chebyshev_Tn | 2, 0.7 | 43.1 ns |
| chebyshev_Un | 2, 0.7 | 42.8 ns |
| circ | 0.7 | 15.1 ns |
| clausen | 3, 0.7 | 378.1 µs |
| clausen2 | 0.7 | 7.0 µs |
| cos | 0.7 | 845.5 ns |
| cosec | 0.7 | 999.2 ns |
| cosech | 0.7 | 3.6 µs |
| cosh | 0.7 | 4.0 µs |
| cot | 0.7 | 1.7 µs |
| coth | 0.7 | 14.8 µs † |
| covercos | 0.7 | 968.8 ns |
| coversin | 0.7 | 801.7 ns |
| cubrt | 0.7 | 5.1 µs |
| digamma | 2.345 | 2.8 µs |
| dilog | 0.7 | 16.0 µs |
| div | 0.7, 0.7 | 34.7 ns |
| erf | 0.7 | 3.8 µs |
| erfc | 0.7 | 3.6 µs |
| erfcinv | 0.5 | 214.1 µs |
| erfinv | 0.5 | 201.2 µs |
| exp | 1.23456789 | 1.6 µs |
| floor | 0.7 | 48.6 ns |
| gamma | 2.345 | 3.4 µs † |
| gammainc_P | 2.3, 1.4 | 15.8 µs |
| gammainc_Q | 2.3, 1.4 | 13.4 µs |
| gammainc_lower | 2.3, 1.4 | 14.0 µs |
| gammainc_upper | 2.3, 1.4 | 15.6 µs |
| gammainv | 9.5 | 101.1 µs |
| hacovercos | 0.7 | 856.5 ns |
| hacoversin | 0.7 | 822.4 ns |
| harmonic_poly | 2, 0.7 | 108.5 ns |
| havercos | 0.7 | 964.1 ns |
| haversin | 0.7 | 857.8 ns |
| hermite_Hn | 2, 0.7 | 51.0 ns |
| hypergeometric_pFq | 0.7, 2.5, 0.3 | 4.0 µs |
| hypot | 0.7, 1.2 | 406.3 ns |
| lambert_w0 | 0.7 | 13.5 µs |
| lambert_wm1 | -0.2 | 17.0 µs |
| lauricella_f | 0.5, 0.3, 2.5 | 19.3 µs |
| ldexp | 1.23456789, 3 | 9.9 ns |
| legendre_chi | 2, 0.7 | 27.9 µs |
| lerch_phi | 0.3, 2, 1.4 | 8.5 µs |
| lg | 2.345678 | 3.3 µs |
| lgamma | 2.345 | 5.3 µs |
| ln | 2.345678 | 3.0 µs |
| log | 2.345678 | 3.2 µs |
| log10 | 2.345678 | 2.9 µs |
| logbeta | 2.3, 4.5 | 30.1 µs |
| logbeta_pdf | 0.5, 2.5, 3.5 | 32.6 µs |
| lommel_s | 0.5, 0.7, 1.2 | 7.0 µs |
| mul | 0.7, 0.7 | 7.6 ns |
| mul_pow10 | 1.23456789, 3 | 39.7 ns |
| neg | 0.7 | 2.1 ns |
| normal_cdf | 0.7 | 3.6 µs |
| normal_logpdf | 0.7 | 27.2 ns |
| normal_pdf | 0.7 | 1.8 µs |
| polygamma | 3, 2.345 | 5.4 µs |
| polylog | 2, 0.7 | 17.2 µs |
| polylog1 | 0.7 | 3.1 µs |
| pow | 1.23456789, 3.5 | 4.9 µs |
| pow10 | 3 | 24.4 ns |
| pow_int | 1.23456789, 3 | 50.1 ns |
| productlog | 0.7 | 15.9 µs |
| qdigamma | 0.5, 2.3 | 27.5 µs |
| rect | 0.7 | 18.3 ns |
| root | 1.2345, 3 | 5.5 µs |
| round | 0.7 | 55.8 ns |
| roundn | 1.23456789, 3 | 6.6 µs |
| rounds | 1.23456789, 3 | 19.2 µs |
| sec | 0.7 | 2.0 µs † |
| sech | 0.7 | 9.2 µs † |
| sgn | 0.7 | 10.1 ns |
| sin | 0.567 | 3.3 µs † |
| sinc | 0.7 | 1.1 µs |
| sinh | 0.7 | 5.1 µs † |
| sqr | 0.7 | 5.6 ns |
| sqrt | 1.23456789 | 379.9 ns |
| step | 0.7 | 9.1 ns |
| struve_h | 2, 0.7 | 98.5 µs |
| struve_l | 2, 0.7 | 80.7 µs |
| sub | 0.7, 0.7 | 7.8 ns |
| tan | 0.7 | 1.9 µs |
| tanh | 0.7 | 1.7 µs |
| tetragamma | 2.345 | 3.4 µs |
| tri | 0.7 | 13.8 ns |
| trigamma | 2.345 | 2.6 µs |
| vercos | 0.7 | 1.0 µs |
| versin | 0.7 | 1.7 µs † |
| zatahp | 2.3, 1.4 | 466.3 µs † |
| zeta | 2.345 | 356.3 µs |
| zetah | 2.3, 1.4 | 737.0 µs † |
| zetap | 2.345 | 408.6 µs |

### `qcomplex`

| Operation | Inputs | 106 bits |
|---|---|---:|
| E1 | 2.345678+0.125i | 67.1 µs |
| Ei | 2.345678+0.125i | 80.8 µs † |
| Li | 2.345+0.125i | 56.6 µs |
| abs | 0.7+0.125i | 597.8 ns |
| acos | 0.7+0.125i | 11.4 µs |
| acosec | 2.345+0.125i | 11.3 µs |
| acosech | 0.7+0.125i | 10.5 µs |
| acosh | 2.345+0.125i | 13.7 µs |
| acot | 0.7+0.125i | 13.4 µs |
| acoth | 2.345+0.125i | 5.6 µs |
| add | 0.7+0.125i, 0.7+0.125i | 18.3 ns |
| appell_f1 | <span style="display:inline-block;width:40ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">0.5+0.125i, 0.7+0.125i, 0.8+0.125i, 2.5+<br/>0.125i, 0.2+0.125i, 0.3+0.125i</span> | 693.3 µs |
| arccovercos | 0.5+0.125i | 9.7 µs |
| arccoversin | 0.5+0.125i | 9.5 µs |
| archacovercos | 0.5+0.125i | 6.6 µs |
| archacoversin | 0.5+0.125i | 6.7 µs |
| archavercos | 0.5+0.125i | 5.9 µs |
| archaversin | 0.5+0.125i | 9.5 µs † |
| arcvercos | 0.5+0.125i | 13.7 µs † |
| arcversin | 0.5+0.125i | 11.1 µs |
| arg | 0.7+0.125i | 1.5 µs |
| asec | 2.345+0.125i | 11.0 µs |
| asech | 0.7+0.125i | 14.4 µs |
| asin | 0.7+0.125i | 10.8 µs |
| asinh | 0.7+0.125i | 10.9 µs |
| atan | 0.7+0.125i | 12.7 µs |
| atan2 | 0.5+0.125i, -0.75+0.125i | 12.6 µs |
| atanh | 0.7+0.125i | 6.7 µs |
| bessel_i | 2, 0.7+0.125i | 112.2 µs |
| bessel_j | 2, 0.7+0.125i | 130.7 µs |
| bessel_k | 2, 0.7+0.125i | 86.3 µs |
| bessel_y | 2, 0.7+0.125i | 107.1 µs |
| beta | 2.3+0.125i, 4.5+0.125i | 41.2 µs |
| beta_pdf | 0.5+0.125i, 2.5+0.125i, 3.5+0.125i | 60.9 µs |
| binomial | 5+0.125i, 2+0.125i | 33.6 µs |
| chebyshev_Tn | 2, 0.7+0.125i | 194.3 ns |
| chebyshev_Un | 2, 0.7+0.125i | 188.5 ns |
| circ | 0.7 | 20.3 ns |
| clausen | 3, 0.7+0.125i | 410.1 µs |
| clausen2 | 0.7+0.125i | 28.8 µs |
| conj | 0.7+0.125i | 4.0 ns |
| cos | 0.7+0.125i | 11.1 µs |
| cosec | 0.7+0.125i | 9.6 µs |
| cosech | 0.7+0.125i | 9.8 µs |
| cosh | 0.7+0.125i | 9.3 µs |
| cot | 0.7+0.125i | 21.3 µs |
| coth | 0.7+0.125i | 19.4 µs |
| covercos | 0.7+0.125i | 9.5 µs |
| coversin | 0.7+0.125i | 11.6 µs |
| cubrt | 0.7+0.125i | 8.8 µs |
| digamma | 2.345+0.125i | 16.0 µs |
| dilog | 0.7+0.125i | 114.1 µs |
| div | 0.7+0.125i, 0.7+0.125i | 174.9 ns |
| erf | 0.7+0.125i | 61.6 µs |
| erfc | 0.7+0.125i | 59.1 µs |
| erfcinv | 0.5+0.125i | 378.9 µs |
| erfinv | 0.5+0.125i | 411.9 µs |
| exp | 1.23456789+0.125i | 3.9 µs |
| floor | 0.7+0.125i | 60.1 ns |
| gamma | 2.345+0.125i | 16.1 µs |
| gammainc_P | 2.3+0.125i, 1.4+0.125i | 81.0 µs |
| gammainc_Q | 2.3+0.125i, 1.4+0.125i | 79.9 µs |
| gammainc_lower | 2.3+0.125i, 1.4+0.125i | 54.2 µs |
| gammainc_upper | 2.3+0.125i, 1.4+0.125i | 73.7 µs |
| gammainv | 9.5+0.125i | 142.3 µs |
| hacovercos | 0.7+0.125i | 9.5 µs |
| hacoversin | 0.7+0.125i | 10.6 µs |
| harmonic_poly | 2, 0.7+0.125i | 665.6 ns |
| havercos | 0.7+0.125i | 10.7 µs |
| haversin | 0.7+0.125i | 12.5 µs † |
| hermite_Hn | 2, 0.7+0.125i | 224.0 ns |
| hypergeometric_pFq | 0.7+0.125i, 2.5+0.125i, 0.3+0.125i | 37.8 µs |
| hypot | 0.7+0.125i, 1.2+0.125i | 1.8 µs |
| imag_part | 0.7+0.125i | 3.3 ns |
| inv | 0.7+0.125i | 199.8 ns |
| lambert_w0 | 0.7+0.125i | 36.0 µs |
| lambert_wm1 | -0.2+0.125i | 47.6 µs † |
| lambert_wn | 1, 0.7+0.125i | 36.4 µs |
| lauricella_f | 0.5+0.125i, 0.3+0.125i, 2.5+0.125i | 205.6 µs |
| ldexp | 1.23456789+0.125i, 3 | 32.8 ns |
| legendre_chi | 2, 0.7+0.125i | 401.6 µs |
| lerch_phi | 0.3+0.125i, 2+0.125i, 1.4+0.125i | 814.5 µs |
| lg | 2.345678+0.125i | 4.7 µs |
| lgamma | 2.345+0.125i | 12.4 µs |
| ln | 2.345678+0.125i | 5.1 µs |
| log | 2.345678+0.125i | 5.5 µs |
| log10 | 2.345678+0.125i | 5.8 µs |
| logbeta | 2.3+0.125i, 4.5+0.125i | 34.4 µs |
| logbeta_pdf | 0.5+0.125i, 2.5+0.125i, 3.5+0.125i | 48.2 µs |
| lommel_s | 0.5+0.125i, 0.7+0.125i, 1.2+0.125i | 143.7 µs |
| mul | 0.7+0.125i, 0.7+0.125i | 61.3 ns |
| neg | 0.7+0.125i | 7.5 ns |
| normal_cdf | 0.7+0.125i | 46.6 µs |
| normal_logpdf | 0.7+0.125i | 175.0 ns |
| normal_pdf | 0.7+0.125i | 4.0 µs |
| polygamma | 3, 2.345+0.125i | 14.7 µs |
| polylog | 2, 0.7+0.125i | 112.4 µs |
| polylog1 | 0.7+0.125i | 8.4 µs † |
| pow | 1.23456789+0.125i, 3.5+0.125i | 12.1 µs † |
| productlog | 0.7+0.125i | 36.7 µs |
| qdigamma | 0.5+0.125i, 2.3+0.125i | 178.6 µs |
| real_part | 0.7+0.125i | 2.9 ns |
| rect | 0.7 | 26.3 ns |
| root | 1.2345+0.125i, 3 | 9.2 µs |
| round | 0.7+0.125i | 79.5 ns |
| roundn | 1.23456789+0.125i, 3 | 11.3 µs |
| rounds | 1.23456789+0.125i, 3 | 36.5 µs |
| sec | 0.7+0.125i | 10.2 µs |
| sech | 0.7+0.125i | 10.2 µs |
| sgn | 0.7 | 11.8 ns |
| sin | 0.567+0.125i | 9.2 µs |
| sinc | 0.7+0.125i | 9.5 µs |
| sinh | 0.7+0.125i | 11.8 µs † |
| sqr | 0.7+0.125i | 57.5 ns |
| sqrt | 1.23456789+0.125i | 4.1 µs |
| step | 0.7 | 14.8 ns |
| struve_h | 2, 0.7+0.125i | 175.6 µs |
| struve_l | 2, 0.7+0.125i | 123.2 µs |
| sub | 0.7+0.125i, 0.7+0.125i | 18.1 ns |
| tan | 0.7+0.125i | 22.4 µs |
| tanh | 0.7+0.125i | 18.7 µs |
| tetragamma | 2.345+0.125i | 4.6 µs |
| tri | 0.7 | 19.1 ns |
| trigamma | 2.345+0.125i | 3.5 µs |
| vercos | 0.7+0.125i | 9.0 µs |
| versin | 0.7+0.125i | 10.3 µs |
| zatahp | 2.3+0.125i, 1.4+0.125i | 571.8 µs |
| zeta | 2.345+0.125i | 411.7 µs |
| zetah | 2.3+0.125i, 1.4+0.125i | 683.5 µs |
| zetap | 2.345+0.125i | 478.0 µs |

### `mpfr`

| Operation | Inputs | 256 bits | 512 bits | 768 bits | 1024 bits | 2048 bits | 4096 bits |
|---|---|---:|---:|---:|---:|---:|---:|
| E1 | 2.345678 | 49.8 µs | 83.8 µs | 148.0 µs | 179.4 µs | 716.8 µs | 2.594 ms |
| Ei | 2.345678 | 46.5 µs | 100.5 µs | 158.4 µs | 212.3 µs | 1.318 ms † | 2.724 ms |
| Li | 2.345 | 44.0 µs | 91.2 µs | 152.0 µs | 203.5 µs † | 679.0 µs | 1.857 ms |
| abs | 0.7 | 94.4 ns | 119.3 ns | 102.7 ns | 97.5 ns | 151.3 ns | 117.3 ns |
| acos | 0.7 | 23.0 µs | 31.3 µs | 45.3 µs | 68.1 µs † | 115.0 µs | 399.5 µs |
| acosec | 2.345 | 20.3 µs | 30.4 µs | 50.0 µs | 51.0 µs | 156.1 µs † | 365.0 µs |
| acosech | 0.7 | 6.6 µs | 11.7 µs | 20.5 µs | 19.0 µs | 59.5 µs † | 159.5 µs |
| acosh | 2.345 | 7.4 µs | 12.0 µs | 15.1 µs | 23.6 µs | 72.3 µs † | 137.9 µs |
| acot | 0.7 | 15.0 µs | 26.1 µs | 43.7 µs | 61.3 µs † | 170.4 µs † | 424.8 µs |
| acoth | 2.345 | 7.6 µs | 11.8 µs | 20.0 µs † | 25.7 µs † | 209.7 µs † | 165.4 µs |
| add | 0.7, 0.7 | 133.8 ns | 168.5 ns | 134.4 ns | 347.9 ns | 237.3 ns | 276.5 ns |
| add_long | 17, 3 | 629.2 ns | 924.5 ns | 1.0 µs | 1.8 µs † | 1.1 µs | 832.4 ns |
| appell_f1 | 0.5, 0.7, 0.8, 2.5, 0.2, 0.3 | 138.4 µs | 402.6 µs | 799.0 µs † | 2.250 ms † | 5.402 ms | 25.774 ms |
| arccovercos | 0.5 | 15.3 µs | 34.8 µs † | 47.2 µs | 60.6 µs | 115.3 µs | 392.6 µs |
| arccoversin | 0.5 | 15.7 µs | 26.1 µs | 37.8 µs | 48.6 µs | 96.3 µs | 332.5 µs |
| archacovercos | 0.5 | 1.5 µs | 1.6 µs | 2.4 µs | 1.8 µs | 1.9 µs | 2.3 µs |
| archacoversin | 0.5 | 2.1 µs † | 1.6 µs | 1.8 µs | 1.7 µs | 2.2 µs † | 2.4 µs |
| archavercos | 0.5 | 1.4 µs | 1.9 µs | 1.9 µs | 1.6 µs | 1.7 µs | 2.4 µs |
| archaversin | 0.5 | 1.7 µs | 2.0 µs | 2.0 µs | 2.0 µs | 1.7 µs | 2.2 µs |
| arcvercos | 0.5 | 17.5 µs | 29.7 µs | 34.0 µs | 49.3 µs | 184.8 µs † | 293.1 µs |
| arcversin | 0.5 | 18.6 µs | 26.5 µs | 40.9 µs | 50.2 µs | 140.4 µs | 322.7 µs |
| arg | 0.7 | 524.6 ns | 658.9 ns | 618.9 ns | 632.2 ns | 900.3 ns | 603.2 ns |
| asec | 2.345 | 22.2 µs | 32.7 µs | 40.0 µs | 58.1 µs | 141.4 µs | 349.5 µs |
| asech | 0.7 | 7.0 µs | 10.2 µs | 19.8 µs | 20.3 µs | 59.5 µs | 149.8 µs |
| asin | 0.7 | 17.6 µs | 27.4 µs | 43.9 µs | 68.9 µs | 141.9 µs | 363.1 µs |
| asinh | 0.7 | 7.3 µs | 10.8 µs | 14.5 µs | 19.7 µs | 50.1 µs | 147.8 µs |
| atan | 0.7 | 15.2 µs | 23.1 µs | 38.5 µs | 56.5 µs | 134.9 µs | 285.7 µs |
| atan2 | 0.5, -0.75 | 14.7 µs | 32.1 µs | 29.3 µs | 66.4 µs | 114.4 µs | 360.9 µs |
| atanh | 0.7 | 6.7 µs | 9.9 µs | 15.3 µs | 17.8 µs | 48.8 µs | 163.9 µs |
| bessel_i | 2, 0.7 | 107.7 µs | 230.6 µs | 440.9 µs | 534.6 µs | 2.047 ms | 9.002 ms |
| bessel_j | 2, 0.7 | 8.1 µs | 19.0 µs | 26.5 µs | 60.4 µs † | 181.8 µs | 894.0 µs |
| bessel_k | 2, 0.7 | 65.3 µs | 106.4 µs | 178.9 µs | 274.7 µs | 1.303 ms † | 5.419 ms |
| bessel_y | 2, 0.7 | 42.6 µs | 98.6 µs | 183.3 µs | 282.3 µs | 1.403 ms † | 5.577 ms † |
| beta | 2.3, 4.5 | 112.2 µs | 300.5 µs | 459.0 µs | 876.7 µs | 3.831 ms | 27.760 ms † |
| beta_pdf | 0.5, 2.5, 3.5 | 103.9 µs | 244.1 µs | 441.0 µs | 585.1 µs | 2.208 ms | 14.646 ms |
| binomial | 5, 2 | 107.4 µs | 276.9 µs | 638.3 µs | 730.1 µs | 3.810 ms | 15.276 ms |
| ceil | 0.7 | 800.9 ns | 746.4 ns | 849.7 ns | 725.1 ns | 659.3 ns | 1.5 µs † |
| chebyshev_Tn | 2, 0.7 | 3.2 µs | 3.0 µs | 4.8 µs | 4.3 µs | 5.0 µs | 8.7 µs |
| chebyshev_Un | 2, 0.7 | 4.0 µs | 3.7 µs | 5.4 µs | 4.1 µs | 18.2 µs † | 9.7 µs † |
| circ | 0.7 | 863.7 ns | 1.2 µs | 2.6 µs † | 1.3 µs | 1.4 µs | 3.1 µs † |
| clausen | 3, 0.7 | 35.8 µs | 72.6 µs | 196.0 µs | 211.9 µs | 1.077 ms | 7.738 ms |
| clausen2 | 0.7 | 26.4 µs | 83.9 µs | 148.8 µs | 273.2 µs | 1.163 ms | 5.814 ms |
| conj | 0.7 | 84.1 ns | 111.9 ns | 88.8 ns | 137.8 ns | 100.1 ns | 152.1 ns |
| cos | 0.7 | 3.1 µs | 4.9 µs | 10.9 µs | 15.6 µs | 48.0 µs | 190.8 µs |
| cosec | 0.7 | 3.7 µs | 7.2 µs | 9.6 µs | 20.5 µs | 53.0 µs | 177.6 µs |
| cosech | 0.7 | 3.5 µs | 6.9 µs | 10.7 µs | 23.1 µs † | 65.5 µs † | 226.1 µs |
| cosh | 0.7 | 3.8 µs | 6.6 µs | 12.6 µs | 19.1 µs | 56.3 µs | 180.8 µs |
| cot | 0.7 | 5.6 µs | 13.0 µs | 20.6 µs | 30.2 µs | 105.9 µs | 446.7 µs |
| coth | 0.7 | 5.9 µs | 16.6 µs | 23.4 µs | 30.5 µs | 98.0 µs | 366.7 µs |
| covercos | 0.7 | 5.0 µs | 6.5 µs | 11.3 µs | 15.8 µs | 53.8 µs | 173.8 µs |
| coversin | 0.7 | 4.0 µs | 6.5 µs | 12.0 µs | 22.2 µs † | 50.4 µs | 244.3 µs |
| cubrt | 0.7 | 13.7 µs | 23.5 µs | 35.9 µs | 43.4 µs | 128.6 µs † | 418.5 µs |
| digamma | 2.345 | 117.4 µs | 312.7 µs | 712.5 µs | 1.028 ms | 4.635 ms | 21.717 ms |
| dilog | 0.7 | 121.4 µs | 231.2 µs | 697.8 µs | 897.7 µs | 4.611 ms | 21.523 ms |
| div | 0.7, 0.7 | 362.9 ns | 707.5 ns | 877.5 ns | 1.3 µs | 4.5 µs | 12.3 µs |
| erf | 0.7 | 21.8 µs | 44.9 µs | 87.7 µs | 131.9 µs | 469.1 µs | 3.078 ms |
| erfc | 0.7 | 23.3 µs | 44.8 µs | 83.9 µs | 169.4 µs † | 638.9 µs | 2.245 ms |
| erfcinv | 0.5 | 162.4 µs | 393.4 µs | 416.7 µs | 784.4 µs | 3.889 ms | 15.085 ms |
| erfinv | 0.5 | 181.0 µs † | 352.6 µs | 450.4 µs | 860.4 µs | 3.554 ms | 16.075 ms |
| exp | 1.23456789 | 3.3 µs | 5.7 µs | 12.9 µs | 20.5 µs | 62.9 µs | 218.8 µs |
| floor | 0.7 | 104.9 ns | 94.4 ns | 124.1 ns | 110.6 ns | 128.1 ns | 152.6 ns |
| gamma | 2.345 | 111.6 µs † | 92.7 µs | 164.2 µs | 335.0 µs | 1.190 ms | 6.718 ms |
| gammainc_P | 2.3, 1.4 | 147.1 µs | 284.8 µs | 489.7 µs | 915.2 µs | 3.230 ms | 15.774 ms |
| gammainc_Q | 2.3, 1.4 | 136.6 µs | 294.1 µs | 622.8 µs | 751.9 µs | 3.312 ms | 16.379 ms |
| gammainc_lower | 2.3, 1.4 | 78.9 µs | 145.2 µs | 339.5 µs | 447.9 µs | 2.117 ms | 9.711 ms |
| gammainc_upper | 2.3, 1.4 | 140.7 µs | 289.8 µs | 574.7 µs | 1.404 ms † | 3.137 ms | 18.009 ms |
| gammainv | 9.5 | 770.8 µs | 1.761 ms | 2.296 ms | 4.979 ms | 11.011 ms | 65.406 ms |
| hacovercos | 0.7 | 5.4 µs | 7.1 µs | 14.3 µs | 19.0 µs | 56.2 µs | 647.4 µs † |
| hacoversin | 0.7 | 5.6 µs | 8.2 µs | 14.5 µs | 18.2 µs | 60.9 µs | 217.9 µs |
| harmonic_poly | 2, 0.7 | 4.6 µs | 6.7 µs | 6.1 µs | 6.8 µs | 12.0 µs | 42.8 µs |
| havercos | 0.7 | 4.4 µs | 6.6 µs | 12.4 µs | 15.7 µs | 54.4 µs | 182.3 µs |
| haversin | 0.7 | 4.2 µs | 6.9 µs | 10.7 µs | 20.9 µs † | 47.9 µs | 218.3 µs |
| hermite_Hn | 2, 0.7 | 4.0 µs | 4.6 µs | 3.9 µs | 4.3 µs | 5.9 µs | 6.7 µs |
| hypergeometric_pFq | 0.7, 2.5, 0.3 | 70.8 µs † | 105.5 µs † | 218.8 µs † | 287.6 µs † | 1.328 ms † | 6.078 ms † |
| hypot | 0.7, 1.2 | 820.9 ns | 948.3 ns | 1.4 µs | 2.1 µs | 3.3 µs | 9.7 µs |
| imag_part | 0.7 | 95.1 ns | 90.1 ns | 85.2 ns | 104.9 ns | 104.6 ns | 87.8 ns |
| inv | 0.7 | 244.1 ns | 317.0 ns | 483.2 ns | 1.5 µs † | 1.7 µs | 5.4 µs |
| lambert_w0 | 0.7 | 56.8 µs | 101.3 µs | 181.3 µs | 375.5 µs | 945.5 µs | 4.059 ms |
| lambert_wm1 | -0.2 | 75.7 µs | 133.9 µs | 245.7 µs | 350.3 µs | 1.063 ms | 8.568 ms † |
| ldexp | 1.23456789, 3 | 156.9 ns | 131.1 ns | 193.3 ns | 236.6 ns | 167.1 ns | 276.0 ns |
| legendre_chi | 2, 0.7 | 55.2 µs | 131.2 µs | 199.8 µs | 335.8 µs | 909.6 µs | 3.454 ms |
| lerch_phi | 0.3, 2, 1.4 | 195.0 µs | 600.9 µs | 1.162 ms † | 1.408 ms † | 6.013 ms † | 28.873 ms † |
| lg | 2.345678 | 13.2 µs | 23.3 µs | 35.9 µs † | 42.0 µs | 117.6 µs | 322.1 µs |
| lgamma | 2.345 | 39.9 µs | 114.9 µs | 210.5 µs | 255.1 µs | 1.075 ms | 6.535 ms |
| ln | 2.345678 | 5.7 µs | 8.4 µs | 15.5 µs | 18.1 µs | 45.8 µs | 156.0 µs |
| log | 2.345678 | 5.6 µs | 9.4 µs | 14.2 µs | 20.1 µs | 56.8 µs | 155.4 µs |
| log10 | 2.345678 | 14.3 µs | 24.5 µs | 35.9 µs | 39.1 µs | 115.7 µs | 240.8 µs |
| logbeta | 2.3, 4.5 | 114.9 µs | 293.1 µs | 643.5 µs | 947.6 µs | 6.025 ms † | 19.888 ms |
| logbeta_pdf | 0.5, 2.5, 3.5 | 111.2 µs | 213.7 µs | 497.1 µs | 774.9 µs | 2.872 ms | 12.836 ms † |
| lommel_s | 0.5, 0.7, 1.2 | 33.1 µs | 72.8 µs | 149.6 µs | 298.7 µs | 959.1 µs | 3.493 ms |
| mul | 0.7, 0.7 | 122.8 ns | 224.7 ns | 257.2 ns | 475.3 ns | 1.2 µs | 3.0 µs |
| mul_long | 17, 3 | 763.9 ns | 868.2 ns | 1.2 µs | 1.0 µs | 1.3 µs † | 1.0 µs |
| mul_pow10 | 1.23456789, 3 | 380.0 ns | 454.3 ns | 801.2 ns | 599.5 ns | 608.7 ns | 819.7 ns |
| neg | 0.7 | 112.9 ns | 97.2 ns | 95.7 ns | 105.2 ns | 132.7 ns | 113.6 ns |
| normal_cdf | 0.7 | 21.2 µs † | 92.8 µs | 77.9 µs | 134.1 µs | 481.6 µs | 2.214 ms |
| normal_logpdf | 0.7 | 6.6 µs | 10.0 µs | 12.5 µs | 17.9 µs | 47.3 µs | 127.3 µs |
| normal_pdf | 0.7 | 4.2 µs | 6.2 µs | 13.8 µs | 22.9 µs | 57.9 µs | 262.1 µs |
| polygamma | 3, 2.345 | 63.2 µs | 126.7 µs | 331.6 µs | 467.7 µs | 1.860 ms | 12.100 ms |
| polylog | 2, 0.7 | 144.6 µs | 332.2 µs | 690.1 µs | 1.063 ms | 3.522 ms | 18.338 ms |
| polylog1 | 0.7 | 7.6 µs | 12.3 µs | 18.1 µs † | 23.1 µs | 57.2 µs † | 142.6 µs |
| pow | 1.23456789, 3.5 | 11.5 µs | 23.3 µs † | 39.0 µs † | 42.3 µs | 116.0 µs | 314.4 µs |
| pow10 | 3 | 1.0 µs † | 441.3 ns | 622.6 ns | 532.5 ns | 341.9 ns | 753.6 ns |
| pow_int | 1.23456789, 3 | 292.8 ns | 570.3 ns | 889.0 ns | 845.4 ns | 2.7 µs | 9.0 µs |
| productlog | 0.7 | 71.8 µs | 108.5 µs | 230.7 µs | 375.2 µs | 960.8 µs | 4.034 ms |
| real_part | 0.7 | 90.7 ns | 100.7 ns | 88.1 ns | 122.9 ns | 106.0 ns | 111.4 ns |
| rect | 0.7 | 1.3 µs | 986.6 ns | 1.3 µs | 1.5 µs † | 1.3 µs | 1.9 µs † |
| root | 1.2345, 3 | 17.1 µs | 22.7 µs | 43.0 µs | 50.4 µs | 112.4 µs | 418.4 µs |
| round | 0.7 | 2.8 µs | 3.2 µs | 4.4 µs † | 3.6 µs † | 2.5 µs | 2.7 µs |
| roundn | 1.23456789, 3 | 6.5 µs | 5.9 µs | 7.9 µs | 8.9 µs | 9.1 µs | 13.6 µs |
| rounds | 1.23456789, 3 | 23.2 µs | 31.8 µs | 46.2 µs | 44.5 µs | 133.3 µs | 346.0 µs |
| sec | 0.7 | 2.6 µs | 5.6 µs | 8.8 µs | 13.3 µs | 46.0 µs | 223.9 µs |
| sech | 0.7 | 3.2 µs | 6.7 µs | 12.0 µs | 20.3 µs | 64.7 µs | 233.1 µs |
| sgn | 0.7 | 349.5 ns | 459.2 ns | 305.2 ns | 335.4 ns | 310.0 ns | 418.6 ns |
| sin | 0.567 | 4.5 µs † | 9.1 µs † | 10.0 µs | 20.3 µs | 59.1 µs | 177.2 µs |
| sinc | 0.7 | 6.6 µs | 10.4 µs | 14.1 µs | 20.7 µs | 73.5 µs † | 240.7 µs |
| sincos | 0.7 | 14.0 µs | 16.8 µs | 22.9 µs | 22.5 µs | 63.2 µs | 190.1 µs |
| sinh | 0.7 | 3.3 µs | 7.9 µs | 10.6 µs | 14.4 µs | 66.5 µs | 233.8 µs |
| sinhcosh | 0.7 | 3.8 µs | 6.6 µs | 11.8 µs | 19.8 µs | 64.8 µs | 228.0 µs |
| sqr | 0.7 | 204.8 ns | 230.2 ns | 369.9 ns | 539.6 ns | 770.4 ns | 1.7 µs |
| sqrt | 1.23456789 | 320.9 ns | 640.2 ns | 726.8 ns | 735.3 ns | 1.8 µs | 3.3 µs |
| step | 0.7 | 232.5 ns | 246.6 ns | 322.9 ns | 402.7 ns | 310.6 ns | 412.1 ns |
| struve_h | 2, 0.7 | 176.7 µs | 425.4 µs | 852.6 µs | 1.052 ms | 2.903 ms | 17.807 ms |
| struve_l | 2, 0.7 | 188.3 µs | 430.2 µs | 702.1 µs | 1.022 ms | 3.534 ms | 16.416 ms |
| sub | 0.7, 0.7 | 90.6 ns | 117.7 ns | 103.4 ns | 167.4 ns | 180.6 ns | 387.1 ns |
| tan | 0.7 | 15.7 µs | 21.6 µs † | 24.1 µs | 32.2 µs | 56.4 µs | 215.3 µs |
| tanh | 0.7 | 3.2 µs | 6.5 µs | 12.5 µs | 19.0 µs | 62.0 µs | 237.2 µs |
| tetragamma | 2.345 | 50.8 µs | 120.3 µs | 302.5 µs | 512.8 µs | 2.665 ms | 13.530 ms |
| tri | 0.7 | 1.3 µs | 1.2 µs | 1.3 µs | 1.2 µs | 1.3 µs | 1.3 µs |
| trigamma | 2.345 | 43.1 µs | 120.4 µs | 197.2 µs | 382.8 µs | 1.704 ms | 11.466 ms † |
| vercos | 0.7 | 2.9 µs | 6.6 µs | 8.9 µs | 17.8 µs † | 57.6 µs | 216.5 µs † |
| versin | 0.7 | 3.1 µs | 5.3 µs | 11.3 µs | 14.2 µs | 40.7 µs | 169.2 µs |
| zatahp | 2.3, 1.4 | 698.6 µs | 1.445 ms | 2.911 ms | 3.868 ms | 25.893 ms | 148.986 ms |
| zeta | 2.345 | 1.049 ms | 4.071 ms | 9.285 ms | 1.389 ms | 5.634 ms | 38.100 ms † |
| zetah | 2.3, 1.4 | 792.0 µs | 1.509 ms | 6.190 ms † | 17.546 ms † | 18.638 ms | 151.420 ms |
| zetap | 2.345 | 256.0 µs | 748.8 µs † | 1.008 ms | 6.088 ms † | 7.380 ms | 47.600 ms |

### `mpc`

| Operation | Inputs | 256 bits | 512 bits | 768 bits | 1024 bits | 2048 bits | 4096 bits |
|---|---|---:|---:|---:|---:|---:|---:|
| E1 | 2.345678+0.125i | 208.2 µs † | 214.5 µs | 330.2 µs | 540.3 µs | 1.771 ms | 8.638 ms |
| Ei | 2.345678+0.125i | 89.1 µs | 190.3 µs | 336.7 µs | 536.4 µs | 1.682 ms | 10.283 ms |
| Li | 2.345+0.125i | 93.3 µs | 254.7 µs | 309.0 µs | 705.7 µs | 2.599 ms | 9.278 ms |
| abs | 0.7+0.125i | 1.5 µs † | 1.4 µs | 1.6 µs | 1.5 µs | 2.9 µs | 8.9 µs |
| acos | 0.7+0.125i | 92.0 µs | 128.8 µs † | 119.0 µs | 79.5 µs | 177.3 µs | 525.6 µs |
| acosec | 2.345+0.125i | 49.6 µs | 72.5 µs | 109.1 µs | 210.2 µs | 364.4 µs | 1.188 ms |
| acosech | 0.7+0.125i | 72.4 µs † | 76.7 µs | 89.5 µs | 197.0 µs | 304.9 µs | 1.184 ms |
| acosh | 2.345+0.125i | 69.7 µs | 82.1 µs | 97.2 µs | 64.4 µs | 266.9 µs | 449.4 µs |
| acot | 0.7+0.125i | 81.1 µs † | 91.7 µs | 106.2 µs | 152.5 µs | 427.8 µs | 892.2 µs |
| acoth | 2.345+0.125i | 132.1 µs † | 62.8 µs | 79.4 µs | 107.2 µs | 230.7 µs | 963.9 µs |
| add | 0.7+0.125i, 0.7+0.125i | 798.6 ns | 440.3 ns | 439.5 ns | 406.0 ns | 459.7 ns | 559.4 ns |
| add_long | 17+0.125i, 3 | 4.2 µs † | 3.4 µs | 2.6 µs | 3.4 µs | 3.2 µs | 2.7 µs |
| appell_f1 | <span style="display:inline-block;width:40ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">0.5+0.125i, 0.7+0.125i, 0.8+0.125i, 2.5+<br/>0.125i, 0.2+0.125i, 0.3+0.125i</span> | 758.5 µs † | 1.460 ms | 2.524 ms | 5.192 ms | 21.524 ms | 127.651 ms |
| arccovercos | 0.5+0.125i | 94.0 µs † | 87.7 µs | 68.9 µs | 145.7 µs | 162.1 µs | 434.2 µs |
| arccoversin | 0.5+0.125i | 80.9 µs | 50.7 µs | 58.1 µs | 187.7 µs | 157.6 µs | 523.8 µs |
| archacovercos | 0.5+0.125i | 16.1 µs † | 19.0 µs | 21.2 µs | 27.1 µs | 44.3 µs | 124.7 µs |
| archacoversin | 0.5+0.125i | 20.3 µs † | 14.3 µs | 22.5 µs | 24.6 µs | 45.8 µs | 115.0 µs |
| archavercos | 0.5+0.125i | 19.8 µs † | 21.1 µs | 21.6 µs | 26.3 µs | 52.1 µs | 146.2 µs |
| archaversin | 0.5+0.125i | 22.9 µs † | 15.9 µs | 23.1 µs | 25.2 µs | 49.1 µs | 124.5 µs |
| arcvercos | 0.5+0.125i | 87.9 µs | 44.4 µs | 129.2 µs | 71.1 µs | 157.1 µs | 523.6 µs |
| arcversin | 0.5+0.125i | 90.8 µs † | 50.0 µs | 111.3 µs | 82.6 µs | 202.7 µs | 435.3 µs |
| arg | 0.7+0.125i | 23.9 µs † | 30.6 µs | 34.3 µs | 49.0 µs | 108.9 µs | 290.4 µs |
| asec | 2.345+0.125i | 72.0 µs † | 70.5 µs | 134.4 µs | 146.3 µs | 333.9 µs | 1.065 ms |
| asech | 0.7+0.125i | 43.7 µs † | 56.6 µs | 65.8 µs | 99.6 µs | 225.5 µs | 505.2 µs |
| asin | 0.7+0.125i | 172.1 µs † | 87.4 µs | 109.8 µs | 135.6 µs | 334.0 µs | 481.8 µs |
| asinh | 0.7+0.125i | 77.0 µs | 72.0 µs | 116.2 µs | 137.2 µs | 348.1 µs | 1.122 ms |
| atan | 0.7+0.125i | 92.0 µs † | 96.5 µs | 91.9 µs | 148.7 µs | 341.1 µs | 859.1 µs |
| atanh | 0.7+0.125i | 41.9 µs | 61.2 µs | 92.5 µs | 126.3 µs | 277.1 µs | 870.5 µs |
| bessel_i | 2, 0.7+0.125i | 298.7 µs † | 475.2 µs | 732.3 µs | 1.199 ms | 4.065 ms | 16.836 ms |
| bessel_j | 2, 0.7+0.125i | 258.1 µs | 548.3 µs † | 870.3 µs | 1.226 ms | 4.295 ms | 16.443 ms |
| bessel_k | 2, 0.7+0.125i | 216.7 µs † | 241.4 µs | 478.8 µs | 738.5 µs | 2.338 ms | 12.875 ms |
| bessel_y | 2, 0.7+0.125i | 406.5 µs † | 412.2 µs | 708.5 µs | 1.097 ms | 4.360 ms | 18.857 ms |
| ceil | 0.7+0.125i | 1.7 µs | 1.5 µs | 1.6 µs | 1.6 µs | 1.7 µs | 1.6 µs |
| chebyshev_Tn | 2, 0.7+0.125i | 12.3 µs | 11.6 µs | 13.1 µs | 11.5 µs | 12.3 µs | 18.6 µs |
| chebyshev_Un | 2, 0.7+0.125i | 14.7 µs | 14.0 µs | 12.2 µs | 16.6 µs | 15.9 µs | 18.5 µs |
| circ | 0.7 | 2.5 µs | 3.0 µs | 2.4 µs | 4.0 µs | 5.5 µs | 8.2 µs |
| clausen | 3, 0.7+0.125i | 229.9 µs † | 371.7 µs | 715.4 µs | 1.184 ms | 4.651 ms | 23.944 ms |
| clausen2 | 0.7+0.125i | 210.3 µs † | 364.9 µs | 648.2 µs | 1.184 ms | 4.947 ms | 23.930 ms |
| conj | 0.7+0.125i | 431.4 ns | 353.8 ns | 310.9 ns | 292.4 ns | 320.1 ns | 344.3 ns |
| cos | 0.7+0.125i | 27.4 µs † | 17.4 µs | 23.6 µs | 35.7 µs | 119.4 µs | 358.3 µs |
| cosec | 0.7+0.125i | 24.4 µs | 17.5 µs | 27.9 µs | 42.0 µs | 107.3 µs | 393.1 µs |
| cosech | 0.7+0.125i | 25.6 µs † | 17.1 µs | 22.8 µs | 97.7 µs | 91.8 µs | 295.8 µs |
| cosh | 0.7+0.125i | 13.5 µs | 14.3 µs | 22.1 µs | 79.1 µs | 84.6 µs | 286.9 µs |
| cot | 0.7+0.125i | 42.7 µs | 38.5 µs | 48.9 µs | 91.1 µs | 222.4 µs | 818.5 µs |
| coth | 0.7+0.125i | 19.6 µs | 31.1 µs | 47.6 µs | 147.8 µs | 203.1 µs | 619.0 µs |
| covercos | 0.7+0.125i | 23.5 µs | 18.1 µs | 33.6 µs | 41.2 µs | 107.9 µs | 354.4 µs |
| coversin | 0.7+0.125i | 20.8 µs | 16.8 µs | 28.0 µs | 50.6 µs | 105.1 µs | 371.8 µs |
| cubrt | 0.7+0.125i | 48.5 µs | 74.3 µs | 88.7 µs | 177.8 µs | 477.2 µs | 1.267 ms |
| digamma | 2.345+0.125i | 281.5 µs | 615.6 µs | 978.4 µs | 1.768 ms | 4.217 ms | 20.915 ms |
| dilog | 0.7+0.125i | 206.6 µs | 448.1 µs | 932.7 µs | 2.250 ms † | 6.381 ms | 25.803 ms |
| div | 0.7+0.125i, 0.7+0.125i | 7.0 µs | 14.5 µs | 14.6 µs | 28.2 µs | 44.8 µs | 104.5 µs |
| erf | 0.7+0.125i | 87.4 µs | 188.7 µs | 312.5 µs | 619.1 µs | 2.006 ms | 8.657 ms |
| erfc | 0.7+0.125i | 80.8 µs | 217.6 µs | 308.6 µs | 693.9 µs | 2.100 ms | 7.949 ms |
| erfcinv | 0.5+0.125i | 764.1 µs | 1.738 ms | 1.913 ms | 3.973 ms | 11.716 ms | 50.151 ms |
| erfinv | 0.5+0.125i | 953.8 µs | 1.685 ms | 2.569 ms | 3.784 ms | 12.417 ms | 47.078 ms |
| exp | 1.23456789+0.125i | 10.9 µs | 21.9 µs | 32.4 µs | 61.1 µs | 177.0 µs | 813.0 µs |
| floor | 0.7+0.125i | 270.8 ns | 312.6 ns | 379.8 ns | 281.1 ns | 327.2 ns | 342.6 ns |
| gamma | 2.345+0.125i | 236.7 µs † | 454.7 µs † | 898.3 µs † | 1.651 ms † | 6.074 ms † | 25.124 ms † |
| gammainc_P | 2.3+0.125i, 1.4+0.125i | 725.5 µs † | 1.439 ms † | 2.522 ms † | 3.612 ms † | 11.988 ms † | 48.070 ms † |
| gammainc_Q | 2.3+0.125i, 1.4+0.125i | 734.1 µs † | 1.767 ms † | 2.622 ms † | 4.192 ms † | 11.122 ms † | 50.357 ms † |
| gammainc_lower | 2.3+0.125i, 1.4+0.125i | 455.5 µs † | 905.2 µs † | 1.525 ms † | 2.606 ms † | 7.079 ms † | 22.421 ms † |
| gammainc_upper | 2.3+0.125i, 1.4+0.125i | 756.9 µs † | 1.420 ms † | 3.278 ms † | 4.077 ms † | 12.761 ms † | 48.500 ms † |
| gammainv | 9.5+0.125i | 4.386 ms † | 7.432 ms † | 13.313 ms † | 18.208 ms † | 81.807 ms † | 272.487 ms † |
| hacovercos | 0.7+0.125i | 25.4 µs | 23.9 µs | 41.5 µs | 40.4 µs | 117.3 µs | 344.2 µs |
| hacoversin | 0.7+0.125i | 23.9 µs | 22.2 µs | 34.9 µs | 43.4 µs | 123.7 µs | 438.6 µs |
| harmonic_poly | 2, 0.7+0.125i | 18.1 µs | 20.4 µs | 17.7 µs | 21.5 µs | 36.5 µs | 80.3 µs |
| havercos | 0.7+0.125i | 29.1 µs | 20.4 µs | 32.5 µs | 46.1 µs | 111.9 µs | 405.0 µs |
| haversin | 0.7+0.125i | 24.7 µs | 24.1 µs | 33.3 µs | 45.2 µs | 113.6 µs | 367.6 µs |
| hermite_Hn | 2, 0.7+0.125i | 13.7 µs | 16.8 µs | 13.6 µs | 13.6 µs | 19.7 µs | 22.0 µs |
| hypergeometric_pFq | 0.7+0.125i, 2.5+0.125i, 0.3+0.125i | 277.0 µs † | 524.0 µs † | 950.0 µs † | 1.526 ms † | 4.071 ms † | 16.523 ms † |
| imag_part | 0.7+0.125i | 144.4 ns | 140.7 ns | 131.5 ns | 132.0 ns | 122.8 ns | 192.9 ns |
| inv | 0.7+0.125i | 1.3 µs | 1.6 µs | 2.1 µs | 2.9 µs | 5.0 µs | 13.3 µs |
| ldexp | 1.23456789+0.125i, 3 | 491.7 ns | 356.5 ns | 330.8 ns | 427.2 ns | 413.7 ns | 338.3 ns |
| legendre_chi | 2, 0.7+0.125i | 163.7 µs | 308.7 µs | 382.9 µs | 696.1 µs | 2.018 ms | 7.355 ms |
| lerch_phi | 0.3+0.125i, 2+0.125i, 1.4+0.125i | 319.8 µs | 861.8 µs | 1.885 ms † | 3.187 ms † | 15.640 ms † | 57.820 ms † |
| lg | 2.345678+0.125i | 26.1 µs | 37.4 µs | 57.1 µs | 93.7 µs | 198.9 µs | 465.2 µs |
| lgamma | 2.345+0.125i | 168.9 µs † | 495.1 µs † | 846.2 µs † | 1.374 ms † | 5.013 ms † | 21.633 ms † |
| ln | 2.345678+0.125i | 20.6 µs | 31.7 µs | 40.2 µs | 57.6 µs | 190.6 µs | 355.4 µs |
| log | 2.345678+0.125i | 24.8 µs | 31.7 µs | 39.5 µs | 65.1 µs | 174.3 µs | 400.6 µs |
| log10 | 2.345678+0.125i | 24.3 µs | 56.0 µs | 55.5 µs | 85.8 µs | 180.7 µs | 572.8 µs |
| lommel_s | 0.5+0.125i, 0.7+0.125i, 1.2+0.125i | 244.6 µs | 432.9 µs | 843.5 µs | 1.334 ms | 7.216 ms † | 21.843 ms |
| mul | 0.7+0.125i, 0.7+0.125i | 1.6 µs | 1.8 µs | 2.0 µs | 3.1 µs | 11.4 µs † | 7.1 µs |
| mul_long | 17+0.125i, 3 | 4.1 µs | 4.1 µs | 3.6 µs | 3.1 µs | 4.9 µs | 5.1 µs |
| mul_pow10 | 1.23456789+0.125i, 3 | 3.4 µs | 3.2 µs | 4.3 µs | 3.1 µs | 4.0 µs | 4.1 µs |
| neg | 0.7+0.125i | 343.0 ns | 288.0 ns | 370.4 ns | 362.3 ns | 354.6 ns | 319.5 ns |
| normal_cdf | 0.7+0.125i | 89.3 µs | 196.5 µs | 408.9 µs | 547.6 µs | 2.271 ms | 8.576 ms |
| normal_logpdf | 0.7+0.125i | 7.3 µs | 13.6 µs | 20.6 µs | 18.5 µs | 40.4 µs | 122.4 µs |
| normal_pdf | 0.7+0.125i | 21.0 µs | 32.0 µs | 55.5 µs | 102.3 µs | 250.9 µs | 940.1 µs |
| polygamma | 3, 2.345+0.125i | 326.8 µs | 748.2 µs | 1.267 ms | 2.267 ms | 8.305 ms | 30.160 ms |
| polylog | 2, 0.7+0.125i | 171.3 µs | 408.3 µs | 927.0 µs | 1.652 ms | 6.076 ms | 26.772 ms |
| polylog1 | 0.7+0.125i | 29.9 µs | 41.0 µs | 70.6 µs | 106.1 µs | 165.7 µs | 434.4 µs |
| pow | 1.23456789+0.125i, 3.5+0.125i | 32.6 µs | 65.4 µs | 90.2 µs | 132.2 µs | 345.1 µs | 1.422 ms |
| pow10 | 3 | 11.5 µs | 18.3 µs | 25.1 µs | 38.4 µs | 87.7 µs † | 189.5 µs |
| pow_int | 1.23456789+0.125i, 3 | 6.8 µs | 7.7 µs | 11.7 µs | 12.0 µs | 12.8 µs | 23.7 µs |
| real_part | 0.7+0.125i | 142.2 ns | 132.3 ns | 147.7 ns | 131.2 ns | 154.6 ns | 188.2 ns |
| rect | 0.7 | 2.6 µs | 3.0 µs | 4.0 µs | 4.0 µs | 4.1 µs | 8.7 µs |
| root | 1.2345+0.125i, 3 | 41.7 µs | 57.1 µs | 186.8 µs | 279.6 µs | 382.0 µs | 1.427 ms |
| round | 0.7+0.125i | 10.3 µs | 11.4 µs | 9.0 µs | 10.1 µs | 10.3 µs | 12.7 µs |
| roundn | 1.23456789+0.125i, 3 | 19.2 µs | 14.5 µs | 18.3 µs | 19.4 µs | 22.8 µs | 34.6 µs |
| rounds | 1.23456789+0.125i, 3 | 43.1 µs | 67.0 µs | 81.6 µs | 88.2 µs | 211.1 µs | 550.8 µs |
| sec | 0.7+0.125i | 19.5 µs | 20.2 µs | 27.4 µs | 34.4 µs | 121.6 µs | 342.2 µs |
| sech | 0.7+0.125i | 14.0 µs | 15.2 µs | 24.8 µs | 85.9 µs | 89.6 µs | 292.0 µs |
| sgn | 0.7 | 788.8 ns | 784.3 ns | 776.7 ns | 969.4 ns | 815.0 ns | 922.0 ns |
| sin | 0.567+0.125i | 17.2 µs | 17.5 µs | 25.9 µs | 39.0 µs | 110.1 µs | 418.2 µs |
| sinc | 0.7+0.125i | 20.9 µs | 30.5 µs | 43.1 µs | 50.0 µs | 129.2 µs | 424.7 µs |
| sinh | 0.7+0.125i | 8.1 µs | 15.8 µs | 28.5 µs | 84.5 µs | 83.6 µs | 281.8 µs |
| sqr | 0.7+0.125i | 1.8 µs | 2.0 µs | 2.2 µs | 3.0 µs | 4.7 µs | 10.6 µs † |
| sqrt | 1.23456789+0.125i | 2.9 µs | 2.5 µs | 3.7 µs | 5.5 µs | 6.7 µs | 15.2 µs |
| step | 0.7 | 820.5 ns | 838.2 ns | 878.9 ns | 954.4 ns | 1.1 µs | 793.2 ns |
| struve_h | 2, 0.7+0.125i | 265.6 µs | 686.6 µs | 945.4 µs | 1.584 ms | 6.746 ms | 21.221 ms |
| struve_l | 2, 0.7+0.125i | 316.5 µs | 589.6 µs | 1.353 ms | 1.687 ms | 5.180 ms | 21.095 ms |
| sub | 0.7+0.125i, 0.7+0.125i | 420.3 ns | 450.0 ns | 371.5 ns | 432.6 ns | 400.7 ns | 571.3 ns |
| tan | 0.7+0.125i | 15.2 µs | 57.3 µs † | 29.2 µs | 47.6 µs | 122.8 µs | 384.9 µs |
| tanh | 0.7+0.125i | 33.4 µs | 23.9 µs | 83.0 µs | 41.5 µs | 99.3 µs | 320.4 µs |
| tetragamma | 2.345+0.125i | 364.9 µs | 792.6 µs | 1.250 ms | 1.997 ms | 6.922 ms | 30.391 ms |
| tri | 0.7 | 2.3 µs | 2.8 µs | 2.8 µs | 2.8 µs | 5.0 µs | 8.0 µs |
| trigamma | 2.345+0.125i | 310.3 µs | 537.4 µs | 1.174 ms | 1.727 ms | 5.537 ms | 27.328 ms |
| vercos | 0.7+0.125i | 17.2 µs | 17.2 µs | 30.2 µs | 40.2 µs | 109.2 µs | 317.7 µs |
| versin | 0.7+0.125i | 22.2 µs | 23.8 µs | 26.4 µs | 46.1 µs | 106.4 µs | 364.4 µs |
| zatahp | 2.3+0.125i, 1.4+0.125i | 2.762 ms | 4.732 ms | 11.396 ms | 14.287 ms | 89.465 ms | 470.201 ms |
| zeta | 2.345+0.125i | 639.8 µs | 1.658 ms † | 2.421 ms | 4.017 ms | 19.855 ms | 96.722 ms |
| zetah | 2.3+0.125i, 1.4+0.125i | 2.280 ms | 15.138 ms † | 9.850 ms | 15.878 ms | 83.732 ms | 436.983 ms |
| zetap | 2.345+0.125i | 988.5 µs † | 1.706 ms | 2.974 ms | 4.932 ms | 26.077 ms | 120.905 ms |

### number / unrestricted integer

| Operation | Inputs | Exact |
|---|---|---:|
| abs | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">-9712853409152031141207811585727780469484705121206232579607434236969009533618825<br/>97201918971499157056640605432269691427807258223374967278846596194834524005116336<br/>17514882293623272276764857474830401626415841035971458657442133370777607803464071<br/>101158385124186229995073045427448918227959893082282919863982307657776</span> | 171.1 ns |
| add | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 121.0 ns |
| bit_and | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 424.5 ns |
| bit_length | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776</span> | 186.7 ns |
| bit_not | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776</span> | 358.2 ns |
| bit_or | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 362.0 ns |
| bit_xor | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 509.2 ns |
| clear_bit | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>701</span> | 548.0 ns |
| div | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>340282366920938463463374607431768211454</span> | 1.6 µs |
| divmod | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 482.7 ns |
| factorial | 5000 | 162.2 µs |
| factors | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">54772119019154636339867862041163448790974484131459861520687327809209914339836012<br/>52275954799541486775711126005967382600118182797233708133775335788335658037065081<br/>31569062299976033041302189833215742992653063887339218309467131548996199881113600<br/>00000000000000000000000000000000000000000000000000000000000000000000000000000000<br/>0000000000000000000000000000000000000000000000</span> | 135.0 µs |
| fibonacci | 10000 | 7.7 µs |
| gcd | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 3.0 µs |
| gcdext | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 4.8 µs |
| is_prime | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">68647976601306097149819007990813932172694353001433054093944634591855431833976560<br/>52122559640661454554977296311391480858037121987999716643812574028291115057151</span> | 549.4 µs |
| isqrt | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776</span> | 580.6 ns |
| lcm | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 3.2 µs |
| mod | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 467.3 ns |
| modinv | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>68647976601306097149819007990813932172694353001433054093944634591855431833976560<br/>52122559640661454554977296311391480858037121987999716643812574028291115057151</span> | 5.8 µs † |
| mul | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 318.1 ns |
| neg | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776</span> | 99.9 ns |
| next_prime | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 5.058 ms |
| partition | 5000 | 7.885 ms |
| pow | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>5</span> | 6.4 µs † |
| pow_int | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>5</span> | 3.1 µs |
| powmod | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662, <br/>68647976601306097149819007990813932172694353001433054093944634591855431833976560<br/>52122559640661454554977296311391480858037121987999716643812574028291115057151</span> | 151.1 µs † |
| prev_prime | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 8.777 ms † |
| prove_prime | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">68647976601306097149819007990813932172694353001433054093944634591855431833976560<br/>52122559640661454554977296311391480858037121987999716643812574028291115057151</span> | 4.657 ms |
| set_bit | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>700</span> | 441.6 ns |
| shl | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>257</span> | 575.3 ns |
| shr | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>257</span> | 430.5 ns |
| sqr | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776</span> | 290.8 ns |
| sub | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 109.0 ns |
| test_bit | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>701</span> | 433.3 ns |

### number / unrestricted fraction

| Operation | Inputs | Exact |
|---|---|---:|
| abs | $\frac{\begin{array}{l}\text{-9712853409152031141207811585727780469484705121206232579607434236969009533618825} \\ \text{97201918971499157056640605432269691427807258223374967278846596194834524005116336} \\ \text{17514882293623272276764857474830401626415841035971458657442133370777607803464071} \\ \text{101158385124186229995073045427448918227959893082282919863982307657776}\end{array}}{\begin{array}{l}\text{68647976601306097149819007990813932172694353001433054093944634591855431833976560} \\ \text{52122559640661454554977296311391480858037121987999716643812574028291115057151}\end{array}}$ | 350.3 ns |
| add | **Input 1**<br/>$\frac{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}{\begin{array}{l}\text{68647976601306097149819007990813932172694353001433054093944634591855431833976560} \\ \text{52122559640661454554977296311391480858037121987999716643812574028291115057151}\end{array}}$<br/><br/>**Input 2**<br/>$\frac{\begin{array}{l}\text{12498846048367045926370716602609982839721641429222959125376462582674869403443996} \\ \text{972648628699047686839582654905384579341977328653591547736951147466435189662}\end{array}}{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}$ | 5.2 µs † |
| div | **Input 1**<br/>$\frac{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}{\begin{array}{l}\text{68647976601306097149819007990813932172694353001433054093944634591855431833976560} \\ \text{52122559640661454554977296311391480858037121987999716643812574028291115057151}\end{array}}$<br/><br/>**Input 2**<br/>$\frac{\begin{array}{l}\text{12498846048367045926370716602609982839721641429222959125376462582674869403443996} \\ \text{972648628699047686839582654905384579341977328653591547736951147466435189662}\end{array}}{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}$ | 10.0 µs † |
| inv | $\frac{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}{\begin{array}{l}\text{68647976601306097149819007990813932172694353001433054093944634591855431833976560} \\ \text{52122559640661454554977296311391480858037121987999716643812574028291115057151}\end{array}}$ | 239.2 ns |
| mul | **Input 1**<br/>$\frac{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}{\begin{array}{l}\text{68647976601306097149819007990813932172694353001433054093944634591855431833976560} \\ \text{52122559640661454554977296311391480858037121987999716643812574028291115057151}\end{array}}$<br/><br/>**Input 2**<br/>$\frac{\begin{array}{l}\text{12498846048367045926370716602609982839721641429222959125376462582674869403443996} \\ \text{972648628699047686839582654905384579341977328653591547736951147466435189662}\end{array}}{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}$ | 4.0 µs |
| neg | $\frac{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}{\begin{array}{l}\text{68647976601306097149819007990813932172694353001433054093944634591855431833976560} \\ \text{52122559640661454554977296311391480858037121987999716643812574028291115057151}\end{array}}$ | 196.1 ns |
| pow | **Input 1**<br/>$\frac{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}{\begin{array}{l}\text{68647976601306097149819007990813932172694353001433054093944634591855431833976560} \\ \text{52122559640661454554977296311391480858037121987999716643812574028291115057151}\end{array}}$<br/><br/>**Input 2**<br/>5 | 41.4 µs |
| pow_int | **Input 1**<br/>$\frac{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}{\begin{array}{l}\text{68647976601306097149819007990813932172694353001433054093944634591855431833976560} \\ \text{52122559640661454554977296311391480858037121987999716643812574028291115057151}\end{array}}$<br/><br/>**Input 2**<br/>5 | 42.6 µs † |
| sqr | $\frac{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}{\begin{array}{l}\text{68647976601306097149819007990813932172694353001433054093944634591855431833976560} \\ \text{52122559640661454554977296311391480858037121987999716643812574028291115057151}\end{array}}$ | 682.2 ns |
| sub | **Input 1**<br/>$\frac{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}{\begin{array}{l}\text{68647976601306097149819007990813932172694353001433054093944634591855431833976560} \\ \text{52122559640661454554977296311391480858037121987999716643812574028291115057151}\end{array}}$<br/><br/>**Input 2**<br/>$\frac{\begin{array}{l}\text{12498846048367045926370716602609982839721641429222959125376462582674869403443996} \\ \text{972648628699047686839582654905384579341977328653591547736951147466435189662}\end{array}}{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}$ | 5.8 µs |

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

<style data-mars-formula-preview>
/* Formula placement belongs to the preview container, not to the alignment
 * of columns inside a TeX array. Apply this to every Markdown guide. */
body .katex-display {
    margin: 1em 0;
    padding-left: 1em;
    text-align: left;
    overflow-x: auto;
    overflow-y: hidden;
}

body .katex-display > .katex {
    text-align: left;
    white-space: nowrap;
}

/* Standalone inline formulae already contain their small TeX indentation.
 * Do not add padding here or prevent ordinary prose from wrapping. */
body p:has(> .katex) {
    text-align: left;
}

/* Keep benchmark fraction digits readable without wrapping math in raw HTML. */
body td > .katex:has(.mfrac) {
    display: inline-block;
    font-size: 1em;
    margin: 0.6em 0;
}
</style>
