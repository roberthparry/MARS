/**
 * @file TeX_format_adapter.c
 * @brief Module-owned typed formatting of immutable mathematical notation.
 *
 * Borrows TeX_t objects and copies their native source through the public renderer, without evaluating mathematics
 * or adding math-mode delimiters. The formatter owns the returned text and applies ordinary text padding.
 */
#include <string.h>

#include "format-t.h"
#include "TeX-t.h"

static bool TeX_fmt_validate(const fmt_options_t *options, fmt_error_t *error)
{
    if ((*options->presentation && strcmp(options->presentation, "latex")) || options->precision >= 0) {
        if (error)
            *error = (fmt_error_t){FMT_ERROR_TYPE, SIZE_MAX, SIZE_MAX, "unsupported notation presentation or precision"};
        return false;
    }
    return true;
}

static string_t *TeX_fmt_render(const void *data, const fmt_options_t *options, fmt_error_t *error)
{
    (void)options;
    const TeX_t *notation;
    memcpy(&notation, data, sizeof(notation));
    if (!notation) {
        if (error)
            *error = (fmt_error_t){FMT_ERROR_ARGUMENT, SIZE_MAX, SIZE_MAX, "notation pointer is NULL"};
        return NULL;
    }
    return TeX_render(notation);
}

static const fmt_adapter_t TeX_fmt_adapter = {
    .validate = TeX_fmt_validate,
    .render = TeX_fmt_render,
    .numeric = false,
};

/* Borrow immutable notation for its native source renderer. */
fmt_arg_t fmt_arg_TeX(const TeX_t *value)
{
    return fmt_arg_adapter(&TeX_fmt_adapter, &value, sizeof(value));
}
