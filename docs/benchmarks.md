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
  present; otherwise every operand changes. Only MPFR and MPC profiles of
  non-integer-only operations support this protocol. Domain failures are recorded.

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
Benchmark run **140**, 2026-11-15T22:41:00.137Z (UTC).

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
| E1 | 2.345678 | 12.6 µs |
| Ei | 2.345678 | 10.0 µs |
| Li | 2.345 | 11.5 µs † |
| abs | 0.7 | 11.5 ns |
| acos | 0.7 | 28.1 ns |
| acosec | 2.345 | 23.3 ns |
| acosech | 0.7 | 93.6 ns |
| acosh | 2.345 | 46.4 ns |
| acot | 0.7 | 73.1 ns |
| acoth | 2.345 | 40.7 ns |
| add | 0.7, 0.7 | 23.8 ns |
| add_long | 17, 3 | 746.0 ns |
| appell_F1 | 0.5, 0.7, 0.8, 2.5, 0.2, 0.3 | 47.0 µs † |
| arccovercos | 0.5 | 134.5 ns |
| arccoversin | 0.5 | 177.3 ns |
| archacovercos | 0.5 | 326.6 ns |
| archacoversin | 0.5 | 281.0 ns |
| archavercos | 0.5 | 369.0 ns |
| archaversin | 0.5 | 370.8 ns |
| arcvercos | 0.5 | 185.0 ns |
| arcversin | 0.5 | 228.4 ns |
| arg | 0.7 | 136.2 ns |
| asec | 2.345 | 28.1 ns |
| asech | 0.7 | 53.8 ns |
| asin | 0.7 | 30.1 ns |
| asinh | 0.7 | 88.2 ns |
| atan | 0.7 | 48.3 ns |
| atan2 | 0.5, -0.75 | 61.3 ns |
| atanh | 0.7 | 43.0 ns |
| bessel_I | 2, 0.7 | 60.7 µs |
| bessel_J | 2, 0.7 | 7.4 µs |
| bessel_K | 2, 0.7 | 28.7 µs |
| bessel_Y | 2, 0.7 | 24.2 µs |
| beta | 2.3, 4.5 | 188.5 ns |
| beta_pdf | 0.5, 2.5, 3.5 | 262.7 ns |
| binomial | 5, 2 | 277.4 ns |
| ceil | 0.7 | 106.1 ns |
| chebyshev_Tn | 2, 0.7 | 920.0 ns |
| chebyshev_Un | 2, 0.7 | 2.0 µs |
| circ | 0.7 | 352.6 ns |
| clausen | 3, 0.7 | 10.5 µs |
| clausen2 | 0.7 | 11.2 µs |
| conj | 0.7 | 12.8 ns |
| cos | 0.7 | 55.1 ns |
| cosec | 0.7 | 42.5 ns |
| cosech | 0.7 | 138.3 ns |
| cosh | 0.7 | 60.6 ns |
| cot | 0.7 | 64.3 ns |
| coth | 0.7 | 82.6 ns |
| covercos | 0.7 | 202.1 ns |
| coversin | 0.7 | 254.3 ns |
| cubrt | 0.7 | 12.8 µs † |
| digamma | 2.345 | 4.0 µs |
| dilog | 0.7 | 18.1 µs |
| div | 0.7, 0.7 | 16.5 ns |
| erf | 0.7 | 88.4 ns |
| erfc | 0.7 | 100.0 ns |
| erfcinv | 0.5 | 367.2 ns |
| erfinv | 0.5 | 471.9 ns |
| exp | 1.23456789 | 83.6 ns |
| floor | 0.7 | 24.5 ns |
| gamma | 2.345 | 199.4 ns |
| gammainc_P | 2.3, 1.4 | 18.5 µs |
| gammainc_Q | 2.3, 1.4 | 15.8 µs |
| gammainc_lower | 2.3, 1.4 | 18.7 µs |
| gammainc_upper | 2.3, 1.4 | 17.2 µs |
| gammainv | 9.5 | 154.5 µs |
| hacovercos | 0.7 | 320.5 ns |
| hacoversin | 0.7 | 364.4 ns |
| harmonic_poly | 2, 0.7 | 2.0 µs |
| havercos | 0.7 | 351.2 ns |
| haversin | 0.7 | 306.1 ns |
| hermite_Hn | 2, 0.7 | 2.8 µs † |
| hypergeometric_pFq | 0.7, 2.5, 0.3 | 13.3 µs † |
| hypot | 0.7, 1.2 | 32.8 ns |
| imag_part | 0.7 | 42.4 ns |
| inv | 0.7 | 20.5 ns |
| lambert_W0 | 0.7 | 22.0 µs |
| lambert_Wm1 | -0.2 | 26.8 µs † |
| lambert_Wn | 1, 0.7 | 36.5 µs |
| lauricella_F | 0.5, 0.3, 2.5 | 32.2 µs † |
| ldexp | 1.23456789, 3 | 97.4 ns |
| legendre_chi | 2, 0.7 | 38.4 µs |
| lerch_phi | 0.3, 2, 1.4 | 57.2 µs |
| lerch_phi (integer order) | 0.3125, -15, 0.375 | 36.0 µs |
| lerch_phi (negative order) | 0.3125, -15, 0.375 | 45.7 µs † |
| lg | 2.345678 | 47.2 ns |
| lgamma | 2.345 | 84.6 ns |
| ln | 2.345678 | 26.5 ns |
| log | 2.345678 | 22.9 ns |
| log10 | 2.345678 | 33.5 ns |
| logbeta | 2.3, 4.5 | 206.7 ns |
| logbeta_pdf | 0.5, 2.5, 3.5 | 338.9 ns |
| lommel_S | 0.5, 0.7, 1.2 | 10.8 µs |
| mul | 0.7, 0.7 | 19.2 ns |
| mul_long | 17, 3 | 739.5 ns |
| mul_pow10 | 1.23456789, 3 | 129.7 ns |
| neg | 0.7 | 12.7 ns |
| normal_cdf | 0.7 | 114.0 ns |
| normal_logpdf | 0.7 | 27.0 ns |
| normal_pdf | 0.7 | 33.8 ns |
| polygamma | 3, 2.345 | 38.6 µs † |
| polylog | 2, 0.7 | 21.1 µs |
| polylog1 | 0.7 | 244.4 ns |
| pow | 1.23456789, 3.5 | 46.3 ns |
| pow10 | 3 | 284.1 ns |
| pow_int | 1.23456789, 3 | 127.6 ns |
| productlog | 0.7 | 29.3 µs |
| qdigamma | 0.5, 2.3 | 99.0 µs † |
| real_part | 0.7 | 42.3 ns |
| rect | 0.7 | 402.2 ns |
| root | 1.2345, 3 | 11.3 µs |
| round | 0.7 | 550.3 ns |
| roundn | 1.23456789, 3 | 6.4 µs |
| rounds | 1.23456789, 3 | 10.6 µs † |
| sec | 0.7 | 38.9 ns |
| sech | 0.7 | 47.4 ns |
| sgn | 0.7 | 218.1 ns |
| sin | 0.567 | 57.4 ns |
| sinc | 0.7 | 4.5 µs |
| sincos | 0.7 | 1.1 µs |
| sinh | 0.7 | 83.7 ns |
| sinhcosh | 0.7 | 185.5 ns |
| sqr | 0.7 | 12.4 ns |
| sqrt | 1.23456789 | 14.2 ns |
| step | 0.7 | 185.2 ns |
| struve_H | 2, 0.7 | 95.0 µs |
| struve_L | 2, 0.7 | 97.2 µs |
| sub | 0.7, 0.7 | 18.3 ns |
| tan | 0.7 | 11.7 µs † |
| tanh | 0.7 | 80.2 ns |
| tetragamma | 2.345 | 5.1 µs |
| tri | 0.7 | 416.8 ns |
| trigamma | 2.345 | 3.9 µs |
| vercos | 0.7 | 174.6 ns |
| versin | 0.7 | 175.6 ns |
| zatahp | 2.3, 1.4 | 60.0 µs |
| zatahp (integer order) | -4, 0.75 | 60.8 µs |
| zeta | 2.345 | 104.0 µs |
| zeta_pair | 2.345 | 229.0 µs |
| zetah | 2.3, 1.4 | 41.5 µs |
| zetap | 2.345 | 141.4 µs |

### `qfloat`

