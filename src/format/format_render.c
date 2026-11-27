/**
 * @file format_render.c
 * @brief Typed scalar rendering, presentation validation and Unicode-aware field padding.
 *
 * Uses category-indexed primitive dispatch and module-owned adapter callbacks. Generated numbers use byte indexing;
 * user-visible widths and string precision use the public string module's grapheme semantics. Values never traverse an
 * unchecked heterogeneous argument list.
 */
#include <float.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "format-t.h"
#define MARS_FORMAT_INTERNAL_ACCESS
#include "format_internal.h"
#define MARS_FORMAT_INTERNAL_ACCESS
#include "format_excel_internal.h"

typedef string_t *(*fmt_scalar_fn)(const fmt_arg_impl_t *, const fmt_options_t *);

static bool fmt_is_numeric(fmt_kind_t kind, const fmt_options_t *options)
{
    if (options->presentation[0] == 'c')
        return false;
    bool boolean_number = kind == FMT_KIND_BOOL && options->presentation[0] && options->presentation[0] != 's';
    return boolean_number || kind == FMT_KIND_SIGNED || kind == FMT_KIND_UNSIGNED ||
           (kind >= FMT_KIND_FLOAT && kind <= FMT_KIND_LONG_DOUBLE) || kind == FMT_KIND_POINTER;
}

static string_t *fmt_text_value(const fmt_arg_impl_t *argument, const fmt_options_t *options)
{
    const string_t *source = argument->kind == FMT_KIND_STRING ? argument->value.string : NULL;
    string_t *text = NULL;
    if (argument->kind == FMT_KIND_CSTRING)
        text = fmt_copy_text(argument->value.cstring ? argument->value.cstring : "(null)");
    else if (argument->kind == FMT_KIND_STRING)
        text = source ? NULL : fmt_copy_text("(null)");
    else if (argument->kind == FMT_KIND_VIEW)
        text = string_from_view(&argument->value.view);
    else if (argument->kind == FMT_KIND_RUNE)
        text = rune_to_string(argument->value.rune);
    else if (argument->kind == FMT_KIND_BOOL)
        text = fmt_copy_text(argument->value.boolean ? "true" : "false");
    if (!source)
        source = text;
    if (source && options->precision >= 0) {
        if (!string_is_valid_utf8(source)) {
            string_free(text);
            fmt_error_set(NULL, FMT_ERROR_ARGUMENT, SIZE_MAX, SIZE_MAX, "text input is not valid UTF-8");
            return NULL;
        }
        string_t *shortened = string_substring(source, 0u, (size_t)options->precision);
        string_free(text);
        return shortened;
    }
    return text ? text : source ? string_clone(source) : NULL;
}

static string_t *fmt_character(uintmax_t scalar)
{
    char bytes[4];
    size_t count;
    if (scalar > 0x10ffffu || (scalar >= 0xd800u && scalar <= 0xdfffu))
        return NULL;
    if (scalar < 0x80u) {
        bytes[0] = (char)scalar;
        count = 1u;
    } else if (scalar < 0x800u) {
        bytes[0] = (char)(0xc0u | (scalar >> 6));
        bytes[1] = (char)(0x80u | (scalar & 0x3fu));
        count = 2u;
    } else if (scalar < 0x10000u) {
        bytes[0] = (char)(0xe0u | (scalar >> 12));
        bytes[1] = (char)(0x80u | ((scalar >> 6) & 0x3fu));
        bytes[2] = (char)(0x80u | (scalar & 0x3fu));
        count = 3u;
    } else {
        bytes[0] = (char)(0xf0u | (scalar >> 18));
        bytes[1] = (char)(0x80u | ((scalar >> 12) & 0x3fu));
        bytes[2] = (char)(0x80u | ((scalar >> 6) & 0x3fu));
        bytes[3] = (char)(0x80u | (scalar & 0x3fu));
        count = 4u;
    }
    string_t *text = string_new();
    if (text && string_append_bytes(text, bytes, count) != 0) {
        string_free(text);
        text = NULL;
    }
    return text;
}

