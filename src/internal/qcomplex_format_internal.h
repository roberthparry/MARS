/**
 * @file qcomplex_format_internal.h
 * @brief Controlled direct complex rendering for typed format consumers.
 *
 * Shares structured native formatting with the format and number modules. Ordinary callers use qcomplex-t.h.
 * Define MARS_SHARED_QCOMPLEX_FORMAT_INTERNAL_ACCESS immediately before inclusion; no argument ownership transfers.
 */
#ifndef MARS_SHARED_QCOMPLEX_FORMAT_INTERNAL_H
#define MARS_SHARED_QCOMPLEX_FORMAT_INTERNAL_H

#if !defined(MARS_SHARED_QCOMPLEX_FORMAT_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0)
#error "Private header requires MARS_SHARED_QCOMPLEX_FORMAT_INTERNAL_ACCESS."
#endif

#include "qcomplex-t.h"
#define MARS_SHARED_QFLOAT_FORMAT_INTERNAL_ACCESS
#include "internal/qfloat_format_internal.h"

/**
 * @brief Render native complex coordinates from resolved options without synthesising a format string.
 * @param value Native complex value, passed by value.
 * @param options Borrowed z/Z/q/Q/f/F/e/E/g/G options; retains established native zero and unit conventions.
 * @return Owned output released with string_free(), or NULL for unsupported options or allocation failure.
 * @note Fixed/scientific/general decimal modes round exact component values without narrowing. No pointers are
 * retained and no mutable state is shared between independent calls.
 */
string_t *qci_format_value_text(qcomplex_t value, const qfi_format_options_t *options);

#endif
