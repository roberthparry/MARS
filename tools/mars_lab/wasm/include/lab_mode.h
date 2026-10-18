/**
 * @file lab_mode.h
 * @brief Shared worksheet identities for the freestanding browser application.
 *
 * All browser controllers use these modes for editor, request and presentation policy.
 * Values are part of the WebAssembly ABI and match the host's worksheet ordering.
 * Counts and invalid sentinels are not selectable modes; each API retains its documented
 * handling of invalid input received from the browser.
 */
#ifndef LAB_WASM_MODE_H
#define LAB_WASM_MODE_H

/** @brief Worksheet identity shared by native browser controllers. */
typedef enum {
    LAB_MODE_EXPRESSION = 0,            /**< Symbolic expression worksheet. */
    LAB_MODE_EQUATION = 1,              /**< Equation solver worksheet. */
    LAB_MODE_DIFFERENTIAL_EQUATION = 2, /**< Differential-equation worksheet. */
    LAB_MODE_MATRIX = 3,                /**< Matrix worksheet. */
    LAB_MODE_INTEGRATOR = 4,            /**< Numerical integration worksheet. */
    LAB_MODE_DATETIME = 5,              /**< Date and time worksheet. */
    LAB_MODE_ALMANAC = 6,               /**< Almanac worksheet. */
    LAB_MODE_COUNT = 7,                 /**< Number of selectable worksheets. */
    LAB_MODE_INVALID = LAB_MODE_COUNT   /**< Sentinel for APIs that reject unknown modes. */
} lab_mode_t;

#endif
