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
The catalogue includes the MARS-defined `lambert_S` and `lambert_S_finite`
workloads, reported as `lambert_S` and `lambert_S (finite section)`. Their
[definition](expression.md#mars-defined-lambert-sum) and
[initial timings](number.md#mars-defined-lambert-sum-timings) identify the convention explicitly.
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
Benchmark run **207**, 2026-11-17T18:12:25.964Z (UTC).

Machine: Intel(R) N95.

System: Linux 7.0.0-27-generic x86_64.

Compiler: 15.2.0; build flags: gcc -Wall -Wextra -Werror -O2 -fPIC -D_GNU_SOURCE -pthread -DHAVE_UNISTRING -I/usr/include/sqlcipher -DSQLITE_HAS_CODEC   -I/usr/include/x86_64-linux-gnu -isystem /usr/include/mit-krb5 -isystem /usr/include/mit-krb5 -I/usr/include/p11-kit-1.

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
| E1 | 2.345678 | 8.9 µs |
| Ei | 2.345678 | 7.9 µs |
| Li | 2.345 | 7.2 µs |
| abs | 0.7 | 9.0 ns |
| acos | 0.7 | 19.3 ns |
| acosec | 2.345 | 17.8 ns |
| acosech | 0.7 | 50.6 ns |
| acosh | 2.345 | 33.4 ns |
| acot | 0.7 | 45.3 ns |
| acoth | 2.345 | 30.2 ns |
| add | 0.7, 0.7 | 15.5 ns |
| add_long | 17, 3 | 518.4 ns |
| appell_F1 | 0.5, 0.7, 0.8, 2.5, 0.2, 0.3 | 28.9 µs |
| arccovercos | 0.5 | 119.5 ns |
| arccoversin | 0.5 | 117.0 ns |
| archacovercos | 0.5 | 253.1 ns |
| archacoversin | 0.5 | 256.1 ns |
| archavercos | 0.5 | 244.5 ns |
| archaversin | 0.5 | 216.3 ns |
| arcvercos | 0.5 | 117.1 ns |
| arcversin | 0.5 | 109.6 ns |
| arg | 0.7 | 76.0 ns |
| asec | 2.345 | 18.0 ns |
| asech | 0.7 | 36.7 ns |
| asin | 0.7 | 19.1 ns |
| asinh | 0.7 | 52.0 ns |
| atan | 0.7 | 32.1 ns |
| atan2 | 0.5, -0.75 | 43.1 ns |
| atanh | 0.7 | 26.1 ns |
| bessel_I | 2, 0.7 | 22.5 µs |
| bessel_J | 2, 0.7 | 6.4 µs |
| bessel_K | 2, 0.7 | 21.7 µs |
| bessel_Y | 2, 0.7 | 20.3 µs |
| beta | 2.3, 4.5 | 134.9 ns |
| beta_pdf | 0.5, 2.5, 3.5 | 172.3 ns |
| binomial | 5, 2 | 185.3 ns |
| ceil | 0.7 | 116.5 ns |
| chebyshev_Tn | 2, 0.7 | 784.0 ns |
| chebyshev_Un | 2, 0.7 | 1.3 µs |
| circ | 0.7 | 292.9 ns |
| clausen | 3, 0.7 | 6.9 µs |
| clausen2 | 0.7 | 7.9 µs |
| conj | 0.7 | 9.0 ns |
| cos | 0.7 | 39.7 ns |
| cosec | 0.7 | 33.2 ns |
| cosech | 0.7 | 56.5 ns |
| cosh | 0.7 | 41.2 ns |
| cot | 0.7 | 42.9 ns |
| coth | 0.7 | 66.3 ns |
| covercos | 0.7 | 120.0 ns |
| coversin | 0.7 | 121.8 ns |
| cubrt | 0.7 | 7.4 µs |
| digamma | 2.345 | 2.8 µs |
| dilog | 0.7 | 15.3 µs |
| div | 0.7, 0.7 | 12.6 ns |
| erf | 0.7 | 77.0 ns |
| erfc | 0.7 | 74.1 ns |
| erfcinv | 0.5 | 318.2 ns |
| erfinv | 0.5 | 322.8 ns |
| exp | 1.23456789 | 37.9 ns |
| floor | 0.7 | 9.2 ns |
| gamma | 2.345 | 168.1 ns |
| gammainc_P | 2.3, 1.4 | 12.9 µs |
| gammainc_Q | 2.3, 1.4 | 14.6 µs |
| gammainc_lower | 2.3, 1.4 | 15.3 µs |
| gammainc_upper | 2.3, 1.4 | 15.1 µs |
| gammainv | 9.5 | 101.8 µs |
| hacovercos | 0.7 | 232.7 ns |
| hacoversin | 0.7 | 233.9 ns |
| harmonic_poly | 2, 0.7 | 1.8 µs |
| havercos | 0.7 | 231.2 ns |
| haversin | 0.7 | 250.3 ns |
| hermite_Hn | 2, 0.7 | 1.7 µs |
| hypergeometric_pFq | 0.7, 2.5, 0.3 | 12.5 µs |
| hypot | 0.7, 1.2 | 24.2 ns |
| imag_part | 0.7 | 29.7 ns |
| inv | 0.7 | 7.9 ns |
| lambert_W0 | 0.7 | 17.3 µs |
| lambert_Wm1 | -0.2 | 15.1 µs |
| lambert_Wn | 1, 0.7 | 28.4 µs |
| lauricella_F | 0.5, 0.3, 2.5 | 17.9 µs |
| ldexp | 1.23456789, 3 | 42.4 ns |
| legendre_chi | 2, 0.7 | 28.0 µs |
| lerch_phi | 0.3, 2, 1.4 | 45.0 µs |
| lerch_phi (integer order) | 0.3125, -15, 0.375 | 27.2 µs |
| lerch_phi (negative order) | 0.3125, -15, 0.375 | 25.7 µs |
| lg | 2.345678 | 22.9 ns |
| lgamma | 2.345 | 56.4 ns |
| ln | 2.345678 | 15.6 ns |
| log | 2.345678 | 14.3 ns |
| log10 | 2.345678 | 22.7 ns |
| logbeta | 2.3, 4.5 | 144.6 ns |
| logbeta_pdf | 0.5, 2.5, 3.5 | 154.3 ns |
| lommel_S | 0.5, 0.7, 1.2 | 7.3 µs |
| mul | 0.7, 0.7 | 16.0 ns |
| mul_long | 17, 3 | 583.7 ns |
| mul_pow10 | 1.23456789, 3 | 69.5 ns |
| neg | 0.7 | 9.0 ns |
| normal_cdf | 0.7 | 75.5 ns |
| normal_logpdf | 0.7 | 16.5 ns |
| normal_pdf | 0.7 | 25.7 ns |
| polygamma | 3, 2.345 | 30.2 µs |
| polylog | 2, 0.7 | 17.6 µs |
| polylog1 | 0.7 | 132.2 ns |
| pow | 1.23456789, 3.5 | 37.4 ns |
| pow10 | 3 | 182.4 ns |
| pow_int | 1.23456789, 3 | 63.7 ns |
| productlog | 0.7 | 16.9 µs |
| qdigamma | 0.5, 2.3 | 24.9 µs |
| real_part | 0.7 | 27.6 ns |
| rect | 0.7 | 284.2 ns |
| root | 1.2345, 3 | 9.3 µs |
| round | 0.7 | 429.8 ns |
| roundn | 1.23456789, 3 | 5.0 µs |
| rounds | 1.23456789, 3 | 6.5 µs |
| sec | 0.7 | 43.5 ns |
| sech | 0.7 | 35.5 ns |
| sgn | 0.7 | 140.0 ns |
| sin | 0.567 | 32.9 ns |
| sinc | 0.7 | 3.4 µs |
| sincos | 0.7 | 660.0 ns |
| sinh | 0.7 | 58.2 ns |
| sinhcosh | 0.7 | 121.2 ns |
| sqr | 0.7 | 9.4 ns |
| sqrt | 1.23456789 | 9.1 ns |
| step | 0.7 | 130.5 ns |
| struve_H | 2, 0.7 | 30.0 µs |
| struve_L | 2, 0.7 | 28.2 µs |
| sub | 0.7, 0.7 | 13.2 ns |
| tan | 0.7 | 6.4 µs |
| tanh | 0.7 | 62.1 ns |
| tetragamma | 2.345 | 3.5 µs |
| tri | 0.7 | 276.5 ns |
| trigamma | 2.345 | 2.8 µs |
| vercos | 0.7 | 137.1 ns |
| versin | 0.7 | 142.4 ns |
| zatahp | 2.3, 1.4 | 50.4 µs |
| zatahp (integer order) | -4, 0.75 | 44.7 µs |
| zeta | 2.345 | 79.2 µs |
| zeta_pair | 2.345 | 182.9 µs |
| zetah | 2.3, 1.4 | 25.9 µs |
| zetap | 2.345 | 112.3 µs |

### `qfloat`

| Operation | Inputs | 106 bits |
|---|---|---:|
| E1 | 2.345678 | 8.1 µs |
| Ei | 2.345678 | 8.5 µs |
| Li | 2.345 | 9.9 µs |
| abs | 0.7 | 2.6 ns |
| acos | 0.7 | 1.9 µs |
| acosec | 2.345 | 1.9 µs |
| acosech | 0.7 | 4.0 µs |
| acosh | 2.345 | 4.4 µs |
| acot | 0.7 | 1.5 µs |
| acoth | 2.345 | 4.0 µs |
| add | 0.7, 0.7 | 8.5 ns |
| appell_F1 | 0.5, 0.7, 0.8, 2.5, 0.2, 0.3 | 97.1 µs |
| arccovercos | 0.5 | 1.8 µs |
| arccoversin | 0.5 | 1.7 µs |
| archacovercos | 0.5 | 31.3 ns |
| archacoversin | 0.5 | 30.0 ns |
| archavercos | 0.5 | 59.0 ns |
| archaversin | 0.5 | 56.2 ns |
| arcvercos | 0.5 | 1.7 µs |
| arcversin | 0.5 | 1.7 µs |
| asec | 2.345 | 1.9 µs |
| asech | 0.7 | 4.0 µs |
| asin | 0.7 | 1.9 µs |
| asinh | 0.7 | 3.7 µs |
| atan | 0.7 | 1.1 µs |
| atan2 | 0.5, -0.75 | 1.4 µs |
| atanh | 0.7 | 3.3 µs |
| bessel_I | 2, 0.7 | 32.7 µs |
| bessel_J | 2, 0.7 | 7.9 µs |
| bessel_K | 2, 0.7 | 38.6 µs |
| bessel_Y | 2, 0.7 | 29.0 µs |
| beta | 2.3, 4.5 | 5.9 µs |
| beta_pdf | 0.5, 2.5, 3.5 | 36.1 µs |
| binomial | 5, 2 | 33.2 µs |
| chebyshev_Tn | 2, 0.7 | 42.4 ns |
| chebyshev_Un | 2, 0.7 | 42.4 ns |
| circ | 0.7 | 17.1 ns |
| clausen | 3, 0.7 | 335.2 µs |
| clausen2 | 0.7 | 5.4 µs |
| cos | 0.7 | 849.9 ns |
| cosec | 0.7 | 1.0 µs |
| cosech | 0.7 | 3.4 µs |
| cosh | 0.7 | 3.8 µs |
| cot | 0.7 | 1.9 µs |
| coth | 0.7 | 7.3 µs |
| covercos | 0.7 | 899.0 ns |
| coversin | 0.7 | 937.5 ns |
| cubrt | 0.7 | 5.1 µs |
| digamma | 2.345 | 2.7 µs |
| dilog | 0.7 | 15.7 µs |
| div | 0.7, 0.7 | 35.7 ns |
| erf | 0.7 | 4.3 µs |
| erfc | 0.7 | 4.2 µs |
| erfcinv | 0.5 | 239.1 µs |
| erfinv | 0.5 | 234.5 µs |
| exp | 1.23456789 | 1.7 µs |
| floor | 0.7 | 20.8 ns |
| gamma | 2.345 | 1.7 µs |
| gammainc_P | 2.3, 1.4 | 14.0 µs |
| gammainc_Q | 2.3, 1.4 | 13.3 µs |
| gammainc_lower | 2.3, 1.4 | 15.4 µs |
| gammainc_upper | 2.3, 1.4 | 13.9 µs |
| gammainv | 9.5 | 100.9 µs |
| hacovercos | 0.7 | 887.1 ns |
| hacoversin | 0.7 | 987.8 ns |
| harmonic_poly | 2, 0.7 | 109.5 ns |
| havercos | 0.7 | 974.2 ns |
| haversin | 0.7 | 878.6 ns |
| hermite_Hn | 2, 0.7 | 53.3 ns |
| hypergeometric_pFq | 0.7, 2.5, 0.3 | 4.1 µs |
| hypot | 0.7, 1.2 | 434.8 ns |
| lambert_W0 | 0.7 | 14.7 µs |
| lambert_Wm1 | -0.2 | 18.0 µs |
| lauricella_F | 0.5, 0.3, 2.5 | 20.4 µs |
| ldexp | 1.23456789, 3 | 10.6 ns |
| legendre_chi | 2, 0.7 | 29.7 µs |
| lerch_phi | 0.3, 2, 1.4 | 9.0 µs |
| lerch_phi (integer order) | 0.3125, -15, 0.375 | 29.6 µs |
| lerch_phi (negative order) | 0.3125, -15, 0.375 | 31.9 µs |
| lg | 2.345678 | 3.0 µs |
| lgamma | 2.345 | 6.0 µs |
| ln | 2.345678 | 6.2 µs † |
| log | 2.345678 | 3.2 µs |
| log10 | 2.345678 | 3.3 µs |
| logbeta | 2.3, 4.5 | 30.1 µs |
| logbeta_pdf | 0.5, 2.5, 3.5 | 32.7 µs |
| lommel_S | 0.5, 0.7, 1.2 | 7.5 µs |
| mul | 0.7, 0.7 | 7.3 ns |
| mul_pow10 | 1.23456789, 3 | 42.0 ns |
| neg | 0.7 | 2.1 ns |
| normal_cdf | 0.7 | 3.7 µs |
| normal_logpdf | 0.7 | 34.1 ns |
| normal_pdf | 0.7 | 1.8 µs |
| polygamma | 3, 2.345 | 5.1 µs |
| polylog | 2, 0.7 | 15.1 µs |
| polylog1 | 0.7 | 3.3 µs |
| pow | 1.23456789, 3.5 | 5.1 µs |
| pow10 | 3 | 29.4 ns |
| pow_int | 1.23456789, 3 | 49.0 ns |
| productlog | 0.7 | 15.4 µs |
| qdigamma | 0.5, 2.3 | 24.5 µs |
| rect | 0.7 | 15.6 ns |
| root | 1.2345, 3 | 4.6 µs |
| round | 0.7 | 41.3 ns |
| roundn | 1.23456789, 3 | 5.9 µs |
| rounds | 1.23456789, 3 | 16.6 µs |
| sec | 0.7 | 860.2 ns |
| sech | 0.7 | 3.4 µs |
| sgn | 0.7 | 8.5 ns |
| sin | 0.567 | 896.8 ns |
| sinc | 0.7 | 922.7 ns |
| sinh | 0.7 | 3.7 µs |
| sqr | 0.7 | 5.1 ns |
| sqrt | 1.23456789 | 382.9 ns |
| step | 0.7 | 7.6 ns |
| struve_H | 2, 0.7 | 37.0 µs |
| struve_L | 2, 0.7 | 41.9 µs |
| sub | 0.7, 0.7 | 7.8 ns |
| tan | 0.7 | 2.2 µs |
| tanh | 0.7 | 2.3 µs |
| tetragamma | 2.345 | 3.5 µs |
| tri | 0.7 | 13.5 ns |
| trigamma | 2.345 | 3.0 µs |
| vercos | 0.7 | 902.6 ns |
| versin | 0.7 | 973.6 ns |
| zatahp | 2.3, 1.4 | 342.6 µs |
| zatahp (integer order) | -4, 0.75 | 351.8 µs |
| zeta | 2.345 | 329.2 µs |
| zetah | 2.3, 1.4 | 330.9 µs |
| zetap | 2.345 | 311.5 µs |

### `qcomplex`

| Operation | Inputs | 106 bits |
|---|---|---:|
| E1 | 2.345678+0.125i | 52.4 µs |
| Ei | 2.345678+0.125i | 49.1 µs |
| Li | 2.345+0.125i | 46.9 µs |
| abs | 0.7+0.125i | 502.1 ns |
| acos | 0.7+0.125i | 9.7 µs |
| acosec | 2.345+0.125i | 9.6 µs |
| acosech | 0.7+0.125i | 9.8 µs |
| acosh | 2.345+0.125i | 13.2 µs |
| acot | 0.7+0.125i | 10.4 µs |
| acoth | 2.345+0.125i | 5.1 µs |
| add | 0.7+0.125i, 0.7+0.125i | 15.6 ns |
| appell_F1 | <span style="display:inline-block;width:40ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">0.5+0.125i, 0.7+0.125i, 0.8+0.125i, 2.5+<br/>0.125i, 0.2+0.125i, 0.3+0.125i</span> | 611.8 µs |
| arccovercos | 0.5+0.125i | 9.5 µs |
| arccoversin | 0.5+0.125i | 9.6 µs |
| archacovercos | 0.5+0.125i | 5.3 µs |
| archacoversin | 0.5+0.125i | 5.3 µs |
| archavercos | 0.5+0.125i | 5.8 µs |
| archaversin | 0.5+0.125i | 5.4 µs |
| arcvercos | 0.5+0.125i | 9.5 µs |
| arcversin | 0.5+0.125i | 10.0 µs |
| arg | 0.7+0.125i | 1.2 µs |
| asec | 2.345+0.125i | 9.1 µs |
| asech | 0.7+0.125i | 14.0 µs |
| asin | 0.7+0.125i | 9.7 µs |
| asinh | 0.7+0.125i | 10.5 µs |
| atan | 0.7+0.125i | 10.5 µs |
| atan2 | 0.5+0.125i, -0.75+0.125i | 10.7 µs |
| atanh | 0.7+0.125i | 5.5 µs |
| bessel_I | 2, 0.7+0.125i | 199.7 µs † |
| bessel_J | 2, 0.7+0.125i | 67.0 µs |
| bessel_K | 2, 0.7+0.125i | 70.4 µs |
| bessel_Y | 2, 0.7+0.125i | 105.4 µs |
| beta | 2.3+0.125i, 4.5+0.125i | 40.8 µs |
| beta_pdf | 0.5+0.125i, 2.5+0.125i, 3.5+0.125i | 58.2 µs |
| binomial | 5+0.125i, 2+0.125i | 30.4 µs |
| chebyshev_Tn | 2, 0.7+0.125i | 171.0 ns |
| chebyshev_Un | 2, 0.7+0.125i | 185.1 ns |
| circ | 0.7 | 20.5 ns |
| clausen | 3, 0.7+0.125i | 358.6 µs |
| clausen2 | 0.7+0.125i | 23.9 µs |
| conj | 0.7+0.125i | 3.9 ns |
| cos | 0.7+0.125i | 8.7 µs |
| cosec | 0.7+0.125i | 9.2 µs |
| cosech | 0.7+0.125i | 8.9 µs |
| cosh | 0.7+0.125i | 8.5 µs |
| cot | 0.7+0.125i | 18.8 µs |
| coth | 0.7+0.125i | 17.6 µs |
| covercos | 0.7+0.125i | 9.3 µs |
| coversin | 0.7+0.125i | 9.2 µs |
| cubrt | 0.7+0.125i | 8.6 µs |
| digamma | 2.345+0.125i | 12.9 µs |
| dilog | 0.7+0.125i | 101.1 µs |
| div | 0.7+0.125i, 0.7+0.125i | 216.9 ns |
| erf | 0.7+0.125i | 49.1 µs |
| erfc | 0.7+0.125i | 51.6 µs |
| erfcinv | 0.5+0.125i | 390.2 µs |
| erfinv | 0.5+0.125i | 328.1 µs |
| exp | 1.23456789+0.125i | 3.7 µs |
| floor | 0.7+0.125i | 69.2 ns |
| gamma | 2.345+0.125i | 17.9 µs |
| gammainc_P | 2.3+0.125i, 1.4+0.125i | 68.7 µs |
| gammainc_Q | 2.3+0.125i, 1.4+0.125i | 64.8 µs |
| gammainc_lower | 2.3+0.125i, 1.4+0.125i | 53.7 µs |
| gammainc_upper | 2.3+0.125i, 1.4+0.125i | 63.3 µs |
| gammainv | 9.5+0.125i | 142.5 µs |
| hacovercos | 0.7+0.125i | 9.0 µs |
| hacoversin | 0.7+0.125i | 9.4 µs |
| harmonic_poly | 2, 0.7+0.125i | 614.9 ns |
| havercos | 0.7+0.125i | 9.5 µs |
| haversin | 0.7+0.125i | 8.9 µs |
| hermite_Hn | 2, 0.7+0.125i | 202.0 ns |
| hypergeometric_pFq | 0.7+0.125i, 2.5+0.125i, 0.3+0.125i | 37.0 µs |
| hypot | 0.7+0.125i, 1.2+0.125i | 1.5 µs |
| imag_part | 0.7+0.125i | 2.6 ns |
| inv | 0.7+0.125i | 179.8 ns |
| lambert_W0 | 0.7+0.125i | 31.9 µs |
| lambert_Wm1 | -0.2+0.125i | 31.1 µs |
| lambert_Wn | 1, 0.7+0.125i | 30.9 µs |
| lauricella_F | 0.5+0.125i, 0.3+0.125i, 2.5+0.125i | 161.8 µs |
| ldexp | 1.23456789+0.125i, 3 | 24.7 ns |
| legendre_chi | 2, 0.7+0.125i | 369.0 µs |
| lerch_phi | 0.3+0.125i, 2+0.125i, 1.4+0.125i | 643.7 µs |
| lerch_phi (integer order) | 0.3125+0.125i, -15, 0.375+0.125i | 221.6 µs |
| lerch_phi (negative order) | 0.3125+0.125i, -15+0.125i, 0.375+0.125i | 1.057 ms |
| lg | 2.345678+0.125i | 5.0 µs |
| lgamma | 2.345+0.125i | 11.1 µs |
| ln | 2.345678+0.125i | 5.1 µs |
| log | 2.345678+0.125i | 5.0 µs |
| log10 | 2.345678+0.125i | 5.2 µs |
| logbeta | 2.3+0.125i, 4.5+0.125i | 34.7 µs |
| logbeta_pdf | 0.5+0.125i, 2.5+0.125i, 3.5+0.125i | 48.1 µs |
| lommel_S | 0.5+0.125i, 0.7+0.125i, 1.2+0.125i | 141.3 µs |
| mul | 0.7+0.125i, 0.7+0.125i | 56.4 ns |
| neg | 0.7+0.125i | 5.7 ns |
| normal_cdf | 0.7+0.125i | 45.9 µs |
| normal_logpdf | 0.7+0.125i | 153.7 ns |
| normal_pdf | 0.7+0.125i | 5.0 µs † |
| polygamma | 3, 2.345+0.125i | 14.2 µs |
| polylog | 2, 0.7+0.125i | 90.0 µs |
| polylog1 | 0.7+0.125i | 4.9 µs |
| pow | 1.23456789+0.125i, 3.5+0.125i | 8.5 µs |
| productlog | 0.7+0.125i | 35.2 µs |
| qdigamma | 0.5+0.125i, 2.3+0.125i | 173.9 µs |
| real_part | 0.7+0.125i | 3.2 ns |
| rect | 0.7 | 22.2 ns |
| root | 1.2345+0.125i, 3 | 8.9 µs |
| round | 0.7+0.125i | 86.2 ns |
| roundn | 1.23456789+0.125i, 3 | 13.6 µs |
| rounds | 1.23456789+0.125i, 3 | 37.5 µs |
| sec | 0.7+0.125i | 9.5 µs |
| sech | 0.7+0.125i | 9.5 µs |
| sgn | 0.7 | 11.8 ns |
| sin | 0.567+0.125i | 11.0 µs |
| sinc | 0.7+0.125i | 9.2 µs |
| sinh | 0.7+0.125i | 9.7 µs |
| sqr | 0.7+0.125i | 55.1 ns |
| sqrt | 1.23456789+0.125i | 4.2 µs |
| step | 0.7 | 10.8 ns |
| struve_H | 2, 0.7+0.125i | 71.7 µs |
| struve_L | 2, 0.7+0.125i | 70.1 µs |
| sub | 0.7+0.125i, 0.7+0.125i | 16.3 ns |
| tan | 0.7+0.125i | 22.5 µs |
| tanh | 0.7+0.125i | 20.6 µs |
| tetragamma | 2.345+0.125i | 4.9 µs |
| tri | 0.7 | 19.3 ns |
| trigamma | 2.345+0.125i | 5.5 µs † |
| vercos | 0.7+0.125i | 10.6 µs |
| versin | 0.7+0.125i | 10.1 µs |
| zatahp | 2.3+0.125i, 1.4+0.125i | 679.2 µs |
| zatahp (integer order) | -4, 0.75+0.125i | 703.1 µs |
| zeta | 2.345+0.125i | 473.6 µs |
| zetah | 2.3+0.125i, 1.4+0.125i | 635.6 µs |
| zetap | 2.345+0.125i | 480.6 µs |

### `mpfr`

| Operation | Inputs | 256 bits | 512 bits | 768 bits | 1024 bits | 2048 bits | 4096 bits |
|---|---|---:|---:|---:|---:|---:|---:|
| E1 | 2.345678 | 52.8 µs | 90.4 µs | 131.9 µs | 172.1 µs | 493.0 µs | 2.213 ms |
| Ei | 2.345678 | 47.7 µs | 94.7 µs † | 119.5 µs | 171.6 µs | 550.3 µs | 2.192 ms |
| Li | 2.345 | 45.8 µs | 71.6 µs | 108.4 µs | 157.6 µs | 495.1 µs | 1.947 ms |
| abs | 0.7 | 106.3 ns | 88.5 ns | 105.0 ns | 98.2 ns | 106.3 ns | 107.0 ns |
| acos | 0.7 | 19.0 µs | 31.1 µs | 36.1 µs | 59.0 µs | 105.2 µs | 295.5 µs |
| acosec | 2.345 | 25.6 µs † | 31.3 µs | 36.3 µs | 52.3 µs | 115.4 µs | 356.0 µs |
| acosech | 0.7 | 6.6 µs | 9.8 µs | 14.4 µs | 18.6 µs | 47.0 µs | 128.1 µs |
| acosh | 2.345 | 7.6 µs | 10.0 µs | 14.5 µs | 16.7 µs | 41.3 µs | 123.1 µs |
| acot | 0.7 | 19.9 µs | 23.5 µs | 38.3 µs | 49.2 µs | 118.2 µs | 357.7 µs |
| acoth | 2.345 | 9.6 µs † | 8.9 µs | 14.4 µs | 20.0 µs | 46.4 µs | 130.9 µs |
| add | 0.7, 0.7 | 136.7 ns | 108.1 ns | 119.2 ns | 117.1 ns | 187.5 ns | 364.9 ns |
| add_long | 17, 3 | 859.2 ns | 732.9 ns | 753.2 ns | 779.2 ns | 767.4 ns | 855.2 ns |
| appell_F1 | 0.5, 0.7, 0.8, 2.5, 0.2, 0.3 | 166.9 µs | 398.1 µs | 686.0 µs | 1.116 ms | 3.176 ms | 11.274 ms |
| arccovercos | 0.5 | 19.4 µs † | 27.2 µs | 32.3 µs | 48.2 µs | 127.9 µs | 296.7 µs |
| arccoversin | 0.5 | 21.0 µs † | 23.9 µs | 32.2 µs | 49.6 µs | 112.6 µs | 308.6 µs |
| archacovercos | 0.5 | 1.6 µs | 1.5 µs | 1.6 µs | 1.4 µs | 2.1 µs | 1.8 µs |
| archacoversin | 0.5 | 2.3 µs † | 1.4 µs | 1.9 µs | 1.5 µs | 1.6 µs | 2.1 µs |
| archavercos | 0.5 | 2.3 µs † | 1.5 µs | 1.7 µs | 1.9 µs | 1.8 µs | 2.3 µs |
| archaversin | 0.5 | 1.8 µs | 1.8 µs | 1.7 µs | 1.7 µs | 1.7 µs | 1.9 µs |
| arcvercos | 0.5 | 18.0 µs | 26.4 µs | 31.5 µs | 52.5 µs | 107.0 µs | 320.5 µs |
| arcversin | 0.5 | 17.0 µs | 26.6 µs | 43.9 µs | 47.9 µs | 110.5 µs | 300.2 µs |
| arg | 0.7 | 713.4 ns | 630.7 ns | 529.6 ns | 546.4 ns | 524.8 ns | 584.6 ns |
| asec | 2.345 | 22.1 µs | 28.4 µs | 39.0 µs | 59.0 µs | 124.8 µs | 353.1 µs |
| asech | 0.7 | 7.3 µs | 12.2 µs | 14.0 µs | 23.0 µs | 43.2 µs | 122.2 µs |
| asin | 0.7 | 25.9 µs † | 29.6 µs | 40.0 µs | 52.7 µs | 117.2 µs | 318.7 µs |
| asinh | 0.7 | 7.1 µs | 11.4 µs | 13.6 µs | 17.0 µs | 41.8 µs | 129.4 µs |
| atan | 0.7 | 15.3 µs | 27.2 µs | 33.5 µs | 45.9 µs | 103.4 µs | 377.2 µs |
| atan2 | 0.5, -0.75 | 16.3 µs | 29.9 µs | 31.3 µs | 48.2 µs | 114.5 µs | 317.6 µs |
| atanh | 0.7 | 8.4 µs | 11.9 µs | 14.8 µs | 18.6 µs | 43.6 µs | 110.8 µs |
| bessel_I | 2, 0.7 | 62.4 µs | 104.3 µs | 152.9 µs | 255.4 µs | 835.6 µs | 3.720 ms |
| bessel_J | 2, 0.7 | 10.1 µs † | 16.5 µs | 25.0 µs | 53.7 µs | 166.6 µs | 815.8 µs |
| bessel_K | 2, 0.7 | 87.3 µs † | 110.1 µs | 166.9 µs | 271.7 µs | 889.2 µs | 4.345 ms |
| bessel_Y | 2, 0.7 | 52.9 µs | 99.7 µs | 142.4 µs | 217.3 µs | 796.5 µs | 4.212 ms |
| beta | 2.3, 4.5 | 146.2 µs † | 284.5 µs | 518.1 µs | 946.6 µs | 3.081 ms | 15.510 ms |
| beta_pdf | 0.5, 2.5, 3.5 | 124.1 µs | 269.1 µs | 355.1 µs | 586.2 µs | 2.077 ms | 11.406 ms |
| binomial | 5, 2 | 146.2 µs | 301.9 µs | 479.5 µs | 742.4 µs | 3.035 ms | 13.847 ms |
| ceil | 0.7 | 683.2 ns | 629.5 ns | 528.7 ns | 517.8 ns | 536.1 ns | 671.8 ns |
| chebyshev_Tn | 2, 0.7 | 4.3 µs † | 3.0 µs | 3.6 µs | 3.3 µs | 4.0 µs | 6.2 µs |
| chebyshev_Un | 2, 0.7 | 5.1 µs † | 4.3 µs | 4.0 µs | 4.2 µs | 4.2 µs | 7.1 µs |
| circ | 0.7 | 1.4 µs † | 901.2 ns | 942.9 ns | 925.8 ns | 1.1 µs | 1.3 µs |
| clausen | 3, 0.7 | 32.5 µs | 74.6 µs | 137.9 µs | 220.2 µs | 975.2 µs | 5.495 ms |
| clausen2 | 0.7 | 36.7 µs | 75.8 µs | 146.2 µs | 229.0 µs | 1.125 ms | 4.790 ms |
| conj | 0.7 | 94.8 ns | 85.4 ns | 90.7 ns | 85.3 ns | 85.2 ns | 108.7 ns |
| cos | 0.7 | 3.0 µs † | 4.7 µs | 8.6 µs | 14.2 µs | 40.7 µs | 144.7 µs |
| cosec | 0.7 | 4.7 µs † | 5.6 µs | 11.0 µs | 15.2 µs | 47.3 µs | 179.1 µs |
| cosech | 0.7 | 3.8 µs | 5.8 µs | 9.7 µs | 15.6 µs | 59.3 µs | 176.2 µs |
| cosh | 0.7 | 3.7 µs | 6.0 µs | 10.4 µs | 14.2 µs | 49.8 µs | 172.1 µs |
| cot | 0.7 | 8.5 µs † | 11.5 µs | 17.5 µs | 28.2 µs | 94.5 µs | 320.8 µs |
| coth | 0.7 | 7.8 µs | 12.3 µs | 20.6 µs | 36.4 µs | 100.0 µs | 365.7 µs |
| covercos | 0.7 | 4.2 µs | 7.0 µs | 10.5 µs | 15.7 µs | 46.3 µs | 188.3 µs |
| coversin | 0.7 | 5.3 µs † | 9.2 µs † | 9.8 µs | 15.9 µs | 42.5 µs | 220.6 µs |
| cubrt | 0.7 | 16.3 µs | 19.8 µs | 31.0 µs | 39.7 µs | 90.2 µs | 323.5 µs |
| digamma | 2.345 | 134.3 µs | 275.2 µs | 508.6 µs | 943.3 µs | 4.152 ms | 21.666 ms |
| dilog | 0.7 | 125.2 µs | 191.8 µs | 400.0 µs | 473.8 µs | 1.328 ms | 4.695 ms |
| div | 0.7, 0.7 | 404.9 ns | 559.4 ns | 974.1 ns | 1.5 µs | 3.4 µs | 11.0 µs |
| erf | 0.7 | 19.8 µs | 41.0 µs | 64.7 µs | 110.7 µs | 482.7 µs | 2.280 ms |
| erfc | 0.7 | 17.9 µs | 45.8 µs | 79.0 µs | 114.1 µs | 469.3 µs | 1.916 ms |
| erfcinv | 0.5 | 97.7 µs | 200.9 µs | 316.3 µs | 566.6 µs | 1.509 ms | 5.738 ms |
| erfinv | 0.5 | 111.2 µs | 203.8 µs | 297.3 µs | 486.3 µs | 1.609 ms | 6.198 ms |
| exp | 1.23456789 | 3.3 µs | 5.6 µs | 9.5 µs | 16.4 µs | 52.3 µs | 189.8 µs |
| floor | 0.7 | 96.5 ns | 93.2 ns | 95.0 ns | 93.6 ns | 116.5 ns | 115.7 ns |
| gamma | 2.345 | 38.3 µs | 101.3 µs | 160.8 µs | 254.2 µs | 996.9 µs | 4.509 ms |
| gammainc_P | 2.3, 1.4 | 113.0 µs | 263.1 µs | 462.4 µs | 635.7 µs | 2.739 ms | 11.351 ms |
| gammainc_Q | 2.3, 1.4 | 152.6 µs | 322.5 µs | 522.0 µs | 863.2 µs | 2.827 ms | 10.792 ms |
| gammainc_lower | 2.3, 1.4 | 70.6 µs | 158.7 µs | 286.0 µs | 432.6 µs | 1.564 ms | 6.298 ms |
| gammainc_upper | 2.3, 1.4 | 134.6 µs | 310.8 µs | 479.3 µs | 805.1 µs | 2.971 ms | 12.024 ms |
| gammainv | 9.5 | 551.5 µs | 1.041 ms | 1.374 ms | 2.178 ms | 5.995 ms | 19.824 ms |
| hacovercos | 0.7 | 5.2 µs | 8.1 µs | 14.2 µs | 16.4 µs | 48.8 µs | 177.8 µs |
| hacoversin | 0.7 | 4.4 µs | 7.9 µs | 25.9 µs † | 19.3 µs | 51.2 µs | 173.9 µs |
| harmonic_poly | 2, 0.7 | 4.0 µs | 6.3 µs | 7.9 µs | 7.4 µs | 12.3 µs | 33.1 µs |
| havercos | 0.7 | 3.5 µs | 6.4 µs | 13.1 µs | 14.4 µs | 56.3 µs | 196.0 µs |
| haversin | 0.7 | 3.5 µs | 6.9 µs | 12.6 µs | 15.3 µs | 47.0 µs | 172.2 µs |
| hermite_Hn | 2, 0.7 | 4.5 µs | 3.9 µs | 6.2 µs † | 4.8 µs | 5.6 µs | 8.7 µs |
| hypergeometric_pFq | 0.7, 2.5, 0.3 | 58.4 µs | 121.5 µs | 197.0 µs | 291.3 µs | 1.336 ms | 7.023 ms |
| hypot | 0.7, 1.2 | 698.8 ns | 1.1 µs | 1.3 µs | 1.7 µs | 3.1 µs | 9.3 µs |
| imag_part | 0.7 | 81.7 ns | 77.0 ns | 82.4 ns | 83.3 ns | 84.4 ns | 83.3 ns |
| inv | 0.7 | 242.5 ns | 303.9 ns | 432.0 ns | 624.9 ns | 1.6 µs | 4.2 µs |
| lambert_W0 | 0.7 | 64.6 µs | 103.9 µs | 185.9 µs | 286.5 µs | 859.5 µs | 3.305 ms |
| lambert_Wm1 | -0.2 | 69.9 µs | 116.2 µs | 209.0 µs | 309.8 µs | 979.2 µs | 3.056 ms |
| ldexp | 1.23456789, 3 | 221.3 ns | 142.3 ns | 216.2 ns | 118.8 ns | 163.6 ns | 133.5 ns |
| legendre_chi | 2, 0.7 | 57.1 µs | 110.1 µs | 183.2 µs | 258.5 µs | 913.4 µs | 3.037 ms |
| lerch_phi | 0.3, 2, 1.4 | 145.7 µs | 242.6 µs | 458.7 µs | 880.8 µs | 3.082 ms | 18.053 ms |
| lerch_phi (integer order) | 0.3125, -15, 0.375 | 32.6 µs | 32.5 µs | 33.9 µs | 34.5 µs | 30.5 µs | 31.1 µs |
| lerch_phi (negative order) | 0.3125, -15, 0.375 | 42.0 µs | 29.7 µs | 29.2 µs | 34.4 µs | 31.3 µs | 40.3 µs |
| lg | 2.345678 | 11.3 µs | 19.0 µs | 26.3 µs | 39.3 µs | 87.6 µs | 249.0 µs |
| lgamma | 2.345 | 37.4 µs | 93.0 µs | 163.2 µs | 290.8 µs | 1.006 ms | 4.367 ms |
| ln | 2.345678 | 5.6 µs | 8.2 µs | 12.2 µs | 17.6 µs | 46.0 µs | 121.6 µs |
| log | 2.345678 | 5.3 µs | 8.9 µs | 12.1 µs | 17.1 µs | 38.7 µs | 118.2 µs |
| log10 | 2.345678 | 12.3 µs | 16.9 µs | 26.1 µs | 44.0 µs | 84.4 µs | 269.1 µs |
| logbeta | 2.3, 4.5 | 124.0 µs | 282.3 µs | 499.4 µs | 801.3 µs | 3.008 ms | 16.288 ms |
| logbeta_pdf | 0.5, 2.5, 3.5 | 97.0 µs | 191.4 µs | 355.1 µs | 565.4 µs | 2.302 ms | 11.201 ms |
| lommel_S | 0.5, 0.7, 1.2 | 29.9 µs | 68.2 µs | 131.7 µs | 212.2 µs | 858.8 µs | 3.511 ms |
| mul | 0.7, 0.7 | 110.7 ns | 191.5 ns | 360.6 ns | 406.1 ns | 1.0 µs | 3.0 µs |
| mul_long | 17, 3 | 742.7 ns | 831.8 ns | 1.1 µs | 769.1 ns | 848.6 ns | 855.1 ns |
| mul_pow10 | 1.23456789, 3 | 352.1 ns | 459.0 ns | 617.4 ns | 614.5 ns | 482.4 ns | 680.9 ns |
| neg | 0.7 | 121.7 ns | 101.4 ns | 97.6 ns | 92.7 ns | 105.7 ns | 99.7 ns |
| normal_cdf | 0.7 | 19.7 µs | 83.9 µs | 79.3 µs | 110.1 µs | 446.7 µs | 1.786 ms |
| normal_logpdf | 0.7 | 5.1 µs | 8.9 µs | 12.7 µs | 17.3 µs | 39.5 µs | 127.6 µs |
| normal_pdf | 0.7 | 3.7 µs | 7.8 µs | 11.6 µs | 16.6 µs | 58.5 µs | 221.5 µs |
| polygamma | 3, 2.345 | 55.1 µs | 136.3 µs | 260.6 µs | 397.4 µs | 2.084 ms | 10.796 ms |
| polylog | 2, 0.7 | 101.3 µs | 189.1 µs | 283.9 µs | 411.5 µs | 1.417 ms | 4.647 ms |
| polylog1 | 0.7 | 6.1 µs | 9.1 µs | 13.3 µs | 18.5 µs | 38.9 µs | 120.3 µs |
| pow | 1.23456789, 3.5 | 10.0 µs | 15.6 µs | 24.2 µs | 42.2 µs | 94.0 µs | 282.3 µs |
| pow10 | 3 | 275.9 ns | 384.4 ns | 390.6 ns | 505.8 ns | 325.9 ns | 619.8 ns |
| pow_int | 1.23456789, 3 | 325.2 ns | 419.4 ns | 592.4 ns | 791.2 ns | 2.6 µs | 7.3 µs |
| productlog | 0.7 | 54.2 µs | 110.9 µs | 181.3 µs | 286.4 µs | 932.8 µs | 3.633 ms |
| real_part | 0.7 | 87.6 ns | 88.1 ns | 84.7 ns | 84.8 ns | 86.1 ns | 171.6 ns |
| rect | 0.7 | 1.0 µs | 1.1 µs | 986.5 ns | 1.2 µs | 1.1 µs | 1.4 µs |
| root | 1.2345, 3 | 13.9 µs | 21.2 µs | 26.2 µs | 39.8 µs | 111.0 µs | 358.1 µs |
| round | 0.7 | 2.5 µs | 2.5 µs | 2.5 µs | 2.9 µs | 3.2 µs | 3.1 µs |
| roundn | 1.23456789, 3 | 5.3 µs | 6.0 µs | 6.2 µs | 6.8 µs | 7.6 µs | 11.4 µs |
| rounds | 1.23456789, 3 | 20.4 µs | 29.8 µs | 42.5 µs | 49.1 µs | 93.4 µs | 239.8 µs |
| sec | 0.7 | 2.4 µs | 6.0 µs | 9.8 µs | 15.2 µs | 42.5 µs | 166.0 µs |
| sech | 0.7 | 3.4 µs | 6.4 µs | 11.8 µs | 15.2 µs | 51.5 µs | 171.6 µs |
| sgn | 0.7 | 266.4 ns | 256.8 ns | 270.1 ns | 244.0 ns | 271.7 ns | 259.6 ns |
| sin | 0.567 | 3.3 µs | 5.5 µs | 9.6 µs | 14.8 µs | 55.2 µs | 167.2 µs |
| sinc | 0.7 | 5.6 µs | 8.7 µs | 12.9 µs | 21.3 µs | 47.5 µs | 188.9 µs |
| sincos | 0.7 | 12.1 µs | 15.1 µs | 19.4 µs | 27.9 µs | 53.0 µs | 181.4 µs |
| sinh | 0.7 | 3.0 µs | 6.3 µs | 9.3 µs | 14.4 µs | 50.9 µs | 181.1 µs |
| sinhcosh | 0.7 | 3.2 µs | 6.8 µs | 9.8 µs | 15.8 µs | 55.8 µs | 187.9 µs |
| sqr | 0.7 | 170.9 ns | 220.1 ns | 352.6 ns | 436.7 ns | 722.5 ns | 2.0 µs |
| sqrt | 1.23456789 | 357.6 ns | 457.6 ns | 609.4 ns | 756.5 ns | 1.6 µs | 3.2 µs |
| step | 0.7 | 241.5 ns | 339.6 ns | 264.1 ns | 290.2 ns | 319.5 ns | 252.3 ns |
| struve_H | 2, 0.7 | 64.3 µs | 113.5 µs | 177.4 µs | 323.0 µs | 877.0 µs | 4.020 ms |
| struve_L | 2, 0.7 | 61.7 µs | 106.9 µs | 176.9 µs | 241.1 µs | 886.3 µs | 3.614 ms |
| sub | 0.7, 0.7 | 84.4 ns | 97.4 ns | 96.8 ns | 105.8 ns | 156.9 ns | 266.5 ns |
| tan | 0.7 | 12.8 µs | 15.1 µs | 23.2 µs | 25.7 µs | 56.1 µs | 202.1 µs |
| tanh | 0.7 | 3.0 µs | 6.6 µs | 10.0 µs | 15.8 µs | 48.0 µs | 173.3 µs |
| tetragamma | 2.345 | 46.0 µs | 138.6 µs | 244.9 µs | 433.4 µs | 2.157 ms | 8.456 ms |
| tri | 0.7 | 941.1 ns | 897.3 ns | 1.1 µs | 993.4 ns | 1.0 µs | 1.3 µs |
| trigamma | 2.345 | 32.8 µs | 102.0 µs | 176.5 µs | 300.1 µs | 1.455 ms | 5.980 ms |
| vercos | 0.7 | 6.7 µs † | 5.6 µs | 9.1 µs | 14.0 µs | 42.3 µs | 170.8 µs |
| versin | 0.7 | 3.3 µs | 5.4 µs | 8.8 µs | 13.8 µs | 41.6 µs | 160.4 µs |
| zatahp | 2.3, 1.4 | 67.8 µs | 144.2 µs | 273.2 µs | 461.6 µs † | 1.966 ms | 8.895 ms |
| zatahp (integer order) | -4, 0.75 | 63.9 µs | 112.0 µs | 172.0 µs | 195.8 µs | 987.0 µs | 3.677 ms |
| zeta | 2.345 | 899.0 µs | 3.963 ms | 9.429 ms | 1.065 ms | 5.121 ms | 27.223 ms |
| zeta_pair | 2.345 | 242.8 µs | 440.7 µs | 842.9 µs | 1.417 ms | 6.110 ms | 31.868 ms |
| zetah | 2.3, 1.4 | 47.0 µs | 85.6 µs | 129.1 µs | 171.1 µs | 729.9 µs | 3.374 ms |
| zetap | 2.345 | 213.6 µs | 412.1 µs | 745.1 µs | 1.427 ms | 6.567 ms | 31.131 ms |

### `mpc`

| Operation | Inputs | 256 bits | 512 bits | 768 bits | 1024 bits | 2048 bits | 4096 bits |
|---|---|---:|---:|---:|---:|---:|---:|
| E1 | 2.345678+0.125i | 113.9 µs | 209.7 µs | 338.3 µs | 523.4 µs | 2.279 ms | 9.759 ms |
| Ei | 2.345678+0.125i | 108.3 µs | 204.3 µs | 336.4 µs | 578.8 µs | 2.325 ms | 9.650 ms |
| Li | 2.345+0.125i | 107.0 µs | 181.6 µs | 315.3 µs | 543.0 µs | 2.191 ms | 11.392 ms |
| abs | 0.7+0.125i | 1.0 µs | 1.3 µs | 1.5 µs | 1.6 µs | 3.9 µs | 6.8 µs |
| acos | 0.7+0.125i | 72.7 µs | 87.7 µs | 148.9 µs | 75.8 µs | 186.4 µs | 554.9 µs |
| acosec | 2.345+0.125i | 48.4 µs | 75.5 µs | 106.1 µs | 155.3 µs | 316.5 µs | 1.094 ms |
| acosech | 0.7+0.125i | 64.7 µs | 91.5 µs | 108.5 µs | 164.8 µs | 333.7 µs | 1.188 ms |
| acosh | 2.345+0.125i | 46.5 µs | 73.9 µs | 108.2 µs | 62.0 µs | 467.1 µs † | 578.8 µs |
| acot | 0.7+0.125i | 47.7 µs | 76.0 µs | 109.1 µs | 138.3 µs | 386.5 µs | 1.299 ms † |
| acoth | 2.345+0.125i | 34.6 µs | 59.6 µs | 92.2 µs | 121.5 µs | 333.1 µs | 1.095 ms † |
| add | 0.7+0.125i, 0.7+0.125i | 429.7 ns | 500.5 ns | 435.6 ns | 411.4 ns | 485.5 ns | 862.0 ns |
| add_long | 17+0.125i, 3 | 3.2 µs | 2.7 µs | 2.8 µs | 2.7 µs | 3.8 µs | 4.3 µs |
| appell_F1 | <span style="display:inline-block;width:40ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">0.5+0.125i, 0.7+0.125i, 0.8+0.125i, 2.5+<br/>0.125i, 0.2+0.125i, 0.3+0.125i</span> | 505.8 µs | 1.227 ms | 2.519 ms | 5.269 ms | 12.560 ms | 55.101 ms † |
| arccovercos | 0.5+0.125i | 53.5 µs | 87.3 µs | 59.3 µs | 151.6 µs | 218.3 µs | 534.6 µs |
| arccoversin | 0.5+0.125i | 51.0 µs | 43.6 µs | 54.9 µs | 146.3 µs | 209.0 µs | 559.6 µs |
| archacovercos | 0.5+0.125i | 13.0 µs | 18.3 µs | 36.8 µs † | 29.7 µs | 49.2 µs | 135.7 µs |
| archacoversin | 0.5+0.125i | 14.0 µs | 16.6 µs | 22.8 µs | 24.5 µs | 50.4 µs | 125.0 µs |
| archavercos | 0.5+0.125i | 12.7 µs | 18.1 µs | 22.6 µs | 26.1 µs | 55.2 µs | 138.4 µs |
| archaversin | 0.5+0.125i | 12.3 µs | 19.3 µs | 23.6 µs | 25.3 µs | 58.3 µs | 152.0 µs |
| arcvercos | 0.5+0.125i | 53.4 µs | 50.9 µs | 123.2 µs | 81.7 µs | 157.8 µs | 448.7 µs |
| arcversin | 0.5+0.125i | 30.4 µs | 47.5 µs | 113.2 µs | 75.5 µs | 145.8 µs | 544.4 µs |
| arg | 0.7+0.125i | 13.7 µs | 24.6 µs | 34.5 µs | 43.1 µs | 112.8 µs | 453.0 µs |
| asec | 2.345+0.125i | 49.3 µs | 73.6 µs | 103.0 µs | 143.0 µs | 328.2 µs | 1.381 ms † |
| asech | 0.7+0.125i | 31.6 µs | 41.1 µs | 57.9 µs | 97.4 µs | 220.4 µs | 548.9 µs |
| asin | 0.7+0.125i | 64.3 µs | 92.1 µs | 118.5 µs | 160.2 µs | 353.9 µs | 513.9 µs |
| asinh | 0.7+0.125i | 52.3 µs | 77.2 µs | 125.5 µs | 152.2 µs | 322.5 µs | 907.3 µs |
| atan | 0.7+0.125i | 47.7 µs | 80.4 µs | 104.0 µs | 138.3 µs | 325.8 µs | 1.051 ms |
| atanh | 0.7+0.125i | 45.7 µs | 78.5 µs | 97.6 µs | 123.9 µs | 339.4 µs | 989.4 µs |
| bessel_I | 2, 0.7+0.125i | 114.2 µs | 188.4 µs | 341.5 µs | 615.7 µs | 2.314 ms | 10.953 ms |
| bessel_J | 2, 0.7+0.125i | 99.7 µs | 204.6 µs | 384.3 µs | 637.9 µs | 2.103 ms | 9.547 ms |
| bessel_K | 2, 0.7+0.125i | 139.6 µs | 276.1 µs | 435.8 µs | 730.8 µs | 2.735 ms | 10.859 ms |
| bessel_Y | 2, 0.7+0.125i | 199.3 µs | 412.9 µs | 803.3 µs | 1.087 ms | 4.500 ms | 19.465 ms |
| ceil | 0.7+0.125i | 1.6 µs | 1.7 µs | 1.7 µs | 2.0 µs | 1.9 µs | 2.0 µs † |
| chebyshev_Tn | 2, 0.7+0.125i | 14.8 µs † | 11.2 µs | 12.6 µs | 13.3 µs | 14.0 µs | 17.1 µs |
| chebyshev_Un | 2, 0.7+0.125i | 13.2 µs | 14.6 µs | 14.2 µs | 20.4 µs † | 16.6 µs | 22.6 µs |
| circ | 0.7 | 2.4 µs | 3.8 µs † | 14.2 µs † | 3.5 µs | 4.4 µs | 8.7 µs |
| clausen | 3, 0.7+0.125i | 162.2 µs | 359.0 µs | 717.3 µs | 1.338 ms | 5.168 ms | 25.328 ms |
| clausen2 | 0.7+0.125i | 142.3 µs | 473.7 µs | 906.1 µs | 1.191 ms | 4.576 ms | 16.636 ms † |
| conj | 0.7+0.125i | 339.2 ns | 321.9 ns | 324.7 ns | 348.6 ns | 324.8 ns | 344.0 ns |
| cos | 0.7+0.125i | 17.7 µs | 16.3 µs | 24.2 µs | 36.0 µs | 114.6 µs | 391.6 µs |
| cosec | 0.7+0.125i | 19.5 µs | 21.3 µs | 35.1 µs | 36.5 µs | 108.3 µs | 417.4 µs |
| cosech | 0.7+0.125i | 14.2 µs | 19.4 µs | 27.3 µs | 90.2 µs | 105.5 µs | 320.9 µs |
| cosh | 0.7+0.125i | 9.0 µs | 15.7 µs | 23.0 µs | 85.6 µs | 92.9 µs | 263.7 µs |
| cot | 0.7+0.125i | 35.9 µs | 33.8 µs | 65.9 µs | 85.0 µs | 244.0 µs | 774.9 µs |
| coth | 0.7+0.125i | 19.3 µs | 33.1 µs | 48.6 µs | 177.7 µs | 187.5 µs | 817.0 µs |
| covercos | 0.7+0.125i | 20.2 µs | 19.3 µs | 28.7 µs | 39.3 µs | 111.4 µs | 386.5 µs |
| coversin | 0.7+0.125i | 21.0 µs | 18.4 µs | 31.6 µs | 39.6 µs | 110.2 µs | 374.4 µs |
| cubrt | 0.7+0.125i | 41.8 µs | 69.6 µs | 111.1 µs | 162.4 µs | 497.1 µs | 1.759 ms † |
| digamma | 2.345+0.125i | 236.7 µs | 372.6 µs | 632.0 µs | 892.7 µs | 3.560 ms | 14.093 ms |
| dilog | 0.7+0.125i | 171.9 µs | 216.7 µs | 384.6 µs | 533.9 µs | 1.719 ms | 6.261 ms |
| div | 0.7+0.125i, 0.7+0.125i | 7.9 µs | 11.4 µs | 17.6 µs | 21.4 µs | 43.2 µs | 113.7 µs |
| erf | 0.7+0.125i | 88.4 µs | 211.6 µs | 352.4 µs | 597.3 µs | 2.382 ms | 10.912 ms |
| erfc | 0.7+0.125i | 83.5 µs | 183.3 µs | 342.0 µs | 558.1 µs | 1.857 ms | 8.918 ms |
| erfcinv | 0.5+0.125i | 575.3 µs | 1.082 ms | 1.502 ms | 2.462 ms | 6.635 ms | 24.048 ms |
| erfinv | 0.5+0.125i | 570.3 µs | 1.051 ms | 1.662 ms | 2.395 ms | 6.832 ms | 26.521 ms |
| exp | 1.23456789+0.125i | 10.8 µs | 21.8 µs | 37.3 µs | 51.9 µs | 159.8 µs | 744.3 µs |
| floor | 0.7+0.125i | 343.2 ns | 320.1 ns | 303.4 ns | 319.0 ns | 357.4 ns | 301.3 ns |
| gamma | 2.345+0.125i | 225.4 µs | 322.2 µs | 538.0 µs | 779.3 µs | 2.613 ms | 8.203 ms |
| gammainc_P | 2.3+0.125i, 1.4+0.125i | 778.6 µs | 1.005 ms | 1.462 ms | 2.100 ms | 7.137 ms | 31.234 ms |
| gammainc_Q | 2.3+0.125i, 1.4+0.125i | 748.4 µs | 963.9 µs | 1.420 ms | 2.564 ms | 6.293 ms | 24.787 ms |
| gammainc_lower | 2.3+0.125i, 1.4+0.125i | 400.6 µs | 661.1 µs | 829.4 µs | 1.221 ms | 3.995 ms | 16.130 ms |
| gammainc_upper | 2.3+0.125i, 1.4+0.125i | 714.3 µs | 1.008 ms | 1.418 ms | 2.392 ms | 7.083 ms | 27.405 ms |
| gammainv | 9.5+0.125i | 2.758 ms | 3.563 ms | 4.470 ms | 5.406 ms | 12.211 ms | 32.116 ms |
| hacovercos | 0.7+0.125i | 29.8 µs † | 23.2 µs | 35.3 µs | 51.0 µs | 124.1 µs | 484.5 µs |
| hacoversin | 0.7+0.125i | 24.6 µs | 25.7 µs | 35.4 µs | 45.3 µs | 122.9 µs | 426.2 µs |
| harmonic_poly | 2, 0.7+0.125i | 19.8 µs | 19.5 µs | 25.6 µs | 24.7 µs | 41.5 µs | 84.5 µs |
| havercos | 0.7+0.125i | 24.5 µs | 21.1 µs | 33.0 µs | 46.4 µs | 125.7 µs | 383.5 µs |
| haversin | 0.7+0.125i | 24.7 µs | 24.4 µs | 32.6 µs | 45.1 µs | 122.9 µs | 403.6 µs |
| hermite_Hn | 2, 0.7+0.125i | 14.1 µs | 14.2 µs | 15.2 µs | 21.3 µs † | 18.8 µs | 21.5 µs |
| hypergeometric_pFq | 0.7+0.125i, 2.5+0.125i, 0.3+0.125i | 197.8 µs | 444.7 µs | 801.1 µs | 1.475 ms | 3.629 ms | 18.258 ms |
| imag_part | 0.7+0.125i | 142.1 ns | 146.9 ns | 132.2 ns | 165.2 ns | 140.9 ns | 236.0 ns |
| inv | 0.7+0.125i | 1.3 µs | 1.9 µs | 2.0 µs | 2.9 µs | 5.1 µs | 16.9 µs |
| ldexp | 1.23456789+0.125i, 3 | 362.7 ns | 372.3 ns | 353.7 ns | 390.0 ns | 662.7 ns | 514.0 ns |
| legendre_chi | 2, 0.7+0.125i | 116.0 µs | 259.5 µs | 438.7 µs | 717.9 µs | 1.977 ms | 8.171 ms |
| lerch_phi | 0.3+0.125i, 2+0.125i, 1.4+0.125i | 452.7 µs | 12.0 µs | 9.8 µs | 10.7 µs | 9.8 µs | 15.2 µs † |
| lerch_phi (integer order) | 0.3125+0.125i, -15, 0.375+0.125i | 45.9 µs | 61.7 µs | 42.9 µs | 47.9 µs | 52.2 µs | 56.2 µs |
| lerch_phi (negative order) | 0.3125+0.125i, -15+0.125i, 0.375+0.125i | 539.8 µs | 11.0 µs | 9.8 µs | 10.5 µs | 11.9 µs | 15.5 µs † |
| lg | 2.345678+0.125i | 30.0 µs | 48.9 µs | 56.2 µs | 91.6 µs | 200.0 µs | 738.9 µs |
| lgamma | 2.345+0.125i | 167.5 µs | 309.5 µs | 408.2 µs | 791.7 µs | 1.858 ms | 9.079 ms |
| ln | 2.345678+0.125i | 20.1 µs | 30.8 µs | 42.8 µs | 58.6 µs | 151.2 µs | 542.5 µs |
| log | 2.345678+0.125i | 18.2 µs | 30.9 µs | 41.3 µs | 61.0 µs | 129.6 µs | 513.3 µs |
| log10 | 2.345678+0.125i | 28.5 µs | 42.7 µs | 52.8 µs | 78.7 µs | 210.3 µs | 752.6 µs |
| lommel_S | 0.5+0.125i, 0.7+0.125i, 1.2+0.125i | 239.2 µs | 540.1 µs | 803.6 µs | 1.321 ms | 4.909 ms | 22.931 ms |
| mul | 0.7+0.125i, 0.7+0.125i | 1.5 µs | 2.4 µs † | 2.2 µs | 2.8 µs | 4.5 µs | 10.6 µs † |
| mul_long | 17+0.125i, 3 | 3.9 µs | 3.8 µs | 6.3 µs † | 3.9 µs | 4.6 µs | 3.7 µs |
| mul_pow10 | 1.23456789+0.125i, 3 | 3.2 µs | 3.7 µs | 4.3 µs | 3.4 µs | 3.6 µs | 5.8 µs † |
| neg | 0.7+0.125i | 352.1 ns | 306.3 ns | 326.4 ns | 283.9 ns | 379.4 ns | 393.5 ns |
| normal_cdf | 0.7+0.125i | 89.1 µs | 229.3 µs | 321.1 µs | 681.1 µs | 2.066 ms | 8.610 ms |
| normal_logpdf | 0.7+0.125i | 8.3 µs | 12.0 µs | 19.8 µs | 21.4 µs | 42.7 µs | 150.5 µs |
| normal_pdf | 0.7+0.125i | 19.4 µs | 34.3 µs | 65.1 µs | 86.6 µs | 260.8 µs | 936.4 µs |
| polygamma | 3, 2.345+0.125i | 348.6 µs | 594.0 µs | 991.6 µs | 1.839 ms | 6.162 ms | 29.444 ms |
| polylog | 2, 0.7+0.125i | 188.9 µs | 221.6 µs | 428.4 µs | 522.9 µs | 1.579 ms | 6.537 ms |
| polylog1 | 0.7+0.125i | 28.3 µs | 42.4 µs | 54.2 µs | 78.7 µs | 166.0 µs | 509.6 µs |
| pow | 1.23456789+0.125i, 3.5+0.125i | 36.2 µs | 58.8 µs | 98.1 µs | 178.9 µs | 367.3 µs | 1.500 ms |
| pow10 | 3 | 511.0 ns | 608.2 ns | 575.8 ns | 688.7 ns | 625.5 ns | 672.0 ns |
| pow_int | 1.23456789+0.125i, 3 | 6.6 µs | 7.3 µs | 12.0 µs | 10.6 µs | 13.7 µs | 25.4 µs |
| real_part | 0.7+0.125i | 129.5 ns | 137.6 ns | 141.8 ns | 152.2 ns | 153.2 ns | 229.2 ns |
| rect | 0.7 | 2.7 µs | 3.4 µs | 2.9 µs | 3.9 µs | 4.4 µs | 8.3 µs |
| root | 1.2345+0.125i, 3 | 38.2 µs | 73.2 µs | 192.3 µs | 276.6 µs | 383.7 µs | 1.376 ms |
| round | 0.7+0.125i | 9.7 µs | 11.4 µs | 9.5 µs | 10.0 µs | 10.6 µs | 10.7 µs |
| roundn | 1.23456789+0.125i, 3 | 16.0 µs | 21.4 µs | 15.7 µs | 23.9 µs | 21.5 µs | 30.2 µs |
| rounds | 1.23456789+0.125i, 3 | 53.3 µs | 84.2 µs | 88.9 µs | 132.0 µs | 221.8 µs | 744.7 µs |
| sec | 0.7+0.125i | 19.4 µs | 19.0 µs | 26.7 µs | 52.5 µs | 110.8 µs | 382.9 µs |
| sech | 0.7+0.125i | 12.7 µs | 18.1 µs | 26.2 µs | 91.5 µs | 90.7 µs | 312.5 µs |
| sgn | 0.7 | 919.1 ns | 823.4 ns | 1.0 µs | 1.5 µs † | 905.8 ns | 874.8 ns |
| sin | 0.567+0.125i | 18.6 µs | 14.8 µs | 24.3 µs | 62.4 µs † | 101.9 µs | 403.1 µs |
| sinc | 0.7+0.125i | 19.6 µs | 36.7 µs | 37.9 µs | 67.9 µs | 121.4 µs | 421.2 µs |
| sinh | 0.7+0.125i | 9.4 µs | 14.7 µs | 24.2 µs | 109.6 µs | 78.3 µs | 284.6 µs |
| sqr | 0.7+0.125i | 1.5 µs | 2.1 µs | 2.4 µs | 3.1 µs | 4.1 µs | 9.3 µs |
| sqrt | 1.23456789+0.125i | 2.3 µs | 3.3 µs | 3.5 µs | 4.4 µs | 6.5 µs | 17.5 µs |
| step | 0.7 | 844.0 ns | 962.5 ns | 914.7 ns | 808.2 ns | 820.3 ns | 950.4 ns |
| struve_H | 2, 0.7+0.125i | 110.8 µs | 263.3 µs | 414.8 µs | 662.7 µs | 1.941 ms | 9.663 ms |
| struve_L | 2, 0.7+0.125i | 109.1 µs | 219.2 µs | 359.2 µs | 521.7 µs | 1.850 ms | 10.540 ms |
| sub | 0.7+0.125i, 0.7+0.125i | 361.8 ns | 391.4 ns | 372.9 ns | 421.7 ns | 469.8 ns | 628.8 ns |
| tan | 0.7+0.125i | 13.4 µs | 19.2 µs | 30.4 µs | 42.2 µs | 111.3 µs | 539.5 µs |
| tanh | 0.7+0.125i | 27.4 µs | 23.1 µs | 71.3 µs | 78.6 µs † | 93.3 µs | 360.8 µs |
| tetragamma | 2.345+0.125i | 340.5 µs | 525.7 µs | 965.1 µs | 2.357 ms † | 4.797 ms | 28.991 ms † |
| tri | 0.7 | 2.3 µs | 2.8 µs | 3.0 µs | 5.3 µs † | 4.3 µs | 8.7 µs |
| trigamma | 2.345+0.125i | 249.5 µs | 389.8 µs | 772.0 µs | 3.713 ms † | 3.891 ms | 17.884 ms |
| vercos | 0.7+0.125i | 20.0 µs | 18.8 µs | 31.3 µs | 60.4 µs † | 103.9 µs | 460.2 µs |
| versin | 0.7+0.125i | 20.0 µs | 18.9 µs | 25.3 µs | 56.4 µs | 119.1 µs | 386.9 µs |
| zatahp | 2.3+0.125i, 1.4+0.125i | 2.621 ms | 4.752 ms | 9.912 ms | 915.0 µs | 3.958 ms | 13.921 ms |
| zatahp (integer order) | -4, 0.75+0.125i | 2.252 ms | 4.009 ms | 8.994 ms | 666.4 µs | 2.997 ms | 14.910 ms |
| zeta | 2.345+0.125i | 702.5 µs | 1.317 ms | 2.863 ms | 4.189 ms | 25.238 ms | 115.351 ms |
| zeta_pair | 2.345+0.125i | 755.2 µs | 1.459 ms | 2.900 ms | 4.835 ms | 24.295 ms | 130.709 ms |
| zetah | 2.3+0.125i, 1.4+0.125i | 2.146 ms | 4.558 ms | 8.565 ms | 884.1 µs † | 3.300 ms | 17.392 ms |
| zetap | 2.345+0.125i | 827.9 µs | 1.653 ms | 2.951 ms | 4.490 ms | 27.050 ms | 122.284 ms |

### number / unrestricted integer

| Operation | Inputs | Exact |
|---|---|---:|
| abs | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">-9712853409152031141207811585727780469484705121206232579607434236969009533618825<br/>97201918971499157056640605432269691427807258223374967278846596194834524005116336<br/>17514882293623272276764857474830401626415841035971458657442133370777607803464071<br/>101158385124186229995073045427448918227959893082282919863982307657776</span> | 92.5 ns |
| add | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 92.0 ns |
| bit_and | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 265.2 ns |
| bit_length | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776</span> | 158.2 ns |
| bit_not | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776</span> | 265.0 ns |
| bit_or | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 194.4 ns |
| bit_xor | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 209.2 ns |
| clear_bit | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>701</span> | 305.6 ns |
| div | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>340282366920938463463374607431768211454</span> | 1.1 µs |
| divmod | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 324.7 ns |
| factorial | 5000 | 109.2 µs |
| factors | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">54772119019154636339867862041163448790974484131459861520687327809209914339836012<br/>52275954799541486775711126005967382600118182797233708133775335788335658037065081<br/>31569062299976033041302189833215742992653063887339218309467131548996199881113600<br/>00000000000000000000000000000000000000000000000000000000000000000000000000000000<br/>0000000000000000000000000000000000000000000000</span> | 95.8 µs |
| fibonacci | 10000 | 6.8 µs |
| gcd | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 3.0 µs |
| gcdext | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 3.6 µs |
| is_prime | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">68647976601306097149819007990813932172694353001433054093944634591855431833976560<br/>52122559640661454554977296311391480858037121987999716643812574028291115057151</span> | 453.4 µs |
| isqrt | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776</span> | 363.1 ns |
| lcm | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 2.8 µs |
| mod | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 236.4 ns |
| modinv | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>68647976601306097149819007990813932172694353001433054093944634591855431833976560<br/>52122559640661454554977296311391480858037121987999716643812574028291115057151</span> | 3.6 µs |
| mul | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 206.1 ns |
| neg | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776</span> | 67.1 ns |
| next_prime | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 3.923 ms |
| partition | 5000 | 6.747 ms |
| pow | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>5</span> | 3.6 µs |
| pow_int | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>5</span> | 3.2 µs |
| powmod | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662, <br/>68647976601306097149819007990813932172694353001433054093944634591855431833976560<br/>52122559640661454554977296311391480858037121987999716643812574028291115057151</span> | 100.4 µs |
| prev_prime | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 5.085 ms |
| prove_prime | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">68647976601306097149819007990813932172694353001433054093944634591855431833976560<br/>52122559640661454554977296311391480858037121987999716643812574028291115057151</span> | 3.391 ms |
| set_bit | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>700</span> | 496.0 ns |
| shl | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>257</span> | 366.0 ns |
| shr | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>257</span> | 280.8 ns |
| sqr | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776</span> | 278.1 ns |
| sub | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 106.4 ns |
| test_bit | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>701</span> | 279.5 ns |

### number / unrestricted fraction

| Operation | Inputs | Exact |
|---|---|---:|
| abs | $\frac{\begin{array}{l}\text{-9712853409152031141207811585727780469484705121206232579607434236969009533618825} \\ \text{97201918971499157056640605432269691427807258223374967278846596194834524005116336} \\ \text{17514882293623272276764857474830401626415841035971458657442133370777607803464071} \\ \text{101158385124186229995073045427448918227959893082282919863982307657776}\end{array}}{\begin{array}{l}\text{68647976601306097149819007990813932172694353001433054093944634591855431833976560} \\ \text{52122559640661454554977296311391480858037121987999716643812574028291115057151}\end{array}}$ | 195.8 ns |
| add | **Input 1**<br/>$\frac{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}{\begin{array}{l}\text{68647976601306097149819007990813932172694353001433054093944634591855431833976560} \\ \text{52122559640661454554977296311391480858037121987999716643812574028291115057151}\end{array}}$<br/><br/>**Input 2**<br/>$\frac{\begin{array}{l}\text{12498846048367045926370716602609982839721641429222959125376462582674869403443996} \\ \text{972648628699047686839582654905384579341977328653591547736951147466435189662}\end{array}}{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}$ | 3.6 µs |
| div | **Input 1**<br/>$\frac{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}{\begin{array}{l}\text{68647976601306097149819007990813932172694353001433054093944634591855431833976560} \\ \text{52122559640661454554977296311391480858037121987999716643812574028291115057151}\end{array}}$<br/><br/>**Input 2**<br/>$\frac{\begin{array}{l}\text{12498846048367045926370716602609982839721641429222959125376462582674869403443996} \\ \text{972648628699047686839582654905384579341977328653591547736951147466435189662}\end{array}}{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}$ | 6.3 µs |
| inv | $\frac{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}{\begin{array}{l}\text{68647976601306097149819007990813932172694353001433054093944634591855431833976560} \\ \text{52122559640661454554977296311391480858037121987999716643812574028291115057151}\end{array}}$ | 157.4 ns |
| mul | **Input 1**<br/>$\frac{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}{\begin{array}{l}\text{68647976601306097149819007990813932172694353001433054093944634591855431833976560} \\ \text{52122559640661454554977296311391480858037121987999716643812574028291115057151}\end{array}}$<br/><br/>**Input 2**<br/>$\frac{\begin{array}{l}\text{12498846048367045926370716602609982839721641429222959125376462582674869403443996} \\ \text{972648628699047686839582654905384579341977328653591547736951147466435189662}\end{array}}{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}$ | 2.7 µs |
| neg | $\frac{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}{\begin{array}{l}\text{68647976601306097149819007990813932172694353001433054093944634591855431833976560} \\ \text{52122559640661454554977296311391480858037121987999716643812574028291115057151}\end{array}}$ | 166.1 ns |
| pow | **Input 1**<br/>$\frac{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}{\begin{array}{l}\text{68647976601306097149819007990813932172694353001433054093944634591855431833976560} \\ \text{52122559640661454554977296311391480858037121987999716643812574028291115057151}\end{array}}$<br/><br/>**Input 2**<br/>5 | 27.6 µs |
| pow_int | **Input 1**<br/>$\frac{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}{\begin{array}{l}\text{68647976601306097149819007990813932172694353001433054093944634591855431833976560} \\ \text{52122559640661454554977296311391480858037121987999716643812574028291115057151}\end{array}}$<br/><br/>**Input 2**<br/>5 | 25.8 µs |
| sqr | $\frac{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}{\begin{array}{l}\text{68647976601306097149819007990813932172694353001433054093944634591855431833976560} \\ \text{52122559640661454554977296311391480858037121987999716643812574028291115057151}\end{array}}$ | 434.8 ns |
| sub | **Input 1**<br/>$\frac{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}{\begin{array}{l}\text{68647976601306097149819007990813932172694353001433054093944634591855431833976560} \\ \text{52122559640661454554977296311391480858037121987999716643812574028291115057151}\end{array}}$<br/><br/>**Input 2**<br/>$\frac{\begin{array}{l}\text{12498846048367045926370716602609982839721641429222959125376462582674869403443996} \\ \text{972648628699047686839582654905384579341977328653591547736951147466435189662}\end{array}}{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}$ | 3.8 µs |

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
