/**
 * @file qfloat_format_adapter.c
 * @brief Module-owned native real adapter for typed brace formatting.
 *
 * Captures native values in the formatter's bounded opaque payload and validates this module's presentation
 * vocabulary. Rendering delegates directly to the structured native formatter, preserving precision and borrowing
 * rules without generated format strings, a global registry or dependencies on unrelated mathematical adapters.
 */
#include <math.h>
#include <string.h>

#include "format-t.h"
#include "qfloat-t.h"
#define MARS_SHARED_QFLOAT_FORMAT_INTERNAL_ACCESS
#include "internal/qfloat_format_internal.h"

_Static_assert(sizeof(qfloat_t) <= FMT_ADAPTER_CAPACITY, "Native value exceeds formatting adapter capacity.");

static bool qfi_fmt_validate(const fmt_options_t *options, fmt_error_t *error)
{
    const char *style = options->presentation;
    if (options->excel)
        return true;
    if ((*style && (style[1] || !strchr("qQfFeEgG", *style)))) {
        if (error)
            *error = (fmt_error_t){FMT_ERROR_TYPE, SIZE_MAX, SIZE_MAX, "unsupported qfloat presentation or flags"};
        return false;
    }
    return true;
}

static string_t *qfi_fmt_render(const void *data, const fmt_options_t *options, fmt_error_t *error)
{
    (void)error;
    qfloat_t value;
    memcpy(&value, data, sizeof(value));
    if (options->excel) {
        if ((!isfinite(value.hi) || !isfinite(value.lo)))
            return fmt_excel_real(options, qf_to_double(value), error);
        char digits[1621];
        int point;
        bool negative;
        qfi_decimal_components(value, digits, &point, &negative);
        fmt_options_t native = *options;
        native.precision = 32;
        return fmt_excel_decimal(&native, digits, point, negative, error);
    }
    const char *style = options->presentation;
    qfi_format_options_t native = {.conversion = *style ? *style : 'q', .precision = options->precision,
                                   .flag_hash = options->alternate};
    return qfi_format_value_text(value, &native);
}

static const fmt_adapter_t qfi_fmt_adapter = {
    .validate = qfi_fmt_validate,
    .render = qfi_fmt_render,
    .numeric = true,
};

/* Capture native real data with this module's immutable formatting adapter. */
fmt_arg_t fmt_arg_qfloat(qfloat_t value)
{
    return fmt_arg_adapter(&qfi_fmt_adapter, &value, sizeof(value));
}