| Operation | Inputs | 106 bits |
|---|---|---:|
| E1 | 2.345678 | 12.4 µs |
| Ei | 2.345678 | 11.3 µs |
| Li | 2.345 | 13.4 µs |
| abs | 0.7 | 3.3 ns |
| acos | 0.7 | 2.7 µs |
| acosec | 2.345 | 1.9 µs |
| acosech | 0.7 | 4.4 µs |
| acosh | 2.345 | 5.8 µs |
| acot | 0.7 | 1.8 µs |
| acoth | 2.345 | 5.0 µs |
| add | 0.7, 0.7 | 9.5 ns |
| appell_F1 | 0.5, 0.7, 0.8, 2.5, 0.2, 0.3 | 108.3 µs |
| arccovercos | 0.5 | 2.9 µs |
| arccoversin | 0.5 | 2.5 µs |
| archacovercos | 0.5 | 47.4 ns |
| archacoversin | 0.5 | 37.9 ns |
| archavercos | 0.5 | 72.3 ns |
| archaversin | 0.5 | 85.3 ns |
| arcvercos | 0.5 | 2.6 µs |
| arcversin | 0.5 | 2.5 µs |
| asec | 2.345 | 2.5 µs |
| asech | 0.7 | 5.4 µs |
| asin | 0.7 | 2.9 µs |
| asinh | 0.7 | 5.2 µs |
| atan | 0.7 | 1.6 µs |
| atan2 | 0.5, -0.75 | 1.9 µs |
| atanh | 0.7 | 4.1 µs |
| bessel_I | 2, 0.7 | 70.1 µs |
| bessel_J | 2, 0.7 | 11.4 µs † |
| bessel_K | 2, 0.7 | 44.0 µs |
| bessel_Y | 2, 0.7 | 39.4 µs |
| beta | 2.3, 4.5 | 6.3 µs |
| beta_pdf | 0.5, 2.5, 3.5 | 45.1 µs |
| binomial | 5, 2 | 45.7 µs |
| chebyshev_Tn | 2, 0.7 | 52.2 ns |
| chebyshev_Un | 2, 0.7 | 61.7 ns |
| circ | 0.7 | 24.4 ns |
| clausen | 3, 0.7 | 461.2 µs |
| clausen2 | 0.7 | 8.1 µs |
| cos | 0.7 | 1.3 µs |
| cosec | 0.7 | 1.2 µs |
| cosech | 0.7 | 6.5 µs † |
| cosh | 0.7 | 5.4 µs |
| cot | 0.7 | 2.6 µs |
| coth | 0.7 | 10.9 µs |
| covercos | 0.7 | 1.4 µs |
| coversin | 0.7 | 1.6 µs † |
| cubrt | 0.7 | 7.5 µs |
| digamma | 2.345 | 3.9 µs |
| dilog | 0.7 | 21.6 µs |
| div | 0.7, 0.7 | 53.4 ns |
| erf | 0.7 | 5.6 µs |
| erfc | 0.7 | 5.4 µs |
| erfcinv | 0.5 | 341.7 µs |
| erfinv | 0.5 | 312.6 µs |
| exp | 1.23456789 | 2.3 µs |
| floor | 0.7 | 19.3 ns |
| gamma | 2.345 | 2.4 µs |
| gammainc_P | 2.3, 1.4 | 19.7 µs |
| gammainc_Q | 2.3, 1.4 | 17.8 µs |
| gammainc_lower | 2.3, 1.4 | 18.9 µs |
| gammainc_upper | 2.3, 1.4 | 21.6 µs |
| gammainv | 9.5 | 144.6 µs |
| hacovercos | 0.7 | 1.4 µs |
| hacoversin | 0.7 | 1.4 µs |
| harmonic_poly | 2, 0.7 | 168.7 ns |
| havercos | 0.7 | 1.4 µs |
| haversin | 0.7 | 1.3 µs |
| hermite_Hn | 2, 0.7 | 74.1 ns |
| hypergeometric_pFq | 0.7, 2.5, 0.3 | 5.8 µs |
| hypot | 0.7, 1.2 | 678.2 ns |
| lambert_W0 | 0.7 | 24.9 µs |
| lambert_Wm1 | -0.2 | 26.7 µs |
| lauricella_F | 0.5, 0.3, 2.5 | 27.9 µs |
| ldexp | 1.23456789, 3 | 16.0 ns |
| legendre_chi | 2, 0.7 | 38.9 µs |
| lerch_phi | 0.3, 2, 1.4 | 13.2 µs |
| lerch_phi (integer order) | 0.3125, -15, 0.375 | 45.4 µs |
| lerch_phi (negative order) | 0.3125, -15, 0.375 | 57.3 µs † |
| lg | 2.345678 | 5.0 µs |
| lgamma | 2.345 | 6.5 µs |
| ln | 2.345678 | 5.0 µs † |
| log | 2.345678 | 4.1 µs |
| log10 | 2.345678 | 3.9 µs |
| logbeta | 2.3, 4.5 | 43.6 µs |
| logbeta_pdf | 0.5, 2.5, 3.5 | 44.8 µs |
| lommel_S | 0.5, 0.7, 1.2 | 10.1 µs |
| mul | 0.7, 0.7 | 12.1 ns |
| mul_pow10 | 1.23456789, 3 | 63.7 ns |
| neg | 0.7 | 3.2 ns |
| normal_cdf | 0.7 | 5.4 µs |
| normal_logpdf | 0.7 | 48.3 ns |
| normal_pdf | 0.7 | 2.7 µs |
| polygamma | 3, 2.345 | 8.1 µs † |
| polylog | 2, 0.7 | 20.2 µs |
| polylog1 | 0.7 | 4.4 µs |
| pow | 1.23456789, 3.5 | 6.2 µs |
| pow10 | 3 | 42.2 ns |
| pow_int | 1.23456789, 3 | 78.1 ns |
| productlog | 0.7 | 22.9 µs |
| qdigamma | 0.5, 2.3 | 33.2 µs |
| rect | 0.7 | 23.1 ns |
| root | 1.2345, 3 | 6.0 µs |
| round | 0.7 | 65.6 ns |
| roundn | 1.23456789, 3 | 8.4 µs |
| rounds | 1.23456789, 3 | 23.7 µs |
| sec | 0.7 | 1.5 µs |
| sech | 0.7 | 5.5 µs |
| sgn | 0.7 | 12.8 ns |
| sin | 0.567 | 1.3 µs |
| sinc | 0.7 | 1.3 µs |
| sinh | 0.7 | 4.9 µs |
| sqr | 0.7 | 7.4 ns |
| sqrt | 1.23456789 | 433.6 ns |
| step | 0.7 | 11.9 ns |
| struve_H | 2, 0.7 | 133.7 µs |
| struve_L | 2, 0.7 | 154.0 µs † |
| sub | 0.7, 0.7 | 11.7 ns |
| tan | 0.7 | 2.5 µs |
| tanh | 0.7 | 2.3 µs |
| tetragamma | 2.345 | 4.4 µs |
| tri | 0.7 | 20.2 ns |
| trigamma | 2.345 | 4.0 µs |
| vercos | 0.7 | 1.2 µs |
| versin | 0.7 | 1.3 µs |
| zatahp | 2.3, 1.4 | 438.4 µs |
| zatahp (integer order) | -4, 0.75 | 400.4 µs |
| zeta | 2.345 | 452.7 µs |
| zetah | 2.3, 1.4 | 450.4 µs |
| zetap | 2.345 | 427.6 µs |

### `qcomplex`

