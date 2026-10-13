/**
 * @file sampler_internal.h
 * @brief Private point ownership and subdivision state for the sampler implementation.
 *
 * Only sampler sources may inspect this state. Consumers use sampler-t.h;
 * construction keeps all mutable work private until the complete result exists.
 */
#ifndef MARS_SAMPLER_INTERNAL_H
#define MARS_SAMPLER_INTERNAL_H
#if !defined(MARS_SAMPLER_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0)
#error "Private header requires MARS_SAMPLER_INTERNAL_ACCESS."
#endif

#include "sampler-t.h"

struct _sampler_t {
    sampler_point_t *points;
    size_t count, capacity, evaluations;
    sampler_options_t options;
    number_t absolute, relative;
    sampler_callback_t callback;
    void *argument;
};

/** @brief Refine one borrowed edge, appending owned ordered samples after its left endpoint. */
bool sampler_refine(sampler_t *sampler, const sampler_point_t *left, const sampler_point_t *right, unsigned depth);

/** @brief Evaluate an owned temporary point under the current computation scope. */
bool sampler_evaluate(sampler_t *sampler, number_t parameter, sampler_point_t *point);

/** @brief Append an independent copy, forcing a gap if requested; input remains borrowed. */
bool sampler_append(sampler_t *sampler, const sampler_point_t *point, bool disconnect);

/** @brief Destroy the three owned coordinates in a temporary point. */
void sampler_point_clear(sampler_point_t *point);

#endif
