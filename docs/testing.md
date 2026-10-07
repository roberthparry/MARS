# Testing MARS

The project provides per-module test targets.

## Test Reports

The native harness prints individual case results, group totals, a final
summary and the slowest cases to the terminal. It does not currently emit
JUnit XML. Capture both standard output and standard error when preserving
verification evidence, and keep a separate log for every isolated batch.

```sh
make test_qfloat
```

- Output examples have separate totals from ordinary correctness cases.
- A successful process exit and final summary are required for a completed
  batch. Reconcile the cases across partitions; a skipped group does not
  establish coverage for its children.

## Run Tests

```sh
make test_number
make test_qfloat
make test_qcomplex
make test_expression
make test_datetime
make test_dictionary
make test_set
make test_array
make test_string
make test_bitset
make test_matrix
make test_integrator
```

## Typical Workflow

During development:

```sh
make debug
make test_qfloat
```

Before committing:

```sh
make release
make check-native-numeric-boundaries
make test_number
make test_qfloat
make test_qcomplex
make test_expression
make test_datetime
make test_dictionary
make test_set
make test_array
make test_string
make test_bitset
make test_matrix
make test_integrator
```

`make release` already includes `check-native-numeric-boundaries`; the explicit
command is useful when changing only qfloat or qcomplex. It rejects direct
MPFR/MPC includes, calls and unresolved symbols in those native double-double
modules.

## Runnable C examples

Executable C examples in the Markdown guides are complete programs, including
headers and `main()`. API signatures and type-declaration reference blocks
are not executable examples; SQL, shell and MARS-language blocks retain their
own languages.

Run `make -j1 test-readme-examples` to extract, compile, link and execute every
C example sequentially. On success the checker reports that every checked
README program passed with zero failures. This check also runs **last** in
`make -j1 test`, after the native suites and their module README tests.

The checker reads each program directly from its guide rather than maintaining
a second copy. It compares its standard output with the accompanying output
block, allowing only trailing whitespace differences. A missing `main()`,
compiler error, non-zero exit, timeout, missing output block or output mismatch
fails the check. It writes extracted sources, executables, compiler logs,
captured output and a JSON report under `build/readme-examples/`.

To run an extracted example yourself, execute its named binary in that
directory. To compile a copied block independently, save it as a C source file
and use the same compiler/linker options recorded by Make: the public headers
are in `include/`, the archive is `build/release/libmars.a`, and the external
libraries are listed in the [build guide](building.md). GNU C extensions are
required; do not disable assertions in examples that use `assert`.

Programs run in separate temporary working directories, so relative database
and file names cannot overwrite project files. File examples receive fresh
paths as command-line arguments. HTTP examples use the local fixture, while
`make -j1 test-http-live` tests the client against the native MARS web server
on loopback, with no external-service calls or opt-in flag. Almanac and
jurisdiction examples require the configured databases described in their
module guides; missing data is a failure, not a skipped success.

The [webserver example](webserver.md) starts its own single-request loopback
server on an ephemeral port and calls it with the MARS client. The dedicated
`make -j1 test_webserver` suite runs its ordinary protocol checks before this
README example, without contacting external services.

The `tools/check_readme_examples.py` entry in `tests/test_config.json` controls
individual README programs. Isolate a failure there, then restore all entries
before the complete check. Identifiers combine the guide path and C-block
number. This is functional example testing, not a memory-test run.

## Resource-Bounded Memory Checks

Run memory checks only when explicitly requested. Use small, sequential batches
instead of an uncapped full-suite run. Isolate failing tests through
`tests/test_config.json`, preserve its original contents, and restore them after
the checks. Run README examples after the other tests.

On the Ophelia DreamQuest mini PC, use a transient systemd user service with
enforced per-batch limits: `MemoryMax=512M`, `MemorySwapMax=0`, `CPUQuota=50%`,
`Nice=15`, `IOWeight=10`, `TasksMax=16`, `RuntimeMaxSec=90`,
`TimeoutStopSec=5` and `OOMPolicy=kill`. These are per-job limits, not changes to
the machine's global settings. Verify the effective cgroup memory, swap and CPU
limits with a lightweight preflight before starting tests. If the limits cannot
be enforced, do not fall back to an unbounded run. These safeguards limit test
resource consumption; they do not guarantee protection against unrelated
machine failures.

