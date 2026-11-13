# Testing MARS

The project provides per-module test targets.

## Test Reports

The native harness prints individual case results, group totals, a final
summary and the slowest cases to the terminal. It does not currently emit
JUnit XML. Optional SQLite run reports are described below. Capture both standard output and standard error when preserving
verification evidence, and keep a separate log for every isolated batch.
`make test` retains a failing suite's standard output and standard error beside
its executable, with a `.log` suffix, and prints that pathname beside `FAIL`.
Successful suites' temporary logs are removed.
The suite summary colours `PASS` bold green and `FAIL` bold red. `make test` supplies
the number of suites, so its progress bar shows completed suites out of that total.
Suite numbers are right-aligned, for example `[ 1/48]` and `[10/48]`, and executable
labels are padded to the longest path in the run so all final `PASS`/`FAIL` results align.
Elapsed durations below a minute use seconds; longer durations use minutes and seconds,
for example `11m 08.6s` instead of `668.6s`. The same format applies to immediate failure
reports and running-case durations.
The final standalone README batch uses one replacing progress row, including its
enabled-example count and elapsed time. Compilation and execution refresh that row;
successful or disabled examples do not print individual lines. Its final summary
retains the bar, count, duration and bold coloured outcome. Failures are still printed
with their example identifier, Markdown source line and diagnostic; compiler and
programme output and `build/readme-examples/results.json` retain the detailed evidence.
While a suite runs, a second progress bar shows completed tests out of the selected
module total, with a percentage, a UTF-8 Braille spinner, an elapsed timer, bold green ✓
and bold red ✗ result counts, and the running check. The native harness counts enabled
leaf cases from `tests/test_config.json`, including helper source files and README
examples, while excluding disabled tests, disabled groups and group wrappers. Saved
profile selections override JSON selections in the same way as actual execution.
If a selected test skips itself at run time, it is also removed from the total.
Failures
are printed immediately, with their test name and elapsed suite time. A terminal
updates the same three rows four times per second; redirected output receives a status
line every ten seconds. The runner
reads appended output incrementally, without rescanning a growing log or adding
threads to the test process. Counts include successful README examples and exclude group
summaries. Final harness counters reconcile the live counts even when a diagnostic did
not end with a newline. If configuration is absent, a module is new, or an unconfigured
case is discovered, the module bar shows discovery activity instead of inventing a
total. The configuration is regenerated normally for the next run. Individual-suite
runs show the module bar and running case on two rows; local configuration fixtures
also use discovery so counting cannot interfere with their configuration setup.
Native test executables accept `--test-plan` to print the current configured count
without running fixtures, writing configuration or recording a test run.
Suites still run sequentially, with no new
execution deadline.

The standalone globe-installation fixture imports the complete packaged coastline once,
in the plaintext storage mode used by normal installations. Release preparation separately
checks a fresh installation of all three stores. A small encrypted catalogue checks
key routing, integrity, retained layers and failed replacement. Calendar cases share one
immutable plaintext catalogue and copy it into separate temporary homes; their settings
stores remain encrypted. Direct fixture imports use transactions, as the production
installer does. These fixtures retain catalogue, migration and rollback assertions without
repeating the complete coastline import for each storage mode. Selection checks use
the real schema, core towns and complete locale data. The shared complete calendar
seed covers bulk worldwide town and holiday-rule imports; the plaintext jurisdiction
installation checks searches for every shipped country.
Repeated calendar translation comparisons publish only their checked year, including
a leap-year and invalid-range rollback check. The full default installation horizon
remains covered separately. Texture regressions use small, valid single-colour PNGs
at the real image dimensions, retaining the 128-region and complete global-grid
address checks. Only the README metadata example loads the real overview photograph.
Photo-only migration checks use a single valid mapped-water polygon and regional
outline; full geography remains covered by the globe and country installation checks.
Their immutable boundary sources are linked rather than copied, and no additional
persistent cache or database is created.
Fixture setup batches related writes in transactions, while interrupted-publication
tests retain their real commit, locking and recovery behaviour.
Country migration checks every historical revision in plaintext and checks encrypted
migration, failed replacement and preserved layers separately, without repeating the
same historical geometry imports in both storage modes.
Fresh imports request a 16 MiB main page-cache allowance, matching the update path; calendar
query fixtures use 8 MiB. This avoids testing large plaintext pages with only a handful
of cache slots. Temporary-table caches have separate, smaller allowances; SQLite allocates
pages on demand, and these settings are not hard process-memory limits.

Executables that also serve native subprocess fixtures may define
`test_program_setup()` to dispatch those commands before test-option parsing.
Ordinary suite registration and test-database reporting happen afterwards; a
fixture command that exits from this hook does not start a test run.
The configuration and Lab suites log SQLCipher errors for their synthetic encrypted fixtures,
without repeating memory-lock quota warnings for every allocation. Encryption and
memory security remain enabled; this test-only log level does not change application diagnostics.
It also prevents repeated warnings from exhausting the calculation workers' bounded output capture.

