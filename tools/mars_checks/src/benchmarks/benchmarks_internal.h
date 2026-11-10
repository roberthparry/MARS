/**
 * @file benchmarks_internal.h
 * @brief Private typed dispatch and persistence boundary for mathematical benchmarks.
 *
 * Only the benchmarks implementation consumes these declarations. Each backend calls the public mathematical API
 * directly. Function lookup uses a sorted catalogue and binary search; no expression parser is timed. All pointers
 * in operation definitions refer to static storage. Application clients use checks_benchmarks.h.
 */
#ifndef MARS_BENCHMARKS_INTERNAL_H
#define MARS_BENCHMARKS_INTERNAL_H
#if !defined(MARS_BENCHMARKS_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0)
#error "Private benchmark header requires MARS_BENCHMARKS_INTERNAL_ACCESS."
#endif

#include <stdbool.h>
#include <stddef.h>

#include "number-t.h"
#include "sqlite-t.h"

/** @brief Typed function alternatives; only the member selected by operation arity and backend is used. */
typedef union {
    /** @brief Evaluate a unary number operation.
     * @param[in] a Borrowed operand, valid throughout the call.
     * @return Owned result, released with num_destroy(); invalid or non-finite results fail the measurement.
     */
    number_t (*n1)(number_t a);

    /** @brief Evaluate a binary number operation.
     * @param[in] a Borrowed first operand. @param[in] b Borrowed second operand.
     * @return Owned result, released with num_destroy(); invalid or non-finite results fail the measurement.
     */
    number_t (*n2)(number_t a, number_t b);

    /** @brief Evaluate a ternary number operation.
     * @param[in] a Borrowed first operand. @param[in] b Borrowed second operand.
     * @param[in] c Borrowed third operand.
     * @return Owned result, released with num_destroy(); invalid or non-finite results fail the measurement.
     */
    number_t (*n3)(number_t a, number_t b, number_t c);

    /** @brief Evaluate a six-operand number operation.
     * @param[in] a Borrowed first operand. @param[in] b Borrowed second operand.
     * @param[in] c Borrowed third operand. @param[in] d Borrowed fourth operand.
     * @param[in] e Borrowed fifth operand. @param[in] f Borrowed sixth operand.
     * @return Owned result, released with num_destroy(); invalid or non-finite results fail the measurement.
     */
    number_t (*n6)(number_t a, number_t b, number_t c, number_t d, number_t e, number_t f);

    /** @brief Evaluate a unary fixed precision real operation.
     * @param[in] a Operand copied by value.
     * @return Result by value, requiring no release; NaN or infinity fails the measurement.
     */
    qfloat_t (*r1)(qfloat_t a);

    /** @brief Evaluate a binary fixed precision real operation.
     * @param[in] a First operand copied by value. @param[in] b Second operand copied by value.
     * @return Result by value, requiring no release; NaN or infinity fails the measurement.
     */
    qfloat_t (*r2)(qfloat_t a, qfloat_t b);

    /** @brief Evaluate a ternary fixed precision real operation.
     * @param[in] a First operand copied by value. @param[in] b Second operand copied by value.
     * @param[in] c Third operand copied by value.
     * @return Result by value, requiring no release; NaN or infinity fails the measurement.
     */
    qfloat_t (*r3)(qfloat_t a, qfloat_t b, qfloat_t c);

    /** @brief Evaluate a six-operand fixed precision real operation.
     * @param[in] a First operand. @param[in] b Second operand. @param[in] c Third operand.
     * @param[in] d Fourth operand. @param[in] e Fifth operand. @param[in] f Sixth operand.
     * @return Result by value, requiring no release; NaN or infinity fails the measurement.
     */
    qfloat_t (*r6)(qfloat_t a, qfloat_t b, qfloat_t c, qfloat_t d, qfloat_t e, qfloat_t f);

    /** @brief Evaluate a unary fixed precision complex operation.
     * @param[in] a Operand copied by value.
     * @return Result by value, requiring no release; a non-finite component fails the measurement.
     */
    qcomplex_t (*c1)(qcomplex_t a);

    /** @brief Evaluate a binary fixed precision complex operation.
     * @param[in] a First operand copied by value. @param[in] b Second operand copied by value.
     * @return Result by value, requiring no release; a non-finite component fails the measurement.
     */
    qcomplex_t (*c2)(qcomplex_t a, qcomplex_t b);

    /** @brief Evaluate a ternary fixed precision complex operation.
     * @param[in] a First operand copied by value. @param[in] b Second operand copied by value.
     * @param[in] c Third operand copied by value.
     * @return Result by value, requiring no release; a non-finite component fails the measurement.
     */
    qcomplex_t (*c3)(qcomplex_t a, qcomplex_t b, qcomplex_t c);

    /** @brief Evaluate a six-operand fixed precision complex operation.
     * @param[in] a First operand. @param[in] b Second operand. @param[in] c Third operand.
     * @param[in] d Fourth operand. @param[in] e Fifth operand. @param[in] f Sixth operand.
     * @return Result by value, requiring no release; a non-finite component fails the measurement.
     */
    qcomplex_t (*c6)(qcomplex_t a, qcomplex_t b, qcomplex_t c, qcomplex_t d, qcomplex_t e, qcomplex_t f);
} benchmark_function_t;