If a batch reaches a resource limit, record it as incomplete and reduce its
scope rather than automatically raising the limits. A timeout or forced stop
does not establish that the final leak scan passed. Apply the same resource
discipline to any prerequisite instrumented build.

Use the normal release binary for Valgrind checks, with full leak reporting,
origin tracking and a non-zero error exit code. Treat definite, indirect and
possible leaks as errors. Retain Valgrind's default 12-frame trace depth for
routine bounded checks; deeper allocation traces can substantially increase
the checker's own memory use. Increasing trace depth is not required for full
leak detection. AddressSanitizer, LeakSanitizer and
UndefinedBehaviorSanitizer provide complementary checks in a separate build;
stop on the first diagnostic. If sandbox restrictions prevent LeakSanitizer's
final scan, report that check as incomplete rather than disabling leak detection
and calling the result clean.

### Recorded Check: 6 September 2026

The focused checks against commit `e879c4d` completed as follows:

- Four native Lab sanitizer cases passed: the infinite-series domain case,
  complex polynomial roots, and bound complex `Ei` and `exp(Li(...))`
  derivatives. No leaks or memory-access errors were reported in those cases.
- A focused Valgrind complex-polynomial check completed with zero bytes in use
  at exit and zero reported errors. Peak service memory was 257.1 MiB, with no
  swap use.
- The unset-binding `exp(Li(x+iy))` case stopped on signed integer overflow in
  `number_special_series_converged`, at `src/number/number_maths.c:237` in that
  revision. The stack passed through complex `Li`/`Ei` evaluation and the Lab's
  display inspection. This was unresolved at the time of that check; the
  aborted case did not complete its final leak scan. The broader 16 September
  audit below records its fix and subsequent regression checks.

Full-suite memory coverage was not completed. These targeted results are not a
project-wide guarantee of memory safety.

### Recorded Check: 15 September 2026

The differential-equation and expression checks based on commit `21292ad`
found two ownership leaks: the Taylor-series formatter replaced an allocated
exponent, and the wave solver replaced an allocated coefficient value. Both
paths now release or avoid the redundant allocation, including all callers of
the shared wave helper.

After these fixes, 13 distinct regression tests and four README examples
completed Valgrind checks with zero reported errors and zero bytes in use at
exit. These cover Taylor-series rendering, wave coefficients and rejected
inputs, the three-dimensional Kirchhoff family, symmetric-argument and
integral rendering, and symbolic-function parsing and substitution. Six
additional modified-Emden and Lie tests were clean before the fixes.

The larger polynomial wave IVP regression reached the enforced 512 MiB limit
and was killed before its final leak scan. At that point its memory result was
incomplete; the 16 September follow-up below closes this particular gap.
The smaller documented polynomial-and-integral wave example subsequently
completed cleanly under the unchanged limits. This does not establish clean
memory coverage for every case in the larger regression.

All memory batches used the safeguards above. Full-suite memory coverage and
sanitizer checks remain incomplete; the earlier undefined-behaviour finding
has not been resolved by these ownership fixes.

With the original test configuration restored, both affected ordinary native
suites passed: 152 differential-equation tests and 20 README examples, followed
by 513 expression tests and its output example.

### Recorded Check: 16 September 2026

The polynomial wave IVP test now has individually selectable subtests in
`tests/diffequation/test_diffequ_wave_ivp.c`. Keep its parent
`test_diffequ_wave_ivp_polynomial_data` enabled in the main differential-equation
test configuration, then select one child in the helper file's matching group.
Each child retains its solution comparison, PDE residual, both initial
conditions and derivation checks. Run the children sequentially in separate
bounded processes, and restore their enabled states afterwards.

All seven cases completed full Valgrind leak scans with zero reported errors
and zero bytes in use at exit. These cover polynomial, constant and quadratic
forcing, a scaled operator, shifted initial time, renamed coordinates and
trigonometric initial data. No assertions or sample points were removed.