Standalone README programmes which open installed almanac, globe or jurisdiction
databases inherit a lazily loaded settings snapshot, using the same `mars.json` and
encrypted `mars.db` as the applications. Explicit environment overrides, including
empty database keys, take precedence. The runner reads settings through an isolated
helper and never puts credentials in command arguments or result files. It does not
install missing data. Other examples, including the disposable globe-reader fixture,
need no installed configuration or desktop key store.

```sh
make test_qfloat
```

- Output examples have separate totals from ordinary correctness cases.
- A successful process exit and final summary are required for a completed
  batch. Reconcile the cases across partitions; a skipped group does not
  establish coverage for its children.

## Optional test database

The unencrypted **`tests/tests.db`** contains case definitions, the catalogue,
selection profiles, run reports and lifetime timing statistics. It belongs in Git alongside the test
sources, so another checkout receives the same database-backed cases. The
shared schema is in `tests/test_database.sql`; module-owned SQL files contain
the reviewable test inputs and expectations.

Running tests appends results and updates statistics in that same database;
case definitions and selection profiles are preserved. The file survives
`make clean`. `make release` synchronises an existing database only when its SQL
definitions or JSON catalogue have changed since the last complete synchronisation.
It compares SHA-256 content fingerprints, so a `git pull` is detected independently
of checkout paths and file modification times. An unchanged database prints
`Test database: up to date; synchronisation skipped.` A missing one prints
`Test database: not present; synchronisation skipped.` Compiling, installing,
`make release` and running MARS Lab never create a test database.
A missing database is initialised only by test execution
or explicit test-database commands. The test database is not installed with the
application.

The first writable open consolidates a recognised older `tests/test_results.db`
transactionally, preserving reports, timestamps and missing definitions and
profiles. Existing definitions win conflicts. Reports are fingerprinted so that
retrying an interrupted cleanup cannot duplicate them. The old file is removed
only after the merge commits; an invalid source or failed merge leaves it for
retry. Compatible older single-file schemas are upgraded in place as well.

The default test selection remains `tests/test_config.json`. Run a suite with
`TEST_ARGS='--test-database off'` to suppress shared test storage. Database-backed
case definitions then use a disposable in-memory schema. To use another existing
parent directory, pass `TEST_ARGS='--test-database /path/to/tests.db'`.
An explicit override uses that single file for definitions, reports and statistics,
without looking for another database alongside it.

The sequential harness keeps counters, diagnostics, nested-case state, owned
records, cleanup stacks and selection snapshots in one private process-lifetime
object. Its resources are released through a common shutdown path, including
command-line exits. Suites continue to use the ordinary harness functions and
hooks; they do not construct or access this internal state.

`make test-db-sync` explicitly merges configured test and group identities into
the catalogue, together with all module case rows. It prints
`Test database: sync complete.` Repeating sync is safe: keys combine the
repository-relative source path and complete dotted group/case path. A missing
entry is retained as **unconfirmed**, rather than deleted or disabled. A renamed
entry gets a new identity while its old identity remains available for review.
Unconfirmed status never controls execution. Sync never deletes test definitions,
case rows, saved selections or run reports, and an invalid import rolls back in
full. History retention is a separate operation after a completed run.

`make test-db-sync-if-needed` performs the same conditional check used by release.
Both explicit and conditional synchronisation record their source fingerprint only
after a complete successful catalogue import. Failed imports leave the previous
fingerprint, cases, selections and history intact; the next release retries them.
If source files change during the import, it rolls back and requests a retry.
Synchronisation retains existing case edits and disabled rows; updating an existing
case's expected result still requires an explicit data migration rather than an
automatic overwrite from its SQL seed.

The catalogue is populated from the JSON registration catalogue, module case
tables and registrations observed during execution. Sync does not parse arbitrary
C source or discover an unexecuted C registration absent from JSON. Neither an
incomplete JSON selection nor a partial test run is evidence that a test should
be removed. `test-db list` includes confirmed/unconfirmed status for review.
Normal execution records observed registrations and results in `tests/tests.db`;
explicit sync also updates its catalogue from JSON configuration and case tables.

Use `make test-db ARGS='help'` for the administration commands; help creates
nothing. `profile-save NAME` explicitly snapshots the current JSON selection,
replacing that named snapshot atomically. `profile-create NAME all` or
`profile-create NAME none` creates a profile without replacing an existing one.
`profile-set NAME SOURCE PATH on|off` changes one rule; a disabled parent excludes
its descendants. Profiles in `tests/tests.db` are shared test configuration.
Pass `TEST_ARGS='--test-profile NAME'` when running a test target
to select a saved profile. Missing profiles fail before executing any cases;
profile runs do not rewrite JSON. `runs` shows recent reports and identifies memory
tool runs. `stats`, optionally followed by a source prefix, displays per-test
statistics; the same data is directly queryable through the `test_case_metrics`
view in SQLite or DBeaver.

