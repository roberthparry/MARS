/**
 * @file lab_worker_plot_prepared.h
 * @brief Private replay records for already solved native curve plots.
 *
 * Used only by solver plotting and its bounded JSON transport. Exact native
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

typedef struct _lab_plot_prepared_t lab_plot_prepared_t;
struct _lab_plot_prepared_t {
    string_t *title, *TeX, *numeric;
    expr_t *curves[16];
    string_t *labels[16];
    size_t count;
    bool family;
};

/** @brief Read a bounded optional server record; reject mismatched sources, modes, axes or precision. */
lab_plot_prepared_t *lab_worker_plot_prepared_read(int argc, char **argv);

/** @brief Emit an optional bounded single-line replay record from borrowed native results. */
void lab_worker_plot_prepared_write(int argc, char **argv, const char *title, const char *TeX,
                                    const expr_t *const *curves, string_t *const *labels, size_t count,
                                    bool family, const string_t *numeric);

/** @brief Release a loaded record and every owned expression, accepting NULL. */
void lab_worker_plot_prepared_free(lab_plot_prepared_t *prepared);
#endif