The isolated quadratic case still exceeded 512 MiB with the earlier 30-frame
trace setting. With Valgrind's default 12 frames, it completed in 76.172 seconds
at 268.9 MiB. The first polynomial case also fell from 475.3 MiB to 257 MiB,
with identical allocation and free counts. The excessive diagnostic trace
depth contributed substantially to the checker's memory consumption.

All seven default-depth runs retained full leak reporting, origin tracking,
allocation/free stack tracking and the existing memory-error checks. They used
the unchanged 512 MiB, zero-swap, 50% CPU and 90-second safeguards. Splitting the
cases also keeps the expensive quadratic check within its own time budget.
This resolves the previously incomplete polynomial-wave regression, not
whole-library memory coverage or the separately recorded sanitizer finding.

After restoring the original enablement and enabling all seven new children,
the full ordinary differential-equation suite passed: 158 tests and 20 README
examples, with none skipped. The previous single parent test is now a group
of seven cases, accounting for the increase from 152 to 158 tests.

### Broader Audit: 16 September 2026

The follow-up audit fixes the earlier signed-overflow diagnostic in complex
`Ei`/`Li` evaluation. Non-finite inputs no longer enter finite-exponent
convergence arithmetic, and the exponent-gap comparison avoids signed
subtraction overflow. A dedicated number regression checks promoted complex
NaNs and infinities, together with the real-axis singular values.

UndefinedBehaviorSanitizer also exposed misaligned dictionary and set slots,
and a null pointer passed to `qsort` for an empty holiday query. Container
headers and strides now preserve fundamental alignment; allocation-size
rounding and arena growth reject overflow. The empty holiday query avoids
sorting fewer than two events. New regressions cover odd-sized and
`long double` elements, growth and removal, impossible element sizes and empty
holiday results. These fixes complement the two ownership fixes recorded above.

The wider differential-equation run subsequently found a 352-byte leak when a
nonlinear characteristic equation passed through the radial-Euler recogniser.
All five number-replacement sites in that recogniser now destroy the previous
value before assigning its replacement. The isolated reproducer, accepted and
rejected radial cases, and the original ten-test batch completed clean final
leak scans after the fix.

The wave checks then exposed redundant number allocations in the independent
spherical-quadrature test helper and the expression evaluator's numerical
integral fallback. Removing those allocations fixed the isolated reproducers
and the original wave batch. The integrator documentation now uses
allocation-free output initialisation, matching the existing README tests, and
states that output slots must not retain an owned previous value.

Isolating the expression regressions also exposed first-use initialisation of
canonical expression constants: evaluating `EXPR_LN10` before constructing an
expression failed its assertion and bypassed the test's cleanup. Evaluation
and rendering now initialise the constants, a first-use rendering regression
was added, and the assertion checks run after cleanup. Both isolated cases
completed cleanly. A separate sixth-root regression found unscoped temporary
products in exact-complex evaluation and a retained temporary in the test's
expected value. The evaluator now uses the same scoped ownership convention
as its sibling binding routines; returned components remain detached and
owned by the caller. The sixth-root reproducer completed its final leak scan
after both fixes.

Full-library AddressSanitizer, LeakSanitizer and UndefinedBehaviorSanitizer
builds have completed the array, bitset, dictionary, set, JSON, datetime,
string, SQLite, timeseries, integrator, jurisdiction, almanac, test-configuration,
qcomplex, qfloat, number and equation suites, including their README examples. The
integrator suite was split into bounded processes. The test-configuration
suite retains its deliberately skipped harness fixtures.

The original unset-binding `exp(Li(x+iy))` Lab case and related unset `Ei` and
bound complex `Li` checks also completed against the fully instrumented
library, with default sanitizer settings and final leak detection enabled.

Matrix solve and extended symbolic-function tests now have separately
selectable children in `test_matrix_solve.c` and `test_matrix_symfunc.c`.
Their assertions and fixtures are preserved. All ten extended symbolic-function
children completed sanitizer checks; the expensive dense six-by-six solve and
inverse checks initially exceeded the time limit.