Statistics output defaults to an aligned Unicode grid, independently of installed
Markdown readers. Use `make -s test-db ARGS='stats' | less -R` to page the report
while retaining its colours and bold headings. No reader is installed or launched
by the command. The `-s` suppresses Make's command echo; dependency
diagnostics go to stderr, leaving the report on stdout.

Use `--format=grid`, `--format=markdown` or `--format=tsv` for an explicit format;
`--format=auto` also selects the native grid. Compact reports group tests
by source and align durations to the right. Grids use double outer borders and
a double rule beneath the header, with single rules between data rows. Grid cells wrap long names without
losing text; narrow terminals use per-test field/value grids instead. Set
`COLUMNS` when piping to specify the available width. The `--details` option
includes every timestamp, memory-test date, comparison and outlier field.
Table headings are bold, source headings are bold bright gold and test names are bright cyan.
Mean values are bold light cyan and median values are bold yellow. Change percentages below −5% have a bold bright green ▲
(faster), above +5% have a bold bright red ▼ (slower), and within ±5% have a bold amber ▬ (little change).
Amber uses the explicit orange RGB colour `#FF8C00` on terminals with true-colour support.
The marker follows the value and remains visible when colours are disabled.
Missing comparisons remain uncoloured. This display band does not change the timing-outlier policy.
Displayed outcomes use `PASS`, `FAIL` and `SKIPPED`.
The native grid retains styling when piped as well as when displayed directly
in a terminal with a supported `TERM`. It shows `PASS` in
bold bright green, `FAIL` in bold bright red and `SKIPPED` in bold bright yellow. `NO_COLOR` or `TERM=dumb` suppresses terminal
styling; `CLICOLOR_FORCE=1` can request styling when a terminal is not otherwise
detected. Colours are presentation only: stored outcomes and timings do not change.

Markdown export always uses standard bold Markdown for headings and outcomes,
without ANSI escape sequences, even when terminal colour is forced. Markdown
readers control their own layout and theme; per-word result colours belong to
the native grid. Glow's `pager: true`, `width: 140` and `preserveNewLines: true`
settings can be used for this plain export without shell aliases.
TSV always includes every original
field and column name, with the stored lowercase outcomes and no terminal escapes.
Control characters within stored text are displayed as literal hexadecimal escapes;
their original values remain unchanged in the database.

For example, the Markdown output from `stats --format=markdown` contains the
following table for a single passing `group.original` test taking 2.5 ms.
The format regression runs this example against an isolated database; actual
names, durations and sample counts reflect your recorded runs.

```text
| **Test** | **Result** | **Last ms** | **Change %** | **Fastest ms** | **Slowest ms** | **Mean ms** | **Median ms** | **Samples** |
| :--- | :--- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| group.original | **PASS** | 2.5 | - | 2.5 | 2.5 | 2.5 | 2.5 | 1 |
```

Profiles are loaded into dictionary indexes and the connection is closed before
tests run. Reports are written afterwards in a transaction. A reporting failure
prints a warning and leaves the console result and exit status intact. Storage
is capped at 64 MiB of SQLite pages, 200 runs and 50,000 detailed results, with
each detail limited to 4,096 characters. Lifetime statistics survive detailed
report pruning. A counted duration distribution preserves the exact lifetime
median, alongside lifetime count, fastest, slowest and mean durations. It also
keeps the last execution date, outcome and duration, the last two ordinary run
durations, their absolute and percentage change, whether the latest ordinary run
took longer, and the last memory-test date. Unexecuted tests have no fabricated
timing or outcome. Dates use UTC; durations use milliseconds.

Significant slowdowns are retained as timing outliers, with the measured duration,
baseline median and threshold recorded in `test_result`. They print a warning to
review recent changes. The assertion outcome remains independent: a passing test
can also have a suspicious timing. Flagged durations are excluded from baseline
means, medians and future regression assessments, so a persistent regression
cannot quietly become normal. Recorded fastest and slowest durations still show
the actual ordinary-run extremes, including flagged timings. The metrics view
also shows outlier counts and the last flagged timing.

The default policy requires seven accepted ordinary samples. A new duration must
exceed the largest of 1.5 times the prior median, that median plus six median
absolute deviations, and that median plus 0.05 ms. These settings live in
`test_timing_policy` and survive synchronisation edits. They help reject normal
jitter; an outlier is a reason to investigate, not proof that a fix is faulty.
Insufficient history never produces a regression warning. Skips and memory-tool
runs are never classified as ordinary timing outliers.

Ordinary timing statistics include executed passes and failures and exclude skips,
disabled cases and memory-tool runs. Valgrind is detected through its client
request when its headers are available. Make's memory-test targets also pass
`--test-memory`, which is available for other instrumentation or builds without
those headers. That flag records classification; it does not launch a memory
checker. Memory-test dates record execution, not a claim that a later Valgrind
leak report was clean. Older reports did not record instrumentation, and retain
their original ordinary classification when imported.