| Operation | Inputs | 106 bits |
|---|---|---:|
| E1 | 2.345678+0.125i | 59.8 µs |
| Ei | 2.345678+0.125i | 92.1 µs † |
| Li | 2.345+0.125i | 64.9 µs |
| abs | 0.7+0.125i | 588.1 ns |
| acos | 0.7+0.125i | 12.8 µs |
| acosec | 2.345+0.125i | 13.1 µs |
| acosech | 0.7+0.125i | 11.1 µs |
| acosh | 2.345+0.125i | 19.8 µs |
| acot | 0.7+0.125i | 13.0 µs |
| acoth | 2.345+0.125i | 7.5 µs |
| add | 0.7+0.125i, 0.7+0.125i | 25.1 ns |
| appell_F1 | <span style="display:inline-block;width:40ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">0.5+0.125i, 0.7+0.125i, 0.8+0.125i, 2.5+<br/>0.125i, 0.2+0.125i, 0.3+0.125i</span> | 914.3 µs |
| arccovercos | 0.5+0.125i | 15.3 µs † |
| arccoversin | 0.5+0.125i | 13.8 µs |
| archacovercos | 0.5+0.125i | 7.6 µs |
| archacoversin | 0.5+0.125i | 7.9 µs |
| archavercos | 0.5+0.125i | 8.3 µs |
| archaversin | 0.5+0.125i | 6.7 µs |
| arcvercos | 0.5+0.125i | 13.4 µs |
| arcversin | 0.5+0.125i | 12.7 µs |
| arg | 0.7+0.125i | 1.3 µs |
| asec | 2.345+0.125i | 10.1 µs |
| asech | 0.7+0.125i | 19.4 µs |
| asin | 0.7+0.125i | 11.9 µs |
| asinh | 0.7+0.125i | 14.1 µs |
| atan | 0.7+0.125i | 13.3 µs |
| atan2 | 0.5+0.125i, -0.75+0.125i | 13.3 µs |
| atanh | 0.7+0.125i | 7.5 µs |
| bessel_I | 2, 0.7+0.125i | 166.0 µs |
| bessel_J | 2, 0.7+0.125i | 201.7 µs † |
| bessel_K | 2, 0.7+0.125i | 108.9 µs |
| bessel_Y | 2, 0.7+0.125i | 119.6 µs |
| beta | 2.3+0.125i, 4.5+0.125i | 45.6 µs |
| beta_pdf | 0.5+0.125i, 2.5+0.125i, 3.5+0.125i | 79.6 µs |
| binomial | 5+0.125i, 2+0.125i | 37.9 µs |
| chebyshev_Tn | 2, 0.7+0.125i | 218.3 ns |
| chebyshev_Un | 2, 0.7+0.125i | 208.0 ns |
| circ | 0.7 | 25.9 ns |
| clausen | 3, 0.7+0.125i | 434.9 µs |
| clausen2 | 0.7+0.125i | 35.8 µs |
| conj | 0.7+0.125i | 5.6 ns |
| cos | 0.7+0.125i | 12.3 µs |
| cosec | 0.7+0.125i | 13.6 µs |
| cosech | 0.7+0.125i | 11.4 µs |
| cosh | 0.7+0.125i | 10.8 µs |
| cot | 0.7+0.125i | 27.1 µs |
| coth | 0.7+0.125i | 25.9 µs |
| covercos | 0.7+0.125i | 13.4 µs |
| coversin | 0.7+0.125i | 12.5 µs |
| cubrt | 0.7+0.125i | 10.5 µs |
| digamma | 2.345+0.125i | 17.9 µs |
| dilog | 0.7+0.125i | 239.0 µs † |
| div | 0.7+0.125i, 0.7+0.125i | 321.6 ns |
| erf | 0.7+0.125i | 74.1 µs |
| erfc | 0.7+0.125i | 68.3 µs |
| erfcinv | 0.5+0.125i | 406.9 µs |
| erfinv | 0.5+0.125i | 416.8 µs |
| exp | 1.23456789+0.125i | 5.1 µs |
| floor | 0.7+0.125i | 58.0 ns |
| gamma | 2.345+0.125i | 21.6 µs |
| gammainc_P | 2.3+0.125i, 1.4+0.125i | 77.7 µs |
| gammainc_Q | 2.3+0.125i, 1.4+0.125i | 91.0 µs |
| gammainc_lower | 2.3+0.125i, 1.4+0.125i | 73.0 µs |
| gammainc_upper | 2.3+0.125i, 1.4+0.125i | 73.8 µs |
| gammainv | 9.5+0.125i | 198.3 µs |
| hacovercos | 0.7+0.125i | 13.2 µs |
| hacoversin | 0.7+0.125i | 10.8 µs |
| harmonic_poly | 2, 0.7+0.125i | 564.4 ns |
| havercos | 0.7+0.125i | 11.9 µs |
| haversin | 0.7+0.125i | 12.9 µs |
| hermite_Hn | 2, 0.7+0.125i | 265.2 ns |
| hypergeometric_pFq | 0.7+0.125i, 2.5+0.125i, 0.3+0.125i | 48.9 µs |
| hypot | 0.7+0.125i, 1.2+0.125i | 2.0 µs |
| imag_part | 0.7+0.125i | 3.3 ns |
| inv | 0.7+0.125i | 255.4 ns |
| lambert_W0 | 0.7+0.125i | 42.4 µs |
| lambert_Wm1 | -0.2+0.125i | 42.7 µs |
| lambert_Wn | 1, 0.7+0.125i | 35.3 µs |
| lauricella_F | 0.5+0.125i, 0.3+0.125i, 2.5+0.125i | 236.8 µs |
| ldexp | 1.23456789+0.125i, 3 | 40.1 ns |
| legendre_chi | 2, 0.7+0.125i | 687.2 µs |
| lerch_phi | 0.3+0.125i, 2+0.125i, 1.4+0.125i | 1.011 ms † |
| lerch_phi (integer order) | 0.3125+0.125i, -15, 0.375+0.125i | 318.3 µs |
| lerch_phi (negative order) | 0.3125+0.125i, -15+0.125i, 0.375+0.125i | 4.462 ms † |
| lg | 2.345678+0.125i | 7.8 µs |
| lgamma | 2.345+0.125i | 21.6 µs † |
| ln | 2.345678+0.125i | 5.7 µs |
| log | 2.345678+0.125i | 6.1 µs |
| log10 | 2.345678+0.125i | 6.3 µs |
| logbeta | 2.3+0.125i, 4.5+0.125i | 42.8 µs |
| logbeta_pdf | 0.5+0.125i, 2.5+0.125i, 3.5+0.125i | 57.2 µs |
| lommel_S | 0.5+0.125i, 0.7+0.125i, 1.2+0.125i | 176.6 µs |
| mul | 0.7+0.125i, 0.7+0.125i | 66.7 ns |
| neg | 0.7+0.125i | 8.6 ns |
| normal_cdf | 0.7+0.125i | 59.8 µs |
| normal_logpdf | 0.7+0.125i | 194.6 ns |
| normal_pdf | 0.7+0.125i | 5.9 µs † |
| polygamma | 3, 2.345+0.125i | 19.2 µs |
| polylog | 2, 0.7+0.125i | 141.7 µs |
| polylog1 | 0.7+0.125i | 6.9 µs |
| pow | 1.23456789+0.125i, 3.5+0.125i | 13.1 µs |
| productlog | 0.7+0.125i | 50.6 µs |
| qdigamma | 0.5+0.125i, 2.3+0.125i | 239.1 µs |
| real_part | 0.7+0.125i | 3.3 ns |
| rect | 0.7 | 26.3 ns |
| root | 1.2345+0.125i, 3 | 11.9 µs |
| round | 0.7+0.125i | 139.6 ns |
| roundn | 1.23456789+0.125i, 3 | 16.0 µs |
| rounds | 1.23456789+0.125i, 3 | 45.8 µs |
| sec | 0.7+0.125i | 11.4 µs |
| sech | 0.7+0.125i | 14.4 µs |
| sgn | 0.7 | 18.0 ns |
| sin | 0.567+0.125i | 12.7 µs |
| sinc | 0.7+0.125i | 10.1 µs |
| sinh | 0.7+0.125i | 9.9 µs |
| sqr | 0.7+0.125i | 66.0 ns |
| sqrt | 1.23456789+0.125i | 6.5 µs † |
| step | 0.7 | 17.2 ns |
| struve_H | 2, 0.7+0.125i | 207.7 µs |
| struve_L | 2, 0.7+0.125i | 184.8 µs |
| sub | 0.7+0.125i, 0.7+0.125i | 25.0 ns |
| tan | 0.7+0.125i | 23.8 µs |
| tanh | 0.7+0.125i | 19.4 µs |
| tetragamma | 2.345+0.125i | 7.9 µs |
| tri | 0.7 | 27.5 ns |
| trigamma | 2.345+0.125i | 5.1 µs |
| vercos | 0.7+0.125i | 10.9 µs |
| versin | 0.7+0.125i | 13.0 µs |
| zatahp | 2.3+0.125i, 1.4+0.125i | 795.3 µs |
| zatahp (integer order) | -4, 0.75+0.125i | 935.9 µs |
| zeta | 2.345+0.125i | 605.2 µs |
| zetah | 2.3+0.125i, 1.4+0.125i | 695.2 µs |
| zetap | 2.345+0.125i | 522.2 µs |

### `mpfr`