For allocation-heavy sanitizer batches, allocation backtraces were shortened
to four frames and the freed-memory quarantine reduced to 64 MiB. Bounds
checks, redzones, undefined-behaviour checks and the final leak scan remain
enabled, with no suppressions. The smaller quarantine reduces how long freed
blocks are retained for detecting use-after-free; these runs are not identical
to default-quarantine coverage. The memory and CPU caps were not raised.
Resource-limited runs are incomplete, not clean passes. At the end of the
90-second audit, in addition to the 17 complete module suites above, completed
sanitizer checks covered 217 of 219
matrix tests, 512 of 514 expression tests, and 156 of 158 differential-equation
tests, together with all their README examples. These six checks each
exceeded the 90-second process limit:

- `test_inverse_expr_dense_6x6`;
- `test_solve_symbolic_dense_six`;
- `test_integrate_iterated_exp_unary_derivatives`;
- `test_integrate_more_by_parts`;
- `test_diffequ_inverse_pde_real_residual`;
- `test_diffequ_series_shifted_residuals`.

All 20 ordinary C test suites and their README examples subsequently passed
against the repaired release library, including those six cases. The final
ordinary totals for the larger suites were 219 matrix tests, 514 expression
tests and 158 differential-equation tests. Ordinary success does not replace
sanitizer checks or establish project-wide memory safety. Each initial audit
process retained a 512 MiB memory limit, no swap, a 50% CPU quota and a
90-second runtime limit.

Additional release-build Valgrind checks completed with zero errors and zero
definite, indirect or possible lost bytes for the complete dictionary and set
suites, non-finite-number handling, the empty holiday query, the isolated radial
leak reproducer, wave quadrature, and the singleton/exact-complex regressions.
The broader radial batch and wave integral-data cross-check initially exceeded
the same time limit under Valgrind. Markdown API coverage and both targeted Lab
complex-integral regressions passed after the final release rebuild.

#### Extended runtime verification

The user subsequently approved longer runs, then unlimited runtime while
retaining the 512 MiB memory cap, zero swap and 50% CPU quota. All six cases
above completed AddressSanitizer, UndefinedBehaviorSanitizer and final
LeakSanitizer checks without errors. Their earlier gaps were runtime limits,
not additional diagnosed memory faults.

| Check | Elapsed time | Peak memory |
| --- | --- | --- |
| Six-by-six symbolic inverse | 233 s | 157 MiB |
| Six-by-six symbolic solve | 325 s | 164 MiB |
| Iterated exponential/unary integration | 213 s | 218 MiB |
| Integration by parts | 96 s | 187 MiB |
| Inverse-PDE real residual | 96 s | 210 MiB |
| Shifted-series residuals | 154 s | 168 MiB |

Together with the earlier sequential batches, this completes configured
sanitizer coverage for all 20 modules and their README examples, including all
219 matrix, 514 expression and 158 differential-equation tests. The reduced
quarantine/backtrace settings described above still apply; passing the tested
paths does not prove memory safety for every possible input.

The two supplementary Valgrind cross-checks also completed once the runtime
limit was removed: all three radial cases passed in 198 seconds and the wave
integral-data case passed in 112 seconds. Both reported zero errors, zero heap
bytes in use at exit and no suppressions. Their peak memory was 309 MiB and
283 MiB respectively. These results close the earlier timeout gaps without
raising memory or CPU limits or requiring another code fix.

The two matrix, one expression and twenty differential-equation README examples
then passed again under the sanitizers, after the ordinary cases. The full
test configuration was restored at the end of the audit.

MARS Lab expression-presentation regressions live in
`tests/tools/test_mars_lab.py`. They exercise native Cartesian complex
evaluation, differentiation and integration as well as the browser's Function
syntax colouring. Markdown examples are collected in
`ZZMarsLabReadmeExamples`, which runs after the other Lab tests.

### Recorded Check: 1–2 October 2026

The matrix audit reproduced a null-array cleanup crash after failed formatter
allocation, leaked entry-binding text after additive-constant formatting failed,
and a leaked row token after its destination vector failed to grow. The fixes
guard unallocated arrays and retain ownership until transfer succeeds.

