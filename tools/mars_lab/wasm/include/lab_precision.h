/**
 * @file lab_precision.h
 * @brief Native custom-precision validation and popover policy for the Lab.
 *
 * Browser event adapters supply DOM capabilities; this interface owns integer
 * admission, decimal-to-binary conversion and the editable draft. It does not
 * change arithmetic settings until Apply submits the existing precision workflow.
 * Boolean parameters use the wasm32 i32 ABI; browser callers must supply exactly 0 or 1.
 */
#ifndef LAB_PRECISION_H
#define LAB_PRECISION_H

#include <stdbool.h>

#include "lab_handle.h"

/** @brief Units for a custom precision request. */
typedef enum {
    LAB_PRECISION_UNIT_BITS   = 0, /**< Binary significand bits. */
    LAB_PRECISION_UNIT_DIGITS = 1, /**< Requested decimal digits. */
} lab_precision_unit_t;

/** @brief Browser event selectors; numeric values are stable ABI identifiers. */
typedef enum {
    LAB_PRECISION_TOGGLE        = 0, /**< Toggle the precision editor. */
    LAB_PRECISION_CLOSE         = 1, /**< Close without applying. */
    LAB_PRECISION_PREVIEW       = 2, /**< Preview the draft. */
    LAB_PRECISION_SELECT_BITS   = 3, /**< Select binary bit units. */
    LAB_PRECISION_SELECT_DIGITS = 4, /**< Select decimal digit units. */
    LAB_PRECISION_APPLY         = 5, /**< Apply the draft. */
    LAB_PRECISION_KEY           = 6, /**< Handle Enter or Escape. */
    LAB_PRECISION_OUTSIDE       = 7, /**< Handle an outside pointer event. */
} lab_precision_action_t;

/**
 * @brief Convert a custom integral precision request to binary bits.
 * @param value Requested whole bits (53..262144) or decimal digits (17..78913), according to unit.
 * @param unit Zero for bits; one for decimal digits, converted with ceil(digits * log2(10)).
 * @return Binary precision, or zero for an invalid unit, fractional/non-finite input or an out-of-range result.
 */
unsigned lab_precision_bits(double value, lab_precision_unit_t unit);

/**
 * @brief Close the custom editor without applying its draft.
 * @param focus True queues keyboard focus on the custom-precision trigger; false leaves focus unchanged.
 */
void lab_precision_close(bool focus);

/**
 * @brief Dispatch a custom-precision event and publish deferred browser services.
 * @param action Toggle 0, close 1, preview input 2, select bits 3, select decimal digits 4, apply 5,
 * keyboard 6 or outside pointer-down 7; other codes leave an empty plan.
 * @param event Scoped browser Event handle; keyboard actions read key/target and outside clicks read target.
 * @details Publishes a calls array and optional prevent/stop flags through lab_dom_return.
 * Apply requests setWorksheetPrecision only for a valid changed draft; no scoped handle is retained.
 */
void lab_precision_dispatch(lab_precision_action_t action, event_jsref event);

/**
 * @brief Install the custom editor's control and document listeners once.
 * @param document Scoped browser Document handle for keyboard and outside-pointer subscriptions.
 */
void lab_precision_install(jsref document);

#endif
