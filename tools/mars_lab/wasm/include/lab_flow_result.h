/**
 * @file lab_flow_result.h
 * @brief Native continuations for result actions and worksheet startup.
 *
 * Shared browser dispatch uses this application interface to sequence editor transfers,
 * native layout requests, clipboard publication and initial state restoration.
 * Browser frames own values and promises; scoped handles never survive a call.
 */
#ifndef LAB_FLOW_RESULT_H
#define LAB_FLOW_RESULT_H

#include "lab_handle.h"

/** @brief Closed workflow selectors for lab_flow_result; numeric values are browser ABI. */
typedef enum {
    LAB_FLOW_RESULT_TRANSFER      = 0, /**< Transfer result to input. */
    LAB_FLOW_RESULT_PRETTY_MATRIX = 1, /**< Render a matrix in pretty form. */
    LAB_FLOW_RESULT_EXPAND_DIGITS = 2, /**< Expand rendered digits. */
    LAB_FLOW_RESULT_COPY_INPUT    = 3, /**< Copy authored input. */
    LAB_FLOW_RESULT_COPY_CARD     = 4, /**< Copy a result card. */
    LAB_FLOW_RESULT_RENDER_LATEX  = 5, /**< Request TeX rendering. */
    LAB_FLOW_RESULT_INITIALISE    = 6, /**< Initialise the worksheet. */
} lab_flow_result_kind_t;

/** @brief Advance a result-action continuation.
 * @param kind Transfer 0, pretty matrix 1, expanded TeX 2, input copy 3, card copy 4,
 * TeX request 5, initial worksheet 6.
 * @param frame Scoped browser handle to the mutable frame holding stage, action inputs and service results.
 * @param view Scoped browser handle to the presentation view supplying caches for pretty-matrix rendering;
 * ignored by the other workflows.
 * @return Scoped browser handle to the next continuation plan; unsupported kinds produce an inert completion.
 */
jsref lab_flow_result(lab_flow_result_kind_t kind, frame_jsref frame, view_jsref view);

#endif