| Operation | Inputs | 256 bits | 512 bits | 768 bits | 1024 bits | 2048 bits | 4096 bits |
|---|---|---:|---:|---:|---:|---:|---:|
| E1 | 2.345678 | 78.0 µs † | 85.4 µs | 171.4 µs | 267.0 µs | 763.8 µs | 2.620 ms |
| Ei | 2.345678 | 62.1 µs | 97.3 µs | 159.7 µs | 247.5 µs | 670.8 µs | 2.756 ms |
| Li | 2.345 | 54.6 µs | 93.4 µs | 169.4 µs | 194.3 µs | 616.4 µs | 2.667 ms |
| abs | 0.7 | 112.2 ns | 121.4 ns | 139.2 ns | 139.8 ns | 174.8 ns | 127.8 ns |
| acos | 0.7 | 26.1 µs | 40.8 µs | 69.0 µs † | 79.7 µs | 165.0 µs | 515.4 µs |
| acosec | 2.345 | 27.0 µs | 32.4 µs | 58.1 µs | 74.1 µs | 165.9 µs | 390.1 µs |
| acosech | 0.7 | 7.6 µs | 12.7 µs | 16.0 µs | 23.4 µs | 63.6 µs | 153.4 µs |
| acosh | 2.345 | 8.3 µs | 14.0 µs | 18.7 µs | 21.8 µs | 49.9 µs | 171.4 µs |
| acot | 0.7 | 22.2 µs | 27.4 µs | 40.6 µs | 50.2 µs | 170.5 µs † | 474.3 µs |
| acoth | 2.345 | 9.8 µs | 14.2 µs | 19.3 µs | 19.4 µs | 64.3 µs | 161.2 µs |
| add | 0.7, 0.7 | 168.8 ns | 167.9 ns | 177.7 ns | 184.8 ns | 228.0 ns | 436.9 ns |
| add_long | 17, 3 | 1.1 µs | 989.7 ns | 979.7 ns | 859.4 ns | 1.1 µs | 1.3 µs |
| appell_F1 | 0.5, 0.7, 0.8, 2.5, 0.2, 0.3 | 154.5 µs | 429.5 µs | 905.0 µs | 1.276 ms | 5.399 ms | 27.944 ms |
| arccovercos | 0.5 | 20.7 µs | 37.3 µs | 44.0 µs | 72.1 µs | 155.6 µs | 419.1 µs |
| arccoversin | 0.5 | 63.9 µs † | 27.5 µs | 39.3 µs | 72.4 µs | 141.7 µs | 371.3 µs |
| archacovercos | 0.5 | 2.7 µs † | 1.8 µs | 2.3 µs | 2.0 µs | 2.6 µs | 2.7 µs |
| archacoversin | 0.5 | 2.3 µs † | 2.5 µs † | 2.9 µs | 2.6 µs | 2.3 µs | 2.4 µs |
| archavercos | 0.5 | 3.0 µs † | 2.6 µs † | 2.0 µs | 3.1 µs † | 1.9 µs | 2.2 µs |
| archaversin | 0.5 | 2.2 µs | 2.1 µs | 2.6 µs | 2.5 µs | 2.5 µs | 2.7 µs |
| arcvercos | 0.5 | 22.0 µs | 39.4 µs | 36.5 µs | 53.9 µs | 152.2 µs | 371.0 µs |
| arcversin | 0.5 | 23.8 µs | 88.9 µs † | 44.1 µs | 58.7 µs | 163.4 µs | 348.5 µs |
| arg | 0.7 | 860.4 ns | 620.2 ns | 819.5 ns | 733.1 ns | 738.2 ns | 1.0 µs |
| asec | 2.345 | 34.2 µs | 31.9 µs | 54.4 µs | 76.2 µs | 190.2 µs † | 422.4 µs |
| asech | 0.7 | 7.8 µs | 13.0 µs | 22.7 µs | 20.3 µs | 60.9 µs | 185.5 µs |
| asin | 0.7 | 23.7 µs | 35.1 µs | 55.4 µs | 74.6 µs | 139.8 µs | 430.9 µs |
| asinh | 0.7 | 7.1 µs | 14.2 µs | 16.6 µs | 23.1 µs | 53.6 µs | 133.6 µs |
| atan | 0.7 | 20.3 µs | 40.6 µs † | 42.9 µs | 60.7 µs | 167.4 µs | 540.2 µs † |
| atan2 | 0.5, -0.75 | 18.4 µs | 34.0 µs | 61.1 µs | 71.1 µs | 147.5 µs | 407.4 µs |
| atanh | 0.7 | 7.3 µs | 14.5 µs | 18.5 µs | 22.7 µs | 56.9 µs | 200.9 µs † |
| bessel_I | 2, 0.7 | 144.6 µs | 311.2 µs | 479.1 µs | 753.8 µs | 2.213 ms | 8.990 ms |
| bessel_J | 2, 0.7 | 12.0 µs | 25.0 µs | 35.4 µs | 59.9 µs | 251.1 µs | 1.265 ms |
| bessel_K | 2, 0.7 | 74.3 µs | 129.5 µs | 232.8 µs | 279.7 µs | 1.374 ms | 5.025 ms |
| bessel_Y | 2, 0.7 | 62.2 µs | 144.7 µs | 188.4 µs | 390.5 µs † | 1.026 ms | 5.055 ms |
| beta | 2.3, 4.5 | 168.2 µs | 346.9 µs | 711.4 µs | 901.7 µs | 4.119 ms | 15.584 ms |
| beta_pdf | 0.5, 2.5, 3.5 | 155.9 µs | 299.0 µs | 552.3 µs | 975.3 µs † | 3.276 ms | 11.317 ms |
| binomial | 5, 2 | 139.8 µs | 287.1 µs | 606.6 µs | 1.378 ms | 4.335 ms | 13.929 ms |
| ceil | 0.7 | 835.6 ns | 853.3 ns | 735.3 ns | 790.1 ns | 747.2 ns | 672.1 ns |
| chebyshev_Tn | 2, 0.7 | 4.3 µs | 4.2 µs | 5.5 µs | 5.5 µs | 8.0 µs † | 6.7 µs |
| chebyshev_Un | 2, 0.7 | 6.5 µs † | 6.4 µs | 8.5 µs † | 5.9 µs | 7.0 µs | 7.5 µs |
| circ | 0.7 | 1.3 µs | 1.3 µs | 1.3 µs | 1.7 µs † | 1.5 µs | 1.3 µs |
| clausen | 3, 0.7 | 44.6 µs † | 94.6 µs | 216.7 µs | 301.2 µs | 1.002 ms | 6.165 ms |
| clausen2 | 0.7 | 42.1 µs | 86.6 µs | 203.5 µs | 376.1 µs † | 1.156 ms | 5.085 ms |
| conj | 0.7 | 104.4 ns | 145.0 ns | 125.1 ns | 125.1 ns | 133.5 ns | 159.7 ns |
| cos | 0.7 | 3.9 µs | 6.4 µs | 11.7 µs | 19.5 µs | 52.1 µs | 216.9 µs |
| cosec | 0.7 | 4.7 µs | 9.6 µs | 15.0 µs | 23.0 µs | 69.6 µs † | 205.1 µs |
| cosech | 0.7 | 4.9 µs | 8.4 µs | 15.6 µs | 25.2 µs | 65.8 µs | 197.2 µs |
| cosh | 0.7 | 4.3 µs | 10.1 µs | 16.3 µs † | 21.4 µs | 69.2 µs | 204.3 µs |
| cot | 0.7 | 9.9 µs | 12.7 µs | 26.4 µs | 35.8 µs | 115.7 µs | 578.9 µs † |
| coth | 0.7 | 6.4 µs | 16.0 µs | 24.7 µs | 40.8 µs | 130.1 µs | 456.8 µs |
| covercos | 0.7 | 6.3 µs | 10.7 µs | 12.3 µs | 28.5 µs † | 66.8 µs | 204.7 µs |
| coversin | 0.7 | 5.1 µs | 9.3 µs | 13.9 µs | 20.2 µs | 60.6 µs | 214.8 µs |
| cubrt | 0.7 | 17.8 µs | 21.2 µs | 44.2 µs | 47.5 µs | 133.1 µs | 357.8 µs |
| digamma | 2.345 | 185.0 µs † | 397.4 µs | 897.7 µs | 1.017 ms | 5.043 ms | 21.618 ms |
| dilog | 0.7 | 156.1 µs | 343.1 µs | 824.8 µs | 1.330 ms | 4.746 ms | 22.420 ms |
| div | 0.7, 0.7 | 493.3 ns | 913.9 ns | 1.2 µs | 1.5 µs | 4.9 µs | 15.7 µs |
| erf | 0.7 | 30.0 µs | 55.5 µs | 104.7 µs | 128.9 µs | 751.0 µs | 3.443 ms |
| erfc | 0.7 | 21.9 µs | 56.0 µs | 110.9 µs | 158.1 µs | 710.7 µs | 2.450 ms |
| erfcinv | 0.5 | 162.5 µs | 212.0 µs | 2.136 ms † | 715.0 µs | 2.182 ms | 9.568 ms |
| erfinv | 0.5 | 127.8 µs | 270.6 µs | 1.284 ms † | 986.3 µs | 2.232 ms | 6.706 ms |
| exp | 1.23456789 | 4.4 µs | 8.1 µs | 15.5 µs | 21.1 µs | 65.7 µs | 298.4 µs |
| floor | 0.7 | 139.4 ns | 121.0 ns | 159.8 ns | 111.2 ns | 126.9 ns | 123.6 ns |
| gamma | 2.345 | 54.2 µs | 94.4 µs | 194.7 µs | 382.0 µs | 1.414 ms | 6.205 ms |
| gammainc_P | 2.3, 1.4 | 148.9 µs | 346.9 µs | 679.8 µs | 1.097 ms | 3.448 ms | 30.317 ms † |
| gammainc_Q | 2.3, 1.4 | 170.2 µs | 449.4 µs | 720.9 µs | 1.110 ms | 4.318 ms † | 20.632 ms |
| gammainc_lower | 2.3, 1.4 | 77.0 µs | 263.2 µs | 374.2 µs | 616.8 µs | 2.052 ms | 16.398 ms † |
| gammainc_upper | 2.3, 1.4 | 186.2 µs | 488.1 µs † | 684.8 µs | 928.9 µs | 4.170 ms | 17.620 ms |
| gammainv | 9.5 | 599.4 µs | 1.446 ms | 1.699 ms | 2.369 ms | 8.138 ms | 28.969 ms |
| hacovercos | 0.7 | 6.7 µs | 11.2 µs | 20.6 µs | 20.7 µs | 67.3 µs | 259.1 µs |
| hacoversin | 0.7 | 6.1 µs | 12.0 µs | 14.6 µs | 19.4 µs | 62.5 µs | 222.1 µs |
| harmonic_poly | 2, 0.7 | 6.1 µs | 6.0 µs | 8.1 µs | 7.2 µs | 18.0 µs | 42.6 µs |
| havercos | 0.7 | 4.6 µs | 8.3 µs | 14.1 µs | 25.9 µs | 69.4 µs | 197.5 µs |
| haversin | 0.7 | 5.3 µs | 11.7 µs † | 15.7 µs | 19.8 µs | 51.0 µs | 186.4 µs |
| hermite_Hn | 2, 0.7 | 5.5 µs | 5.4 µs | 5.2 µs | 5.5 µs | 6.3 µs | 8.5 µs |
| hypergeometric_pFq | 0.7, 2.5, 0.3 | 66.4 µs † | 179.7 µs † | 248.3 µs † | 406.4 µs † | 1.617 ms † | 7.605 ms † |
| hypot | 0.7, 1.2 | 938.2 ns | 1.9 µs | 1.6 µs | 2.4 µs | 4.7 µs | 11.7 µs |
| imag_part | 0.7 | 144.3 ns | 148.3 ns | 114.4 ns | 89.6 ns | 111.0 ns | 119.4 ns |
| inv | 0.7 | 297.4 ns | 604.9 ns | 571.0 ns | 742.2 ns | 1.9 µs | 5.0 µs |
| lambert_W0 | 0.7 | 73.0 µs | 151.1 µs | 213.5 µs | 429.0 µs | 1.361 ms | 3.912 ms |
| lambert_Wm1 | -0.2 | 75.1 µs | 150.6 µs | 282.7 µs | 317.0 µs | 1.156 ms | 3.353 ms |
| ldexp | 1.23456789, 3 | 208.3 ns | 151.1 ns | 233.3 ns | 144.7 ns | 225.8 ns | 191.0 ns |
| legendre_chi | 2, 0.7 | 73.1 µs | 138.4 µs | 188.9 µs | 402.1 µs | 982.0 µs | 3.946 ms |
| lerch_phi | 0.3, 2, 1.4 | 153.9 µs | 341.7 µs | 609.4 µs | 1.142 ms † | 4.429 ms † | 19.140 ms † |
| lerch_phi (integer order) | 0.3125, -15, 0.375 | 43.7 µs | 32.0 µs | 41.2 µs | 33.3 µs | 44.5 µs | 44.3 µs |
| lerch_phi (negative order) | 0.3125, -15, 0.375 | 43.8 µs | 39.7 µs | 42.8 µs | 45.0 µs | 39.1 µs | 57.3 µs † |
| lg | 2.345678 | 13.1 µs | 23.2 µs | 43.8 µs | 49.6 µs | 92.8 µs | 384.6 µs |
| lgamma | 2.345 | 47.5 µs | 157.3 µs | 214.7 µs | 352.8 µs | 1.285 ms | 6.765 ms |
| ln | 2.345678 | 6.6 µs | 10.6 µs | 16.2 µs | 24.4 µs | 52.9 µs | 163.9 µs |
| log | 2.345678 | 6.0 µs | 10.4 µs | 12.6 µs | 23.3 µs | 59.1 µs | 144.5 µs |
| log10 | 2.345678 | 16.6 µs | 20.5 µs | 36.0 µs | 46.8 µs | 131.1 µs † | 329.5 µs |
| logbeta | 2.3, 4.5 | 145.4 µs | 317.1 µs | 558.0 µs | 1.387 ms | 4.568 ms | 19.267 ms |
| logbeta_pdf | 0.5, 2.5, 3.5 | 137.4 µs | 251.7 µs | 485.2 µs | 845.8 µs | 2.482 ms | 10.924 ms |
| lommel_S | 0.5, 0.7, 1.2 | 123.7 µs † | 107.6 µs | 180.7 µs | 294.5 µs | 1.029 ms | 4.193 ms |
| mul | 0.7, 0.7 | 185.5 ns | 268.0 ns | 380.3 ns | 389.2 ns | 1.2 µs | 3.1 µs |
| mul_long | 17, 3 | 1.0 µs | 1.6 µs † | 1.3 µs | 1.2 µs | 1.1 µs | 1.0 µs |
| mul_pow10 | 1.23456789, 3 | 581.0 ns | 490.5 ns | 691.5 ns | 857.7 ns | 854.8 ns | 824.5 ns |
| neg | 0.7 | 185.2 ns | 139.4 ns | 144.3 ns | 141.5 ns | 127.3 ns | 149.5 ns |
| normal_cdf | 0.7 | 22.9 µs | 110.1 µs | 83.3 µs | 133.2 µs | 601.8 µs † | 2.749 ms |
| normal_logpdf | 0.7 | 6.9 µs | 10.6 µs | 17.3 µs | 21.2 µs | 54.2 µs | 130.5 µs |
| normal_pdf | 0.7 | 5.5 µs | 8.6 µs | 15.0 µs | 25.1 µs | 84.1 µs | 256.0 µs |
| polygamma | 3, 2.345 | 74.1 µs | 178.4 µs | 343.4 µs | 485.4 µs | 3.002 ms | 12.352 ms |
| polylog | 2, 0.7 | 118.4 µs | 378.9 µs | 621.7 µs | 1.022 ms | 4.779 ms | 22.205 ms |
| polylog1 | 0.7 | 8.2 µs | 13.9 µs | 16.8 µs | 23.4 µs | 42.8 µs | 141.1 µs |
| pow | 1.23456789, 3.5 | 15.6 µs † | 20.8 µs | 35.5 µs | 45.9 µs | 127.3 µs | 341.6 µs |
| pow10 | 3 | 396.9 ns | 458.7 ns | 537.8 ns | 814.5 ns | 525.6 ns | 567.0 ns |
| pow_int | 1.23456789, 3 | 393.6 ns | 471.9 ns | 694.1 ns | 955.7 ns | 3.1 µs | 10.1 µs |
| productlog | 0.7 | 68.5 µs | 145.3 µs | 198.1 µs | 323.1 µs | 1.276 ms | 4.204 ms |
| real_part | 0.7 | 134.6 ns | 118.2 ns | 133.6 ns | 161.6 ns | 107.7 ns | 118.7 ns |
| rect | 0.7 | 1.9 µs | 1.8 µs | 1.5 µs | 1.7 µs | 1.5 µs | 1.6 µs |
| root | 1.2345, 3 | 19.0 µs | 27.9 µs | 36.1 µs | 52.7 µs | 124.8 µs | 385.4 µs |
| round | 0.7 | 3.8 µs | 3.7 µs | 4.0 µs | 3.2 µs | 3.4 µs | 3.2 µs |
| roundn | 1.23456789, 3 | 6.8 µs | 8.5 µs | 10.2 µs | 9.2 µs | 9.5 µs | 12.7 µs |
| rounds | 1.23456789, 3 | 22.2 µs | 38.8 µs | 132.1 µs † | 74.6 µs | 100.7 µs | 314.3 µs |
| sec | 0.7 | 3.4 µs | 5.5 µs | 10.0 µs | 20.2 µs | 59.5 µs | 166.4 µs |
| sech | 0.7 | 4.6 µs | 8.9 µs | 13.4 µs | 22.4 µs | 59.1 µs | 213.6 µs |
| sgn | 0.7 | 281.6 ns | 322.3 ns | 280.9 ns | 280.1 ns | 287.3 ns | 298.3 ns |
| sin | 0.567 | 4.2 µs | 9.7 µs | 15.5 µs | 18.4 µs | 59.9 µs | 239.2 µs |
| sinc | 0.7 | 6.2 µs | 12.9 µs | 18.4 µs | 63.6 µs † | 66.9 µs | 182.3 µs |
| sincos | 0.7 | 13.3 µs | 25.4 µs † | 25.4 µs | 24.9 µs | 83.9 µs | 179.0 µs |
| sinh | 0.7 | 5.0 µs | 9.1 µs | 14.9 µs | 24.7 µs † | 68.5 µs | 186.4 µs |
| sinhcosh | 0.7 | 4.9 µs | 7.9 µs | 13.2 µs | 23.9 µs | 54.7 µs | 183.2 µs |
| sqr | 0.7 | 203.8 ns | 304.8 ns | 479.8 ns | 601.1 ns | 860.0 ns | 2.3 µs |
| sqrt | 1.23456789 | 454.4 ns | 502.6 ns | 655.9 ns | 961.3 ns | 1.5 µs | 3.1 µs |
| step | 0.7 | 340.0 ns | 334.1 ns | 347.9 ns | 419.6 ns | 446.6 ns | 441.6 ns |
| struve_H | 2, 0.7 | 188.7 µs | 531.1 µs | 700.1 µs | 1.165 ms | 3.869 ms | 13.787 ms |
| struve_L | 2, 0.7 | 223.5 µs | 520.2 µs | 950.8 µs | 1.113 ms | 4.972 ms † | 15.303 ms |
| sub | 0.7, 0.7 | 145.8 ns | 130.8 ns | 135.3 ns | 143.1 ns | 210.8 ns | 340.5 ns |
| tan | 0.7 | 18.8 µs | 21.1 µs | 30.3 µs | 31.7 µs | 85.1 µs | 223.1 µs |
| tanh | 0.7 | 4.2 µs | 9.3 µs | 14.4 µs | 21.8 µs † | 142.8 µs † | 186.0 µs |
| tetragamma | 2.345 | 62.2 µs | 144.4 µs | 295.6 µs | 622.6 µs | 2.686 ms | 12.698 ms |
| tri | 0.7 | 1.3 µs | 1.6 µs | 1.9 µs | 1.1 µs | 1.2 µs | 1.4 µs |
| trigamma | 2.345 | 38.4 µs | 139.1 µs | 243.4 µs | 358.9 µs | 2.238 ms | 8.499 ms |
| vercos | 0.7 | 5.0 µs † | 8.6 µs † | 10.8 µs | 19.7 µs | 55.1 µs | 152.6 µs |
| versin | 0.7 | 4.1 µs | 7.2 µs | 10.5 µs | 15.2 µs | 56.9 µs | 470.7 µs † |
| zatahp | 2.3, 1.4 | 108.5 µs | 228.8 µs | 293.0 µs | 477.3 µs | 1.946 ms | 10.176 ms |
| zatahp (integer order) | -4, 0.75 | 99.1 µs | 120.0 µs | 297.8 µs † | 243.5 µs | 1.109 ms | 4.193 ms |
| zeta | 2.345 | 1.271 ms | 5.608 ms | 13.350 ms | 1.611 ms | 6.778 ms | 26.796 ms |
| zeta_pair | 2.345 | 328.2 µs | 582.0 µs | 1.053 ms | 2.189 ms | 8.743 ms | 38.335 ms |
| zetah | 2.3, 1.4 | 48.8 µs | 98.1 µs | 157.1 µs | 203.2 µs | 1.149 ms | 3.693 ms |
| zetap | 2.345 | 345.4 µs | 792.6 µs † | 1.132 ms | 2.115 ms † | 7.489 ms | 34.579 ms |

