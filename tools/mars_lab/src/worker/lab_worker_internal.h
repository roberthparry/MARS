/**
 * @file lab_worker_internal.h
 * @brief Calculation entry points linked into the MARS Lab executable.
 *
 * Private to the worker module and its dispatcher. Each entry point runs once
 * in a fresh child process; callers must use lab_process.h for bounded execution.
 * These routines may initialise process-wide mathematical state and are not
 * re-entrant services for the listener or request processes.
 */
#ifndef MARS_LAB_WORKER_INTERNAL_H
#define MARS_LAB_WORKER_INTERNAL_H

#if !defined(MARS_LAB_WORKER_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0)
#error "Private header requires MARS_LAB_WORKER_INTERNAL_ACCESS."
#endif

#include "number-t.h"
#include "string-t.h"
#include "expression-t.h"
#include "equation-t.h"
#include "sampler-t.h"
#include "plot-t.h"

/** @brief Plot equation sides, two-coordinate contours or explicit differential-equation curves and surfaces. */
int lab_worker_solver_plot(int argc, char **argv);

/** @brief Render a bounded real zero contour for an equation with two selected coordinates. */
int lab_worker_equation_contour(equation_t *equation, expr_t *first, expr_t *second, char **argv);

/** @brief Render a bounded isosurface for three free real equation coordinates. */
int lab_worker_equation_mesh(equation_t *equation, expr_t *first, expr_t *second, expr_t *third, char **argv);

/** @brief Plot a one-variable integrand, shading the supplied definite integration interval. */
int lab_worker_integrand_plot(int argc, char **argv);

/** @brief Return owning worksheet-precision solution records, using the normal equation solution formatter. */
string_t *lab_worker_equation_plot_solutions(const equation_t *equation, int precision);

/** @brief Build up to sixteen labelled representative ODE curves, varying only generated integration constants. */
bool lab_worker_plot_family(expr_bindings_t *authored, const char *axis, const expr_t *const *branches,
                             size_t branch_count, expr_t **curves, string_t **labels, size_t *count, bool *family);

/** @brief Plot a native solution surface with two declared axes and a named dependent variable. */
int lab_worker_plot_surface(const char *source, const expr_t *expression, const expr_t *first,
                            const expr_t *second, const expr_t *function, int argc, char **argv);

/** @brief Refine automatic family bounds using binned sampler activity and located stationary points. */
void lab_worker_plot_frame(char *const *sources, size_t count, const char *axis, number_t bounds[4]);

/** @brief Frame native equation intersections and turning points; optionally refine automatic horizontal bounds. */
bool lab_worker_plot_equation_frame(const expr_t *const *sides, char *const *sources, const char *axis,
                                    bool horizontal, number_t bounds[4]);

/** @brief Refine bounded surface-extremum candidates with gradients and Hessians for automatic height limits. */
bool lab_worker_surface_extrema(plot_t *plot, const char *source, const char *first, const char *second,
                                const number_t *x, const number_t *y, const number_t *z, size_t side);

/** @brief Parse a plotting range with guard precision; return owning bounds without changing worksheet precision. */
bool lab_worker_plot_bounds(const char *lower, const char *upper, number_t *lo, number_t *hi);

/** @brief Choose omitted curve or surface bounds with bounded real-domain probes; preserve explicit endpoints. */
bool lab_worker_plot_ranges(const char *source, const char *first, const char *second,
                            const char *const limits[4], number_t bounds[4]);

/** @brief Determine guarded coordinate precision for a finite range; zero denotes invalid bounds. */
size_t lab_worker_plot_range_precision(number_t lower, number_t upper);

/** @brief Choose validation precision for cancellation on a tiny range. */
size_t lab_worker_plot_check_precision(number_t lower, number_t upper);

/** @brief Emit plot records from a borrowed SVG; optional function and axis symbols name a surface title. */
int lab_worker_plot_output(const char *source, const expr_t *expression, const string_t *svg, int error,
                           const expr_t *function, const expr_t *first, const expr_t *second);

/** @brief Dispatch a curve or surface request using explicit axes or native unset-variable metadata. */
int lab_worker_plot_request(const char *source, const expr_t *expression, expr_bindings_t *bindings,
                            const expr_t *variable, int argc, char **argv);

