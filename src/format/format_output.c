/**
 * @file format_output.c
 * @brief Render immutable templates and deliver checked string, builder and stream output.
 *
 * Field access is by compiled argument index, with layout verification and no repeated template parsing or
 * name lookup. Appends stage the complete result so formatting failures preserve the caller's destination.
 */
#include <limits.h>
#include <string.h>

#include "format-t.h"
#define MARS_FORMAT_INTERNAL_ACCESS
#include "format_internal.h"

/* Render a compiled template without parsing names, options or template syntax again. */
string_t *fmt_render(const fmt_t *format, const fmt_arg_t *arguments, size_t count, fmt_error_t *error)
{
    fmt_error_clear(error);
    if (!format || count != format->arguments || (count && !arguments)) {
        fmt_error_set(error, FMT_ERROR_ARGUMENT, SIZE_MAX, SIZE_MAX, "compiled argument count does not match");
        return NULL;
    }
    string_t *out = string_new();
    if (!out) {
        fmt_error_set(error, FMT_ERROR_MEMORY, SIZE_MAX, SIZE_MAX, "cannot allocate formatted output");
        return NULL;
    }
    for (size_t i = 0; i < format->count; ++i) {
        const fmt_part_t *part = &format->parts[i];
        const char *bytes = string_c_str(format->text) + part->offset;
        size_t length = part->length;
        string_t *core = NULL;
        if (part->argument != SIZE_MAX) {
            fmt_arg_impl_t arg = fmt_arg_unpack(&arguments[part->argument]);
            const fmt_layout_t *layout = &format->layout[part->argument];
            const char *name = layout->name ? string_c_str(layout->name) : NULL;
            if ((arg.kind == FMT_KIND_ADAPTER && arg.value.adapter.type != layout->adapter) ||
                arg.kind != layout->kind || !!name != !!arg.name || (name && strcmp(name, arg.name))) {
                fmt_error_set(error, FMT_ERROR_TYPE, part->offset, part->argument, "compiled binding layout changed");
                goto fail;
            }
            core = fmt_render_field(&arg, part, error);
            if (!core)
                goto fail;
            bytes = string_c_str(core);
            length = string_byte_length(core);
        }
        if (length > (size_t)INT_MAX - string_byte_length(out)) {
            fmt_error_set(error, FMT_ERROR_RANGE, part->offset, part->argument, "formatted output exceeds INT_MAX");
            string_free(core);
            goto fail;
        }
        int appended = string_append_bytes(out, bytes, length);
        string_free(core);
        if (appended != 0) {
            fmt_error_set(error, FMT_ERROR_MEMORY, part->offset, part->argument, "cannot grow formatted output");
            goto fail;
        }
    }
    fmt_error_clear(error);
    return out;

fail:
    string_free(out);
    return NULL;
}

/* Create a one-shot formatted string while keeping the compiled implementation reusable. */
string_t *fmt_string_args(const char *pattern, const fmt_arg_t *arguments, size_t count, fmt_error_t *error)
{
    fmt_t *format = fmt_compile(pattern, arguments, count, error);
    string_t *out = format ? fmt_render(format, arguments, count, error) : NULL;
    fmt_free(format);
    return out;
}

/* Stage formatted text before changing an existing string or builder. */
int fmt_append_args(string_t *out, const char *pattern, const fmt_arg_t *arguments, size_t count, fmt_error_t *error)
{
    if (!out) {
        fmt_error_set(error, FMT_ERROR_ARGUMENT, SIZE_MAX, SIZE_MAX, "append destination is required");
        return -1;
    }
    string_t *text = fmt_string_args(pattern, arguments, count, error);
    if (!text)
        return -1;
    size_t length = string_byte_length(text);
    int status = string_append_bytes(out, string_c_str(text), length);
    string_free(text);
    if (status != 0) {
        fmt_error_set(error, FMT_ERROR_MEMORY, SIZE_MAX, SIZE_MAX, "cannot append formatted output");
        return -1;
    }
    return (int)length;
}

/* Render completely before starting stream output, then report short writes as checked I/O errors. */
int fmt_fprint_args(FILE *stream, const char *pattern, const fmt_arg_t *arguments, size_t count, fmt_error_t *error)
{
    if (!stream) {
        fmt_error_set(error, FMT_ERROR_ARGUMENT, SIZE_MAX, SIZE_MAX, "output stream is required");
        return -1;
    }
    string_t *text = fmt_string_args(pattern, arguments, count, error);
    if (!text)
        return -1;
    size_t length = string_byte_length(text);
    size_t written = fwrite(string_c_str(text), 1u, length, stream);
    bool failed = ferror(stream) != 0;
    string_free(text);
    if (written != length || failed) {
        fmt_error_set(error, FMT_ERROR_IO, SIZE_MAX, SIZE_MAX, "cannot write complete formatted output");
        return -1;
    }
    return (int)length;
}
