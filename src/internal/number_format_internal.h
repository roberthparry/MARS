/**
 * @file number_format_internal.h
 * @brief Controlled precision-preserving numeric presentation for typed format consumers.
 *
 * Exposes the number module's established pretty/scientific rendering without disclosing backend storage.
 * Define MARS_SHARED_NUMBER_FORMAT_INTERNAL_ACCESS immediately before inclusion. Ordinary callers use number-t.h.
 */
#ifndef MARS_SHARED_NUMBER_FORMAT_INTERNAL_H
#define MARS_SHARED_NUMBER_FORMAT_INTERNAL_H

#if !defined(MARS_SHARED_NUMBER_FORMAT_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0)
#error "Private header requires MARS_SHARED_NUMBER_FORMAT_INTERNAL_ACCESS."
#endif

#include "string-t.h"
#include "number-t.h"

/**
 * @brief Render a borrowed numeric value with the established n/N presentation conventions.
 * @param value Borrowed shallow numeric handle; not retained, mutated or destroyed.
 * @param presentation 'n' for native pretty output or 'N' for native scientific output.
 * @param precision Non-negative requested precision, or -1 for backend defaults; exact values retain their exact form.
 * @return Owned text released with string_free(), or NULL for an unsupported presentation, invalid value or allocation failure.
 * @note Preserves backend precision and native complex conventions. Independent calls use no shared scratch state.
 */
string_t *numi_format_value_text(number_t value, char presentation, int precision);

#endif