### `mpc`

| Operation | Inputs | 256 bits | 512 bits | 768 bits | 1024 bits | 2048 bits | 4096 bits |
|---|---|---:|---:|---:|---:|---:|---:|
| E1 | 2.345678+0.125i | 107.1 µs | 193.3 µs | 401.5 µs | 643.1 µs | 1.961 ms | 9.657 ms |
| Ei | 2.345678+0.125i | 82.8 µs | 228.1 µs | 342.6 µs | 650.9 µs | 2.134 ms | 7.605 ms |
| Li | 2.345+0.125i | 92.2 µs | 184.9 µs | 348.7 µs | 653.8 µs | 3.213 ms | 8.886 ms |
| abs | 0.7+0.125i | 1.2 µs | 1.6 µs | 1.5 µs | 1.6 µs | 3.4 µs | 6.6 µs |
| acos | 0.7+0.125i | 58.5 µs | 110.7 µs | 136.0 µs | 96.3 µs | 231.5 µs | 489.2 µs |
| acosec | 2.345+0.125i | 61.3 µs | 95.5 µs | 124.9 µs | 148.6 µs | 417.5 µs | 1.046 ms |
| acosech | 0.7+0.125i | 53.0 µs | 85.6 µs | 126.8 µs | 161.0 µs | 430.9 µs | 966.6 µs |
| acosh | 2.345+0.125i | 65.1 µs | 85.3 µs | 104.6 µs | 62.9 µs | 421.3 µs | 528.2 µs |
| acot | 0.7+0.125i | 53.8 µs | 89.7 µs | 90.8 µs | 156.4 µs | 381.3 µs | 955.9 µs |
| acoth | 2.345+0.125i | 41.9 µs | 61.0 µs | 90.9 µs | 128.8 µs | 272.0 µs | 927.3 µs |
| add | 0.7+0.125i, 0.7+0.125i | 435.7 ns | 530.8 ns | 502.1 ns | 465.2 ns | 639.2 ns | 699.4 ns |
| add_long | 17+0.125i, 3 | 3.1 µs | 2.9 µs | 3.5 µs | 3.6 µs | 3.6 µs | 3.4 µs |
| appell_F1 | <span style="display:inline-block;width:40ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">0.5+0.125i, 0.7+0.125i, 0.8+0.125i, 2.5+<br/>0.125i, 0.2+0.125i, 0.3+0.125i</span> | 678.3 µs | 1.382 ms | 2.900 ms | 5.450 ms | 25.354 ms | 140.734 ms |
| arccovercos | 0.5+0.125i | 84.3 µs | 97.8 µs | 56.9 µs | 231.8 µs | 182.5 µs | 458.3 µs |
| arccoversin | 0.5+0.125i | 61.9 µs | 44.2 µs | 56.3 µs | 176.9 µs | 163.7 µs | 494.0 µs |
| archacovercos | 0.5+0.125i | 15.5 µs | 18.1 µs | 21.3 µs | 33.3 µs | 53.1 µs | 135.9 µs |
| archacoversin | 0.5+0.125i | 17.8 µs | 23.2 µs | 21.2 µs | 35.5 µs | 50.5 µs | 139.1 µs |
| archavercos | 0.5+0.125i | 18.7 µs | 17.0 µs | 23.4 µs | 29.0 µs | 59.7 µs | 157.4 µs |
| archaversin | 0.5+0.125i | 13.4 µs | 23.8 µs | 25.2 µs | 33.7 µs | 56.9 µs | 130.5 µs |
| arcvercos | 0.5+0.125i | 56.8 µs | 44.2 µs | 116.8 µs | 83.0 µs | 181.4 µs | 446.4 µs |
| arcversin | 0.5+0.125i | 44.0 µs | 66.3 µs | 125.9 µs | 77.0 µs | 170.9 µs | 527.9 µs |
| arg | 0.7+0.125i | 16.5 µs | 24.6 µs | 43.0 µs | 72.9 µs | 108.9 µs | 450.3 µs |
| asec | 2.345+0.125i | 60.8 µs | 96.6 µs | 106.7 µs | 153.5 µs | 356.0 µs | 1.001 ms |
| asech | 0.7+0.125i | 31.7 µs | 48.1 µs | 59.1 µs | 115.7 µs | 186.9 µs | 632.9 µs |
| asin | 0.7+0.125i | 83.4 µs | 90.8 µs | 133.4 µs | 205.2 µs | 382.4 µs | 493.3 µs |
| asinh | 0.7+0.125i | 59.5 µs | 90.7 µs | 121.6 µs | 157.6 µs | 369.5 µs | 926.7 µs |
| atan | 0.7+0.125i | 48.0 µs | 81.0 µs | 109.2 µs | 210.7 µs † | 322.8 µs | 1.016 ms |
| atanh | 0.7+0.125i | 50.3 µs | 92.7 µs | 108.3 µs | 193.1 µs | 316.6 µs | 931.9 µs |
| bessel_I | 2, 0.7+0.125i | 224.7 µs | 426.8 µs | 706.5 µs | 1.353 ms | 4.037 ms | 21.275 ms |
| bessel_J | 2, 0.7+0.125i | 246.1 µs | 522.5 µs | 767.7 µs | 1.324 ms | 5.008 ms | 18.615 ms |
| bessel_K | 2, 0.7+0.125i | 128.0 µs | 267.1 µs | 491.2 µs | 719.7 µs | 3.301 ms | 13.470 ms |
| bessel_Y | 2, 0.7+0.125i | 221.6 µs | 457.7 µs | 715.5 µs | 1.358 ms | 4.697 ms | 20.093 ms |
| ceil | 0.7+0.125i | 2.1 µs | 1.7 µs | 1.7 µs | 2.0 µs | 1.7 µs | 1.8 µs |
| chebyshev_Tn | 2, 0.7+0.125i | 10.7 µs | 10.6 µs | 11.2 µs | 16.7 µs | 14.7 µs | 17.4 µs |
| chebyshev_Un | 2, 0.7+0.125i | 17.7 µs | 14.5 µs | 19.5 µs | 17.9 µs | 17.2 µs | 21.7 µs |
| circ | 0.7 | 2.4 µs | 2.7 µs | 3.0 µs | 4.0 µs | 5.0 µs | 8.1 µs |
| clausen | 3, 0.7+0.125i | 575.0 µs † | 423.3 µs | 723.0 µs | 2.795 ms † | 5.294 ms | 27.414 ms |
| clausen2 | 0.7+0.125i | 195.2 µs | 426.7 µs | 840.5 µs | 1.279 ms | 6.129 ms | 32.607 ms |
| conj | 0.7+0.125i | 351.0 ns | 311.9 ns | 395.6 ns | 348.1 ns | 332.3 ns | 378.5 ns |
| cos | 0.7+0.125i | 23.3 µs | 16.3 µs | 24.9 µs | 45.9 µs | 107.3 µs | 347.2 µs |
| cosec | 0.7+0.125i | 20.7 µs | 20.8 µs | 33.2 µs | 52.6 µs | 114.4 µs | 391.9 µs |
| cosech | 0.7+0.125i | 15.7 µs | 17.6 µs | 26.4 µs | 130.4 µs | 110.5 µs | 360.4 µs |
| cosh | 0.7+0.125i | 10.8 µs | 17.5 µs | 21.7 µs | 118.0 µs | 102.2 µs | 279.7 µs |
| cot | 0.7+0.125i | 46.4 µs | 33.1 µs | 54.2 µs | 101.1 µs | 232.2 µs | 998.6 µs |
| coth | 0.7+0.125i | 22.9 µs | 37.5 µs | 46.4 µs | 178.5 µs | 211.4 µs | 603.9 µs |
| covercos | 0.7+0.125i | 20.9 µs | 18.7 µs | 31.4 µs | 53.2 µs | 130.1 µs | 454.0 µs |
| coversin | 0.7+0.125i | 28.3 µs | 22.5 µs | 27.5 µs | 54.1 µs | 128.0 µs | 456.1 µs |
| cubrt | 0.7+0.125i | 49.1 µs | 79.2 µs | 103.7 µs | 172.3 µs | 407.3 µs | 1.325 ms |
| digamma | 2.345+0.125i | 335.9 µs | 510.8 µs | 874.7 µs | 2.434 ms † | 4.873 ms | 24.558 ms |
| dilog | 0.7+0.125i | 174.8 µs | 470.2 µs | 1.051 ms | 1.931 ms | 6.545 ms | 27.958 ms |
| div | 0.7+0.125i, 0.7+0.125i | 10.3 µs | 13.8 µs | 19.1 µs | 26.5 µs | 44.1 µs | 114.5 µs |
| erf | 0.7+0.125i | 90.6 µs | 226.4 µs | 322.9 µs | 551.8 µs | 2.230 ms | 10.196 ms |
| erfc | 0.7+0.125i | 114.8 µs | 227.1 µs | 391.6 µs | 557.7 µs | 2.182 ms | 11.720 ms |
| erfcinv | 0.5+0.125i | 1.123 ms | 1.250 ms | 1.815 ms | 2.472 ms | 8.579 ms | 27.177 ms |
| erfinv | 0.5+0.125i | 591.5 µs | 1.089 ms | 1.995 ms | 2.711 ms | 7.172 ms | 39.851 ms † |
| exp | 1.23456789+0.125i | 10.7 µs | 21.2 µs | 36.2 µs | 61.3 µs | 204.8 µs | 726.9 µs |
| floor | 0.7+0.125i | 345.8 ns | 316.2 ns | 369.6 ns | 310.3 ns | 326.9 ns | 343.9 ns |
| gamma | 2.345+0.125i | 252.7 µs † | 403.6 µs † | 510.9 µs † | 751.7 µs † | 2.610 ms † | 8.316 ms † |
| gammainc_P | 2.3+0.125i, 1.4+0.125i | 1.133 ms † | 1.204 ms † | 2.122 ms † | 3.062 ms † | 9.068 ms † | 37.741 ms † |
| gammainc_Q | 2.3+0.125i, 1.4+0.125i | 832.7 µs † | 1.515 ms † | 2.468 ms † | 3.525 ms † | 9.067 ms † | 35.509 ms † |
| gammainc_lower | 2.3+0.125i, 1.4+0.125i | 517.0 µs † | 728.6 µs † | 1.281 ms † | 2.046 ms † | 7.098 ms † | 26.929 ms † |
| gammainc_upper | 2.3+0.125i, 1.4+0.125i | 862.8 µs † | 1.294 ms † | 2.129 ms † | 3.410 ms † | 8.991 ms † | 35.074 ms † |
| gammainv | 9.5+0.125i | 3.964 ms † | 4.168 ms † | 4.409 ms † | 6.602 ms † | 16.023 ms † | 39.573 ms † |
| hacovercos | 0.7+0.125i | 24.0 µs | 25.5 µs | 33.0 µs | 54.9 µs | 123.7 µs | 483.4 µs |
| hacoversin | 0.7+0.125i | 27.2 µs | 32.4 µs | 37.5 µs | 52.4 µs | 120.4 µs | 411.2 µs |
| harmonic_poly | 2, 0.7+0.125i | 21.7 µs | 21.0 µs | 23.8 µs | 25.3 µs | 35.4 µs | 80.0 µs |
| havercos | 0.7+0.125i | 25.6 µs | 22.8 µs | 36.0 µs | 48.4 µs | 151.9 µs | 407.4 µs |
| haversin | 0.7+0.125i | 34.3 µs | 23.6 µs | 32.2 µs | 54.6 µs | 138.6 µs | 427.0 µs |
| hermite_Hn | 2, 0.7+0.125i | 19.5 µs | 15.4 µs | 15.2 µs | 21.5 µs | 23.4 µs | 26.4 µs |
| hypergeometric_pFq | 0.7+0.125i, 2.5+0.125i, 0.3+0.125i | 348.4 µs † | 549.5 µs † | 912.0 µs † | 1.948 ms † | 3.876 ms † | 14.722 ms † |
| imag_part | 0.7+0.125i | 144.1 ns | 178.6 ns | 151.9 ns | 164.6 ns | 178.9 ns | 229.4 ns |
| inv | 0.7+0.125i | 1.3 µs | 2.1 µs | 2.6 µs | 2.9 µs | 4.8 µs | 14.9 µs |
| ldexp | 1.23456789+0.125i, 3 | 364.7 ns | 386.5 ns | 359.9 ns | 447.4 ns | 512.1 ns | 405.3 ns |
| legendre_chi | 2, 0.7+0.125i | 134.2 µs | 289.3 µs | 479.4 µs | 694.4 µs | 2.267 ms | 8.087 ms |
| lerch_phi | 0.3+0.125i, 2+0.125i, 1.4+0.125i | 542.0 µs | 10.0 µs | 10.7 µs | 13.8 µs | 11.0 µs | 12.1 µs |
| lerch_phi (integer order) | 0.3125+0.125i, -15, 0.375+0.125i | 45.4 µs | 47.8 µs | 47.9 µs | 47.8 µs | 45.6 µs | 58.0 µs |
| lerch_phi (negative order) | 0.3125+0.125i, -15+0.125i, 0.375+0.125i | 669.8 µs | 13.2 µs | 10.9 µs | 14.1 µs | 13.4 µs | 11.1 µs |
| lg | 2.345678+0.125i | 30.9 µs | 43.6 µs | 70.2 µs | 90.2 µs | 192.9 µs | 587.2 µs |
| lgamma | 2.345+0.125i | 190.1 µs † | 308.5 µs † | 510.7 µs † | 708.2 µs † | 2.116 ms † | 7.221 ms † |
| ln | 2.345678+0.125i | 18.9 µs | 34.5 µs | 41.0 µs | 58.5 µs | 145.1 µs | 430.5 µs |
| log | 2.345678+0.125i | 28.5 µs | 32.7 µs | 43.8 µs | 73.1 µs | 147.3 µs | 436.5 µs |
| log10 | 2.345678+0.125i | 34.4 µs | 50.1 µs | 61.3 µs | 81.6 µs | 260.2 µs | 643.4 µs |
| lommel_S | 0.5+0.125i, 0.7+0.125i, 1.2+0.125i | 290.6 µs | 618.4 µs | 816.7 µs | 1.685 ms | 6.437 ms | 19.330 ms |
| mul | 0.7+0.125i, 0.7+0.125i | 2.3 µs | 1.8 µs | 2.5 µs | 3.8 µs | 4.7 µs | 8.1 µs |
| mul_long | 17+0.125i, 3 | 5.4 µs | 4.5 µs | 4.5 µs | 4.5 µs | 4.4 µs | 4.7 µs |
| mul_pow10 | 1.23456789+0.125i, 3 | 4.0 µs | 3.2 µs | 4.2 µs | 3.5 µs | 4.7 µs | 3.4 µs |
| neg | 0.7+0.125i | 349.6 ns | 328.8 ns | 362.2 ns | 367.1 ns | 362.6 ns | 447.5 ns |
| normal_cdf | 0.7+0.125i | 97.8 µs | 236.6 µs | 409.1 µs | 705.4 µs | 2.153 ms | 10.193 ms |
| normal_logpdf | 0.7+0.125i | 10.0 µs | 13.1 µs | 19.4 µs | 28.9 µs | 49.7 µs | 144.4 µs |
| normal_pdf | 0.7+0.125i | 26.5 µs | 35.2 µs | 57.3 µs | 86.3 µs | 260.9 µs | 1.088 ms |
| polygamma | 3, 2.345+0.125i | 350.9 µs | 914.6 µs | 1.403 ms | 1.983 ms | 8.047 ms | 33.463 ms |
| polylog | 2, 0.7+0.125i | 238.1 µs | 435.3 µs | 871.0 µs | 1.629 ms | 6.082 ms | 28.369 ms |
| polylog1 | 0.7+0.125i | 28.9 µs | 45.2 µs | 55.0 µs | 77.8 µs | 191.9 µs | 474.5 µs |
| pow | 1.23456789+0.125i, 3.5+0.125i | 42.6 µs | 89.6 µs | 101.1 µs | 139.2 µs | 443.8 µs | 1.645 ms |
| pow10 | 3 | 541.8 ns | 613.1 ns | 832.6 ns | 766.5 ns | 600.0 ns | 706.7 ns |
| pow_int | 1.23456789+0.125i, 3 | 9.3 µs | 8.1 µs | 10.8 µs | 10.1 µs | 12.7 µs | 24.9 µs |
| real_part | 0.7+0.125i | 180.1 ns | 165.1 ns | 156.0 ns | 149.8 ns | 140.3 ns | 212.7 ns |
| rect | 0.7 | 3.0 µs | 2.7 µs | 2.9 µs | 3.8 µs | 5.5 µs | 11.0 µs |
| root | 1.2345+0.125i, 3 | 60.7 µs | 70.2 µs | 193.5 µs | 298.3 µs | 442.7 µs | 1.377 ms |
| round | 0.7+0.125i | 14.2 µs | 13.3 µs | 10.7 µs | 11.0 µs | 12.5 µs | 11.2 µs |
| roundn | 1.23456789+0.125i, 3 | 15.3 µs | 21.6 µs | 20.8 µs | 18.6 µs | 25.2 µs | 38.8 µs |
| rounds | 1.23456789+0.125i, 3 | 62.9 µs | 73.2 µs | 82.2 µs | 110.4 µs | 233.8 µs | 548.8 µs |
| sec | 0.7+0.125i | 19.2 µs | 19.6 µs | 27.3 µs | 40.8 µs | 129.6 µs | 394.7 µs |
| sech | 0.7+0.125i | 18.3 µs | 20.1 µs | 29.7 µs | 90.6 µs | 103.9 µs | 325.6 µs |
| sgn | 0.7 | 1.3 µs | 981.7 ns | 1.1 µs | 848.3 ns | 1.3 µs | 1.3 µs |
| sin | 0.567+0.125i | 21.2 µs | 16.4 µs | 27.8 µs | 41.1 µs | 106.6 µs | 378.4 µs |
| sinc | 0.7+0.125i | 19.8 µs | 28.3 µs | 37.8 µs | 54.6 µs | 137.5 µs | 424.4 µs |
| sinh | 0.7+0.125i | 12.3 µs | 14.9 µs | 25.5 µs | 97.6 µs | 85.3 µs | 282.9 µs |
| sqr | 0.7+0.125i | 2.1 µs | 2.0 µs | 2.6 µs | 2.9 µs | 4.1 µs | 9.3 µs |
| sqrt | 1.23456789+0.125i | 2.6 µs | 2.8 µs | 4.0 µs | 5.0 µs | 7.7 µs | 14.7 µs |
| step | 0.7 | 1.2 µs | 982.4 ns | 928.5 ns | 890.5 ns | 1.0 µs | 938.2 ns |
| struve_H | 2, 0.7+0.125i | 288.9 µs | 557.6 µs | 913.1 µs | 1.615 ms | 5.978 ms | 23.284 ms |
| struve_L | 2, 0.7+0.125i | 360.0 µs | 567.2 µs | 1.061 ms | 1.763 ms | 5.762 ms | 22.795 ms |
| sub | 0.7+0.125i, 0.7+0.125i | 447.8 ns | 371.9 ns | 432.3 ns | 481.2 ns | 516.4 ns | 631.5 ns |
| tan | 0.7+0.125i | 18.7 µs | 21.3 µs | 35.1 µs | 51.4 µs | 119.6 µs | 417.3 µs |
| tanh | 0.7+0.125i | 27.0 µs | 30.0 µs | 74.4 µs | 39.3 µs | 98.9 µs | 367.2 µs |
| tetragamma | 2.345+0.125i | 413.6 µs | 858.7 µs | 1.579 ms | 2.172 ms | 7.866 ms | 39.931 ms |
| tri | 0.7 | 3.3 µs | 2.6 µs | 3.4 µs | 3.4 µs | 4.8 µs | 9.3 µs |
| trigamma | 2.345+0.125i | 311.4 µs | 690.8 µs | 1.205 ms | 2.114 ms | 5.914 ms | 29.023 ms |
| vercos | 0.7+0.125i | 37.9 µs † | 18.3 µs | 27.9 µs | 59.1 µs † | 110.7 µs | 460.5 µs |
| versin | 0.7+0.125i | 26.6 µs | 21.1 µs | 37.2 µs | 56.0 µs | 129.6 µs | 420.9 µs |
| zatahp | 2.3+0.125i, 1.4+0.125i | 2.626 ms | 5.274 ms | 9.506 ms | 1.198 ms | 3.110 ms | 12.691 ms |
| zatahp (integer order) | -4, 0.75+0.125i | 2.203 ms | 4.975 ms | 10.554 ms | 779.8 µs | 3.574 ms | 15.856 ms |
| zeta | 2.345+0.125i | 975.1 µs | 1.471 ms | 2.731 ms | 4.335 ms | 19.315 ms | 116.961 ms |
| zeta_pair | 2.345+0.125i | 948.9 µs | 1.794 ms | 3.305 ms | 5.616 ms | 21.954 ms | 125.945 ms |
| zetah | 2.3+0.125i, 1.4+0.125i | 2.174 ms | 5.037 ms | 8.790 ms | 718.8 µs | 3.198 ms | 14.239 ms |
| zetap | 2.345+0.125i | 708.1 µs | 1.503 ms | 3.298 ms | 6.035 ms | 23.591 ms | 131.738 ms |