The 64 MiB limit applies to the complete database, including lifetime duration
samples. Reaching it causes a reporting warning and transaction rollback; it
never silently discards case definitions or lifetime statistics. SQLite journal
files can temporarily require additional space.

Storage implementation details stay private to `tests/test_config`. Authorised
storage fixtures use `tests/internal/test_database_internal.h`, which exposes
only required declarations and no implementation representation. Ordinary test
support interfaces remain in `tests/include`.

### Module case tables

Each migrated module supplies `tests/<module>/test_<module>.sql`. When the directory
already begins with `test_`, reuse that prefix: the harness regressions use
`tests/test_test_harness/test_test_harness.sql`. The importer
discovers these files automatically and applies them in a deterministic order,
inside the same transaction as catalogue synchronisation. The shared
`tests/test_database.sql` contains only the management schema. For example:

| SQL source | Case data |
| --- | --- |
| `tests/TeX/test_TeX.sql` | Calculus, relations, matrices, bindings, indices, quantum notation and rejection cases |
| `tests/expression/test_expression.sql` | Parser equivalence, function names, rational powers, roots, projection notation and fractions |
| `tests/string/test_string.sql` | Quoted shell words, escapes, comments and malformed input |
| `tests/equation/test_equation.sql` | Solution counts, principal-root domains and relation separators |
| `tests/diffequation/test_diffequation.sql` | Partial initial and boundary conditions |
| `tests/matrix/test_matrix.sql` | Determinants |
| `tests/sqlite/test_sqlite.sql` | Queries against isolated fixtures |

The original C groups and their checking logic remain in place. Migrated rows
retain their original assertions, including round trips, ownership, exactness
and rejection checks. Tests requiring stateful fixtures or executable setup
remain C tests. Catalogue entries describe tests; they are not substitutes for
executable cases. The initial general-purpose checking operations also remain:

| Table | Operation | Expected data |
| --- | --- | --- |
| `expression_cases` | `evaluate` | Exact numerical value |
| `equation_cases` | `solve_count` | Number of derived solutions |
| `matrix_cases` | `determinant` | Exact determinant |
| `diffequation_cases` | `partial_conditions` | Remaining constants: 0, 1 or 2 |
| `sql_cases` | `scalar_query` | Single text-convertible result |

Each table stores `name`, `operation`, `input`, `expected`, `precision` and
`enabled`. Native C callbacks supply the assertions and reject unknown operations.
Each enabled row is independently selectable and reported. Adding a row for an
existing operation needs no new C test function. A new kind of assertion still
requires a C callback. `test_case_runner` maps each group to its module table and
operation using indexed lookups; there is no compiled list of module tables.
Case names are stable within their original groups. Seed imports use
`INSERT OR IGNORE`, preserving existing edits and disabled cases. Removing a row
from an SQL file never removes it from the database during synchronisation.

Prefer new data-driven cases in the appropriate module SQL file and synchronise
them into `tests/tests.db`. README examples are deliberately different: their
explicit runnable C functions or programmes remain synchronised with the
Markdown source and expected output, and run after ordinary tests.

For example, this complete programme demonstrates inserting and inspecting a row
in an isolated in-memory fixture. To add a real case, apply its `INSERT` to the
test database, or add the row to the tracked SQL source. The standalone source is
[`tests/test_test_harness/readme_case_table.c`](../tests/test_test_harness/readme_case_table.c).

```c
/**
 * @file readme_case_table.c
 * @brief Runnable README example: add a mathematical case row to a disposable in-memory table.
 *
 * Kept byte-for-byte in docs/testing.md. The README checker compiles that documented programme and compares
 * its output. This example uses a minimal case-table fixture and never opens the shared test catalogue.
 */
#include <stdio.h>
#include <stdlib.h>

#include "sqlite-t.h"
#include "string-t.h"

/* Add and inspect one isolated case without changing shared test definitions. */
int main(void)
{
    int status = EXIT_FAILURE;
    string_t *path = string_new_with(":memory:");
    sqlite_t *database = path ? sqlite_open_plain(path) : NULL;
    string_free(path);
    sqlite_stmt_t *query = NULL;
    if (!database || !sqlite_exec_cstr(database,
        "CREATE TABLE expression_cases (\n"
        "    name TEXT PRIMARY KEY, operation TEXT NOT NULL, input TEXT NOT NULL, expected TEXT NOT NULL,\n"
        "    precision INTEGER NOT NULL DEFAULT 32, enabled INTEGER NOT NULL DEFAULT 1\n"
        ") WITHOUT ROWID;\n"
        "INSERT INTO expression_cases(name, operation, input, expected)\n"
        "VALUES ('another_sum', 'evaluate', '1/2+1/3', '5/6');"))
        goto done;
    query = sqlite_stmt_prepare(database,
        "SELECT name, expected FROM expression_cases WHERE name = 'another_sum'");
    if (query && sqlite_stmt_step(query) == SQLITE_STEP_ROW &&
        printf("%s|%s\n", sqlite_stmt_column_text(query, 0), sqlite_stmt_column_text(query, 1)) >= 0 &&
        sqlite_stmt_step(query) == SQLITE_STEP_DONE)
        status = EXIT_SUCCESS;
done:
    sqlite_stmt_finalize(query);
    sqlite_close(database);
    if (status != EXIT_SUCCESS)
        fputs("Could not add or read the example case.\n", stderr);
    return status;
}
```

