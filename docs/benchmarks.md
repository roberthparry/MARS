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
Parenthesised qualifiers in the tables describe parameter-specific workloads
of the same public function. The internal keys used by `--operation` filters
and the benchmark database remain separate from these display labels.
The `zatahp` (integer order) workload (`--operation zatahp_integral`) checks the
Hurwitz derivative at fixed integral order -4; its complex profile varies only the offset. It calls the
ordinary `num_zatahp` API, providing a control for integer-order performance.
The `lerch_phi` (negative order) workload (`--operation lerch_phi_negative_order`)
calls `num_lerch_phi` with base operands
`z = 0.3125`, `s = -15` and `a = 0.375`, checking the growth allowance in precision
planning. Its changing-input profile perturbs all three operands, including the order,
so its real samples exercise non-integral orders rather than integer-power shortcuts.
The matching `lerch_phi` (integer order) workload (`--operation lerch_phi_integer_order`)
keeps order -15 real in both backends; its changing-input protocol varies only the argument and offset. This
separately measures the finite rational kernel at changing inputs. Both workloads
call `num_lerch_phi`; its parameters select the appropriate algorithm automatically.
Complex profiles skip operations restricted to real integers. Integer-only APIs
run once with exact operands in the **number / unrestricted integer** (`exact-integer`)
profile; an arbitrary floating precision would be meaningless for those algorithms.
This profile also measures addition, subtraction, multiplication, exact division,
negation, absolute value, squaring and positive integer powers. The
**number / unrestricted fraction** (`exact-fraction`) profile measures the same
arithmetic and reciprocals using exact rational operands. These profiles have zero
precision bits in storage because their results are exact, rather than rounded to a
floating-point precision.