### number / unrestricted integer

| Operation | Inputs | Exact |
|---|---|---:|
| abs | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">-9712853409152031141207811585727780469484705121206232579607434236969009533618825<br/>97201918971499157056640605432269691427807258223374967278846596194834524005116336<br/>17514882293623272276764857474830401626415841035971458657442133370777607803464071<br/>101158385124186229995073045427448918227959893082282919863982307657776</span> | 109.5 ns |
| add | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 107.2 ns |
| bit_and | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 240.4 ns |
| bit_length | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776</span> | 164.0 ns |
| bit_not | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776</span> | 259.3 ns |
| bit_or | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 206.1 ns |
| bit_xor | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 202.5 ns |
| clear_bit | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>701</span> | 406.8 ns |
| div | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>340282366920938463463374607431768211454</span> | 1.2 µs |
| divmod | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 335.4 ns |
| factorial | 5000 | 126.1 µs |
| factors | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">54772119019154636339867862041163448790974484131459861520687327809209914339836012<br/>52275954799541486775711126005967382600118182797233708133775335788335658037065081<br/>31569062299976033041302189833215742992653063887339218309467131548996199881113600<br/>00000000000000000000000000000000000000000000000000000000000000000000000000000000<br/>0000000000000000000000000000000000000000000000</span> | 134.9 µs |
| fibonacci | 10000 | 6.6 µs |
| gcd | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 3.0 µs |
| gcdext | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 3.7 µs |
| is_prime | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">68647976601306097149819007990813932172694353001433054093944634591855431833976560<br/>52122559640661454554977296311391480858037121987999716643812574028291115057151</span> | 521.4 µs |
| isqrt | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776</span> | 417.0 ns |
| lcm | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 4.6 µs |
| mod | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 339.1 ns |
| modinv | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>68647976601306097149819007990813932172694353001433054093944634591855431833976560<br/>52122559640661454554977296311391480858037121987999716643812574028291115057151</span> | 4.0 µs |
| mul | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 219.9 ns |
| neg | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776</span> | 62.4 ns |
| next_prime | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 5.274 ms |
| partition | 5000 | 7.074 ms |
| pow | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>5</span> | 3.8 µs |
| pow_int | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>5</span> | 2.9 µs |
| powmod | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662, <br/>68647976601306097149819007990813932172694353001433054093944634591855431833976560<br/>52122559640661454554977296311391480858037121987999716643812574028291115057151</span> | 144.7 µs |
| prev_prime | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 6.317 ms |
| prove_prime | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">68647976601306097149819007990813932172694353001433054093944634591855431833976560<br/>52122559640661454554977296311391480858037121987999716643812574028291115057151</span> | 4.039 ms |
| set_bit | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>700</span> | 301.1 ns |
| shl | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>257</span> | 438.1 ns |
| shr | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>257</span> | 313.7 ns |
| sqr | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776</span> | 285.3 ns |
| sub | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 108.1 ns |
| test_bit | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>701</span> | 325.1 ns |

