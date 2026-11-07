/**
 * @file lab_worker_plot_prepared.h
 * @brief Private replay records for already solved native curve plots.
 *
 * Used by completed equation evaluations, solver plotting and their bounded JSON transport. Exact native
 * expression renderings retain bindings; the browser never interprets these records.
 */
#ifndef MARS_LAB_WORKER_PLOT_PREPARED_H
#define MARS_LAB_WORKER_PLOT_PREPARED_H
#if !defined(MARS_LAB_WORKER_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0)
#error "Private header requires MARS_LAB_WORKER_INTERNAL_ACCESS."
#endif
#include "expression-t.h"
#include "string-t.h"

/** @brief Independently owned replay data; release the complete record with lab_worker_plot_prepared_free(). */
typedef struct _lab_plot_prepared_t {
    string_t *title, *TeX, *numeric; /**< Owned titles and unchanged labelled numerical-result records. */
    number_t *roots; /**< Owned finite root values, or NULL when the plot uses real curves. */
    size_t root_count; /**< Number of owned roots, at most 256; destroy each before freeing roots. */
    expr_t *curves[16]; /**< Owned expressions for the first count entries. */
    string_t *labels[16]; /**< Owned labels for the first count entries. */
    size_t count; /**< Number of populated curve and label entries, from one to sixteen. */
    bool family; /**< True when curves are representative solution-family members. */
} lab_plot_prepared_t;

/**
 * @brief Read a bounded optional replay record from this isolated worker's standard input.
 * @param argc Number of entries in @p argv; requires twenty, including the final cache flag.
 * @param[in] argv Borrowed worker arguments used to verify exact source, axis, precision and mode identities.
 * @return Independently owned replay data, released with lab_worker_plot_prepared_free(), or NULL for missing
 * flags, unreadable input, invalid or oversized records, mismatches or allocation failure. A miss performs no solve.
 * Argument strings remain caller-owned and are not retained. Only a process-local duplicate of the regular
 * standard-input file is opened and closed; callers must not read it concurrently.
 */
lab_plot_prepared_t *lab_worker_plot_prepared_read(int argc, char **argv);

/**
 * @brief Emit completed native curves and numerical results for reuse by a subsequent plot worker.
 * @param[in] keys Four borrowed strings: exact authored source, axis name, precision text and worksheet mode.
 * @param[in] title Borrowed plain-text title; NULL emits an empty title.
 * @param[in] TeX Borrowed native TeX title; NULL emits an empty title.
 * @param[in] curves Borrowed array of @p count native expressions, serialised without changing their bindings.
 * @param[in] labels Optional borrowed array of @p count labels; NULL emits empty labels.
 * @param count Number of curves, from one to sixteen.
 * @param family Whether the curves are representative members of a solution family.
 * @param[in] numeric Optional borrowed labelled numerical-result records, copied without recalculation or rounding.
 * @param[in] roots Optional borrowed array of @p root_count finite numerical roots for a complex-plane diagram.
 * @param root_count Number of roots, at most 256; zero retains ordinary curve plotting.
 * @return Nothing. Emits a single bounded record on standard output; invalid inputs, allocation or size failure
 * omit it. No ownership transfers, inputs need remain valid only for the call, and no files are written.
 * This operation is used only within an isolated calculation process.
 */
void lab_worker_plot_prepared_emit(const char *const keys[4], const char *title, const char *TeX,
                                   const expr_t *const *curves, string_t *const *labels, size_t count,
                                   bool family, const string_t *numeric, const number_t *roots, size_t root_count);

/** @brief Emit an optional bounded single-line replay record from borrowed native results.
 * @param argc Number of entries in @p argv; requires twenty, including the final cache flag.
 * @param[in] argv Borrowed plot-worker arguments; exact source, axis, precision and mode supply the cache identity.
 * @param[in] title Borrowed plain-text title, or NULL for an empty title.
 * @param[in] TeX Borrowed TeX title, or NULL for an empty title.
 * @param[in] curves Borrowed array of @p count native expressions.
 * @param[in] labels Optional borrowed array of @p count labels.
 * @param count Number of curves, from one to sixteen.
 * @param family Whether the curves represent a solution family.
 * @param[in] numeric Optional borrowed labelled numerical results to retain unchanged.
 * @param[in] roots Optional borrowed array of @p root_count finite numerical roots; no ownership transfers.
 * @param root_count Number of roots, at most 256; zero retains ordinary curve plotting.
 * @return Nothing. Missing cache flags or invalid, oversized or unallocatable records emit nothing.
 * Inputs remain caller-owned and need remain valid only for this isolated-worker call.
 */
void lab_worker_plot_prepared_write(int argc, char **argv, const char *title, const char *TeX,
                                    const expr_t *const *curves, string_t *const *labels, size_t count,
                                    bool family, const string_t *numeric, const number_t *roots, size_t root_count);

/**
 * @brief Release a loaded replay record and all its owned expressions and strings.
 * @param[in,out] prepared Caller-owned record to destroy; NULL is harmless and other aliases become invalid.
 * @return Nothing. Performs no calculation or file I/O and must not overlap uses of the same record.
 */
void lab_worker_plot_prepared_free(lab_plot_prepared_t *prepared);
#endif