static string_t *fmt_integer_value(const fmt_arg_impl_t *argument, const fmt_options_t *options)
{
    char conversion = options->presentation[0];
    bool negative = argument->kind == FMT_KIND_SIGNED && argument->value.signed_value < 0;
    uintmax_t magnitude = argument->kind == FMT_KIND_SIGNED ?
        (negative ? (uintmax_t)0 - (uintmax_t)argument->value.signed_value : (uintmax_t)argument->value.signed_value) :
        argument->kind == FMT_KIND_BOOL ? (uintmax_t)argument->value.boolean :
        argument->kind == FMT_KIND_POINTER ? (uintmax_t)(uintptr_t)argument->value.pointer : argument->value.unsigned_value;
    if (conversion == 'c')
        return negative ? NULL : fmt_character(magnitude);
    unsigned base = conversion == 'b' ? 2u : conversion == 'o' ? 8u :
                    conversion == 'x' || conversion == 'X' || argument->kind == FMT_KIND_POINTER ? 16u : 10u;
    const char *alphabet = conversion == 'X' ? "0123456789ABCDEF" : "0123456789abcdef";
    char digits[sizeof(uintmax_t) * CHAR_BIT];
    size_t count = 0;
    do {
        digits[count++] = alphabet[magnitude % base];
        magnitude /= base;
    } while (magnitude);
    string_t *text = string_new();
    if (!text)
        return NULL;
    if (negative && string_append_char(text, '-') != 0)
        goto fail;
    if ((options->alternate || argument->kind == FMT_KIND_POINTER) && base != 10u) {
        const char *prefix = base == 2u ? "0b" : base == 8u ? "0o" : conversion == 'X' ? "0X" : "0x";
        if (string_append_cstr(text, prefix) != 0)
            goto fail;
    }
    while (count) {
        if (string_append_char(text, digits[--count]) != 0)
            goto fail;
    }
    return text;

fail:
    string_free(text);
    return NULL;
}

static string_t *fmt_boolean_value(const fmt_arg_impl_t *argument, const fmt_options_t *options)
{
    return options->presentation[0] && options->presentation[0] != 's' ?
        fmt_integer_value(argument, options) : fmt_text_value(argument, options);
}

static string_t *fmt_real_value(const fmt_arg_impl_t *argument, const fmt_options_t *options)
{
    static const char *const ordinary[128] = {
        ['e'] = "%.*Le",
        ['E'] = "%.*LE",
        ['f'] = "%.*Lf",
        ['F'] = "%.*LF",
        ['g'] = "%.*Lg",
        ['G'] = "%.*LG",
    };
    static const char *const alternate[128] = {
        ['e'] = "%#.*Le",
        ['E'] = "%#.*LE",
        ['f'] = "%#.*Lf",
        ['F'] = "%#.*LF",
        ['g'] = "%#.*Lg",
        ['G'] = "%#.*LG",
    };
    unsigned conversion = options->presentation[0] ? (unsigned char)options->presentation[0] : 'g';
    int precision = options->precision;
    if (precision < 0) {
        if (options->presentation[0])
            precision = 6;
        else
            precision = argument->kind == FMT_KIND_FLOAT ? FLT_DECIMAL_DIG :
                        argument->kind == FMT_KIND_DOUBLE ? DBL_DECIMAL_DIG : LDBL_DECIMAL_DIG;
    }
    const char *format = options->alternate ? alternate[conversion] : ordinary[conversion];
    int needed = snprintf(NULL, 0u, format, precision, argument->value.real);
    if (needed < 0)
        return NULL;
    char *bytes = malloc((size_t)needed + 1u);
    if (!bytes)
        return NULL;
    snprintf(bytes, (size_t)needed + 1u, format, precision, argument->value.real);
    string_t *text = fmt_copy_text(bytes);
    free(bytes);
    return text;
}