Output:

```text
another_sum|5/6
```

The source-synchronisation and fixture README test runs last in
`make -j1 test_test_harness`. `make -j1 test-readme-examples` compiles the exact
documented programme and checks the output above. Keep shared case changes in both
the readable SQL source and `tests/tests.db`. After adding seed rows to the SQL
source, run `make test-db-sync` and include both files in the same commit.
Ordinary test runs read the database without applying seed changes.
Initialisation and explicit sync use `INSERT OR IGNORE`: they add new seed names
without replacing edited expectations or disabling cases. Updating an existing
expectation therefore requires an explicit reviewed database change as well as
the SQL edit; a fresh database imports the updated SQL source. Bulk migration remains
incremental so compiled coverage is retained until equivalent row runners pass.
Moving text fixtures can reduce embedded data; it does not remove compiled
assertion code or the linked mathematical library, which make up most binary size.

### SQLite tests and fixtures

The test-storage implementation uses the raw SQLite C API, independently of the
MARS `sqlite_t` wrapper. The SQLite suite and test-storage suite disable shared
database selection, retaining JSON selection and isolated case data. Their results
are saved only after execution; a reporting failure never changes their exit status.
Use `--test-database off` to suppress that final report as well. Their
normal table/query tests can explicitly initialise a disposable database from the
same schema using `test_database_fixture()`. They never open the shared history
as the connection under test, and existing fixture destinations are rejected.
Encryption, wrong-key, corruption and other engine-specific tests retain their
dedicated fixtures. No test-storage connection remains open while native module
case callbacks execute.

## Run Tests

The maintenance applications have native C suites in
`tools/mars_checks/tests/` and `tools/mars_config/tests/`. Their root targets are
`test_mars_checks` and `test_mars_config`; `make -j1 test` in either application
directory delegates to the corresponding root target. Test binaries remain in
the application's own `build/<mode>/tests/` directory. Run these suites
sequentially, just like library and Lab suites.

Run `make -j1 test_lock` and then `make -j1 test_thread` for the native
synchronisation modules. These cover backend dispatch, acquisition ownership,
cloned lifetimes, timeouts, scope cleanup, completion, joining and detachment.
Each suite runs its README examples last. The configuration suite also exercises
the installer progress reporter's thread-backed heartbeat and shutdown.