Five allocation-failure regressions use linker wrapping confined to the matrix
test executable. Production allocation functions are unchanged, and the wrappers
remain inactive outside the individual failure checks. An additional native
regression renders the symbolic two-by-two matrix power in Expression, Function
and TeX styles. The end-to-end Lab check with an exact pi binding exposed further
temporary-number and expression leaks in common-factor and reciprocal-power
simplification. These are fixed by releasing the borrowed-argument temporaries
after the constructors have copied or retained them, and by releasing generated
numeric exponents after use.

The 30 matrix parser cases, 16 existing formatter cases, five allocation-failure
cases and new three-card regression completed full leak scans with zero reported
errors and zero heap bytes at exit. The generated matrix Function programme also
passed with unset, pi and rational bindings. The end-to-end exact-pi Lab check
previously leaked 14,472 bytes; after the fixes, all 4,671,196 allocations were
freed and Valgrind reported zero errors.

The matrix README checks also exposed 1,816 leaked bytes in Bessel and Struve
evaluation: detached numeric results from order shifts, Taylor coefficients and
matrix norms were not released by the surrounding temporary-number scope.
These caller-owned values require explicit cleanup, including early exits.
The broader cylindrical batch additionally identified detached Bessel Y expected
values leaked by the test fixtures themselves; those now use the same explicit
cleanup as production callers. Ten cylindrical regression cases completed
family-specific scans with zero errors and zero heap bytes at exit.
All seven matrix README examples then completed clean final Valgrind scans,
with all 74,605 allocations freed.

The memory checks use the release build, full Valgrind leak reporting and the standard
512 MiB, no-swap, 50% CPU and 90-second limits above, without suppressions. An
initial combined parser batch exceeded the time limit; splitting out its
symbolic-power case allowed both batches to complete their final leak scans.
The combined cylindrical batch also exceeded the time limit and was split into
smaller family-specific runs; interrupted runs are not counted as clean scans.
The isolated multi-order Bessel Y numeric case initially exceeded 90 seconds
with zero reported errors before termination. It is now split into six named
cases: zero, first, negative and half orders, plus separate dense and diagonal
matrix checks for the complex order. All original inputs, precision and
assertions are retained. The four real-order cases complete in one bounded
batch; each complex-order layout completes separately in approximately
79 seconds. All six cases completed full Valgrind scans with zero errors and
zero heap bytes at exit, closing the previous coverage gap without increasing
any resource limit or changing production code.
The test configuration is restored after isolated runs. This targeted audit is
not a claim of whole-project memory safety.

Ordinary verification passed all 20 native suites and 131 Function-runner/matrix
Lab tests. After the final cylindrical and fixture changes, the complete matrix
suite passed again: 242 tests and seven README examples. All 59 Python README
checks passed last.
After splitting the Bessel Y checks, the complete matrix suite passed again
with 247 tests and all seven README examples, which ran last.

#### Bessel Y ownership follow-up

A later scope-lifetime audit found defects not exercised by those earlier
numeric-value checks. Both fixed-precision Bessel Y wrappers leaked their
detached numeric result (64 bytes per isolated real-valued call). Integrating
`BesselY(1/3,x)` leaked 760 bytes in detached Lommel parameters. A Bessel Y
matrix evaluated inside a temporary numeric scope retained freed off-diagonal
storage after that scope ended; reading it produced invalid accesses and a
segmentation fault.

The wrappers now release their converted result. Bessel I and Lommel parameter
construction use scope-tracked arithmetic, including rejected-parameter paths.
All numeric matrix storage setters detach their copies from the caller's
temporary scope, while ordinary owning reads retain their existing temporary
scope behaviour. Dense, sparse, diagonal and both triangular layouts share
this ownership rule. Detached results in the Bessel Y number tests and their
README example are also released explicitly.

Eight selectable regressions cover the two wrappers, detached scalar lifetime,
three integration construction/rejection paths, and two matrix storage/lifetime
cases. The matrix cases also exercise sparse replacement/removal, in-place
materialisation and automatic reclamation of temporary reads. The three focused
Valgrind batches completed with zero errors and zero heap bytes at exit, without
suppressions or relaxed resource limits. An existing orthogonal-polynomial
fixture's detached expected value was also corrected after its leak was exposed
during expression verification.

