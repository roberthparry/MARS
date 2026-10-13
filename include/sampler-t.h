/**
 * @file sampler-t.h
 * @brief Bounded real curve sampling with precision-preserving coordinates and explicit gaps.
 *
 * sampler_t owns an immutable ordered sample set. Callbacks support ordinary or
 * parametric curves; the expression adapter evaluates independent binding
 * snapshots. Coordinates remain number_t values until a renderer maps them.
 * Adaptive midpoint tests are heuristics, not proofs of continuity or error
 * bounds: narrow features and oscillations can be missed. No threads are started.
 * Sampling inherits computation_run() cancellation and work limits. Results must
 * not be destroyed while borrowed points are in use; mathematical inputs and
 * callbacks require the same external synchronisation as number_t/expr_t.
 */
#ifndef MARS_SAMPLER_H
#define MARS_SAMPLER_H

#include <stdbool.h>
#include <stddef.h>

#include "number-t.h"
#include "expr_context-t.h"

/** @brief Opaque owner of completed samples; release with sampler_free(). */
typedef struct _sampler_t sampler_t;

/** @brief Sampling strategies; uniform sampling does not probe between its points. */
typedef enum { SAMPLER_UNIFORM, SAMPLER_ADAPTIVE } sampler_method_t;

/** @brief Callback outcome: a domain gap is distinct from a fatal evaluation failure. */
typedef enum { SAMPLER_ERROR = -1, SAMPLER_GAP, SAMPLER_VALUE } sampler_status_t;

/**
 * @brief Return owned x/y coordinates for borrowed parameter; outputs start at NUM_NAN.
 * Assign each output once, including when returning GAP/ERROR; the sampler destroys
 * both. Do not destroy parameter. ERROR uses callback errno, or EIO if unset.
 * GAP indicates no real point. Non-finite/complex VALUE coordinates become gaps.
 * The borrowed argument need only survive the synchronous constructor call.
 */
typedef sampler_status_t (*sampler_callback_t)(number_t parameter, number_t *x, number_t *y, void *argument);

/** @brief Copied settings; use sampler_options_default() before overriding individual fields. */
typedef struct {
    sampler_method_t method; /**< Uniform or midpoint-adaptive subdivision. */
    size_t intervals; /**< Initial equal parameter intervals, at least one. */
    size_t max_evaluations; /**< Hard callback limit, including midpoint probes; at least intervals + 1. */
    unsigned max_depth; /**< Adaptive subdivision limit, zero to 24. */
    double absolute_tolerance; /**< Non-negative finite chord deviation, in coordinate units. */
    double relative_tolerance; /**< Non-negative finite deviation relative to coordinate magnitudes. */
} sampler_options_t;

/** @brief Borrowed sample; never destroy its number_t members or modify this record. */
typedef struct {
    number_t parameter; /**< Monotonically increasing parameter, independently owned by the sampler. */
    number_t x; /**< Real coordinate when valid. */
    number_t y; /**< Real coordinate when valid. */
    bool valid; /**< False for domain gaps, complex values and non-finite coordinates. */
    bool break_before; /**< Do not connect this point to its predecessor. */
} sampler_point_t;

/** @brief Adaptive defaults: 32 intervals, 65537 evaluations, depth 12, tolerances 1e-6 and 1e-3. */
sampler_options_t sampler_options_default(void);

/**
 * @brief Sample a callback over finite real lower < upper; NULL options selects defaults.
 * Inputs are borrowed. Returns a complete owned set or NULL; never publishes partial
 * results. EINVAL denotes invalid settings/range, E2BIG the evaluation cap, ENOMEM
 * allocation failure; scoped stopping preserves computation errno. Unresolved
 * adaptive intervals at maximum depth become disconnected points, not false lines.
 */
sampler_t *sampler_new(sampler_callback_t callback, void *argument, number_t lower, number_t upper,
                       const sampler_options_t *options);

/**
 * @brief Sample y=expression against a variable, preserving the original graph and context.
 * NULL context captures source bindings. Other unset symbols yield gaps; samples
 * replace the selected variable's binding without treating it as an assumption.
 * Both expression and variable must be non-NULL and variable must be a variable node.
 * Return/error/ownership policy is the same as sampler_new().
 */
sampler_t *sampler_expression(const expr_t *expression, const expr_t *variable, const expr_context_t *context,
                              number_t lower, number_t upper, const sampler_options_t *options);

/** @brief Release all points, accepting NULL and preserving errno. */
void sampler_free(sampler_t *sampler);

/** @brief Read point count; NULL returns zero with EINVAL. */
size_t sampler_count(const sampler_t *sampler);

/** @brief Read total callback evaluations, including discarded probes; NULL returns zero with EINVAL. */
size_t sampler_evaluations(const sampler_t *sampler);

/** @brief Borrow a point until destruction; invalid sampler/index returns NULL with EINVAL/ERANGE. */
const sampler_point_t *sampler_point(const sampler_t *sampler, size_t index);

#endif