typedef struct {
    fmt_scalar_fn render;
    const char *presentations;
} fmt_dispatch_t;

static const fmt_dispatch_t fmt_dispatch[FMT_KIND_COUNT] = {
    [FMT_KIND_SIGNED]      = {fmt_integer_value, "dboxXc"},
    [FMT_KIND_UNSIGNED]    = {fmt_integer_value, "dboxXc"},
    [FMT_KIND_BOOL]        = {fmt_boolean_value, "sdboxX"},
    [FMT_KIND_FLOAT]       = {fmt_real_value,    "fFeEgG"},
    [FMT_KIND_DOUBLE]      = {fmt_real_value,    "fFeEgG"},
    [FMT_KIND_LONG_DOUBLE] = {fmt_real_value,    "fFeEgG"},
    [FMT_KIND_CSTRING]     = {fmt_text_value,    "s"},
    [FMT_KIND_STRING]      = {fmt_text_value,    "s"},
    [FMT_KIND_VIEW]        = {fmt_text_value,    "s"},
    [FMT_KIND_RUNE]        = {fmt_text_value,    "s"},
    [FMT_KIND_POINTER]     = {fmt_integer_value, "p"},
};

bool fmt_validate(const fmt_layout_t *layout, const fmt_options_t *options, size_t offset, size_t argument, fmt_error_t *error)
{
    fmt_kind_t kind = layout->kind;
    if (kind <= FMT_KIND_NONE || kind >= FMT_KIND_COUNT)
        return fmt_error_set(error, FMT_ERROR_TYPE, offset, argument, "unsupported argument category");
    if (kind == FMT_KIND_CUSTOM)
        return true;
    const char *style = options->presentation;
    bool numeric = kind == FMT_KIND_ADAPTER ? layout->adapter->numeric : fmt_is_numeric(kind, options);
    if (!numeric && (options->sign != '-' || options->alternate || options->zero || options->alignment == '='))
        return fmt_error_set(error, FMT_ERROR_TYPE, offset, argument, "numeric options require a numeric argument");
    if (kind == FMT_KIND_ADAPTER) {
        fmt_error_t failure = {FMT_OK, SIZE_MAX, SIZE_MAX, NULL};
        if (layout->adapter->validate && !layout->adapter->validate(options, &failure))
            return fmt_error_set(error, failure.code == FMT_OK ? FMT_ERROR_TYPE : failure.code, offset, argument,
                                 failure.message ? failure.message : "adapter rejected field options");
        return true;
    }
    if (options->excel) {
        if (kind == FMT_KIND_POINTER)
            return fmt_error_set(error, FMT_ERROR_TYPE, offset, argument, "Excel codes do not format addresses");
        return true;
    }
    const char *allowed = fmt_dispatch[kind].presentations;
    if (*style && (style[1] || !strchr(allowed, *style)))
        return fmt_error_set(error, FMT_ERROR_TYPE, offset, argument, "presentation is incompatible with argument type");
    bool integer = kind == FMT_KIND_SIGNED || kind == FMT_KIND_UNSIGNED || kind == FMT_KIND_POINTER ||
                   (kind == FMT_KIND_BOOL && numeric);
    if (integer && options->precision >= 0)
        return fmt_error_set(error, FMT_ERROR_TYPE, offset, argument, "integer precision is not supported");
    if (kind == FMT_KIND_POINTER && (options->alternate || options->sign != '-'))
        return fmt_error_set(error, FMT_ERROR_TYPE, offset, argument, "numeric flag is unsupported for this type");
    if (integer && options->alternate && (!*style || *style == 'd'))
        return fmt_error_set(error, FMT_ERROR_TYPE, offset, argument, "alternate integer output requires b, o, x or X");
    return true;
}

