/**
 * @file qfloat_format_internal.h
 * @brief Controlled native real formatting support for numeric and typed output.
 *
 * Shares resolved conversion options and real-value rendering with native complex, number and typed formatters.
 * Consumers forward parsed options without generating another format string.
 * Consumers define MARS_SHARED_QFLOAT_FORMAT_INTERNAL_ACCESS immediately before inclusion. Ordinary callers
 * should use qfloat-t.h or qcomplex-t.h; this facade is not an installed public API.
 */
#ifndef MARS_SHARED_QFLOAT_FORMAT_INTERNAL_H
#define MARS_SHARED_QFLOAT_FORMAT_INTERNAL_H

#if !defined(MARS_SHARED_QFLOAT_FORMAT_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0)
#error "Private header requires MARS_SHARED_QFLOAT_FORMAT_INTERNAL_ACCESS."
#endif

#include <stdarg.h>

#include "string-t.h"
#include "qfloat-t.h"

/** @brief Resolved native formatting options shared with the complex formatter; owns no storage. */
typedef struct {
    char conversion; /**< Native q/Q, exact decimal f/F/e/E/g/G, or owning complex z/Z conversion. */
    int flag_plus; /**< Non-zero requests an explicit positive sign. */
    int flag_space; /**< Non-zero reserves a space for a positive sign when flag_plus is absent. */
    int flag_minus; /**< Non-zero requests left alignment. */
    int flag_zero; /**< Non-zero requests zero padding. */
    int flag_hash; /**< Non-zero requests alternate formatting. */
    int width; /**< Resolved non-negative minimum field width. */
    int precision; /**< Resolved precision, or a negative value for the conversion's default. */
} qfi_format_options_t;

/**
 * @brief Resolve numeric formatting options from an already parsed string conversion.
 * @param[out] opts Required caller-owned destination, populated on success.
 * @param[in] spec Required borrowed conversion specification with no length modifier.
 * @param[in,out] ap Argument list positioned before optional dynamic width and precision; consumes only those arguments.
 * @return Zero on success, or -1 for a NULL destination/specification or unsupported length modifier.
 * @note Allocates nothing, retains no pointers and does not consume the numeric value. Reentrant with separate storage.
 */
int qfi_format_options_from_spec(qfi_format_options_t *opts, const string_format_spec_t *spec, va_list ap);

/**
 * @brief Format a native real value directly from resolved options without parsing a generated format string.
 * @param[in] x Real value, passed by value.
 * @param[in] opts Borrowed resolved options with conversion q/Q/f/F/e/E/g/G; not retained or modified.
 * @return Owned text released with string_free(), or NULL for NULL options, an unsupported conversion or allocation failure.
 * @note q/Q follows qf_sprintf conventions; decimal modes round exact stored components to nearest, ties to even.
 * Applies resolved padding after conversion and retains no shared mutable state.
 */
string_t *qfi_format_value_text(qfloat_t x, const qfi_format_options_t *opts);

/**
 * @brief Render the exact sum of binary components using fixed, scientific or general decimal notation.
 * @param value Native scalar, passed by value; never narrowed or parsed through text.
 * @param options Borrowed f/F/e/E/g/G options; precision -1 selects six, otherwise require zero through 10000.
 * Width, left alignment and zero padding are handled by the caller; sign and alternate-form flags are honoured.
 * @return Owned unpadded core released with string_free(), or NULL for unsupported options or allocation failure.
 * @note Rounds the exact component sum to nearest with ties to even. Uses bounded native binary64 scratch storage;
 * no configurable numeric backend is invoked, and independent callers share no mutable state.
 */
string_t *qfi_format_decimal_text(qfloat_t value, const qfi_format_options_t *options);

/**
 * @brief Extract the exact unsigned decimal coefficient of two finite binary64 components.
 * @param value Finite native scalar; no numeric backend or text parsing is used.
 * @param[out] digits Required caller-owned buffer of at least 1621 bytes, receiving a NUL-terminated coefficient.
 * @param[out] point Required destination for the decimal point position relative to the first digit.
 * @param[out] negative Required destination for the sign, preserving an input signed zero.
 * @return Coefficient digit count, always positive. Allocates nothing and retains no inputs; independent calls are safe.
 */
int qfi_decimal_components(qfloat_t value, char *digits, int *point, bool *negative);

#endif
