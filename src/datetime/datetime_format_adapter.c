/**
 * @file datetime_format_adapter.c
 * @brief Module-owned civil datetime bindings for typed interpolation.
 *
 * Borrows native datetime values and delegates presentation to the calendar module. ISO, date and time modes
 * provide predictable defaults. Case-sensitive date codes use M for months and m for minutes; excel: selects
 * Excel's contextual month/minute interpretation.
 * Accessors may materialise the datetime's lazy calendar caches, so shared mutable values require synchronisation.
 */
#include <string.h>

#include "format-t.h"
#include "datetime-t.h"

static const char *datetime_fmt_pattern(const char *style)
{
    if (!*style || !strcmp(style, "iso") || !strcmp(style, "datetime"))
        return "%yyyy-%mm-%ddT@Hh:@mm:@ss";
    if (!strcmp(style, "date"))
        return "%yyyy-%mm-%dd";
    if (!strcmp(style, "time"))
        return "@Hh:@mm:@ss";
    if (!strcmp(style, "d"))
        return "%d";
    if (!strcmp(style, "m"))
        return "@m";
    if (!strcmp(style, "M"))
        return "%m";
    if (!strcmp(style, "y"))
        return "%yy";
    if (!strcmp(style, "h"))
        return "@Hh";
    if (!strcmp(style, "s"))
        return "@s";
    return NULL;
}

static bool datetime_fmt_validate(const fmt_options_t *options, fmt_error_t *error)
{
    if ((!options->excel && !datetime_fmt_pattern(options->presentation)) || options->precision >= 0) {
        if (error)
            *error = (fmt_error_t){FMT_ERROR_TYPE, SIZE_MAX, SIZE_MAX, "unsupported datetime presentation or precision"};
        return false;
    }
    return true;
}

static string_t *datetime_fmt_render(const void *data, const fmt_options_t *options, fmt_error_t *error)
{
    const datetime_t *value;
    memcpy(&value, data, sizeof(value));
    if (!value) {
        if (error)
            *error = (fmt_error_t){FMT_ERROR_ARGUMENT, SIZE_MAX, SIZE_MAX, "datetime pointer is NULL"};
        return NULL;
    }
    if (options->excel)
        return fmt_excel_datetime(options, datetime_year(value), (unsigned)datetime_month(value), datetime_day(value),
                                   datetime_hour(value), datetime_minute(value), datetime_second(value), error);
    string_t *pattern = string_new_with(datetime_fmt_pattern(options->presentation));
    string_t *out = pattern ? datetime_format_text(value, pattern) : NULL;
    string_free(pattern);
    return out;
}

static const fmt_adapter_t datetime_fmt_adapter = {
    .validate = datetime_fmt_validate,
    .render = datetime_fmt_render,
    .numeric = false,
};

/* Borrow a civil datetime for the module's native calendar renderer. */
fmt_arg_t fmt_arg_datetime(const datetime_t *value)
{
    return fmt_arg_adapter(&datetime_fmt_adapter, &value, sizeof(value));
}