Ordinary verification passed all 24 number tests, 18 selected matrix tests
(including both complex-order Bessel Y layouts) and nine affected expression
tests. Further bounded memory batches passed 16 matrix cases and three
expression evaluation/reverse-mode/orthogonal-polynomial cases with zero errors
and zero heap bytes at exit. All seven matrix README examples and the number
README example group, including the Bessel Y examples for all three scalar
APIs, ran last and completed equally clean final scans.

Full ordinary matrix and expression runs exceeded the 90-second limit in
unrelated symbolic inverse, solve and integration cases. A combined affected
expression memory batch also exceeded the limit in composed integration;
the completed construction/rejection and evaluation batches remain separate
evidence, not full memory coverage for the composed cases. Interrupted runs
are not counted as passes. The original test settings are restored apart from
the eight new regression registrations. These checks do not establish
whole-project memory safety.

#### Full-suite follow-up

The subsequent full number-suite AddressSanitizer/LeakSanitizer run exposed
1,808 leaked bytes in modified Bessel K evaluation and its Wronskian fixture.
Detached working values, intermediate series and replaced results now receive
explicit cleanup, as do the qfloat and qcomplex wrappers and affected fixtures.
A new ownership regression checks results after their input scope has ended,
integer recurrences, real and complex half orders, and rejected arguments.
It also exposed precision loss when a negative real order was unnecessarily
promoted to a complex value; real working values now retain their requested
precision regardless of sign. The isolated regression and existing Bessel I
precision case pass with address, undefined-behaviour and final leak checks.

The expression batches subsequently exposed 480 leaked bytes in Struve order
shifts used by ordinary and reverse-mode derivatives. The same detached-helper
pattern was corrected in both Struve families' integral and hypergeometric
parameters, including rejected-parameter paths. A selectable integration
ownership regression and the complete reverse-mode group pass their final
leak checks. A further 56-byte leak in inverse-Fourier branch classification
was traced to an unreleased literal value; the existing Fourier copy/rendering
regression reproduces it, and the complete Fourier/signal group passes after
the cleanup.

The related detached-arithmetic audit identified the same ownership mistake in
the Lerch reverse-mode finite-difference denominator. That temporary now remains
in the caller's numeric scope, and invalid unpacking does not allocate its step.
A new selectable regression checks all three argument derivatives against the
geometric value and a convergent logarithmic series. It passes the final leak
scan. The Bessel K ownership regression additionally verifies negative real
orders and the principal branch on the negative real axis at 256-bit precision.

Ordinary tests and README examples are recorded separately, with README examples
reserved for the final phase. The six previously identified slow symbolic cases
were allowed to run individually for up to 15 minutes with the user's approval;
the 512 MiB memory cap, zero swap, 50% CPU quota and strictly sequential
execution remained unchanged. Timeouts and
empty selections do not count as completed coverage.

A one-frame allocation-trace experiment was rejected: a deliberate 37-byte
leak was not reported with `malloc_context_size=1`, whereas the retained
four-frame setting reported it and returned a failing exit status. No
one-frame run contributes to the coverage record. An optimised (`-O2`)
instrumented build was used to retry slow cases without changing their inputs,
assertions, precision or sanitizer checks.

The dense six-by-six solve also exceeded 15 minutes in that optimised build,
with a peak of 168.6 MiB. The user approved a 30-minute allowance for this
isolated case only; the memory, swap, CPU and sequential-execution limits
remain unchanged.

All 1,505 ordinary native cases across the 20 module suites completed both fresh
release verification and AddressSanitizer, UndefinedBehaviorSanitizer and final
LeakSanitizer checks. This includes 249 matrix, 543 expression and 190
differential-equation cases. All 88 native README examples/checks subsequently
passed in both builds. Executed names were reconciled against the source
registrations and README selections; no application cases were omitted.
The harness's deliberately skipped fixtures remain intentional controls.

