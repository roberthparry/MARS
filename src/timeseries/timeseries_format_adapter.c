/**
 * @file timeseries_format_adapter.c
 * @brief Native time-series presentations for typed interpolation.
 *
 * Borrows a series and delegates inline, pretty and CSV rendering to its owning module. Three checked style slots
 * form a minimal perfect hash using the third byte; formatting does not evaluate models or copy observations.
 * Empty readable series have an empty representation and CSV output retains the native header.
 */
#include <string.h>

#include "format-t.h"
#include "timeseries-t.h"

static int ts_fmt_style(const char *name)
{
    static const struct { const char *name; ts_string_style_t style; } styles[3] = {
        {"inline", TS_STRING_INLINE},
        {"csv",    TS_STRING_CSV},
        {"pretty", TS_STRING_PRETTY},
    };
    if (!*name)
        return TS_STRING_INLINE;
    if (strlen(name) < 3u)
        return -1;
    size_t slot = (unsigned char)name[2] % 3u;
    return strcmp(name, styles[slot].name) ? -1 : (int)styles[slot].style;
}

static bool ts_fmt_validate(const fmt_options_t *options, fmt_error_t *error)
{
    if (ts_fmt_style(options->presentation) < 0 || options->precision >= 0) {
        if (error)
            *error = (fmt_error_t){FMT_ERROR_TYPE, SIZE_MAX, SIZE_MAX, "unsupported time-series presentation or precision"};
        return false;
    }
    return true;
}

static string_t *ts_fmt_render(const void *data, const fmt_options_t *options, fmt_error_t *error)
{
    const timeseries_t *value;
    memcpy(&value, data, sizeof(value));
    if (!value) {
        if (error)
            *error = (fmt_error_t){FMT_ERROR_ARGUMENT, SIZE_MAX, SIZE_MAX, "time-series pointer is NULL"};
        return NULL;
    }
    ts_string_style_t style = (ts_string_style_t)ts_fmt_style(options->presentation);
    return style != TS_STRING_CSV && !ts_length(value) ? string_new() : ts_to_text(value, style);
}

static const fmt_adapter_t ts_fmt_adapter = {.validate = ts_fmt_validate, .render = ts_fmt_render, .numeric = false};

/* Capture a borrowed series without acquiring any observations or indices. */
fmt_arg_t fmt_arg_timeseries(const timeseries_t *value)
{
    return fmt_arg_adapter(&ts_fmt_adapter, &value, sizeof(value));
}
