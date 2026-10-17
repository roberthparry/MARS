/**
 * @file lab_precision.h
 * @brief Native custom-precision validation and popover policy for the Lab.
 *
 * Browser event adapters supply DOM capabilities; this interface owns integer
 * admission, decimal-to-binary conversion and the editable draft. It does not
 * change arithmetic settings until Apply submits the existing precision workflow.
 */
#ifndef LAB_PRECISION_H
#define LAB_PRECISION_H

/** @brief Convert an integral bits (unit zero) or decimal-digits request; return zero when out of range. */
unsigned lab_precision_bits(double value, unsigned unit);

/** @brief Close the custom editor, optionally returning keyboard focus to its trigger. */
void lab_precision_close(int focus);

/** @brief Dispatch a custom-precision event and return deferred services through the DOM bridge. */
void lab_precision_dispatch(unsigned action, int event);

/** @brief Install the custom editor's control and document listeners once. */
void lab_precision_install(int document);

#endif