/**
 * @brief Sample a privately reparsed formula at spot-checked precision, returned through precision_bits on success.
 * Bounds are borrowed; pass NUM_NAN for both vertical bounds when not using a manual viewport.
 * Returns an owning sampler, or NULL on failure. Worksheet precision is restored on every exit.
 */
sampler_t *lab_worker_plot_samples(const char *source, const char *variable, number_t lower, number_t upper,
                                  number_t ymin, number_t ymax, size_t *precision_bits);

/** @brief Sample with range-aware precision; previews reduce subdivision work and omit derivative construction. */
sampler_t *lab_worker_plot_samples_quality(const char *source, const char *variable, number_t lower, number_t upper,
                                          number_t ymin, number_t ymax, size_t *precision_bits, bool preview);

/** @brief Emit an in-memory real curve SVG for a selected variable and mathematical bound texts. */
int lab_worker_plot(const char *source, const expr_t *expression, const expr_t *variable, const char *lower, const char *upper,
                    const char *scale, const char *y_lower, const char *y_upper, bool preview);

/**
 * @brief Formats a numerical card value with decimal or Unicode power-of-ten notation.
 * @param value Borrowed value; nonzero imaginary components are retained.
 * @param precision Significant decimal digits, or non-positive to use the current default.
 * @return Caller-owned text, released with string_free, or NULL on allocation failure.
 */
string_t *lab_worker_number_text(number_t value, int precision);

/**
 * @brief Runs the built-in expression calculation.
 * @param argc Argument count including the mode name.
 * @param argv Borrowed argument vector with the mode name at index zero.
 * @return Conventional process exit status.
 */
int lab_worker_expression(int argc, char **argv);

/**
 * @brief Runs the built-in equation calculation.
 * @param argc Argument count including the mode name.
 * @param argv Borrowed argument vector with the mode name at index zero.
 * @return Conventional process exit status.
 */
int lab_worker_equation(int argc, char **argv);

/**
 * @brief Runs the built-in diffequation calculation.
 * @param argc Argument count including the mode name.
 * @param argv Borrowed argument vector with the mode name at index zero.
 * @return Conventional process exit status.
 */
int lab_worker_diffequation(int argc, char **argv);

/**
 * @brief Runs the built-in matrix calculation.
 * @param argc Argument count including the mode name.
 * @param argv Borrowed argument vector with the mode name at index zero.
 * @return Conventional process exit status.
 */
int lab_worker_matrix(int argc, char **argv);

/**
 * @brief Runs the built-in integrator calculation.
 * @param argc Argument count including the mode name.
 * @param argv Borrowed argument vector with the mode name at index zero.
 * @return Conventional process exit status.
 */
int lab_worker_integrator(int argc, char **argv);

/**
 * @brief Runs the built-in datetime calculation.
 * @param argc Argument count including the mode name.
 * @param argv Borrowed argument vector with the mode name at index zero.
 * @return Conventional process exit status.
 */
int lab_worker_datetime(int argc, char **argv);

/**
 * @brief Runs the built-in almanac calculation.
 * @param argc Argument count including the mode name.
 * @param argv Borrowed argument vector with the mode name at index zero.
 * @return Conventional process exit status.
 */
int lab_worker_almanac(int argc, char **argv);

/**
 * @brief Runs the built-in almanac event calculation.
 * @param argc Argument count including the mode name.
 * @param argv Borrowed argument vector with the mode name at index zero.
 * @return Conventional process exit status.
 */
int lab_worker_almanac_event(int argc, char **argv);

/**
 * @brief Runs the built-in holiday calculation.
 * @param argc Argument count including the mode name.
 * @param argv Borrowed argument vector with the mode name at index zero.
 * @return Conventional process exit status.
 */
int lab_worker_holiday(int argc, char **argv);

/**
 * @brief Runs the built-in ophelia calculation.
 * @param argc Argument count including the mode name.
 * @param argv Borrowed argument vector with the mode name at index zero.
 * @return Conventional process exit status.
 */
int lab_worker_ophelia(int argc, char **argv);

#endif