All 944 ordinary Python integration/policy tests and their 59 README checks
also passed, covering all 38 Python test modules, with no skips or missing
test identifiers. README checks ran after ordinary verification. The release
library, scratch helpers and parser benchmark were rebuilt, and the public
distribution and native numeric-boundary checks passed.

The completed native memory checks retained leak detection, immediate failure
on sanitizer diagnostics, four allocation-stack frames and a 64 MiB allocation
quarantine; no sanitizer suppressions were used. The smaller quarantine and
shorter traces accommodate the memory cap and can reduce diagnostic reach or
detail compared with larger settings. The deliberate leak control verifies
that leak reporting is active, not that every possible leak is detectable.
This is complete registered-suite coverage, not a proof of memory safety for
all possible inputs or execution paths. The normal enabled test configuration
was restored after the isolated runs.

| Isolated sanitizer case | Case time | Peak memory |
| --- | --- | --- |
| Six-by-six symbolic inverse | 678.27 s | 154.4 MiB |
| Six-by-six symbolic solve | 1,030.78 s | 171.6 MiB |
| Iterated exponential/unary integration | 206.91 s | 223.2 MiB |
| Integration by parts | 101.36 s | 197.4 MiB |
| Inverse-PDE real residual | 32.29 s | 172.6 MiB |
| Shifted-series residuals | 165.58 s | 171.7 MiB |

The fresh release-build dense solve passed in 139.59 seconds under the same
50% CPU and memory limits, compared with 1,030.78 seconds for the instrumented
build: approximately 7.4 times slower with sanitizers. Both completed. This is
a measured checker-build overhead for that allocation-heavy symbolic test,
not evidence of a hang or a Valgrind timing; these runs used ASan/UBSan/LSan.

One isolated Python inverse-Laplace branch-verification test exceeded its own
30-second subprocess timeout under the CPU cap. It passed with that subprocess
allowance raised to 120 seconds, within the existing 15-minute isolated-case
limit. Only the temporary verification runner adjusts this timeout; the test's
inputs and assertions are unchanged, and its neighbouring tests pass with
their original limits.

## Notes

- Run commands from the repository root.
- Always run tests sequentially, with `-j1` for Make test targets. Never overlap
  test suites or memory-test suites; wait for each to finish before starting
  the next.
- The test output is intended to read cleanly in a normal terminal or in the
  Visual Studio Code integrated terminal.

## Recommended Suite Shape

The harness works best when each suite keeps a clear boundary between
infrastructure and domain semantics.

- The harness should own execution, grouping, counting, skip/fail handling,
  file/line reporting, fixtures, and generic validity plumbing.
- The suite should own what counts as a valid result for its domain.
- Presentation checks should stay explicit; they should not be hidden inside a
  generic validity contract.

In practice, the recommended pattern is:

1. Declare a config mode with `TEST_SUITE_CONFIG(...)`.
2. Register named validity checkers in `TEST_SUITE_SETUP(...)`.
3. Require those checkers in setup with `TEST_REQUIRE_VALIDITY_CHECKER(...)`.
4. Expose a small suite-local assertion vocabulary in the suite header.
5. Allow multiple semantic validity lanes when the suite genuinely needs them.
   A mature suite often needs more than one notion of correctness:
   exact equality, tolerance-aware equality, mp-real equality, complex
   equality, and presentation or string rendering checks.
6. Keep README/example/output cases on the output lane with
   `TEST_RUN_OUTPUT(...)` or `TEST_RUN_OUTPUT_TAGS(...)`.
   When a suite has both ordinary correctness cases and README/demo cases,
   prefer putting them beneath two explicit top-level groups:
   `tests` and `readme_examples`.
7. Remove old suite-local comparison engines once the harness-backed validity
   path is established. Remaining helpers should be thin wrappers for
   expected-value construction, labelling, or other domain-specific setup.

Output examples still participate in config discovery, enable/disable
selection, filtering, and machine-readable reporting, but they are counted
separately from correctness cases in the terminal summary.

The current model suites are:

- `qfloat`: suite-owned closeness contracts, including tolerance-aware variants
  for numerically sensitive regions.