The separate `make -j1 lab-browser-test` target uses real Firefox to test the
WebAssembly codec worker, bounded job queue, cancellation, deadlines, malformed
messages, startup failures and recovery. This is message-based browser work,
not the native pthread backend. Run it sequentially after native tests; see
[background codec jobs](mars-lab.md#background-codec-jobs) for its resource limits.
Finally, `make -j1 test-readme-examples` compiles and runs the complete Markdown
C programmes, including the thread and lock examples and their expected output.

Installer tests use disposable private directories beneath `/tmp`, not the
user's configured databases. The checker suite exercises passing and failing
fixtures; failures deliberately produced inside a test are not suite failures.
Calendar fixtures select `Shrewsbury, GB-ENG` explicitly: the worldwide catalogue
also contains American towns named Shrewsbury. Resolution tests retain all these
entries and check both ambiguous unqualified names and jurisdiction-qualified
choices. Calendar rendering fixtures use the native streaming SQL importer so
nested town-catalogue includes follow the same rules as installation.
The README checker reuses the native C loopback protocol fixtures for HTTP,
HTTPS, WebSocket and gRPC examples. OpenSSL development files are required for
their TLS support; Python is not used. Fixtures belong under `tests/http/fixtures/`
and their executable is built under `tests/build/<mode>/http/fixtures/`.

The checker suite also tests coverage-report validation and release-evidence
publication with disposable synthetic inputs. `make check-source-policy` runs
the native inline-size, expression perfect-hash layout and no-Python audits.
These audits also run as part of `make test` and `make check-compliance`.
The `mars_checks source-policy` selectors `inline`, `functiontables` and `python`
can run these independently. Generated build trees and third-party sources are
excluded; historical data-provenance prose is not treated as executable code.

Mathematical worker integration regressions live in the native
`tools/mars_lab/tests/test_lab_math_*.c` suites. They exercise the prebuilt C
workers, decode their output through the native evaluator interface and compare
against numerical references and exact renderings. They are registered in
`test_lab_native`, with mathematical README examples after all ordinary cases.
Use `tests/test_config.json` to isolate a failing case; the corresponding source
file is the configuration key. The shared helper owns per-case strings and
worker results and releases them between cases.

The optional satellite preparation suite is
`tools/mars_lab/tests/satellite/test_satellite_mask.c`. Its explicit
`tools/mars_lab/build/release/tests/test_satellite_mask` target requires the GDAL
development files, selected through `LAB_GDAL_CFLAGS` and `LAB_GDAL_LIBS` when needed.
It checks coastal alpha, unchanged land colours, inland lakes, separate islands,
missing observations, smaller encoded output and protection of existing inputs.
Ordinary library and Lab tests do not acquire this preparation dependency.

Make targets default `TMPDIR` to the active build tree's `tmp` directory and create
it before compiler probes and compilation. Explicit `TMPDIR` values still take
precedence. Per-case scratch directories inherit that setting; standalone tools
use the platform temporary-directory default when it is absent.

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
Globe reader identity examples receive a small temporary SQLite database in their
disposable working directory. Child-only settings select that file; installed
databases and the parent process's settings are left untouched.

To run an extracted example yourself, execute its named binary in that
directory. To compile a copied block independently, save it as a C source file
and use the same compiler/linker options recorded by Make: the public headers
are in `include/`, the archive is `build/release/libmars.a`, and the external
libraries are listed in the [build guide](building.md). GNU C extensions are
required; do not disable assertions in examples that use `assert`.

Programs run in separate temporary working directories, so relative database
and file names cannot overwrite project files. File examples receive fresh
paths as command-line arguments. HTTP examples use the local fixture, while
`make -j1 test_http_live` tests the client against the native MARS web server
on loopback, with no external-service calls or opt-in flag. Almanac and
jurisdiction examples require the configured databases described in their
module guides; missing data is a failure, not a skipped success.

The [webserver example](webserver.md) starts its own single-request loopback
server on an ephemeral port and calls it with the MARS client. The dedicated
`make -j1 test_webserver` suite runs its ordinary protocol checks before this
README example, without contacting external services.

The `tools/mars_checks/src/readme/checks_readme.c` entry in `tests/test_config.json` controls
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
leak detection.

Check whether an editor-launched shell supplies a foreign `LOCPATH` before
interpreting locale-related leaks. On this system, a standalone libc
`newlocale`/`freelocale` check leaked 46 bytes with VS Code's bundled locale
path, but reported no leaks when `LOCPATH` was unset. Use the system locale
search path for these checks; do not suppress the allocation report or change
the application's locale semantics to conceal it.

For MARS Lab evaluation checks, trace exec'd calculation children with
`--trace-children=yes`: checking the test harness alone does not check the
integrated workers. Use a per-process `--log-file` containing `%p` so Valgrind
diagnostics do not enter the worker's result protocol. System utility children
may be excluded with `--trace-children-skip=/bin/*,/usr/*`; do not exclude the
Lab executable. Inspect both the harness result and each worker's final error
summary. Forked test fixtures must release their inherited resource handles
before `_exit`, while leaving directory removal to their parent.

AddressSanitizer, LeakSanitizer and
UndefinedBehaviorSanitizer provide complementary checks in a separate build;
stop on the first diagnostic. If sandbox restrictions prevent LeakSanitizer's
final scan, report that check as incomplete rather than disabling leak detection
and calling the result clean.

### Recorded Check: 19 October 2026

Memory checks found and corrected numerical temporary-scope ownership errors,
unreleased integration-matcher scales, and detached Lab plot-range and precision
temporaries. Focused number, expression, matrix, qfloat, qcomplex, plot, regex and
Lab regressions completed with zero Valgrind errors and zero heap bytes at exit.
The integration regression also completed cleanly with a specifically approved
five-minute allowance; it took about 94 seconds at the half-core CPU limit.
The smaller number README examples were checked last and completed cleanly.

The Mersenne-prime README search was shortened, with approval, from exponents
2–4423 to 2–2203 (the first 16 Mersenne primes), and its documentation was
synchronised. The isolated Valgrind rerun passed in 99.586 seconds overall
(93.79 seconds in the example), within the approved five-minute allowance.
It reported zero errors, no suppressions and zero heap bytes at exit, with
173.2 MiB peak memory and no swap under the half-core CPU limit.

These results are not a complete-suite memory clearance. The original broad memory run was
interrupted during configuration fixtures, before the checks-tool suite, and a
subsequent ordinary regression run stalled in the Lab suite's pipe wait. A further
ordinary Lab run made progress without failures but reached its four-minute
diagnostic timeout at the eclipse previews; the isolated 18 process tests passed. Those
remaining checks require separate isolation and completion. Temporary test
selections were restored, retaining the new plot-range ownership regression.

A subsequent bounded audit reproduced a configuration-test child leak: the
forked child retained its inherited temporary-directory handle. It now frees
that handle before `_exit`, while the parent remains responsible for removing
the directory. Checker subprocess tests now accept both synchronous `ENOENT`
and the documented child exit status 127 for a failed `posix_spawn` execution.
Their assertions run after resource cleanup. README fixture startup rejects
known missing or non-executable paths before allocating a listener, preserving
retry and recovery behaviour when execution errors are reported asynchronously.
The configuration Make fixture also resolves its own executable before passing
it to Make: a raw `/proc/<pid>/exe` path otherwise launches Valgrind itself.
Its isolated memory rerun passed in 23.309 seconds with no reported errors.

The checker controls and parser/build/coverage batches completed under Valgrind
with no errors, as did the isolated README fixture retry/recovery regression.
The corrected controls and fixture regression also passed without Valgrind.
The initially combined evidence/header/documentation and configuration/locale
batches hit the 90-second cap. After resuming with the approved five-minute
limit, separate batches completed 23 checker evidence/policy/header cases,
four documentation cases and 17 configuration/storage/weather/locale/Make cases,
all with zero reported Valgrind errors. Database conflict/path checks and the
trace/stream/precedence checks also completed with zero errors in the parent
and fixture children. A combined database batch had reached its deadline;
allocations reported during forced termination are not counted as completed
leak checks. Completed selections are checkpointed locally for the remaining
sequential run. The original full packaged database-import
cases took approximately 12 and 88 minutes under Valgrind, so they need an
explicitly agreed longer allowance or alternative instrumented coverage.
The baseline also contains failed HTTP and Lab worker/mathematical assertions
whose successful reruns have not yet been established. No complete-suite
memory clearance or commit is recorded by this follow-up.

A separate native jurisdiction-install check reproduced SQLCipher `mlock`
warnings during calendar generation with an 8 MiB locked-memory limit. Releasing
the unused import cache before that phase allowed the same Limmen/Dutch install
to complete in 184.233 seconds with no locking warnings, without disabling
SQLCipher memory protection or raising the limit. This native check is not a
replacement for the outstanding full-import Valgrind coverage.
The final-build streaming-import regression also passed under Valgrind in
75.429 seconds, with zero errors and no definite, indirect or possible leaks
in either the runner or its fixture child.

The earlier reduced worldwide town supplement contained 9,061 locations in total. Its
native import/count/duplicate/default checks passed, as did eight JSON cases
(including a 4,096-name multilingual round trip) and the catalogue visitor
ordering/early-stop case. JSON and catalogue visitor Valgrind reruns reported
zero errors and no definite, indirect or possible leaks. The visitor run retained
72 reachable bytes. A fresh disposable full installation completed in 185.534
seconds without SQLCipher locking warnings. The isolated town-import and source
allow-list memory checks also passed in 53.023 seconds, with zero errors and no
lost allocations in either the runner or its fixture child. Native page-catalogue
preparation took 1.142 CPU seconds (2.324 seconds elapsed at the half-core limit),
producing 1,828,873 bytes of JSON with a 58.6 MiB process peak. These are initial
server-side loading measurements, not browser click-latency measurements or full-suite clearance.

The subsequent 50,000-record catalogue passed the isolated native import,
duplicate-name, source allow-list, preserved-default and requested-town coverage
checks in 2.70 seconds. Coverage assertions include Wales, Ireland, Wexford,
the three Nykøbing towns, Castricum, Alkmaar and Madalena on Pico. Regenerating
the 46,472-row supplement produced byte-identical SQL without memory-lock warnings.
The HTTP catalogue regression passed in 22.80 seconds with a 50,000-town fixture,
decoding every page with the real wire codec and checking the 512-record limit,
advancing cursors and final empty page. Wrong-key decryption diagnostics are
expected in its failure-path check. These are focused native checks, not new
Valgrind results or a measurement of browser interaction latency at 50,000 towns.

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
`tools/mars_lab/tests/`. The C route suite and browser checks exercise the
native adapters and result cards without a Python server. README examples run
after the ordinary native cases.

Number-presentation coverage includes `test_lab_syntax_number_display`,
`test_lab_syntax_mode_numbers` and `test_lab_extra_number_card_markup`.
These check decimal thresholds, signed and large exponents, complex components,
lossless source spans, HTML escaping, and numerical output from the Expression,
Matrix, Integrator and Function workers at 17 and 78 digits. Existing equation
regressions cover the corresponding root displays. Presentation tests also
ensure that abbreviating a long mantissa retains its exponent. The browser
suite verifies that Value cards install the native power-of-ten markup and
Function execution displays the native text without client-side number parsing.

Help coverage includes `test_lab_page_help` for the shipped topic catalogue and
`test_lab_extra_help_examples` for live symbolic, binding, sum, transform,
matrix and equation examples. Browser checks verify mode filtering, full-width
Help and keyboard return in all seven modes, plus wrapping without horizontal
overflow at desktop, tablet and phone widths.

Lab test fixtures honour `TMPDIR` and otherwise use `/tmp`. The `lab-browser-test`
target sets `TMPDIR` to its absolute build directory so Snap-packaged Firefox can
access the disposable profile: Snap's private `/tmp` cannot see the host's `/tmp`.
Each fixture creates a private directory and removes it after the browser exits;
the installed Lab and browser profile are untouched.

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

The migrated native mathematical worker tests use a bounded 120-second deadline
per worker call, including generated-function calls, to retain that allowance
under the 50% CPU cap. Their mathematical inputs, precision and assertions are
unchanged. Worker failures report the deadline, errno and source expression;
this test-only allowance does not change the Lab's request deadlines.

### Recorded Check: 11 October 2026

The native MARS Lab audit, based on commit `e463829`, found and fixed ownership
leaks in matrix result formatting, integrator bounds/results and numerical root
families. Output slots now use non-allocating initial values when the receiving
API replaces them; matrix TeX construction no longer creates a redundant
temporary clone. Forked test fixtures now release their inherited temporary-directory
handle before exiting. The missing-executable assertion also accepts the POSIX
child-exit-127 outcome as well as a synchronous spawn error.

Sequential Valgrind batches completed 113 ordinary Lab cases with no reported
memory errors. These cover process/state/runtime handling, presentation,
Protobuf boundaries, evaluation, HTTP contracts/rejections, calendar/timezone
presentation and Function programmes. Calculation workers were traced as well
as the harness. Expanded regressions cover scalar and eigenvalue matrix
operations, non-square rejection, finite and infinite integration bounds,
numerical integration and cube-root-family output. The 11 documented Function
programmes subsequently passed as the final README batch, also with no reported
memory errors. No suppressions were added.

The checks retained the 512 MiB, zero-swap, 50% CPU and 90-second limits above.
Memory coverage remains incomplete:

- The encrypted catalogue case reached the service deadline during SQLCipher
  key derivation.
- The TeX-route matrix-power case exceeded its production worker deadline
  under Valgrind. A direct worker check also reached the 90-second service
  limit during symbolic simplification. Termination-time allocation reports
  are not a completed final leak scan; this case is not recorded as clean.
- The 386-digit integrator deadline case and encrypted datetime-cache case
  were checked functionally, not under Valgrind in this audit.
- The separate 263 mathematical regressions, their 33 output-example groups,
  browser/WebAssembly instrumentation and the wider library memory suites were
  not rerun.

All 117 ordinary cases in the selected Lab infrastructure/evaluation suites
passed without instrumentation, including the four cases without completed
memory coverage. Source-policy checks passed. The original test configuration
was restored after the README batch. These results establish scoped regression
coverage, not a whole-project memory-safety guarantee.

## Notes

- Run commands from the repository root.
- Always run tests sequentially, with `-j1` for Make test targets. Never overlap
  test suites or memory-test suites; wait for each to finish before starting
  the next.
- The test output is intended to read cleanly in a normal terminal or in the
  Visual Studio Code integrated terminal.

## Recommended Suite Shape

The execution core lives in `tests/test_harness`: `test_harness_runtime.c` supplies
`main()`, calls the suite's `tests_main()`, and owns assertions, nested execution,
fixtures, capture, timings and summaries. `test_harness_text.c` supplies a small
C-allocated diagnostic buffer. These sources and `test_harness.h` have no MARS
library dependencies. The bootstrap executable
`tests/build/release/test_test_harness/test_harness_bootstrap` links without `libmars`, JSON
or SQLite and checks those facilities, including failure and skip subprocesses.
It intentionally creates no configuration or test database.

JSON selections, persistence, catalogue, profiles and results live alongside the
runtime in the `tests/test_harness` implementation module. Its regression suite
lives in `tests/test_test_harness`. The optional bridge in
`tests/test_harness/test_harness_mars.c` connects those services through
`test_harness_adapter.h`; ordinary MARS suites link it to preserve their existing
selection, progress planning and recorded history. The core retains no database
connection or representation-specific knowledge. SQLite implementation tests
continue to disable automatic database selection before their fixtures run.

The harness regression source moves preserve existing case names. Storage updates
the source identities transactionally across the catalogue, selections, results
and timing distributions; recorded outcomes, outlier evidence and memory-test
dates remain intact. A conflicting destination aborts the relocation rather than
discarding either case history.

Domain validity callbacks receive opaque values and a core-owned `test_text_t`
diagnostic sink. Append ordinary C text with `test_text_append_cstr()` or standard
printf arguments with `test_text_append_format()`; do not retain or free the sink.
The domain callback owns any mathematical formatting and releases its temporary
objects itself. Expression helpers live in `tests/expression/test_expr_support.c`,
with declarations in `tests/include/test_expr_support.h`; only the expression and
matrix suites link them. A suite must never include another suite's private header.

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

For example, `tests/test_test_harness/test_test_harness.c` uses
`tests/test_test_harness/test_test_harness.json` in local mode.

Use `TEST_CONFIG_NONE` only for suites that genuinely cannot participate in
config I/O. The ordinary project suites, including `string` and `dictionary`,
are expected to use the shared global config.