/** @brief Static mathematical operation and typed backend entry points. */
typedef struct {
    const char *name; /**< Canonical operation identifier; also the SQL catalogue primary key. */
    unsigned arity; /**< Number of operands: one, two, three or six. */
    benchmark_function_t number; /**< Generic number entry point, always present. */
    benchmark_function_t real; /**< Direct qfloat entry point, or zero when unavailable. */
    benchmark_function_t complex_fn; /**< Direct qcomplex entry point, or zero when unavailable. */
    const char *arguments[6]; /**< Domain-appropriate real operand literals, parsed outside timing. */
    unsigned complex_mask; /**< Bits marking operands which receive a non-zero imaginary part. */
    bool integer_only; /**< True selects the exact-integer profile instead of inexact floating representations. */
} benchmark_operation_t;

/**
 * @brief Static description of the timing protocol, including warm-up and batching parameters.
 * @details This immutable string is stored with every run and must match for timing comparisons. Its storage
 * lasts for the process lifetime and must not be released. Change it whenever the measurement protocol changes.
 */
extern const char benchmark_protocol[];

/**
 * @brief Imaginary component added to permitted complex operands, shared by workers and reports.
 * @details Immutable real literal with process lifetime; never modify or release its storage.
 */
extern const char benchmark_imaginary_literal[];

/**
 * @brief Construct the exact literal spelling used to prepare one numeric worker operand.
 * @param[in] real Borrowed real literal, valid throughout the call; copied into the result.
 * @param[in] complex_value True appends an imaginary component, including explicit zero when required by MPC.
 * @param[in] imaginary True uses benchmark_imaginary_literal; false uses zero. Ignored for a real value.
 * @return Owned string released with string_free(), or NULL for a null real literal or allocation failure.
 * No operand is parsed or evaluated and no shared state is modified.
 */
string_t *benchmark_argument_literal(const char *real, bool complex_value, bool imaginary);

/**
 * @brief Describe the actual values represented by saved real literals, backend and imaginary-component mask.
 * @param[in] arguments Borrowed comma-separated real literals from the saved case; at most six operands.
 * @param[in] backend Borrowed saved backend name; qcomplex and mpc apply the imaginary mask.
 * @param[in] complex_mask Bits selecting operands receiving benchmark_imaginary_literal; other operands stay real.
 * @return Owned display string released with string_free(), or NULL for invalid masked input or allocation failure.
 * Uses saved case data only; it does not consult the current catalogue, run mathematics or change observations.
 */
string_t *benchmark_arguments_text(const char *arguments, const char *backend, unsigned complex_mask);

/**
 * @brief Find an operation by exact canonical name using binary search.
 * @param[in] name Borrowed non-null identifier.
 * @return Borrowed static definition, or NULL for an unknown name; never release it.
 */
const benchmark_operation_t *benchmark_operation(const char *name);

/**
 * @brief Return the complete static operation catalogue.
 * @param[out] count Required destination for the number of sorted definitions.
 * @return Borrowed static array, valid for the process lifetime; never release it.
 */
const benchmark_operation_t *benchmark_operations(size_t *count);

/**
 * @brief Test the correctly typed direct entry point for an operation's arity.
 * @param[in] operation Borrowed static definition returned by benchmark_operation(); NULL is unavailable.
 * @param[in] complex_backend True selects qcomplex, false selects qfloat.
 * @return True when that direct backend exists; false otherwise. No union member of another arity is inspected.
 */
bool benchmark_operation_supported(const benchmark_operation_t *operation, bool complex_backend);