### number / unrestricted fraction

| Operation | Inputs | Exact |
|---|---|---:|
| abs | $\frac{\begin{array}{l}\text{-9712853409152031141207811585727780469484705121206232579607434236969009533618825} \\ \text{97201918971499157056640605432269691427807258223374967278846596194834524005116336} \\ \text{17514882293623272276764857474830401626415841035971458657442133370777607803464071} \\ \text{101158385124186229995073045427448918227959893082282919863982307657776}\end{array}}{\begin{array}{l}\text{68647976601306097149819007990813932172694353001433054093944634591855431833976560} \\ \text{52122559640661454554977296311391480858037121987999716643812574028291115057151}\end{array}}$ | 220.1 ns |
| add | **Input 1**<br/>$\frac{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}{\begin{array}{l}\text{68647976601306097149819007990813932172694353001433054093944634591855431833976560} \\ \text{52122559640661454554977296311391480858037121987999716643812574028291115057151}\end{array}}$<br/><br/>**Input 2**<br/>$\frac{\begin{array}{l}\text{12498846048367045926370716602609982839721641429222959125376462582674869403443996} \\ \text{972648628699047686839582654905384579341977328653591547736951147466435189662}\end{array}}{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}$ | 4.1 µs |
| div | **Input 1**<br/>$\frac{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}{\begin{array}{l}\text{68647976601306097149819007990813932172694353001433054093944634591855431833976560} \\ \text{52122559640661454554977296311391480858037121987999716643812574028291115057151}\end{array}}$<br/><br/>**Input 2**<br/>$\frac{\begin{array}{l}\text{12498846048367045926370716602609982839721641429222959125376462582674869403443996} \\ \text{972648628699047686839582654905384579341977328653591547736951147466435189662}\end{array}}{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}$ | 8.7 µs |
| inv | $\frac{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}{\begin{array}{l}\text{68647976601306097149819007990813932172694353001433054093944634591855431833976560} \\ \text{52122559640661454554977296311391480858037121987999716643812574028291115057151}\end{array}}$ | 155.4 ns |
| mul | **Input 1**<br/>$\frac{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}{\begin{array}{l}\text{68647976601306097149819007990813932172694353001433054093944634591855431833976560} \\ \text{52122559640661454554977296311391480858037121987999716643812574028291115057151}\end{array}}$<br/><br/>**Input 2**<br/>$\frac{\begin{array}{l}\text{12498846048367045926370716602609982839721641429222959125376462582674869403443996} \\ \text{972648628699047686839582654905384579341977328653591547736951147466435189662}\end{array}}{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}$ | 3.3 µs |
| neg | $\frac{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}{\begin{array}{l}\text{68647976601306097149819007990813932172694353001433054093944634591855431833976560} \\ \text{52122559640661454554977296311391480858037121987999716643812574028291115057151}\end{array}}$ | 144.0 ns |
| pow | **Input 1**<br/>$\frac{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}{\begin{array}{l}\text{68647976601306097149819007990813932172694353001433054093944634591855431833976560} \\ \text{52122559640661454554977296311391480858037121987999716643812574028291115057151}\end{array}}$<br/><br/>**Input 2**<br/>5 | 40.4 µs |
| pow_int | **Input 1**<br/>$\frac{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}{\begin{array}{l}\text{68647976601306097149819007990813932172694353001433054093944634591855431833976560} \\ \text{52122559640661454554977296311391480858037121987999716643812574028291115057151}\end{array}}$<br/><br/>**Input 2**<br/>5 | 51.2 µs |
| sqr | $\frac{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}{\begin{array}{l}\text{68647976601306097149819007990813932172694353001433054093944634591855431833976560} \\ \text{52122559640661454554977296311391480858037121987999716643812574028291115057151}\end{array}}$ | 615.3 ns |
| sub | **Input 1**<br/>$\frac{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}{\begin{array}{l}\text{68647976601306097149819007990813932172694353001433054093944634591855431833976560} \\ \text{52122559640661454554977296311391480858037121987999716643812574028291115057151}\end{array}}$<br/><br/>**Input 2**<br/>$\frac{\begin{array}{l}\text{12498846048367045926370716602609982839721641429222959125376462582674869403443996} \\ \text{972648628699047686839582654905384579341977328653591547736951147466435189662}\end{array}}{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}$ | 4.8 µs |

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
