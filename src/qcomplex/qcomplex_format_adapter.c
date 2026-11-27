/**
 * @file qcomplex_format_adapter.c
 * @brief Module-owned native complex adapter for typed brace formatting.
 *
 * Captures native values in the formatter's bounded opaque payload and validates this module's presentation
 * vocabulary. Rendering delegates directly to the structured native formatter, preserving precision and borrowing
 * rules without generated format strings, a global registry or dependencies on unrelated mathematical adapters.
 */
#include <math.h>
#include <string.h>

#include "format-t.h"
#include "qcomplex-t.h"
#define MARS_SHARED_QCOMPLEX_FORMAT_INTERNAL_ACCESS
#include "internal/qcomplex_format_internal.h"

_Static_assert(sizeof(qcomplex_t) <= FMT_ADAPTER_CAPACITY, "Native value exceeds formatting adapter capacity.");

static bool qci_fmt_validate(const fmt_options_t *options, fmt_error_t *error)
{
    const char *style = options->presentation;
    if (options->excel)
        return true;
    if ((*style && (style[1] || !strchr("qQfFeEgG", *style)))) {
        if (error)
            *error = (fmt_error_t){FMT_ERROR_TYPE, SIZE_MAX, SIZE_MAX, "unsupported qcomplex presentation or flags"};
        return false;
    }
    return true;
}

static string_t *qci_fmt_render(const void *data, const fmt_options_t *options, fmt_error_t *error)
{
    (void)error;
    qcomplex_t value;
    memcpy(&value, data, sizeof(value));
    if (options->excel) {
        if (!qf_eq(qc_imag(value), QF_ZERO)) {
            if (error)
                *error = (fmt_error_t){FMT_ERROR_TYPE, SIZE_MAX, SIZE_MAX,
                                      "Excel numeric codes require a real complex value"};
            return NULL;
        }
        qfloat_t scalar = qc_real(value);
        if ((!isfinite(scalar.hi) || !isfinite(scalar.lo)))
            return fmt_excel_real(options, qf_to_double(scalar), error);
        char digits[1621];
        int point;
        bool negative;
        qfi_decimal_components(scalar, digits, &point, &negative);
        fmt_options_t native = *options;
        native.precision = 32;
        return fmt_excel_decimal(&native, digits, point, negative, error);
    }
    const char *style = options->presentation;
    qfi_format_options_t native = {.conversion = *style ? *style : 'q', .precision = options->precision,
                                   .flag_hash = options->alternate};
    return qci_format_value_text(value, &native);
}

static const fmt_adapter_t qci_fmt_adapter = {
    .validate = qci_fmt_validate,
    .render = qci_fmt_render,
    .numeric = true,
};

/* Capture native complex data with this module's immutable formatting adapter. */
fmt_arg_t fmt_arg_qcomplex(qcomplex_t value)
{
    return fmt_arg_adapter(&qci_fmt_adapter, &value, sizeof(value));
}