/**
 * @brief Measure one catalogue case in an isolated worker and print machine-readable samples.
 * @param[in] root Borrowed repository root; storage is not created or changed.
 * @param[in] argc Number of literal worker arguments.
 * @param[in] argv Borrowed arguments: operation, backend, bits and sample count.
 * @return Zero after finite results and all timings were written; one on invalid mathematics or clock failure;
 * two on invalid arguments, or three for an unavailable backend. No mathematical result or operand escapes this call.
 */
int benchmark_measure(const string_t *root, int argc, char **argv);

/**
 * @brief Open and optionally initialise the persistent benchmark database.
 * @param[in] root Borrowed repository root containing the schema and storage directory.
 * @param[in] create True initialises schema and merges all known operations without deleting historical cases.
 * @return Owned plaintext connection released with sqlite_close(), or NULL on absence or failure.
 * @note A false create flag checks file existence first and never creates a database.
 */
sqlite_t *benchmark_database_open(const string_t *root, bool create);

/**
 * @brief Apply stored age, run-count, raw-sample and file-size limits transactionally.
 * @param[in,out] db Borrowed writable benchmark connection with schema version two.
 * @param[in] protected_run Current or requested resumable run which must survive cleanup; zero protects none.
 * @param[in] headroom True attempts to leave one quarter of the configured capacity available for new checkpoints.
 * @return True after pruning, necessary compaction and installation of the SQLite page cap; false on failure.
 * @note The caller must hold the exclusive benchmark file lock and have no active statements or transaction.
 * Retained summaries, failure evidence and outlier flags are unchanged. Expired runs and their samples are removed
 * together. If the protected run alone exhausts capacity, no further database growth is permitted.
 */
bool benchmark_retention_apply(sqlite_t *db, long protected_run, bool headroom);

/**
 * @brief Check storage headroom between workers and reclaim older history when nearing the page cap.
 * @param[in,out] db Borrowed benchmark connection; the caller holds its exclusive benchmark file lock.
 * @param[in] protected_run Active positive run identifier, never deleted or stripped of raw samples.
 * @return True when the check or required cleanup succeeds; false stops the run with saved checkpoints intact.
 * @note No numerical timing occurs here. Call with no active statements or transaction before launching a worker.
 */
bool benchmark_retention_checkpoint(sqlite_t *db, long protected_run);

/**
 * @brief Display, configure or explicitly apply the persisted benchmark retention policy.
 * @param[in] root Borrowed repository root locating the benchmark database file used for writer exclusion.
 * @param[in,out] db Borrowed open benchmark connection; configuration and pruning require schema version two.
 * @param[in] argc Number of literal policy options; zero displays settings unless prune is true.
 * @param[in] argv Borrowed --max-mib, --keep-runs, --max-days and --raw-runs value pairs, valid during the call.
 * @param[in] prune True applies the current policy without changing it; no options are accepted in this mode.
 * @return Zero on success, one on lock, storage or output failure, or two on invalid options.
 * @note Display alone never writes. Mutation takes a non-waiting exclusive benchmark file lock; settings and logical
 * deletions commit together. Temporary journals and compaction files are outside the main database's size cap.
 */
int benchmark_retention_command(const string_t *root, sqlite_t *db, int argc, char **argv, bool prune);

/**
 * @brief Render the selected run as a Markdown timing table or replace marked documentation sections.
 * @param[in] root Borrowed repository root locating Markdown guides.
 * @param[in,out] db Borrowed benchmark database; report generation never mutates measurements.
 * @param[in] run Positive completed run identifier; zero selects the most recent completed run, requiring a full
 * catalogue when update is true. Individual failures remain visible in that run's report.
 * @param[in] update True replaces the managed tables; false writes the complete report to stdout.
 * @return Zero on success or one on missing data, malformed markers, or I/O failure.
 * @note Table refreshes never execute benchmarks or mix measurements from different runs.
 */
int benchmark_report(const string_t *root, sqlite_t *db, long run, bool update);

/**
 * @brief Print a saved benchmark run with compact tables by default, as Unicode, Markdown or tab-separated values.
 * @param[in,out] db Borrowed open benchmark connection; observations and definitions are never changed.
 * @param[in] argc Number of literal filtering and presentation options.
 * @param[in] argv Borrowed options: --run, --backend, --bits, --operation, --details, --summary or
 * --format=grid|markdown|tsv. Actual operands appear in every view; --details adds diagnostic metadata.
 * @return Zero after writing the selected rows, one on missing storage, allocation or output failure, or two on
 * invalid options. All temporary snapshots are released before return; no mathematical operation is executed.
 * @note Colours are confined to grid output and respect NO_COLOR. Pipe the grid through less -R for coloured paging.
 */