Integer workloads use a dense 1024-bit dividend, a dense 512-bit divisor and
exponent, and the 521-bit Mersenne prime 2⁵²¹ − 1. The dividend and divisor share
the non-trivial 128-bit GCD 2·(2¹²⁷ − 1). Bit operations address positions 700 and
701 or shift by 257 bits. Factorisation uses the 1215-bit smooth composite
2⁵¹²·3²⁵⁶·5¹²⁸, exercising large integers without an unbounded prime search.
Factorial and Fibonacci accept native integer indices by API design: the cases
5,000! and F(10,000) produce exact 54233-bit and 6942-bit results. The partition
case is p(5,000). Input parsing and exact-integer result checks are outside timing.
Saved arguments distinguish these workloads from earlier small-input timings.
Integer division uses the dividend's 128-bit common divisor so its result remains
an integer. The fraction operands are the 1024-bit dividend over the 521-bit prime
and the 512-bit divisor over the 1024-bit dividend; canonical reduction precedes
timing. The rational working-size budget is 16384 bits, sufficient for these
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
The catalogue includes the MARS-defined `lambert_S`, `lambert_S_constant_base`,
`lambert_S_finite` and `lambert_S_unit_circle` workloads, reported as `lambert_S`,
`lambert_S (constant base)`, `lambert_S (finite section)` and `lambert_S (unit-circle base)`. The last
workload takes a real phase `x = 0.001` and evaluates the real component of
`lambert_S(-1,exp(2i*x),1,1,-1,1000)`, including construction of the complex
base. It keeps that phase real in every backend and in changing-input runs;
the additional catalogue keys describe workloads, not public functions.
The constant-base workload uses `(-1,1,1,1,-1,1000)` and keeps `q=1` fixed;
its complex profiles perturb only the amplitude and starting coordinate.
Their [definition](expression.md#mars-defined-lambert-sum) and
[recorded timings](number.md#mars-defined-lambert-sum-timings) identify the convention explicitly.
For example, `make bench-db ARGS='stats --operation zeta' | less -R` displays only
zeta measurements from the latest run. Add `--run ID` to inspect a particular saved run;
add `--backend mpc --bits 4096` to select its 4096-bit complex measurements.
The equivalent `run --operation zeta` command measures only that function.
For `run`, `--operation` also accepts comma-separated names, with duplicates measured
once. Each selected operation remains a separate case with its own stored operands.
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

`run --mode warm` selects that default warmed protocol. Two additional modes
separate cache effects from calculation speed:

- `--mode cold` measures one call per sample in a child forked from a worker
  which has prepared operands but has never evaluated the operation. Forking,
  parsing, result checks and disposal remain outside the timer. These are cold
  mathematical calls; executable loading and application startup are excluded.
- `--mode changing` measures one call per sample without warm-up. Before sample
  ordinal zero, eligible real parts increase by exactly 1/128; each subsequent
  sample increases them again by 1/128. The stored arguments are the original
  base values. The complex-argument mask identifies eligible operands when
  present; otherwise every operand changes. Number-double, qfloat, qcomplex, MPFR and MPC
  profiles of non-integer-only operations support this protocol; unrestricted integer and
  fraction profiles remain excluded. Fixed complex profiles retain their imaginary parts.
  Domain failures are recorded.

The complete timing protocol is stored in `benchmark_run.clock`. Cold, changing
and warmed observations never share a timing baseline. Resume restores the saved
mode and operands; an unrecognised protocol cannot be resumed.

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
Benchmark run **278**, 2026-11-23T16:28:52.642Z (UTC).

Machine: Intel(R) N95.

System: Linux 7.0.0-27-generic x86_64.

Compiler: 15.2.0; build flags: gcc -Wall -Wextra -Werror -O2 -fPIC -std=gnu17 -D_GNU_SOURCE -pthread -DHAVE_UNISTRING -I/usr/include/sqlcipher -DSQLITE_HAS_CODEC   -I/usr/include/x86_64-linux-gnu -isystem /usr/include/mit-krb5 -isystem /usr/include/mit-krb5 -I/usr/include/p11-kit-1.

Median per call from 7 timed batches.

Timing method: CLOCK_MONOTONIC; warmup=100ms; target_batch=5ms; max_batch=1048576; storage_chunk=512.

Argument parsing, output validation, disposal and database writes are outside timing.
Timings use spaces for thousands grouping and a decimal point; bit counts are ungrouped.
Inputs show the actual operands, including +0.125i for permitted complex arguments; integer orders remain real.
Parenthesised qualifiers describe parameter-specific workloads of the same public function.
`number-double` includes generic number dispatch; the other fixed profiles call qfloat/qcomplex directly.
Requested multiprecision bits do not guarantee that every algorithm achieves that accuracy.
Operations and precision columns with only unsupported results are omitted; — denotes an unsupported cell.
Failures and timeouts remain visible. † marks a retained slowdown outlier.

### `number-double`

| Operation | Inputs | 53 bits |
|---|---|---:|
| E1 | 2.345678 | 7.0 µs |
| Ei | 2.345678 | 8.6 µs |
| Li | 2.345 | 5.9 µs |
| abs | 0.7 | 7.9 ns |
| acos | 0.7 | 21.1 ns |
| acosec | 2.345 | 17.8 ns |
| acosech | 0.7 | 53.9 ns |
| acosh | 2.345 | 27.6 ns |
| acot | 0.7 | 48.8 ns |
| acoth | 2.345 | 27.3 ns |
| add | 0.7, 0.7 | 14.5 ns |
| add_long | 17, 3 | 474.3 ns |
| appell_F1 | 0.5, 0.7, 0.8, 2.5, 0.2, 0.3 | 23.3 µs |
| appell_F2 | 0.5, 0.7, 0.8, 2.5, 2.6, 0.125, 0.0625 | 505.8 µs |
| appell_F3 | 0.5, 0.6, 0.7, 0.8, 2.5, 0.2, 0.3 | 892.2 µs |
| appell_F4 | 0.5, 0.7, 2.5, 2.6, 0.025, 0.04 | 294.9 µs |
| arccovercos | 0.5 | 103.0 ns |
| arccoversin | 0.5 | 90.2 ns |
| archacovercos | 0.5 | 199.3 ns |
| archacoversin | 0.5 | 227.1 ns |
| archavercos | 0.5 | 189.5 ns |
| archaversin | 0.5 | 201.2 ns |
| arcvercos | 0.5 | 101.9 ns |
| arcversin | 0.5 | 97.5 ns |
| arg | 0.7 | 73.6 ns |
| asec | 2.345 | 15.8 ns |
| asech | 0.7 | 29.3 ns |
| asin | 0.7 | 16.6 ns |
| asinh | 0.7 | 47.7 ns |
| atan | 0.7 | 27.7 ns |
| atan2 | 0.5, -0.75 | 47.0 ns |
| atanh | 0.7 | 24.1 ns |
| bessel_I | 2, 0.7 | 12.0 µs |
| bessel_J | 2, 0.7 | 6.4 µs |
| bessel_K | 2, 0.7 | 21.6 µs |
| bessel_Y | 2, 0.7 | 18.6 µs |
| beta | 2.3, 4.5 | 129.8 ns |
| beta_pdf | 0.5, 2.5, 3.5 | 152.4 ns |
| binomial | 5, 2 | 196.4 ns |
| ceil | 0.7 | 57.6 ns |
| chebyshev_Tn | 2, 0.7 | 721.3 ns |
| chebyshev_Un | 2, 0.7 | 1.0 µs |
| circ | 0.7 | 252.7 ns |
| clausen | 3, 0.7 | 7.2 µs |
| clausen2 | 0.7 | 8.2 µs |
| conj | 0.7 | 7.6 ns |
| cos | 0.7 | 28.2 ns |
| cosec | 0.7 | 31.8 ns |
| cosech | 0.7 | 52.2 ns |
| cosh | 0.7 | 33.5 ns |
| cot | 0.7 | 43.6 ns |
| coth | 0.7 | 56.8 ns |
| covercos | 0.7 | 104.3 ns |
| coversin | 0.7 | 115.5 ns |
| cubrt | 0.7 | 7.4 µs |
| digamma | 2.345 | 2.5 µs |
| dilog | 0.7 | 13.2 µs |
| div | 0.7, 0.7 | 10.2 ns |
| erf | 0.7 | 71.0 ns |
| erfc | 0.7 | 61.2 ns |
| erfcinv | 0.5 | 289.1 ns |
| erfinv | 0.5 | 276.9 ns |
| exp | 1.23456789 | 37.2 ns |
| floor | 0.7 | 7.9 ns |
| gamma | 2.345 | 94.1 ns |
| gammainc_P | 2.3, 1.4 | 12.3 µs |
| gammainc_Q | 2.3, 1.4 | 11.6 µs |
| gammainc_lower | 2.3, 1.4 | 13.2 µs |
| gammainc_upper | 2.3, 1.4 | 15.6 µs |
| gammainv | 9.5 | 90.9 µs |
| hacovercos | 0.7 | 224.4 ns |
| hacoversin | 0.7 | 208.0 ns |
| harmonic_poly | 2, 0.7 | 1.4 µs |
| havercos | 0.7 | 209.6 ns |
| haversin | 0.7 | 197.7 ns |
| hermite_Hn | 2, 0.7 | 1.8 µs |
| hypergeometric_pFq | 0.7, 2.5, 0.3 | 10.9 µs |
| hypot | 0.7, 1.2 | 20.7 ns |
| imag_part | 0.7 | 21.5 ns |
| inv | 0.7 | 7.2 ns |
| lambert_S | 0.5, 0.25, 1.25, 1, -1, inf | 19.4 µs |
| lambert_S (constant base) | -1, 1, 1, 1, -1, 1000 | 2.9 µs |
| lambert_S (finite section) | -1, 0.7, 1, 1, -1, 1000 | 59.4 µs |
| lambert_S (unit-circle base) | 0.001 | 2.139 ms |
| lambert_W0 | 0.7 | 15.9 µs |
| lambert_Wm1 | -0.2 | 13.9 µs |
| lambert_Wn | 1, 0.7 | 26.0 µs |
| lauricella_F | 0.5, 0.3, 2.5 | 17.4 µs |
| ldexp | 1.23456789, 3 | 38.1 ns |
| legendre_chi | 2, 0.7 | 28.5 µs |
| lerch_phi | 0.3, 2, 1.4 | 41.9 µs |
| lerch_phi (integer order) | 0.3125, -15, 0.375 | 23.3 µs |
| lerch_phi (negative order) | 0.3125, -15, 0.375 | 22.6 µs |
| lg | 2.345678 | 21.5 ns |
| lgamma | 2.345 | 53.5 ns |
| ln | 2.345678 | 15.5 ns |
| log | 2.345678 | 12.4 ns |
| log10 | 2.345678 | 19.4 ns |
| logbeta | 2.3, 4.5 | 117.1 ns |
| logbeta_pdf | 0.5, 2.5, 3.5 | 138.5 ns |
| lommel_S | 0.5, 0.7, 1.2 | 6.5 µs |
| mul | 0.7, 0.7 | 13.6 ns |
| mul_long | 17, 3 | 498.6 ns |
| mul_pow10 | 1.23456789, 3 | 57.3 ns |
| neg | 0.7 | 7.6 ns |
| normal_cdf | 0.7 | 64.9 ns |
| normal_logpdf | 0.7 | 13.8 ns |
| normal_pdf | 0.7 | 22.1 ns |
| polygamma | 3, 2.345 | 20.8 µs |
| polylog | 2, 0.7 | 13.4 µs |
| polylog1 | 0.7 | 110.5 ns |
| pow | 1.23456789, 3.5 | 30.4 ns |
| pow10 | 3 | 164.3 ns |
| pow_int | 1.23456789, 3 | 60.2 ns |
| productlog | 0.7 | 14.7 µs |
| qdigamma | 0.5, 2.3 | 24.4 µs |
| real_part | 0.7 | 20.5 ns |
| rect | 0.7 | 259.2 ns |
| root | 1.2345, 3 | 6.8 µs |
| round | 0.7 | 376.7 ns |
| roundn | 1.23456789, 3 | 3.8 µs |
| rounds | 1.23456789, 3 | 6.1 µs |
| sec | 0.7 | 27.1 ns |
| sech | 0.7 | 31.4 ns |
| sgn | 0.7 | 110.9 ns |
| sin | 0.567 | 32.7 ns |
| sinc | 0.7 | 3.1 µs |
| sincos | 0.7 | 567.5 ns |
| sinh | 0.7 | 50.1 ns |
| sinhcosh | 0.7 | 109.3 ns |
| sqr | 0.7 | 8.1 ns |
| sqrt | 1.23456789 | 8.0 ns |
| step | 0.7 | 104.0 ns |
| struve_H | 2, 0.7 | 11.8 µs |
| struve_L | 2, 0.7 | 12.2 µs |
| sub | 0.7, 0.7 | 10.1 ns |
| tan | 0.7 | 5.5 µs |
| tanh | 0.7 | 55.3 ns |
| tetragamma | 2.345 | 3.5 µs |
| tri | 0.7 | 219.5 ns |
| trigamma | 2.345 | 2.5 µs |
| vercos | 0.7 | 105.0 ns |
| versin | 0.7 | 112.2 ns |
| zatahp | 2.3, 1.4 | 40.5 µs |
| zatahp (integer order) | -4, 0.75 | 35.2 µs |
| zeta | 2.345 | 66.7 µs |
| zeta_pair | 2.345 | 168.5 µs |
| zetah | 2.3, 1.4 | 27.7 µs |
| zetap | 2.345 | 117.9 µs |

### `qfloat`

| Operation | Inputs | 106 bits |
|---|---|---:|
| E1 | 2.345678 | 8.5 µs |
| Ei | 2.345678 | 8.1 µs |
| Li | 2.345 | 9.4 µs |
| abs | 0.7 | 2.5 ns |
| acos | 0.7 | 1.6 µs |
| acosec | 2.345 | 1.7 µs |
| acosech | 0.7 | 3.3 µs |
| acosh | 2.345 | 3.5 µs |
| acot | 0.7 | 1.5 µs |
| acoth | 2.345 | 2.9 µs |
| add | 0.7, 0.7 | 6.0 ns |
| appell_F1 | 0.5, 0.7, 0.8, 2.5, 0.2, 0.3 | 80.9 µs |
| appell_F2 | 0.5, 0.7, 0.8, 2.5, 2.6, 0.125, 0.0625 | 495.3 µs |
| appell_F3 | 0.5, 0.6, 0.7, 0.8, 2.5, 0.2, 0.3 | 837.8 µs |
| appell_F4 | 0.5, 0.7, 2.5, 2.6, 0.025, 0.04 | 315.2 µs |
| arccovercos | 0.5 | 1.6 µs |
| arccoversin | 0.5 | 1.5 µs |
| archacovercos | 0.5 | 27.6 ns |
| archacoversin | 0.5 | 25.2 ns |
| archavercos | 0.5 | 51.6 ns |
| archaversin | 0.5 | 49.9 ns |
| arcvercos | 0.5 | 1.8 µs |
| arcversin | 0.5 | 1.6 µs |
| asec | 2.345 | 1.7 µs |
| asech | 0.7 | 3.5 µs |
| asin | 0.7 | 1.7 µs |
| asinh | 0.7 | 3.1 µs |
| atan | 0.7 | 1.1 µs |
| atan2 | 0.5, -0.75 | 1.2 µs |
| atanh | 0.7 | 2.8 µs |
| bessel_I | 2, 0.7 | 16.1 µs |
| bessel_J | 2, 0.7 | 6.2 µs |
| bessel_K | 2, 0.7 | 27.7 µs |
| bessel_Y | 2, 0.7 | 24.1 µs |
| beta | 2.3, 4.5 | 5.5 µs |
| beta_pdf | 0.5, 2.5, 3.5 | 34.7 µs |
| binomial | 5, 2 | 27.9 µs |
| chebyshev_Tn | 2, 0.7 | 35.2 ns |
| chebyshev_Un | 2, 0.7 | 37.0 ns |
| circ | 0.7 | 13.6 ns |
| clausen | 3, 0.7 | 295.8 µs |
| clausen2 | 0.7 | 5.2 µs |
| cos | 0.7 | 922.1 ns |
| cosec | 0.7 | 847.6 ns |
| cosech | 0.7 | 3.2 µs |
| cosh | 0.7 | 3.0 µs |
| cot | 0.7 | 1.8 µs |
| coth | 0.7 | 6.1 µs |
| covercos | 0.7 | 826.2 ns |
| coversin | 0.7 | 790.6 ns |
| cubrt | 0.7 | 4.7 µs |
| digamma | 2.345 | 2.3 µs |
| dilog | 0.7 | 13.1 µs |
| div | 0.7, 0.7 | 28.6 ns |
| erf | 0.7 | 3.6 µs |
| erfc | 0.7 | 3.4 µs |
| erfcinv | 0.5 | 218.0 µs |
| erfinv | 0.5 | 200.2 µs |
| exp | 1.23456789 | 1.6 µs |
| floor | 0.7 | 15.4 ns |
| gamma | 2.345 | 1.6 µs |
| gammainc_P | 2.3, 1.4 | 11.7 µs |
| gammainc_Q | 2.3, 1.4 | 11.9 µs |
| gammainc_lower | 2.3, 1.4 | 13.2 µs |
| gammainc_upper | 2.3, 1.4 | 13.4 µs |
| gammainv | 9.5 | 102.5 µs |
| hacovercos | 0.7 | 848.2 ns |
| hacoversin | 0.7 | 832.8 ns |
| harmonic_poly | 2, 0.7 | 109.2 ns |
| havercos | 0.7 | 827.2 ns |
| haversin | 0.7 | 869.2 ns |
| hermite_Hn | 2, 0.7 | 45.3 ns |
| hypergeometric_pFq | 0.7, 2.5, 0.3 | 3.8 µs |
| hypot | 0.7, 1.2 | 418.3 ns |
| lambert_S | 0.5, 0.25, 1.25, 1, -1, inf | 17.4 µs |
| lambert_S (constant base) | -1, 1, 1, 1, -1, 1000 | 339.2 ns |
| lambert_S (finite section) | -1, 0.7, 1, 1, -1, 1000 | 61.5 µs |
| lambert_S (unit-circle base) | 0.001 | 396.6 µs |
| lambert_W0 | 0.7 | 84.6 µs |
| lambert_Wm1 | -0.2 | 16.7 µs |
| lauricella_F | 0.5, 0.3, 2.5 | 16.8 µs |
| ldexp | 1.23456789, 3 | 9.7 ns |
| legendre_chi | 2, 0.7 | 25.3 µs |
| lerch_phi | 0.3, 2, 1.4 | 8.0 µs |
| lerch_phi (integer order) | 0.3125, -15, 0.375 | 27.8 µs |
| lerch_phi (negative order) | 0.3125, -15, 0.375 | 26.9 µs |
| lg | 2.345678 | 2.9 µs |
| lgamma | 2.345 | 4.8 µs |
| ln | 2.345678 | 2.9 µs |
| log | 2.345678 | 2.9 µs |
| log10 | 2.345678 | 2.9 µs |
| logbeta | 2.3, 4.5 | 26.1 µs |
| logbeta_pdf | 0.5, 2.5, 3.5 | 29.1 µs |
| lommel_S | 0.5, 0.7, 1.2 | 6.8 µs |
| mul | 0.7, 0.7 | 6.8 ns |
| mul_pow10 | 1.23456789, 3 | 37.2 ns |
| neg | 0.7 | 1.9 ns |
| normal_cdf | 0.7 | 3.3 µs |
| normal_logpdf | 0.7 | 26.2 ns |
| normal_pdf | 0.7 | 1.6 µs |
| polygamma | 3, 2.345 | 4.3 µs |
| polylog | 2, 0.7 | 15.5 µs |
| polylog1 | 0.7 | 2.7 µs |
| pow | 1.23456789, 3.5 | 4.3 µs |
| pow10 | 3 | 26.2 ns |
| pow_int | 1.23456789, 3 | 43.2 ns |
| productlog | 0.7 | 95.7 µs |
| qdigamma | 0.5, 2.3 | 23.7 µs |
| rect | 0.7 | 14.4 ns |
| root | 1.2345, 3 | 4.6 µs |
| round | 0.7 | 36.9 ns |
| roundn | 1.23456789, 3 | 6.1 µs |
| rounds | 1.23456789, 3 | 14.7 µs |
| sec | 0.7 | 829.3 ns |
| sech | 0.7 | 3.4 µs |
| sgn | 0.7 | 6.1 ns |
| sin | 0.567 | 799.4 ns |
| sinc | 0.7 | 1.0 µs |
| sinh | 0.7 | 3.5 µs |
| sqr | 0.7 | 7.2 ns |
| sqrt | 1.23456789 | 312.1 ns |
| step | 0.7 | 7.3 ns |
| struve_H | 2, 0.7 | 17.7 µs |
| struve_L | 2, 0.7 | 19.0 µs |
| sub | 0.7, 0.7 | 6.9 ns |
| tan | 0.7 | 1.8 µs |
| tanh | 0.7 | 1.6 µs |
| tetragamma | 2.345 | 3.3 µs |
| tri | 0.7 | 12.4 ns |
| trigamma | 2.345 | 2.5 µs |
| vercos | 0.7 | 872.4 ns |
| versin | 0.7 | 859.3 ns |
| zatahp | 2.3, 1.4 | 282.4 µs |
| zatahp (integer order) | -4, 0.75 | 312.4 µs |
| zeta | 2.345 | 272.6 µs |
| zetah | 2.3, 1.4 | 314.4 µs |
| zetap | 2.345 | 290.2 µs |

### `qcomplex`

| Operation | Inputs | 106 bits |
|---|---|---:|
| E1 | 2.345678+0.125i | 46.7 µs |
| Ei | 2.345678+0.125i | 54.1 µs |
| Li | 2.345+0.125i | 47.3 µs |
| abs | 0.7+0.125i | 470.8 ns |
| acos | 0.7+0.125i | 8.2 µs |
| acosec | 2.345+0.125i | 8.5 µs |
| acosech | 0.7+0.125i | 8.0 µs |
| acosh | 2.345+0.125i | 12.4 µs |
| acot | 0.7+0.125i | 9.6 µs |
| acoth | 2.345+0.125i | 5.2 µs |
| add | 0.7+0.125i, 0.7+0.125i | 14.9 ns |
| appell_F1 | <span style="display:inline-block;width:40ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">0.5+0.125i, 0.7+0.125i, 0.8+0.125i, 2.5+<br/>0.125i, 0.2+0.125i, 0.3+0.125i</span> | 596.6 µs |
| appell_F2 | <span style="display:inline-block;width:40ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">0.5+0.125i, 0.7+0.125i, 0.8+0.125i, 2.5+<br/>0.125i, 2.6+0.125i, 0.125+0.125i, 0.0625<br/>+0.125i</span> | 1.281 ms |
| appell_F3 | <span style="display:inline-block;width:40ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">0.5+0.125i, 0.6+0.125i, 0.7+0.125i, 0.8+<br/>0.125i, 2.5+0.125i, 0.2+0.125i, 0.3+0.12<br/>5i</span> | 963.5 µs |
| appell_F4 | <span style="display:inline-block;width:40ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">0.5+0.125i, 0.7+0.125i, 2.5+0.125i, 2.6+<br/>0.125i, 0.025+0.125i, 0.04+0.125i</span> | 3.389 ms |
| arccovercos | 0.5+0.125i | 8.0 µs |
| arccoversin | 0.5+0.125i | 8.6 µs |
| archacovercos | 0.5+0.125i | 4.9 µs |
| archacoversin | 0.5+0.125i | 5.2 µs |
| archavercos | 0.5+0.125i | 5.1 µs |
| archaversin | 0.5+0.125i | 4.8 µs |
| arcvercos | 0.5+0.125i | 9.3 µs |
| arcversin | 0.5+0.125i | 7.9 µs |
| arg | 0.7+0.125i | 1.2 µs |
| asec | 2.345+0.125i | 8.0 µs |
| asech | 0.7+0.125i | 12.0 µs |
| asin | 0.7+0.125i | 8.1 µs |
| asinh | 0.7+0.125i | 7.8 µs |
| atan | 0.7+0.125i | 9.2 µs |
| atan2 | 0.5+0.125i, -0.75+0.125i | 8.6 µs |
| atanh | 0.7+0.125i | 5.0 µs |
| bessel_I | 2, 0.7+0.125i | 24.9 µs |
| bessel_J | 2, 0.7+0.125i | 25.3 µs |
| bessel_K | 2, 0.7+0.125i | 84.6 µs |
| bessel_Y | 2, 0.7+0.125i | 103.1 µs |
| beta | 2.3+0.125i, 4.5+0.125i | 40.6 µs |
| beta_pdf | 0.5+0.125i, 2.5+0.125i, 3.5+0.125i | 51.2 µs |
| binomial | 5+0.125i, 2+0.125i | 24.6 µs |
| chebyshev_Tn | 2, 0.7+0.125i | 157.0 ns |
| chebyshev_Un | 2, 0.7+0.125i | 151.2 ns |
| circ | 0.7 | 18.7 ns |
| clausen | 3, 0.7+0.125i | 309.8 µs |
| clausen2 | 0.7+0.125i | 22.8 µs |
| conj | 0.7+0.125i | 3.8 ns |
| cos | 0.7+0.125i | 8.2 µs |
| cosec | 0.7+0.125i | 8.1 µs |
| cosech | 0.7+0.125i | 8.7 µs |
| cosh | 0.7+0.125i | 7.9 µs |
| cot | 0.7+0.125i | 15.7 µs |
| coth | 0.7+0.125i | 15.0 µs |
| covercos | 0.7+0.125i | 7.9 µs |
| coversin | 0.7+0.125i | 8.4 µs |
| cubrt | 0.7+0.125i | 7.2 µs |
| digamma | 2.345+0.125i | 10.7 µs |
| dilog | 0.7+0.125i | 85.7 µs |
| div | 0.7+0.125i, 0.7+0.125i | 150.4 ns |
| erf | 0.7+0.125i | 40.5 µs |
| erfc | 0.7+0.125i | 43.2 µs |
| erfcinv | 0.5+0.125i | 295.1 µs |
| erfinv | 0.5+0.125i | 286.4 µs |
| exp | 1.23456789+0.125i | 3.5 µs |
| floor | 0.7+0.125i | 37.5 ns |
| gamma | 2.345+0.125i | 12.6 µs |
| gammainc_P | 2.3+0.125i, 1.4+0.125i | 59.2 µs |
| gammainc_Q | 2.3+0.125i, 1.4+0.125i | 58.5 µs |
| gammainc_lower | 2.3+0.125i, 1.4+0.125i | 47.0 µs |
| gammainc_upper | 2.3+0.125i, 1.4+0.125i | 56.5 µs |
| gammainv | 9.5+0.125i | 136.0 µs |
| hacovercos | 0.7+0.125i | 8.6 µs |
| hacoversin | 0.7+0.125i | 8.4 µs |
| harmonic_poly | 2, 0.7+0.125i | 438.4 ns |
| havercos | 0.7+0.125i | 7.8 µs |
| haversin | 0.7+0.125i | 8.5 µs |
| hermite_Hn | 2, 0.7+0.125i | 171.0 ns |
| hypergeometric_pFq | 0.7+0.125i, 2.5+0.125i, 0.3+0.125i | 32.0 µs |
| hypot | 0.7+0.125i, 1.2+0.125i | 1.3 µs |
| imag_part | 0.7+0.125i | 2.4 ns |
| inv | 0.7+0.125i | 150.6 ns |
| lambert_S | <span style="display:inline-block;width:40ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">0.5+0.125i, 0.25+0.125i, 1.25+0.125i, 1,<br/> -1, inf</span> | 85.2 µs |
| lambert_S (constant base) | -1+0.125i, 1, 1+0.125i, 1, -1, 1000 | 4.4 µs |
| lambert_S (finite section) | <span style="display:inline-block;width:40ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">-1+0.125i, 0.7+0.125i, 1+0.125i, 1, -1, <br/>1000</span> | 260.3 µs |
| lambert_S (unit-circle base) | 0.001 | 391.0 µs |
| lambert_W0 | 0.7+0.125i | 32.1 µs |
| lambert_Wm1 | -0.2+0.125i | 27.8 µs |
| lambert_Wn | 1, 0.7+0.125i | 30.6 µs |
| lauricella_F | 0.5+0.125i, 0.3+0.125i, 2.5+0.125i | 142.4 µs |
| ldexp | 1.23456789+0.125i, 3 | 21.4 ns |
| legendre_chi | 2, 0.7+0.125i | 326.8 µs |
| lerch_phi | 0.3+0.125i, 2+0.125i, 1.4+0.125i | 567.8 µs |
| lerch_phi (integer order) | 0.3125+0.125i, -15, 0.375+0.125i | 191.0 µs |
| lerch_phi (negative order) | 0.3125+0.125i, -15+0.125i, 0.375+0.125i | 967.7 µs |
| lg | 2.345678+0.125i | 5.2 µs |
| lgamma | 2.345+0.125i | 10.1 µs |
| ln | 2.345678+0.125i | 4.5 µs |
| log | 2.345678+0.125i | 4.2 µs |
| log10 | 2.345678+0.125i | 4.5 µs |
| logbeta | 2.3+0.125i, 4.5+0.125i | 30.9 µs |
| logbeta_pdf | 0.5+0.125i, 2.5+0.125i, 3.5+0.125i | 41.1 µs |
| lommel_S | 0.5+0.125i, 0.7+0.125i, 1.2+0.125i | 116.9 µs |
| mul | 0.7+0.125i, 0.7+0.125i | 48.7 ns |
| neg | 0.7+0.125i | 5.0 ns |
| normal_cdf | 0.7+0.125i | 39.3 µs |
| normal_logpdf | 0.7+0.125i | 142.7 ns |
| normal_pdf | 0.7+0.125i | 3.4 µs |
| polygamma | 3, 2.345+0.125i | 12.2 µs |
| polylog | 2, 0.7+0.125i | 87.0 µs |
| polylog1 | 0.7+0.125i | 4.5 µs |
| pow | 1.23456789+0.125i, 3.5+0.125i | 7.7 µs |
| productlog | 0.7+0.125i | 35.5 µs |
| qdigamma | 0.5+0.125i, 2.3+0.125i | 180.8 µs |
| real_part | 0.7+0.125i | 2.4 ns |
| rect | 0.7 | 19.6 ns |
| root | 1.2345+0.125i, 3 | 8.1 µs |
| round | 0.7+0.125i | 84.6 ns |
| roundn | 1.23456789+0.125i, 3 | 9.5 µs |
| rounds | 1.23456789+0.125i, 3 | 33.6 µs |
| sec | 0.7+0.125i | 9.5 µs |
| sech | 0.7+0.125i | 8.4 µs |
| sgn | 0.7 | 10.7 ns |
| sin | 0.567+0.125i | 8.2 µs |
| sinc | 0.7+0.125i | 7.9 µs |
| sinh | 0.7+0.125i | 8.2 µs |
| sqr | 0.7+0.125i | 51.0 ns |
| sqrt | 1.23456789+0.125i | 3.5 µs |
| step | 0.7 | 10.4 ns |
| struve_H | 2, 0.7+0.125i | 54.9 µs |
| struve_L | 2, 0.7+0.125i | 65.9 µs |
| sub | 0.7+0.125i, 0.7+0.125i | 16.0 ns |
| tan | 0.7+0.125i | 15.5 µs |
| tanh | 0.7+0.125i | 16.4 µs |
| tetragamma | 2.345+0.125i | 3.9 µs |
| tri | 0.7 | 17.7 ns |
| trigamma | 2.345+0.125i | 3.1 µs |
| vercos | 0.7+0.125i | 8.9 µs |
| versin | 0.7+0.125i | 8.1 µs |
| zatahp | 2.3+0.125i, 1.4+0.125i | 549.8 µs |
| zatahp (integer order) | -4, 0.75+0.125i | 517.8 µs |
| zeta | 2.345+0.125i | 421.3 µs |
| zetah | 2.3+0.125i, 1.4+0.125i | 509.8 µs |
| zetap | 2.345+0.125i | 425.1 µs |

### `mpfr`

| Operation | Inputs | 256 bits | 512 bits | 768 bits | 1024 bits | 2048 bits | 4096 bits |
|---|---|---:|---:|---:|---:|---:|---:|
| E1 | 2.345678 | 34.7 µs | 70.8 µs | 117.0 µs | 157.4 µs | 477.4 µs | 1.824 ms |
| Ei | 2.345678 | 38.8 µs | 70.0 µs | 107.1 µs | 141.6 µs | 441.9 µs | 2.160 ms |
| Li | 2.345 | 35.1 µs | 59.9 µs | 109.5 µs | 148.5 µs | 428.2 µs | 1.654 ms |
| abs | 0.7 | 83.4 ns | 86.8 ns | 80.6 ns | 82.2 ns | 97.1 ns | 105.7 ns |
| acos | 0.7 | 15.6 µs | 24.7 µs | 35.2 µs | 48.7 µs | 97.9 µs | 333.1 µs |
| acosec | 2.345 | 16.6 µs | 32.0 µs | 32.3 µs | 49.6 µs | 113.4 µs | 330.1 µs |
| acosech | 0.7 | 5.2 µs | 8.3 µs | 13.3 µs | 15.8 µs | 42.5 µs | 113.3 µs |
| acosh | 2.345 | 5.4 µs | 8.6 µs | 12.0 µs | 17.9 µs | 37.5 µs | 105.6 µs |
| acot | 0.7 | 14.9 µs | 22.4 µs | 29.7 µs | 46.8 µs | 111.0 µs | 328.6 µs |
| acoth | 2.345 | 6.6 µs | 9.5 µs | 12.5 µs | 16.8 µs | 43.3 µs | 109.4 µs |
| add | 0.7, 0.7 | 108.0 ns | 106.4 ns | 112.4 ns | 114.8 ns | 169.3 ns | 330.9 ns |
| add_long | 17, 3 | 770.5 ns | 613.4 ns | 596.9 ns | 627.7 ns | 745.9 ns | 758.1 ns |
| appell_F1 | 0.5, 0.7, 0.8, 2.5, 0.2, 0.3 | 281.8 µs † | 306.9 µs | 604.9 µs | 949.9 µs | 3.011 ms | 9.482 ms |
| appell_F2 | 0.5, 0.7, 0.8, 2.5, 2.6, 0.125, 0.0625 | 511.2 µs | 1.230 ms | 1.884 ms | 3.029 ms | 11.135 ms | 52.265 ms |
| appell_F3 | 0.5, 0.6, 0.7, 0.8, 2.5, 0.2, 0.3 | 726.0 µs | 1.886 ms | 2.982 ms | 4.642 ms | 22.797 ms | 125.130 ms |
| appell_F4 | 0.5, 0.7, 2.5, 2.6, 0.025, 0.04 | 374.4 µs | 950.6 µs | 1.665 ms | 2.587 ms | 11.801 ms | 61.374 ms |
| arccovercos | 0.5 | 13.5 µs | 25.6 µs | 32.1 µs | 43.8 µs | 89.2 µs | 302.5 µs |
| arccoversin | 0.5 | 14.0 µs | 21.5 µs | 32.4 µs | 43.4 µs | 104.5 µs | 269.7 µs |
| archacovercos | 0.5 | 1.3 µs | 1.4 µs | 1.9 µs | 1.3 µs | 1.4 µs | 1.8 µs |
| archacoversin | 0.5 | 1.2 µs | 1.4 µs | 1.7 µs | 1.4 µs | 1.4 µs | 1.7 µs |
| archavercos | 0.5 | 1.4 µs | 1.5 µs | 1.6 µs | 1.4 µs | 1.5 µs | 1.8 µs |
| archaversin | 0.5 | 1.3 µs | 1.5 µs | 1.7 µs | 1.5 µs | 1.5 µs | 1.6 µs |
| arcvercos | 0.5 | 13.7 µs | 22.9 µs | 32.3 µs | 42.5 µs | 107.0 µs | 286.8 µs |
| arcversin | 0.5 | 14.1 µs | 23.6 µs | 29.0 µs | 44.5 µs | 111.7 µs | 289.3 µs |
| arg | 0.7 | 542.1 ns | 508.7 ns | 505.7 ns | 528.1 ns | 595.0 ns | 514.7 ns |
| asec | 2.345 | 16.9 µs | 25.7 µs | 32.5 µs | 50.3 µs | 120.3 µs | 342.3 µs |
| asech | 0.7 | 6.2 µs | 10.0 µs | 13.1 µs | 16.6 µs | 43.1 µs | 126.3 µs |
| asin | 0.7 | 15.9 µs | 26.6 µs | 32.1 µs | 49.0 µs | 117.2 µs | 290.1 µs |
| asinh | 0.7 | 5.1 µs | 7.9 µs | 12.0 µs | 17.0 µs | 37.2 µs | 116.0 µs |
| atan | 0.7 | 12.6 µs | 21.7 µs | 26.5 µs | 45.2 µs | 109.9 µs | 323.0 µs |
| atan2 | 0.5, -0.75 | 13.6 µs | 22.2 µs | 28.3 µs | 44.4 µs | 100.9 µs | 295.3 µs |
| atanh | 0.7 | 5.6 µs | 10.4 µs | 12.5 µs | 17.2 µs | 40.5 µs | 101.8 µs |
| bessel_I | 2, 0.7 | 27.1 µs | 60.5 µs | 97.8 µs | 168.5 µs | 563.0 µs | 1.546 ms |
| bessel_J | 2, 0.7 | 6.9 µs | 14.6 µs | 24.4 µs | 42.4 µs | 189.9 µs | 761.1 µs |
| bessel_K | 2, 0.7 | 48.8 µs | 101.6 µs | 163.3 µs | 221.6 µs | 807.4 µs | 3.735 ms |
| bessel_Y | 2, 0.7 | 41.8 µs | 85.0 µs | 140.8 µs | 221.4 µs | 801.3 µs | 3.510 ms |
| beta | 2.3, 4.5 | 97.2 µs | 248.0 µs | 435.3 µs | 704.9 µs | 2.841 ms | 13.956 ms |
| beta_pdf | 0.5, 2.5, 3.5 | 92.2 µs | 201.0 µs | 345.0 µs | 567.5 µs | 2.061 ms | 8.981 ms |
| binomial | 5, 2 | 97.7 µs | 255.1 µs | 417.3 µs | 735.3 µs | 2.744 ms | 12.159 ms |
| ceil | 0.7 | 454.5 ns | 470.4 ns | 491.1 ns | 512.3 ns | 503.7 ns | 557.0 ns |
| chebyshev_Tn | 2, 0.7 | 2.8 µs | 2.9 µs | 3.0 µs | 3.3 µs | 3.3 µs | 5.6 µs |
| chebyshev_Un | 2, 0.7 | 3.1 µs | 3.4 µs | 3.9 µs | 3.4 µs | 4.5 µs | 6.1 µs |
| circ | 0.7 | 829.6 ns | 893.7 ns | 848.7 ns | 885.7 ns | 903.0 ns | 1.1 µs |
| clausen | 3, 0.7 | 25.2 µs | 59.1 µs | 123.7 µs | 200.5 µs | 906.3 µs | 4.155 ms |
| clausen2 | 0.7 | 23.3 µs | 69.1 µs | 123.4 µs | 219.1 µs | 864.8 µs | 4.143 ms |
| conj | 0.7 | 80.9 ns | 72.0 ns | 76.7 ns | 80.6 ns | 89.2 ns | 93.3 ns |
| cos | 0.7 | 2.2 µs | 4.2 µs | 7.4 µs | 11.3 µs | 35.6 µs | 156.3 µs |
| cosec | 0.7 | 3.0 µs | 5.8 µs | 9.1 µs | 14.0 µs | 46.9 µs | 148.2 µs |
| cosech | 0.7 | 2.8 µs | 6.1 µs | 10.2 µs | 13.5 µs | 44.6 µs | 187.8 µs |
| cosh | 0.7 | 2.6 µs | 5.5 µs | 9.4 µs | 14.7 µs | 44.4 µs | 211.6 µs |
| cot | 0.7 | 5.2 µs | 10.4 µs | 15.8 µs | 24.7 µs | 77.3 µs | 317.1 µs |
| coth | 0.7 | 5.4 µs | 11.3 µs | 18.0 µs | 29.9 µs | 96.7 µs | 313.8 µs |
| covercos | 0.7 | 3.5 µs | 6.1 µs | 9.8 µs | 13.9 µs | 45.9 µs | 168.5 µs |
| coversin | 0.7 | 3.2 µs | 6.3 µs | 9.7 µs | 14.2 µs | 53.9 µs | 147.3 µs |
| cubrt | 0.7 | 12.0 µs | 17.8 µs | 25.8 µs | 34.2 µs | 117.7 µs | 310.6 µs |
| digamma | 2.345 | 94.3 µs | 277.2 µs | 504.6 µs | 943.9 µs | 4.880 ms | 17.382 ms |
| dilog | 0.7 | 97.9 µs | 179.2 µs | 263.3 µs | 395.2 µs | 1.460 ms | 4.181 ms |
| div | 0.7, 0.7 | 315.0 ns | 493.9 ns | 830.5 ns | 1.2 µs | 3.1 µs | 9.9 µs |
| erf | 0.7 | 15.2 µs | 36.9 µs | 62.9 µs | 102.0 µs | 450.1 µs | 1.788 ms |
| erfc | 0.7 | 16.9 µs | 37.8 µs | 67.0 µs | 107.8 µs | 455.2 µs | 2.074 ms |
| erfcinv | 0.5 | 97.7 µs | 179.1 µs | 285.4 µs | 481.2 µs | 1.396 ms | 5.375 ms |
| erfinv | 0.5 | 84.7 µs | 174.4 µs | 273.4 µs | 436.6 µs | 1.626 ms | 5.650 ms |
| exp | 1.23456789 | 2.7 µs | 5.2 µs | 9.2 µs | 14.3 µs | 59.4 µs | 186.6 µs |
| floor | 0.7 | 78.1 ns | 87.7 ns | 84.0 ns | 86.7 ns | 86.7 ns | 100.8 ns |
| gamma | 2.345 | 33.2 µs | 75.2 µs | 150.1 µs | 262.3 µs | 1.008 ms | 4.658 ms |
| gammainc_P | 2.3, 1.4 | 104.7 µs | 253.6 µs | 376.4 µs | 596.7 µs | 2.128 ms | 9.243 ms |
| gammainc_Q | 2.3, 1.4 | 121.5 µs | 258.5 µs | 467.0 µs | 726.2 µs | 2.627 ms | 11.688 ms |
| gammainc_lower | 2.3, 1.4 | 56.8 µs | 147.4 µs | 233.9 µs | 343.4 µs | 1.219 ms | 6.019 ms |
| gammainc_upper | 2.3, 1.4 | 122.2 µs | 298.4 µs | 493.0 µs | 705.9 µs | 2.588 ms | 11.123 ms |
| gammainv | 9.5 | 435.7 µs | 892.4 µs | 1.401 ms | 1.806 ms | 5.271 ms | 19.251 ms |
| hacovercos | 0.7 | 4.3 µs | 6.7 µs | 11.3 µs | 16.8 µs | 45.7 µs | 168.9 µs |
| hacoversin | 0.7 | 4.7 µs | 7.6 µs | 10.8 µs | 15.0 µs | 47.6 µs | 153.7 µs |
| harmonic_poly | 2, 0.7 | 3.9 µs | 4.4 µs | 5.3 µs | 5.9 µs | 11.2 µs | 28.1 µs |
| havercos | 0.7 | 3.3 µs | 5.8 µs | 8.6 µs | 15.4 µs | 44.6 µs | 160.5 µs |
| haversin | 0.7 | 3.2 µs | 5.6 µs | 10.1 µs | 13.3 µs | 44.2 µs | 158.0 µs |
| hermite_Hn | 2, 0.7 | 3.8 µs | 3.6 µs | 3.9 µs | 3.9 µs | 5.2 µs | 7.4 µs |
| hypergeometric_pFq | 0.7, 2.5, 0.3 | 43.6 µs | 66.0 µs | 115.6 µs | 191.1 µs | 545.3 µs | 2.108 ms |
| hypot | 0.7, 1.2 | 616.3 ns | 867.3 ns | 1.1 µs | 1.6 µs | 2.7 µs | 8.6 µs |
| imag_part | 0.7 | 69.3 ns | 80.9 ns | 76.9 ns | 70.1 ns | 85.4 ns | 80.4 ns |
| inv | 0.7 | 203.1 ns | 279.5 ns | 425.3 ns | 542.3 ns | 1.2 µs | 3.7 µs |
| lambert_S | 0.5, 0.25, 1.25, 1, -1, inf | 26.7 µs | 57.2 µs | 63.4 µs | 95.2 µs | 273.6 µs | 1.043 ms |
| lambert_S (constant base) | -1, 1, 1, 1, -1, 1000 | 7.6 µs | 8.1 µs | 8.7 µs | 8.3 µs | 10.5 µs | 19.5 µs |
| lambert_S (finite section) | -1, 0.7, 1, 1, -1, 1000 | 47.2 µs | 103.0 µs | 136.9 µs | 231.6 µs | 715.6 µs | 2.796 ms |
| lambert_S (unit-circle base) | 0.001 | 3.044 ms | 3.928 ms | 4.985 ms | 6.037 ms | 12.703 ms | 32.552 ms |
| lambert_W0 | 0.7 | 45.3 µs | 101.0 µs | 163.6 µs | 266.7 µs | 779.9 µs | 3.086 ms |
| lambert_Wm1 | -0.2 | 59.7 µs | 100.6 µs | 186.6 µs | 278.5 µs | 844.2 µs | 2.831 ms |
| ldexp | 1.23456789, 3 | 108.1 ns | 113.2 ns | 112.7 ns | 122.2 ns | 135.2 ns | 129.8 ns |
| legendre_chi | 2, 0.7 | 49.9 µs | 99.5 µs | 170.3 µs | 252.8 µs | 785.9 µs | 2.782 ms |
| lerch_phi | 0.3, 2, 1.4 | 102.9 µs | 212.7 µs | 417.8 µs | 694.8 µs | 2.886 ms | 16.132 ms |
| lerch_phi (integer order) | 0.3125, -15, 0.375 | 27.1 µs | 27.8 µs | 29.7 µs | 30.4 µs | 32.4 µs | 32.0 µs |
| lerch_phi (negative order) | 0.3125, -15, 0.375 | 26.3 µs | 27.8 µs | 27.9 µs | 31.3 µs | 26.9 µs | 29.5 µs |
| lg | 2.345678 | 10.8 µs | 17.1 µs | 22.7 µs | 33.7 µs | 81.6 µs | 205.6 µs |
| lgamma | 2.345 | 33.7 µs | 79.1 µs | 154.8 µs | 231.0 µs | 889.5 µs | 4.342 ms |
| ln | 2.345678 | 5.0 µs | 7.1 µs | 11.3 µs | 15.5 µs | 38.0 µs | 98.9 µs |
| log | 2.345678 | 4.6 µs | 8.0 µs | 10.5 µs | 15.1 µs | 40.2 µs | 108.0 µs |
| log10 | 2.345678 | 11.0 µs | 15.9 µs | 24.4 µs | 33.0 µs | 82.8 µs | 223.4 µs |
| logbeta | 2.3, 4.5 | 113.5 µs | 251.5 µs | 487.8 µs | 785.4 µs | 2.798 ms | 13.185 ms |
| logbeta_pdf | 0.5, 2.5, 3.5 | 85.6 µs | 201.5 µs | 333.9 µs | 556.6 µs | 1.940 ms | 10.734 ms |
| lommel_S | 0.5, 0.7, 1.2 | 27.0 µs | 64.3 µs | 131.0 µs | 182.9 µs | 678.0 µs | 3.147 ms |
| mul | 0.7, 0.7 | 111.0 ns | 158.1 ns | 241.3 ns | 337.7 ns | 915.8 ns | 3.0 µs |
| mul_long | 17, 3 | 645.5 ns | 763.9 ns | 832.2 ns | 679.6 ns | 700.3 ns | 743.1 ns |
| mul_pow10 | 1.23456789, 3 | 322.7 ns | 396.9 ns | 578.7 ns | 578.6 ns | 476.9 ns | 664.9 ns |
| neg | 0.7 | 84.9 ns | 85.3 ns | 91.6 ns | 91.3 ns | 95.7 ns | 95.9 ns |
| normal_cdf | 0.7 | 13.9 µs | 74.9 µs | 61.5 µs | 98.7 µs | 397.0 µs | 1.859 ms |
| normal_logpdf | 0.7 | 4.8 µs | 7.5 µs | 11.0 µs | 15.9 µs | 38.0 µs | 106.1 µs |
| normal_pdf | 0.7 | 3.1 µs | 6.6 µs | 11.1 µs | 17.7 µs | 55.8 µs | 174.7 µs |
| polygamma | 3, 2.345 | 48.6 µs | 114.1 µs | 275.0 µs | 405.8 µs | 1.795 ms | 8.840 ms |
| polylog | 2, 0.7 | 90.4 µs | 169.8 µs | 286.3 µs | 424.6 µs | 1.286 ms | 4.004 ms |
| polylog1 | 0.7 | 5.8 µs | 8.3 µs | 13.2 µs | 17.0 µs | 40.4 µs | 120.9 µs |
| pow | 1.23456789, 3.5 | 9.3 µs | 14.0 µs | 22.9 µs | 31.4 µs | 83.9 µs | 253.3 µs |
| pow10 | 3 | 228.4 ns | 274.4 ns | 349.5 ns | 478.0 ns | 319.8 ns | 452.8 ns |
| pow_int | 1.23456789, 3 | 285.2 ns | 387.4 ns | 520.9 ns | 760.8 ns | 2.2 µs | 6.4 µs |
| productlog | 0.7 | 49.1 µs | 93.5 µs | 175.2 µs | 241.1 µs | 822.7 µs | 3.187 ms |
| real_part | 0.7 | 73.7 ns | 82.7 ns | 96.0 ns | 85.7 ns | 85.6 ns | 87.7 ns |
| rect | 0.7 | 847.9 ns | 838.4 ns | 875.8 ns | 905.8 ns | 935.8 ns | 1.1 µs |
| root | 1.2345, 3 | 16.0 µs | 19.9 µs | 29.6 µs | 39.1 µs | 86.1 µs | 256.1 µs |
| round | 0.7 | 5.3 µs † | 2.6 µs | 2.3 µs | 2.3 µs | 2.4 µs | 2.7 µs |
| roundn | 1.23456789, 3 | 13.6 µs † | 5.1 µs | 5.2 µs | 5.8 µs | 7.0 µs | 9.5 µs |
| rounds | 1.23456789, 3 | 20.7 µs | 24.1 µs | 30.4 µs | 40.6 µs | 86.9 µs | 253.6 µs |
| sec | 0.7 | 2.2 µs | 5.2 µs | 7.9 µs | 12.4 µs | 38.8 µs | 157.7 µs |
| sech | 0.7 | 2.9 µs | 5.4 µs | 9.3 µs | 13.6 µs | 45.2 µs | 163.5 µs |
| sgn | 0.7 | 200.2 ns | 237.7 ns | 235.1 ns | 235.4 ns | 218.7 ns | 270.2 ns |
| sin | 0.567 | 2.9 µs | 5.6 µs | 8.7 µs | 12.7 µs | 40.0 µs | 161.3 µs |
| sinc | 0.7 | 4.4 µs | 7.8 µs | 12.5 µs | 17.4 µs | 43.7 µs | 154.8 µs |
| sincos | 0.7 | 11.8 µs | 12.6 µs | 15.9 µs | 19.2 µs | 48.7 µs | 171.7 µs |
| sinh | 0.7 | 3.0 µs | 5.9 µs | 10.2 µs | 14.1 µs | 43.1 µs | 170.2 µs |
| sinhcosh | 0.7 | 2.9 µs | 5.7 µs | 10.2 µs | 13.3 µs | 47.9 µs | 164.4 µs |
| sqr | 0.7 | 136.4 ns | 206.7 ns | 310.7 ns | 433.4 ns | 648.1 ns | 1.6 µs |
| sqrt | 1.23456789 | 313.2 ns | 428.6 ns | 522.1 ns | 642.4 ns | 1.4 µs | 2.8 µs |
| step | 0.7 | 199.9 ns | 214.6 ns | 231.3 ns | 260.2 ns | 218.7 ns | 234.5 ns |
| struve_H | 2, 0.7 | 33.2 µs | 63.0 µs | 106.2 µs | 157.4 µs | 663.0 µs | 1.488 ms |
| struve_L | 2, 0.7 | 30.2 µs | 61.3 µs | 102.5 µs | 164.6 µs | 624.1 µs | 1.588 ms |
| sub | 0.7, 0.7 | 79.6 ns | 94.5 ns | 95.2 ns | 92.3 ns | 132.1 ns | 289.9 ns |
| tan | 0.7 | 13.2 µs | 13.8 µs | 17.8 µs | 24.7 µs | 57.5 µs | 163.7 µs |
| tanh | 0.7 | 3.0 µs | 5.4 µs | 9.9 µs | 13.4 µs | 42.9 µs | 168.0 µs |
| tetragamma | 2.345 | 42.9 µs | 116.7 µs | 218.6 µs | 400.0 µs | 1.993 ms | 7.054 ms |
| tri | 0.7 | 838.4 ns | 875.1 ns | 872.1 ns | 884.6 ns | 970.8 ns | 1.1 µs |
| trigamma | 2.345 | 30.6 µs | 80.7 µs | 173.7 µs | 281.6 µs | 1.408 ms | 5.495 ms |
| vercos | 0.7 | 2.5 µs | 5.0 µs | 8.4 µs | 11.5 µs | 40.1 µs | 146.5 µs |
| versin | 0.7 | 2.6 µs | 4.8 µs | 7.3 µs | 12.7 µs | 40.8 µs | 139.9 µs |
| zatahp | 2.3, 1.4 | 65.7 µs | 139.7 µs | 254.4 µs | 304.8 µs | 1.475 ms | 8.783 ms |
| zatahp (integer order) | -4, 0.75 | 56.0 µs | 104.7 µs | 173.6 µs | 173.7 µs | 801.4 µs | 3.897 ms |
| zeta | 2.345 | 805.6 µs | 3.814 ms | 9.021 ms | 1.060 ms | 4.239 ms | 23.157 ms |
| zeta_pair | 2.345 | 189.6 µs | 365.1 µs | 816.5 µs | 1.363 ms | 6.331 ms | 29.368 ms |
| zetah | 2.3, 1.4 | 34.6 µs | 65.3 µs | 118.8 µs | 150.8 µs | 703.3 µs | 3.520 ms |
| zetap | 2.345 | 173.3 µs | 371.2 µs | 667.8 µs | 1.259 ms | 6.173 ms | 30.729 ms |

### `mpc`

| Operation | Inputs | 256 bits | 512 bits | 768 bits | 1024 bits | 2048 bits | 4096 bits |
|---|---|---:|---:|---:|---:|---:|---:|
| E1 | 2.345678+0.125i | 94.3 µs | 185.9 µs | 322.3 µs | 578.9 µs | 2.004 ms | 7.563 ms |
| Ei | 2.345678+0.125i | 90.0 µs | 171.5 µs | 319.9 µs | 566.6 µs | 2.094 ms | 6.962 ms |
| Li | 2.345+0.125i | 81.5 µs | 172.9 µs | 318.5 µs | 534.6 µs | 2.245 ms | 7.965 ms |
| abs | 0.7+0.125i | 951.0 ns | 1.1 µs | 1.4 µs | 1.6 µs | 2.7 µs | 7.5 µs |
| acos | 0.7+0.125i | 53.2 µs | 84.9 µs | 108.3 µs | 69.7 µs | 172.5 µs | 457.5 µs |
| acosec | 2.345+0.125i | 42.1 µs | 70.5 µs | 109.0 µs | 143.1 µs | 327.0 µs | 1.061 ms |
| acosech | 0.7+0.125i | 49.2 µs | 73.6 µs | 85.9 µs | 139.7 µs | 332.8 µs | 873.6 µs |
| acosh | 2.345+0.125i | 40.9 µs | 69.9 µs | 78.8 µs | 63.7 µs | 286.1 µs | 431.9 µs |
| acot | 0.7+0.125i | 49.9 µs | 74.7 µs | 98.3 µs | 130.0 µs | 296.7 µs | 912.7 µs |
| acoth | 2.345+0.125i | 30.1 µs | 57.2 µs | 64.2 µs | 105.8 µs | 262.3 µs | 825.5 µs |
| add | 0.7+0.125i, 0.7+0.125i | 470.2 ns | 377.7 ns | 394.9 ns | 377.7 ns | 489.8 ns | 584.0 ns |
| add_long | 17+0.125i, 3 | 2.4 µs | 2.7 µs | 2.5 µs | 2.5 µs | 2.8 µs | 2.9 µs |
| appell_F1 | <span style="display:inline-block;width:40ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">0.5+0.125i, 0.7+0.125i, 0.8+0.125i, 2.5+<br/>0.125i, 0.2+0.125i, 0.3+0.125i</span> | 488.0 µs | 1.355 ms | 2.103 ms | 3.736 ms | 11.673 ms | 34.771 ms |
| appell_F2 | <span style="display:inline-block;width:40ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">0.5+0.125i, 0.7+0.125i, 0.8+0.125i, 2.5+<br/>0.125i, 2.6+0.125i, 0.125+0.125i, 0.0625<br/>+0.125i</span> | 3.190 ms | 7.627 ms | 16.188 ms | 29.398 ms | 85.695 ms | 303.816 ms |
| appell_F3 | <span style="display:inline-block;width:40ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">0.5+0.125i, 0.6+0.125i, 0.7+0.125i, 0.8+<br/>0.125i, 2.5+0.125i, 0.2+0.125i, 0.3+0.12<br/>5i</span> | 2.764 ms | 6.876 ms | 11.908 ms | 24.518 ms | 100.210 ms | 399.153 ms |
| appell_F4 | <span style="display:inline-block;width:40ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">0.5+0.125i, 0.7+0.125i, 2.5+0.125i, 2.6+<br/>0.125i, 0.025+0.125i, 0.04+0.125i</span> | 5.267 ms | 13.604 ms | 23.731 ms | 40.661 ms | 153.659 ms | 578.877 ms |
| arccovercos | 0.5+0.125i | 52.3 µs | 76.5 µs | 55.7 µs | 138.3 µs | 139.0 µs | 386.6 µs |
| arccoversin | 0.5+0.125i | 49.8 µs | 39.7 µs | 54.7 µs | 142.3 µs | 165.3 µs | 431.3 µs |
| archacovercos | 0.5+0.125i | 11.8 µs | 14.9 µs | 19.3 µs | 24.9 µs | 48.1 µs | 120.2 µs |
| archacoversin | 0.5+0.125i | 13.0 µs | 17.6 µs | 21.2 µs | 24.4 µs | 46.1 µs | 108.6 µs |
| archavercos | 0.5+0.125i | 13.5 µs | 15.9 µs | 20.4 µs | 23.8 µs | 51.6 µs | 125.8 µs |
| archaversin | 0.5+0.125i | 12.0 µs | 17.4 µs | 19.6 µs | 23.0 µs | 50.3 µs | 114.9 µs |
| arcvercos | 0.5+0.125i | 55.3 µs | 39.5 µs | 97.3 µs | 69.7 µs | 166.0 µs | 433.0 µs |
| arcversin | 0.5+0.125i | 27.8 µs | 38.2 µs | 107.2 µs | 72.8 µs | 161.5 µs | 401.3 µs |
| arg | 0.7+0.125i | 13.2 µs | 25.7 µs | 31.6 µs | 44.5 µs | 108.6 µs | 287.3 µs |
| asec | 2.345+0.125i | 45.6 µs | 70.9 µs | 89.7 µs | 141.5 µs | 320.4 µs | 990.1 µs |
| asech | 0.7+0.125i | 24.8 µs | 45.8 µs | 59.4 µs | 79.3 µs | 170.7 µs | 506.4 µs |
| asin | 0.7+0.125i | 54.8 µs | 82.1 µs | 121.1 µs | 149.2 µs | 342.5 µs | 485.5 µs |
| asinh | 0.7+0.125i | 45.3 µs | 66.5 µs | 92.5 µs | 139.3 µs | 347.5 µs | 941.4 µs |
| atan | 0.7+0.125i | 45.4 µs | 71.9 µs | 98.8 µs | 140.8 µs | 317.7 µs | 785.1 µs |
| atanh | 0.7+0.125i | 38.3 µs | 62.4 µs | 90.5 µs | 135.4 µs | 306.7 µs | 816.4 µs |
| bessel_I | 2, 0.7+0.125i | 51.6 µs | 127.9 µs | 197.9 µs | 335.6 µs | 986.0 µs | 4.035 ms |
| bessel_J | 2, 0.7+0.125i | 55.2 µs | 111.1 µs | 213.3 µs | 360.0 µs | 1.152 ms | 4.648 ms |
| bessel_K | 2, 0.7+0.125i | 126.1 µs | 214.6 µs | 423.5 µs | 756.4 µs | 2.460 ms | 10.430 ms |
| bessel_Y | 2, 0.7+0.125i | 174.5 µs | 375.1 µs | 630.0 µs | 958.4 µs | 3.960 ms | 16.601 ms |
| ceil | 0.7+0.125i | 1.7 µs | 1.6 µs | 1.6 µs | 1.4 µs | 1.9 µs | 1.6 µs |
| chebyshev_Tn | 2, 0.7+0.125i | 11.3 µs | 11.4 µs | 10.8 µs | 10.6 µs | 14.1 µs | 17.8 µs |
| chebyshev_Un | 2, 0.7+0.125i | 13.3 µs | 12.3 µs | 13.7 µs | 14.8 µs | 15.5 µs | 21.2 µs |
| circ | 0.7 | 2.1 µs | 2.4 µs | 2.8 µs | 3.0 µs | 3.8 µs | 7.3 µs |
| clausen | 3, 0.7+0.125i | 128.5 µs | 373.1 µs | 676.5 µs | 1.180 ms | 4.916 ms | 23.311 ms |
| clausen2 | 0.7+0.125i | 129.7 µs | 410.7 µs | 800.2 µs | 1.025 ms | 3.607 ms | 11.528 ms |
| conj | 0.7+0.125i | 303.5 ns | 264.7 ns | 272.7 ns | 321.6 ns | 296.9 ns | 371.3 ns |
| cos | 0.7+0.125i | 15.5 µs | 15.8 µs | 24.1 µs | 33.1 µs | 107.0 µs | 341.4 µs |
| cosec | 0.7+0.125i | 17.3 µs | 17.6 µs | 25.0 µs | 36.5 µs | 105.7 µs | 382.7 µs |
| cosech | 0.7+0.125i | 11.6 µs | 15.7 µs | 25.8 µs | 84.0 µs | 75.7 µs | 321.3 µs |
| cosh | 0.7+0.125i | 7.7 µs | 13.0 µs | 22.0 µs | 82.3 µs | 89.6 µs | 269.6 µs |
| cot | 0.7+0.125i | 39.6 µs | 32.2 µs | 47.0 µs | 68.0 µs | 215.3 µs | 679.5 µs |
| coth | 0.7+0.125i | 18.3 µs | 31.1 µs | 43.9 µs | 173.5 µs | 169.5 µs | 542.5 µs |
| covercos | 0.7+0.125i | 18.7 µs | 16.1 µs | 25.4 µs | 36.2 µs | 102.6 µs | 392.0 µs |
| coversin | 0.7+0.125i | 21.2 µs | 17.1 µs | 24.1 µs | 35.8 µs | 110.1 µs | 338.4 µs |
| cubrt | 0.7+0.125i | 41.5 µs | 60.1 µs | 87.2 µs | 141.9 µs | 366.1 µs | 1.274 ms |
| digamma | 2.345+0.125i | 215.1 µs | 337.1 µs | 568.4 µs | 861.9 µs | 3.438 ms | 12.110 ms |
| dilog | 0.7+0.125i | 173.8 µs | 203.2 µs | 331.8 µs | 537.2 µs | 1.593 ms | 5.311 ms |
| div | 0.7+0.125i, 0.7+0.125i | 7.7 µs | 9.9 µs | 16.8 µs | 20.0 µs | 44.7 µs | 95.2 µs |
| erf | 0.7+0.125i | 86.3 µs | 179.1 µs | 274.9 µs | 386.9 µs | 1.037 ms | 4.164 ms |
| erfc | 0.7+0.125i | 82.6 µs | 165.7 µs | 260.2 µs | 363.6 µs | 1.227 ms | 3.646 ms |
| erfcinv | 0.5+0.125i | 586.1 µs | 1.017 ms | 1.443 ms | 2.107 ms | 5.014 ms | 15.797 ms |
| erfinv | 0.5+0.125i | 503.1 µs | 1.221 ms | 1.356 ms | 1.839 ms | 5.214 ms | 14.236 ms |
| exp | 1.23456789+0.125i | 10.2 µs | 21.9 µs | 33.7 µs | 53.3 µs | 155.8 µs | 627.8 µs |
| floor | 0.7+0.125i | 263.8 ns | 270.5 ns | 257.7 ns | 262.4 ns | 289.9 ns | 281.2 ns |
| gamma | 2.345+0.125i | 227.0 µs | 314.2 µs | 511.0 µs | 769.6 µs | 2.184 ms | 7.723 ms |
| gammainc_P | 2.3+0.125i, 1.4+0.125i | 671.5 µs | 943.8 µs | 1.344 ms | 1.896 ms | 5.631 ms | 25.812 ms |
| gammainc_Q | 2.3+0.125i, 1.4+0.125i | 768.5 µs | 984.8 µs | 1.380 ms | 2.195 ms | 6.504 ms | 23.224 ms |
| gammainc_lower | 2.3+0.125i, 1.4+0.125i | 315.7 µs | 513.3 µs | 758.3 µs | 987.6 µs | 3.846 ms | 15.658 ms |
| gammainc_upper | 2.3+0.125i, 1.4+0.125i | 697.2 µs | 949.0 µs | 1.265 ms | 2.161 ms | 8.359 ms | 24.185 ms |
| gammainv | 9.5+0.125i | 2.573 ms | 3.310 ms | 4.357 ms | 4.872 ms | 11.466 ms | 27.475 ms |
| hacovercos | 0.7+0.125i | 23.8 µs | 21.1 µs | 26.8 µs | 46.3 µs | 105.9 µs | 375.4 µs |
| hacoversin | 0.7+0.125i | 21.3 µs | 23.3 µs | 31.6 µs | 41.3 µs | 114.8 µs | 407.3 µs |
| harmonic_poly | 2, 0.7+0.125i | 15.1 µs | 18.1 µs | 20.6 µs | 21.4 µs | 31.3 µs | 77.8 µs |
| havercos | 0.7+0.125i | 24.5 µs | 21.5 µs | 28.7 µs | 46.3 µs | 104.4 µs | 427.5 µs |
| haversin | 0.7+0.125i | 23.2 µs | 21.7 µs | 26.7 µs | 42.2 µs | 114.6 µs | 452.5 µs |
| hermite_Hn | 2, 0.7+0.125i | 13.3 µs | 13.2 µs | 15.8 µs | 16.0 µs | 15.2 µs | 22.9 µs |
| hypergeometric_pFq | 0.7+0.125i, 2.5+0.125i, 0.3+0.125i | 203.5 µs | 167.6 µs | 253.0 µs | 424.5 µs | 1.340 ms | 4.881 ms |
| imag_part | 0.7+0.125i | 122.6 ns | 121.2 ns | 132.6 ns | 128.7 ns | 160.8 ns | 192.8 ns |
| inv | 0.7+0.125i | 1.3 µs | 1.5 µs | 2.1 µs | 2.1 µs | 6.6 µs † | 12.3 µs |
| lambert_S | <span style="display:inline-block;width:40ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">0.5+0.125i, 0.25+0.125i, 1.25+0.125i, 1,<br/> -1, inf</span> | 193.7 µs | 369.8 µs | 477.3 µs | 837.6 µs | 1.956 ms | 5.217 ms |
| lambert_S (constant base) | -1+0.125i, 1, 1+0.125i, 1, -1, 1000 | 17.8 µs | 17.4 µs | 23.2 µs | 21.6 µs | 26.5 µs | 30.8 µs |
| lambert_S (finite section) | <span style="display:inline-block;width:40ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">-1+0.125i, 0.7+0.125i, 1+0.125i, 1, -1, <br/>1000</span> | 413.9 µs | 615.1 µs | 948.6 µs | 1.738 ms | 4.390 ms | 15.024 ms |
| lambert_S (unit-circle base) | 0.001 | 3.285 ms | 4.305 ms | 4.600 ms | 6.417 ms | 13.535 ms | 32.703 ms |
| ldexp | 1.23456789+0.125i, 3 | 302.2 ns | 327.5 ns | 303.3 ns | 314.7 ns | 371.4 ns | 372.1 ns |
| legendre_chi | 2, 0.7+0.125i | 124.8 µs | 238.3 µs | 439.2 µs | 626.1 µs | 2.045 ms | 7.184 ms |
| lerch_phi | 0.3+0.125i, 2+0.125i, 1.4+0.125i | 346.5 µs | 9.7 µs | 10.1 µs | 11.0 µs | 11.7 µs | 11.0 µs |
| lerch_phi (integer order) | 0.3125+0.125i, -15, 0.375+0.125i | 40.1 µs | 38.1 µs | 43.7 µs | 43.6 µs | 56.4 µs | 53.8 µs |
| lerch_phi (negative order) | 0.3125+0.125i, -15+0.125i, 0.375+0.125i | 467.5 µs | 9.9 µs | 10.1 µs | 11.2 µs | 10.4 µs | 10.5 µs |
| lg | 2.345678+0.125i | 26.2 µs | 40.9 µs | 51.4 µs | 76.0 µs | 183.6 µs | 580.9 µs |
| lgamma | 2.345+0.125i | 169.6 µs | 266.1 µs | 395.4 µs | 603.1 µs | 1.982 ms | 6.832 ms |
| ln | 2.345678+0.125i | 18.6 µs | 28.8 µs | 37.0 µs | 61.6 µs | 175.2 µs | 422.0 µs |
| log | 2.345678+0.125i | 16.3 µs | 27.2 µs | 35.1 µs | 54.3 µs | 126.4 µs | 408.7 µs |
| log10 | 2.345678+0.125i | 25.6 µs | 42.8 µs | 53.5 µs | 82.3 µs | 203.3 µs | 470.7 µs |
| lommel_S | 0.5+0.125i, 0.7+0.125i, 1.2+0.125i | 239.5 µs | 431.2 µs | 796.2 µs | 744.6 µs | 3.365 ms | 12.895 ms |
| mul | 0.7+0.125i, 0.7+0.125i | 1.6 µs | 1.7 µs | 2.3 µs | 2.7 µs | 4.2 µs | 7.6 µs |
| mul_long | 17+0.125i, 3 | 3.5 µs | 3.7 µs | 3.9 µs | 3.3 µs | 3.6 µs | 3.5 µs |
| mul_pow10 | 1.23456789+0.125i, 3 | 3.3 µs | 2.9 µs | 3.4 µs | 3.2 µs | 2.8 µs | 3.5 µs |
| neg | 0.7+0.125i | 283.9 ns | 262.3 ns | 282.7 ns | 297.6 ns | 295.3 ns | 338.0 ns |
| normal_cdf | 0.7+0.125i | 86.0 µs | 199.8 µs | 266.9 µs | 413.7 µs | 1.206 ms | 4.116 ms |
| normal_logpdf | 0.7+0.125i | 7.6 µs | 12.7 µs | 14.0 µs | 19.0 µs | 39.2 µs | 109.9 µs |
| normal_pdf | 0.7+0.125i | 19.3 µs | 29.3 µs | 50.9 µs | 72.6 µs | 261.5 µs | 867.2 µs |
| polygamma | 3, 2.345+0.125i | 312.3 µs | 571.2 µs | 878.1 µs | 1.671 ms | 6.502 ms | 22.681 ms |
| polylog | 2, 0.7+0.125i | 167.1 µs | 216.7 µs | 320.0 µs | 456.7 µs | 1.499 ms | 5.581 ms |
| polylog1 | 0.7+0.125i | 23.7 µs | 40.7 µs | 46.8 µs | 74.9 µs | 162.2 µs | 435.5 µs |
| pow | 1.23456789+0.125i, 3.5+0.125i | 30.7 µs | 54.8 µs | 106.7 µs | 123.5 µs | 380.6 µs | 1.265 ms |
| pow10 | 3 | 450.6 ns | 504.0 ns | 632.9 ns | 608.9 ns | 468.8 ns | 654.5 ns |
| pow_int | 1.23456789+0.125i, 3 | 5.7 µs | 7.9 µs | 8.0 µs | 8.9 µs | 12.8 µs | 20.4 µs |
| real_part | 0.7+0.125i | 121.7 ns | 124.4 ns | 140.5 ns | 134.5 ns | 125.8 ns | 186.8 ns |
| rect | 0.7 | 2.6 µs | 2.7 µs | 2.7 µs | 3.2 µs | 4.6 µs | 8.0 µs |
| root | 1.2345+0.125i, 3 | 34.4 µs | 58.7 µs | 185.5 µs | 271.2 µs | 364.0 µs | 1.130 ms |
| round | 0.7+0.125i | 8.5 µs | 10.3 µs | 10.5 µs | 10.4 µs | 10.4 µs | 10.7 µs |
| roundn | 1.23456789+0.125i, 3 | 15.4 µs | 15.0 µs | 14.7 µs | 15.9 µs | 20.4 µs | 34.8 µs |
| rounds | 1.23456789+0.125i, 3 | 43.1 µs | 60.0 µs | 76.2 µs | 93.1 µs | 203.1 µs | 559.7 µs |
| sec | 0.7+0.125i | 17.7 µs | 18.0 µs | 24.5 µs | 42.7 µs | 109.7 µs | 352.7 µs |
| sech | 0.7+0.125i | 10.5 µs | 14.6 µs | 23.6 µs | 116.5 µs | 84.8 µs | 258.1 µs |
| sgn | 0.7 | 753.9 ns | 825.8 ns | 763.3 ns | 1.1 µs | 800.0 ns | 892.4 ns |
| sin | 0.567+0.125i | 16.0 µs | 14.6 µs | 24.5 µs | 35.8 µs | 109.7 µs | 362.6 µs |
| sinc | 0.7+0.125i | 18.1 µs | 28.3 µs | 34.7 µs | 46.6 µs | 125.4 µs | 380.1 µs |
| sinh | 0.7+0.125i | 8.2 µs | 13.2 µs | 22.2 µs | 84.5 µs | 82.6 µs | 250.8 µs |
| sqr | 0.7+0.125i | 1.6 µs | 1.7 µs | 2.0 µs | 2.5 µs | 3.7 µs | 7.5 µs |
| sqrt | 1.23456789+0.125i | 2.5 µs | 2.6 µs | 3.4 µs | 4.2 µs | 5.9 µs | 12.6 µs |
| step | 0.7 | 799.5 ns | 823.3 ns | 881.3 ns | 823.1 ns | 826.6 ns | 846.3 ns |
| struve_H | 2, 0.7+0.125i | 52.5 µs | 113.1 µs | 233.5 µs | 379.0 µs | 1.113 ms | 3.914 ms |
| struve_L | 2, 0.7+0.125i | 55.0 µs | 115.2 µs | 214.3 µs | 336.5 µs | 1.031 ms | 4.466 ms |
| sub | 0.7+0.125i, 0.7+0.125i | 374.5 ns | 374.5 ns | 398.3 ns | 357.3 ns | 401.6 ns | 545.9 ns |
| tan | 0.7+0.125i | 13.1 µs | 20.5 µs | 26.8 µs | 40.7 µs | 110.8 µs | 408.0 µs |
| tanh | 0.7+0.125i | 25.9 µs | 16.1 µs | 84.4 µs † | 37.2 µs | 90.4 µs | 278.6 µs |
| tetragamma | 2.345+0.125i | 311.6 µs | 504.7 µs | 882.5 µs | 1.422 ms | 4.946 ms | 21.759 ms |
| tri | 0.7 | 2.3 µs | 2.2 µs | 2.9 µs | 3.3 µs | 4.0 µs | 8.1 µs |
| trigamma | 2.345+0.125i | 250.0 µs | 375.6 µs | 695.0 µs | 1.221 ms | 3.715 ms | 14.651 ms |
| vercos | 0.7+0.125i | 17.9 µs | 18.5 µs | 24.9 µs | 45.3 µs | 92.7 µs | 352.0 µs |
| versin | 0.7+0.125i | 19.3 µs | 18.6 µs | 23.9 µs | 40.5 µs | 94.3 µs | 354.3 µs |
| zatahp | 2.3+0.125i, 1.4+0.125i | 2.157 ms | 4.847 ms | 9.563 ms | 924.0 µs | 2.925 ms | 10.779 ms |
| zatahp (integer order) | -4, 0.75+0.125i | 2.255 ms | 4.145 ms | 7.561 ms | 706.1 µs | 2.837 ms | 14.022 ms |
| zeta | 2.345+0.125i | 572.9 µs | 1.092 ms | 2.409 ms | 4.164 ms | 19.290 ms | 106.555 ms |
| zeta_pair | 2.345+0.125i | 662.9 µs | 1.522 ms | 2.746 ms | 4.215 ms | 19.425 ms | 120.717 ms |
| zetah | 2.3+0.125i, 1.4+0.125i | 2.136 ms | 3.819 ms | 8.121 ms | 630.1 µs | 2.688 ms | 13.424 ms |
| zetap | 2.345+0.125i | 678.4 µs | 1.488 ms | 2.829 ms | 4.237 ms | 18.663 ms | 110.694 ms |

### number / unrestricted integer

| Operation | Inputs | Exact |
|---|---|---:|
| abs | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">-9712853409152031141207811585727780469484705121206232579607434236969009533618825<br/>97201918971499157056640605432269691427807258223374967278846596194834524005116336<br/>17514882293623272276764857474830401626415841035971458657442133370777607803464071<br/>101158385124186229995073045427448918227959893082282919863982307657776</span> | 111.1 ns |
| add | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 90.2 ns |
| bit_and | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 206.6 ns |
| bit_length | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776</span> | 211.8 ns |
| bit_not | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776</span> | 330.6 ns |
| bit_or | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 201.9 ns |
| bit_xor | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 184.4 ns |
| clear_bit | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>701</span> | 253.5 ns |
| div | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>340282366920938463463374607431768211454</span> | 1.0 µs |
| divmod | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 295.9 ns |
| factorial | 5000 | 127.9 µs |
| factors | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">54772119019154636339867862041163448790974484131459861520687327809209914339836012<br/>52275954799541486775711126005967382600118182797233708133775335788335658037065081<br/>31569062299976033041302189833215742992653063887339218309467131548996199881113600<br/>00000000000000000000000000000000000000000000000000000000000000000000000000000000<br/>0000000000000000000000000000000000000000000000</span> | 97.4 µs |
| fibonacci | 10000 | 4.9 µs |
| gcd | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 3.0 µs |
| gcdext | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 3.3 µs |
| is_prime | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">68647976601306097149819007990813932172694353001433054093944634591855431833976560<br/>52122559640661454554977296311391480858037121987999716643812574028291115057151</span> | 426.9 µs |
| isqrt | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776</span> | 332.5 ns |
| lcm | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 2.8 µs |
| mod | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 225.5 ns |
| modinv | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>68647976601306097149819007990813932172694353001433054093944634591855431833976560<br/>52122559640661454554977296311391480858037121987999716643812574028291115057151</span> | 3.5 µs |
| mul | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 214.7 ns |
| neg | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776</span> | 57.8 ns |
| next_prime | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 3.402 ms |
| partition | 5000 | 5.990 ms |
| pow | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>5</span> | 3.4 µs |
| pow_int | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>5</span> | 2.5 µs |
| powmod | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662, <br/>68647976601306097149819007990813932172694353001433054093944634591855431833976560<br/>52122559640661454554977296311391480858037121987999716643812574028291115057151</span> | 104.3 µs |
| prev_prime | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 4.516 ms |
| prove_prime | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">68647976601306097149819007990813932172694353001433054093944634591855431833976560<br/>52122559640661454554977296311391480858037121987999716643812574028291115057151</span> | 3.038 ms |
| set_bit | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>700</span> | 283.2 ns |
| shl | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>257</span> | 403.2 ns |
| shr | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>257</span> | 277.6 ns |
| sqr | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776</span> | 256.9 ns |
| sub | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 79.8 ns |
| test_bit | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>701</span> | 280.9 ns |

### number / unrestricted fraction

| Operation | Inputs | Exact |
|---|---|---:|
| abs | $\frac{\begin{array}{l}\text{-9712853409152031141207811585727780469484705121206232579607434236969009533618825} \\ \text{97201918971499157056640605432269691427807258223374967278846596194834524005116336} \\ \text{17514882293623272276764857474830401626415841035971458657442133370777607803464071} \\ \text{101158385124186229995073045427448918227959893082282919863982307657776}\end{array}}{\begin{array}{l}\text{68647976601306097149819007990813932172694353001433054093944634591855431833976560} \\ \text{52122559640661454554977296311391480858037121987999716643812574028291115057151}\end{array}}$ | 189.6 ns |
| add | **Input 1**<br/>$\frac{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}{\begin{array}{l}\text{68647976601306097149819007990813932172694353001433054093944634591855431833976560} \\ \text{52122559640661454554977296311391480858037121987999716643812574028291115057151}\end{array}}$<br/><br/>**Input 2**<br/>$\frac{\begin{array}{l}\text{12498846048367045926370716602609982839721641429222959125376462582674869403443996} \\ \text{972648628699047686839582654905384579341977328653591547736951147466435189662}\end{array}}{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}$ | 4.2 µs |
| div | **Input 1**<br/>$\frac{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}{\begin{array}{l}\text{68647976601306097149819007990813932172694353001433054093944634591855431833976560} \\ \text{52122559640661454554977296311391480858037121987999716643812574028291115057151}\end{array}}$<br/><br/>**Input 2**<br/>$\frac{\begin{array}{l}\text{12498846048367045926370716602609982839721641429222959125376462582674869403443996} \\ \text{972648628699047686839582654905384579341977328653591547736951147466435189662}\end{array}}{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}$ | 5.1 µs |
| inv | $\frac{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}{\begin{array}{l}\text{68647976601306097149819007990813932172694353001433054093944634591855431833976560} \\ \text{52122559640661454554977296311391480858037121987999716643812574028291115057151}\end{array}}$ | 137.9 ns |
| mul | **Input 1**<br/>$\frac{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}{\begin{array}{l}\text{68647976601306097149819007990813932172694353001433054093944634591855431833976560} \\ \text{52122559640661454554977296311391480858037121987999716643812574028291115057151}\end{array}}$<br/><br/>**Input 2**<br/>$\frac{\begin{array}{l}\text{12498846048367045926370716602609982839721641429222959125376462582674869403443996} \\ \text{972648628699047686839582654905384579341977328653591547736951147466435189662}\end{array}}{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}$ | 2.2 µs |
| neg | $\frac{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}{\begin{array}{l}\text{68647976601306097149819007990813932172694353001433054093944634591855431833976560} \\ \text{52122559640661454554977296311391480858037121987999716643812574028291115057151}\end{array}}$ | 144.5 ns |
| pow | **Input 1**<br/>$\frac{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}{\begin{array}{l}\text{68647976601306097149819007990813932172694353001433054093944634591855431833976560} \\ \text{52122559640661454554977296311391480858037121987999716643812574028291115057151}\end{array}}$<br/><br/>**Input 2**<br/>5 | 27.9 µs |
| pow_int | **Input 1**<br/>$\frac{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}{\begin{array}{l}\text{68647976601306097149819007990813932172694353001433054093944634591855431833976560} \\ \text{52122559640661454554977296311391480858037121987999716643812574028291115057151}\end{array}}$<br/><br/>**Input 2**<br/>5 | 29.6 µs |
| sqr | $\frac{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}{\begin{array}{l}\text{68647976601306097149819007990813932172694353001433054093944634591855431833976560} \\ \text{52122559640661454554977296311391480858037121987999716643812574028291115057151}\end{array}}$ | 483.2 ns |
| sub | **Input 1**<br/>$\frac{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}{\begin{array}{l}\text{68647976601306097149819007990813932172694353001433054093944634591855431833976560} \\ \text{52122559640661454554977296311391480858037121987999716643812574028291115057151}\end{array}}$<br/><br/>**Input 2**<br/>$\frac{\begin{array}{l}\text{12498846048367045926370716602609982839721641429222959125376462582674869403443996} \\ \text{972648628699047686839582654905384579341977328653591547736951147466435189662}\end{array}}{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}$ | 3.7 µs |

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
  `tetragamma`, `erf`, `erfc`, `gammainv`, `lambert_W0`, `lambert_Wm1`,
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
- `qf_lambert_W0` and `qf_lambert_Wm1`
- `qf_Ei` and `qf_E1`
- `qf_beta` and `qf_logbeta`

Current sample timings with `MARS_BENCH_SCALE=5` on the benchmark machine:

```text
exp_1                 bits=256  avg_µs=   1.370 avg_ms=     0.001
log_10                bits=256  avg_µs=   2.471 avg_ms=     0.002
gamma_2_3             bits=256  avg_µs=   1.246 avg_ms=     0.001
lgamma_2_3            bits=256  avg_µs=   3.797 avg_ms=     0.004
gammainv_9_5          bits=256  avg_µs=  82.548 avg_ms=     0.083
lambert_Wm1_-0_1      bits=256  avg_µs=  58.889 avg_ms=     0.059
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
- `qc_productlog` and `qc_lambert_Wm1`
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
lambert_Wm1_-0_2_-0_1i       avg_µs=    19.262 avg_ms=     0.019
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