- `number`: separate lanes for semantic value equality, exact string rendering,
  and prefix/presentation checks.
- `matrix`: suite-owned double, mp-real, and complex validity lanes; output
  examples on the output lane; and no remaining parallel legacy comparison
  subsystem.

Those three suites now show the intended end-state of the harness more clearly
than older compatibility-era suites.

## Recommended Authoring Pattern

For a new or modernised suite, the preferred shape is:

1. Put foundational readiness checks in `TEST_SUITE_SETUP(...)`.
2. Register one or more named validity contracts there.
3. Require those contracts before any case runs.
4. Expose a tiny suite-local vocabulary such as:
   `ASSERT_QFLOAT_CLOSE(...)`, `ASSERT_NUMBER_EQ(...)`, or
   `TEST_ASSERT_MATRIX_D_CLOSE(...)`.
5. Use fixtures for per-case resources and harness helpers for temporary files,
   directories, environment overrides, and stream capture.
6. Keep semantic equality checks on the validity path.
7. Keep formatting, presentation, and README checks explicit.

The important rule is that the harness should not need to know your domain
semantics in advance. The suite provides those semantics; the harness provides
the execution, reporting, and failure machinery.

## Benchmarks

The repository also includes focused benchmark targets. For the current symbolic
integrator work:

```sh
make bench_integrator
make bench_matrix_expr
make bench_number_maths
make bench_number_scope
```

These benchmark targets track the numeric and symbolic hot paths we are
actively optimising. See [`benchmarks.md`](./benchmarks.md) for benchmark-
specific notes and sample results.

---

## Enabling and Disabling Tests

Individual tests and whole groups can be skipped without recompilation by
editing `tests/test_config.json`. The harness reads this file at startup
through the `json_t` parser and regenerates missing keys in the enabled state
during a full discovery run, so new tests automatically appear as `true` on
first run.

A missing key always means **enabled**. Set a value to `false` to skip it. The
configuration file is ordinary JSON, so strings, escapes, booleans, arrays and
objects follow JSON syntax rather than a hand-written test-harness subset.

### Flat tests

Most test files list their tests as simple booleans at the top level of their entry.
Entries are keyed by the test source's path under `tests/`:

```json
{
  "tests/array/test_array.c": {
    "test_ints": true,
    "test_strings": false,
    "test_swap_rotate": true
  }
}
```

Setting `"test_strings": false` causes that test to be reported as `SKIP` and excluded from the pass/fail count.

### Grouped tests

Some tests are organised into groups. A group object has an `"enabled"` key for the group itself, plus one key per member:

```json
{
  "tests/expression/test_expression.c": {
    "test_arithmetic": {
      "enabled": true,
      "test_add": true,
      "test_sub": false,
      "test_mul": true
    }
  }
}
```

Setting `"enabled": false` on the group skips every test inside it regardless of the individual values. Setting an individual member to `false` skips only that member while leaving the rest of the group active.

Groups can be nested to any depth; a test is only run if every ancestor in its chain is enabled.

For suites that expose both ordinary correctness cases and README/demo cases,
the recommended top-level shape is:

```json
{
  "tests/string/test_string.c": {
    "tests": {
      "enabled": true
    },
    "readme_examples": {
      "enabled": true
    }
  }
}
```

That gives three quick modes without recompilation:

- run both: leave both groups enabled
- run just correctness tests: set `"readme_examples": { "enabled": false }`
- run just README examples: set `"tests": { "enabled": false }`

### Modes

Test files declare one of two modes before including `test_harness.h`:

| Mode | File consulted |
|---|---|
| `TEST_CONFIG_GLOBAL` | `tests/test_config.json` (shared by all test binaries) |
| `TEST_CONFIG_LOCAL` | `<normalised test source path>.json` (one file per test binary) |
| `TEST_CONFIG_NONE` | no config file is read or written |

For example, `tests/test_test_config/test_test_config.c` uses
`tests/test_test_config/test_test_config.json` in local mode.

Use `TEST_CONFIG_NONE` only for suites that genuinely cannot participate in
config I/O. The ordinary project suites, including `string` and `dictionary`,
are expected to use the shared global config.