int benchmark_stats(sqlite_t *db, int argc, char **argv);

/** @brief Owned live benchmark display; release with benchmark_progress_finish(). */
typedef struct _benchmark_progress_t benchmark_progress_t;

/** @brief Persisted outcome counts used to initialise a new or resumed display. */
typedef struct {
    size_t total; /**< Total number of selected cases, including pending and unsupported cases. */
    size_t passed; /**< Completed cases with finite timed samples. */
    size_t failed; /**< Completed cases with an unsuccessful mathematical or worker result. */
    size_t timed_out; /**< Cases whose worker exceeded its deadline. */
    size_t skipped; /**< Cases unsupported by the selected backend. */
} benchmark_progress_counts_t;

/**
 * @brief Allocate a display initialised from committed case counts.
 * @param[in] counts Borrowed totals copied before return; completed outcomes must not exceed total.
 * @param[in] timeout Positive per-case execution deadline in seconds.
 * @param[in] verbose True retains the detailed one-line-per-case listing instead of live compact progress.
 * @return Owned display released with benchmark_progress_finish(), or NULL on allocation, clock or invalid-count error.
 * @note Terminal capabilities and colour preferences are detected on stderr. No thread or timer is created.
 */
benchmark_progress_t *benchmark_progress_new(const benchmark_progress_counts_t *counts, double timeout, bool verbose);

/**
 * @brief Identify the worker about to execute and reset its elapsed clock.
 * @param[in,out] progress Borrowed display; NULL is ignored.
 * @param[in] operation Borrowed canonical operation name, copied into a bounded display label.
 * @param[in] backend Borrowed backend name, copied into a bounded display label.
 * @param[in] bits Requested precision; zero denotes exact-integer arithmetic.
 * @return No value. Control characters in labels cannot enter terminal output.
 */
void benchmark_progress_case(benchmark_progress_t *progress, const char *operation, const char *backend, int bits);

/**
 * @brief Update the heartbeat through the process runner's synchronous observer callback.
 * @param[in] output_path Unused borrowed worker output path; its contents are never read or modified.
 * @param[in] elapsed Monotonic elapsed seconds for the current worker.
 * @param[in,out] context Borrowed benchmark_progress_t retained until worker collection completes.
 * @return No value. Rendering is throttled across workers, leaving mathematical timing inside the child unchanged.
 */
void benchmark_progress_observe(const string_t *output_path, double elapsed, void *context);

/**
 * @brief Count one successfully committed outcome and preserve visible failures.
 * @param[in,out] progress Borrowed display; NULL is ignored.
 * @param[in] outcome Borrowed stored outcome: pass, fail, timeout or unsupported.
 * @return No value. Unknown outcomes are ignored; failures and timeouts receive permanent diagnostic lines.
 * @note Call only after saving the case and its samples. Rendering never writes to the benchmark database.
 */
void benchmark_progress_complete(benchmark_progress_t *progress, const char *outcome);

/**
 * @brief Clear live terminal rows, print final counts and release the display.
 * @param[in] progress Owned display consumed by this call; NULL is accepted.
 * @param[in] complete True means every selected case was attempted and saved, even if some measurements failed.
 * @return No value. Interrupted runs retain their actual counts; pending cases are never presented as completed.
 */
void benchmark_progress_finish(benchmark_progress_t *progress, bool complete);

/**
 * @brief Run or resume sequential measurements with durable per-case checkpoints and progress.
 * @param[in] root Borrowed repository root containing benchmark sources and the release library.
 * @param[in,out] db Borrowed writable benchmark storage, initialised by benchmark_database_open().
 * @param[in] argc Number of literal selection and measurement options.
 * @param[in] argv Borrowed options: --backend, --bits, --operation, --samples, --timeout, --resume or --verbose.
 * @return Zero after all selected cases finish without failures, one on failed measurements or storage errors,
 * or two for invalid options. Unsupported direct APIs are explicitly recorded and are not failures.
 * @note A subprocess deadline bounds each case. An interrupted run can be resumed without deleting its results.
 */
int benchmark_run(const string_t *root, sqlite_t *db, int argc, char **argv);

#endif