static bool fmt_padding(string_t *out, char fill, size_t count)
{
    char block[64];
    memset(block, fill, sizeof(block));
    while (count) {
        size_t chunk = count < sizeof(block) ? count : sizeof(block);
        if (string_append_bytes(out, block, chunk) != 0)
            return false;
        count -= chunk;
    }
    return true;
}

static string_t *fmt_pad(string_t *core, bool numeric, const fmt_options_t *options, bool *range_error)
{
    if (!options->width)
        return core;
    size_t width = string_length(core);
    if (strchr(string_c_str(core), '\033')) {
        string_t *visible = string_new();
        const char *bytes = string_c_str(core);
        for (size_t i = 0u; visible && bytes[i];) {
            if (bytes[i] == '\033' && bytes[i + 1u] == '[') {
                size_t end = i + 2u;
                while ((bytes[end] >= '0' && bytes[end] <= '9') || bytes[end] == ';')
                    ++end;
                if (bytes[end] == 'm') {
                    i = end + 1u;
                    continue;
                }
            }
            if (string_append_bytes(visible, bytes + i++, 1u) != 0) {
                string_free(visible);
                visible = NULL;
            }
        }
        if (!visible) {
            string_free(core);
            return NULL;
        }
        width = string_length(visible);
        string_free(visible);
    }
    if (options->width <= width)
        return core;
    size_t pad = options->width - width;
    if (pad > (size_t)INT_MAX - string_byte_length(core)) {
        *range_error = true;
        string_free(core);
        return NULL;
    }
    char alignment = options->alignment ? options->alignment : numeric ? '>' : '<';
    char fill = options->fill;
    if (options->zero && fill == ' ') {
        fill = '0';
        if (!options->alignment)
            alignment = '=';
    }
    size_t left = alignment == '<' ? 0u : alignment == '^' ? pad / 2u : pad;
    size_t right = pad - left;
    size_t prefix = 0;
    const char *bytes = string_c_str(core);
    size_t length = string_byte_length(core);
    if (alignment == '=') {
        if (length && (bytes[0] == '-' || bytes[0] == '+' || bytes[0] == ' '))
            prefix = 1u;
        if (length >= prefix + 2u && bytes[prefix] == '0' && bytes[prefix + 1u] &&
            strchr("xXbo", bytes[prefix + 1u]))
            prefix += 2u;
    }
    string_t *out = string_new();
    if (out && (string_append_bytes(out, bytes, prefix) != 0 || !fmt_padding(out, fill, left) ||
                string_append_bytes(out, bytes + prefix, length - prefix) != 0 || !fmt_padding(out, fill, right))) {
        string_free(out);
        out = NULL;
    }
    string_free(core);
    return out;
}

