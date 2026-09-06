# Testing MARS

The project provides per-module test targets.

## Machine-Readable Reports

The harness emits JUnit-style XML beside each suite source file by default.
For example, the qfloat suite writes `tests/qfloat/test_qfloat.junit.xml`.

```sh
make test_qfloat
```

- Because report emission lives in the shared harness runtime, every suite gets
  it automatically.
- Output examples are included in machine-readable reports, but remain separate
  from correctness totals in the terminal summary.

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
possible leaks as errors. AddressSanitizer, LeakSanitizer and
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
  display inspection. This remains an unresolved undefined-behaviour finding;
  the aborted case did not complete its final leak scan.

Full-suite memory coverage was not completed. These targeted results are not a
project-wide guarantee of memory safety. Update this dated record when the
overflow is fixed and its regression checks have completed.

MARS Lab expression-presentation regressions live in
`tests/tools/test_mars_lab.py`. They exercise native Cartesian complex
evaluation, differentiation and integration as well as the browser's Function
syntax colouring. Markdown examples are collected in
`ZZMarsLabReadmeExamples`, which runs after the other Lab tests.

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