string_t *fmt_render_field(const fmt_arg_impl_t *argument, const fmt_part_t *part, fmt_error_t *error)
{
    fmt_error_clear(error);
    const fmt_options_t *options = &part->options;
    bool numeric = argument->kind == FMT_KIND_ADAPTER ? argument->value.adapter.type->numeric :
                                                     fmt_is_numeric(argument->kind, options);
    string_t *core = NULL;
    if ((argument->kind == FMT_KIND_SIGNED || argument->kind == FMT_KIND_UNSIGNED) &&
        options->presentation[0] == 'c') {
        bool invalid = argument->kind == FMT_KIND_SIGNED && argument->value.signed_value < 0;
        uintmax_t scalar = argument->kind == FMT_KIND_SIGNED ? (uintmax_t)argument->value.signed_value :
                                                            argument->value.unsigned_value;
        if (invalid || scalar > 0x10ffffu || (scalar >= 0xd800u && scalar <= 0xdfffu)) {
            fmt_error_set(error, FMT_ERROR_ARGUMENT, part->offset, part->argument, "character is not a Unicode scalar");
            return NULL;
        }
    }
    if (argument->kind == FMT_KIND_RUNE && rune_is_none(argument->value.rune)) {
        fmt_error_set(error, FMT_ERROR_ARGUMENT, part->offset, part->argument, "rune is unset or outside its source");
        return NULL;
    }
    if (argument->kind == FMT_KIND_CUSTOM || argument->kind == FMT_KIND_ADAPTER) {
        fmt_error_t custom = {FMT_OK, SIZE_MAX, SIZE_MAX, NULL};
        fmt_custom_fn render = argument->kind == FMT_KIND_ADAPTER ? argument->value.adapter.type->render :
                                                                  argument->value.custom.render;
        const void *value = argument->kind == FMT_KIND_ADAPTER ? (const void *)argument->value.adapter.data :
                                                               argument->value.custom.value;
        if (render)
            core = render(value, options, &custom);
        if (!core) {
            if (custom.code == FMT_OK)
                custom = *fmt_last_error();
            fmt_code_t fallback = argument->kind == FMT_KIND_ADAPTER ? FMT_ERROR_MEMORY : FMT_ERROR_ARGUMENT;
            fmt_error_set(error, custom.code == FMT_OK ? fallback : custom.code, part->offset, part->argument,
                          custom.message ? custom.message : "argument formatter failed or is missing");
            return NULL;
        }
    } else if (options->excel) {
        if (argument->kind >= FMT_KIND_FLOAT && argument->kind <= FMT_KIND_LONG_DOUBLE) {
            fmt_options_t native = *options;
            native.precision = argument->kind == FMT_KIND_FLOAT ? FLT_DECIMAL_DIG :
                               argument->kind == FMT_KIND_DOUBLE ? DBL_DECIMAL_DIG : LDBL_DECIMAL_DIG;
            core = fmt_excel_real(&native, argument->value.real, error);
        } else if (argument->kind == FMT_KIND_SIGNED || argument->kind == FMT_KIND_UNSIGNED ||
                   argument->kind == FMT_KIND_BOOL) {
            fmt_options_t decimal = {.presentation = "d"};
            string_t *coefficient = fmt_integer_value(argument, &decimal);
            const char *digits = coefficient ? string_c_str(coefficient) : NULL;
            bool negative = digits && *digits == '-';
            if (negative)
                ++digits;
            if (digits)
                core = fmt_excel_decimal(options, digits, (int)strlen(digits), negative, error);
            string_free(coefficient);
        } else {
            string_t *value = fmt_text_value(argument, options);
            if (value)
                core = fmt_excel_text(options, value);
            string_free(value);
        }
    } else {
        core = fmt_dispatch[argument->kind].render(argument, options);
    }
    if (!core) {
        fmt_error_t failure = *fmt_last_error();
        fmt_error_set(error, failure.code == FMT_OK ? FMT_ERROR_MEMORY : failure.code,
                      part->offset, part->argument, failure.code == FMT_OK ? "cannot render argument" : failure.message);
        return NULL;
    }
    if (string_byte_length(core) > INT_MAX) {
        string_free(core);
        fmt_error_set(error, FMT_ERROR_RANGE, part->offset, part->argument, "rendered field exceeds INT_MAX");
        return NULL;
    }
    if (!string_is_valid_utf8(core)) {
        string_free(core);
        fmt_error_set(error, FMT_ERROR_ARGUMENT, part->offset, part->argument, "rendered value is not valid UTF-8");
        return NULL;
    }
    if (numeric && !options->excel && options->sign != '-' &&
        string_c_str(core)[0] != '-' && string_c_str(core)[0] != '+' && string_c_str(core)[0] != ' ') {
        char sign[2] = {options->sign, '\0'};
        if (string_insert(core, 0u, sign) != 0) {
            string_free(core);
            core = NULL;
        }
    }
    bool range_error = false;
    if (core)
        core = fmt_pad(core, numeric, options, &range_error);
    if (!core)
        fmt_error_set(error, range_error ? FMT_ERROR_RANGE : FMT_ERROR_MEMORY, part->offset, part->argument,
                      range_error ? "padded field exceeds INT_MAX" : "cannot allocate padded field output");
    return core;
}
