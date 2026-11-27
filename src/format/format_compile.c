/**
 * @file format_compile.c
 * @brief Compile UTF-8 brace templates and resolve typed named or positional bindings.
 *
 * Stores literal spans and parsed field options in an immutable sequence. Named bindings use the public
 * dictionary's hashed lookup during compilation; rendering subsequently indexes the supplied argument array.
 */
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "dictionary-t.h"
#include "format-t.h"
#define MARS_FORMAT_INTERNAL_ACCESS
#include "format_internal.h"
#define MARS_FORMAT_INTERNAL_ACCESS
#include "format_excel_internal.h"

static size_t fmt_name_hash(const void *key)
{
    const unsigned char *name = (const unsigned char *)*(const char *const *)key;
    size_t hash = (size_t)UINT64_C(14695981039346656037);
    while (*name) {
        hash ^= *name++;
        hash *= (size_t)UINT64_C(1099511628211);
    }
    return hash;
}

static int fmt_name_compare(const void *a, const void *b)
{
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

static string_t *fmt_compile_text(const char *text, size_t argument, fmt_error_t *error)
{
    string_t *copy = string_new();
    if (!copy || string_append_bytes(copy, text, strlen(text)) != 0) {
        string_free(copy);
        fmt_error_set(error, FMT_ERROR_MEMORY, SIZE_MAX, argument, "cannot copy template or binding name");
        return NULL;
    }
    if (!string_is_valid_utf8(copy)) {
        string_free(copy);
        fmt_error_set(error, argument == SIZE_MAX ? FMT_ERROR_SYNTAX : FMT_ERROR_ARGUMENT,
                      SIZE_MAX, argument, "template or binding name is not valid UTF-8");
        return NULL;
    }
    return copy;
}

static dictionary_t *fmt_names(const fmt_t *format)
{
    dictionary_t *names = dictionary_create(sizeof(const char *), sizeof(size_t), fmt_name_hash, fmt_name_compare,
                                            NULL, NULL, NULL, NULL, NULL);
    if (!names)
        return NULL;
    for (size_t i = 0; i < format->arguments; ++i) {
        if (!format->layout[i].name)
            continue;
        const char *name = string_c_str(format->layout[i].name);
        size_t previous;
        size_t index = dictionary_get(names, &name, &previous) ? SIZE_MAX : i;
        if (!dictionary_set(names, &name, &index)) {
            dictionary_destroy(names);
            return NULL;
        }
    }
    return names;
}

static bool fmt_push(fmt_t *format, fmt_part_t part, fmt_error_t *error)
{
    if (format->count == format->capacity) {
        size_t next = format->capacity ? format->capacity * 2u : 16u;
        if (next < format->capacity || next > SIZE_MAX / sizeof(*format->parts))
            return fmt_error_set(error, FMT_ERROR_RANGE, part.offset, part.argument, "too many template parts");
        fmt_part_t *parts = realloc(format->parts, next * sizeof(*parts));
        if (!parts)
            return fmt_error_set(error, FMT_ERROR_MEMORY, part.offset, part.argument, "cannot allocate template parts");
        format->parts = parts;
        format->capacity = next;
    }
    format->parts[format->count++] = part;
    return true;
}

static bool fmt_literal(fmt_t *format, size_t start, size_t length, fmt_error_t *error)
{
    return length == 0u || fmt_push(format, (fmt_part_t){.offset = start, .length = length, .argument = SIZE_MAX}, error);
}

static bool fmt_alignment(char ch)
{
    return ch == '<' || ch == '>' || ch == '^' || ch == '=';
}

static bool fmt_decimal(const char *text, size_t end, size_t *position, size_t limit, size_t *value)
{
    size_t number = 0;
    size_t start = *position;
    while (*position < end && text[*position] >= '0' && text[*position] <= '9') {
        unsigned digit = (unsigned)(text[*position] - '0');
        if (number > limit / 10u || (number == limit / 10u && digit > limit % 10u))
            return false;
        number = number * 10u + digit;
        ++*position;
    }
    *value = number;
    return *position != start;
}

static bool fmt_options(fmt_t *format, fmt_part_t *part, size_t start, size_t end, fmt_error_t *error)
{
    const char *text = string_c_str(format->text);
    size_t at = start;
    part->options = (fmt_options_t){.fill = ' ', .sign = '-', .precision = -1};
    string_t *code = string_substr(format->text, start, end - start);
    if (!code)
        return fmt_error_set(error, FMT_ERROR_MEMORY, at, part->argument, "cannot copy field specification");
    bool excel = fmt_excel_detect(string_c_str(code));
    string_free(code);
    if (excel) {
        if (at < end && (fmt_alignment(text[at]) ||
            (text[at] != '[' && at + 1u < end && fmt_alignment(text[at + 1u])))) {
            if (fmt_alignment(text[at]))
                part->options.alignment = text[at++];
            else {
                part->options.fill = text[at++];
                part->options.alignment = text[at++];
            }
            if (!fmt_decimal(text, end, &at, INT_MAX, &part->options.width) || at == end || text[at++] != ':')
                return fmt_error_set(error, FMT_ERROR_SYNTAX, at, part->argument,
                                     "Excel field alignment requires a width followed by a colon");
        }
        part->presentation = string_substr(format->text, at, end - at);
        if (!part->presentation)
            return fmt_error_set(error, FMT_ERROR_MEMORY, at, part->argument, "cannot copy Excel code");
        part->options.presentation = string_c_str(part->presentation);
        part->options.excel = fmt_excel_compile(part->options.presentation, error);
        if (!part->options.excel) {
            fmt_error_t failure = *fmt_last_error();
            return fmt_error_set(error, failure.code, part->offset, part->argument, failure.message);
        }
        return fmt_validate(&format->layout[part->argument], &part->options, part->offset, part->argument, error);
    }
    if (at + 1u < end && fmt_alignment(text[at + 1u])) {
        if ((unsigned char)text[at] < 32u || (unsigned char)text[at] > 126u)
            return fmt_error_set(error, FMT_ERROR_SYNTAX, at, part->argument, "fill must be a single ASCII character");
        part->options.fill = text[at++];
        part->options.alignment = text[at++];
    } else if (at < end && fmt_alignment(text[at])) {
        part->options.alignment = text[at++];
    }
    if (at < end && (text[at] == '+' || text[at] == '-' || text[at] == ' '))
        part->options.sign = text[at++];
    if (at < end && text[at] == '#') {
        part->options.alternate = true;
        ++at;
    }
    if (at < end && text[at] == '0') {
        part->options.zero = true;
        ++at;
    }
    if (at < end && text[at] >= '0' && text[at] <= '9') {
        if (!fmt_decimal(text, end, &at, INT_MAX, &part->options.width))
            return fmt_error_set(error, FMT_ERROR_RANGE, at, part->argument, "field width exceeds INT_MAX");
    }
    if (at < end && text[at] == '.') {
        size_t precision;
        ++at;
        if (at == end || text[at] < '0' || text[at] > '9')
            return fmt_error_set(error, FMT_ERROR_SYNTAX, at, part->argument, "precision requires decimal digits");
        if (!fmt_decimal(text, end, &at, 10000u, &precision))
            return fmt_error_set(error, FMT_ERROR_RANGE, at, part->argument, "precision exceeds 10000");
        part->options.precision = (int)precision;
    }
    for (size_t i = at; i < end; ++i) {
        if (!((text[i] >= 'a' && text[i] <= 'z') || (text[i] >= 'A' && text[i] <= 'Z') || text[i] == '_'))
            return fmt_error_set(error, FMT_ERROR_SYNTAX, i, part->argument, "invalid presentation name");
    }
    part->presentation = string_substr(format->text, at, end - at);
    if (!part->presentation)
        return fmt_error_set(error, FMT_ERROR_MEMORY, at, part->argument, "cannot allocate presentation name");
    part->options.presentation = string_c_str(part->presentation);
    return fmt_validate(&format->layout[part->argument], &part->options, part->offset, part->argument, error);
}

static bool fmt_selector(fmt_t *format, dictionary_t **names, size_t start, size_t end, size_t *automatic,
                          unsigned *mode, size_t *index, fmt_error_t *error)
{
    const char *text = string_c_str(format->text);
    if (start == end) {
        if (*mode == 2u)
            return fmt_error_set(error, FMT_ERROR_SYNTAX, start, SIZE_MAX, "cannot mix automatic and numeric fields");
        *mode = 1u;
        *index = (*automatic)++;
    } else {
        size_t position = start;
        bool numeric = true;
        for (size_t i = start; i < end; ++i)
            numeric = numeric && text[i] >= '0' && text[i] <= '9';
        if (numeric) {
            if (*mode == 1u)
                return fmt_error_set(error, FMT_ERROR_SYNTAX, start, SIZE_MAX, "cannot mix automatic and numeric fields");
            *mode = 2u;
            if (!fmt_decimal(text, end, &position, SIZE_MAX, index))
                return fmt_error_set(error, FMT_ERROR_RANGE, start, SIZE_MAX, "argument index is too large");
        } else {
            if (!*names)
                *names = fmt_names(format);
            if (!*names)
                return fmt_error_set(error, FMT_ERROR_MEMORY, start, SIZE_MAX, "cannot allocate binding lookup");
            string_t *key = string_substr(format->text, start, end - start);
            const char *name = key ? string_c_str(key) : NULL;
            bool allocated = key != NULL;
            bool found = name && dictionary_get(*names, &name, index);
            string_free(key);
            if (!allocated)
                return fmt_error_set(error, FMT_ERROR_MEMORY, start, SIZE_MAX, "cannot allocate binding name");
            if (!found)
                return fmt_error_set(error, FMT_ERROR_MISSING, start, SIZE_MAX, "unknown named binding");
            if (*index == SIZE_MAX)
                return fmt_error_set(error, FMT_ERROR_AMBIGUOUS, start, SIZE_MAX, "named binding is ambiguous");
        }
    }
    if (*index >= format->arguments)
        return fmt_error_set(error, FMT_ERROR_MISSING, start, *index, "argument index is outside the binding array");
    return true;
}

static bool fmt_scan(fmt_t *format, fmt_error_t *error)
{
    const char *text = string_c_str(format->text);
    size_t length = string_byte_length(format->text);
    size_t at = 0, literal = 0, automatic = 0;
    unsigned mode = 0;
    dictionary_t *names = NULL;
    bool ok = false;

    while (at < length) {
        char delimiter = text[at];
        if (delimiter != '{' && delimiter != '}') {
            ++at;
            continue;
        }
        if (!fmt_literal(format, literal, at - literal, error))
            goto done;
        if (at + 1u < length && text[at + 1u] == delimiter) {
            if (!fmt_literal(format, at, 1u, error))
                goto done;
            at += 2u;
            literal = at;
            continue;
        }
        if (delimiter == '}') {
            fmt_error_set(error, FMT_ERROR_SYNTAX, at, SIZE_MAX, "unmatched closing brace");
            goto done;
        }
        size_t close = at + 1u;
        size_t colon = SIZE_MAX;
        bool quoted = false;
        while (close < length) {
            if (colon != SIZE_MAX && text[close] == '\\' && close + 1u < length) {
                close += 2u;
                continue;
            }
            if (colon != SIZE_MAX && text[close] == '"')
                quoted = !quoted;
            if (quoted) {
                ++close;
                continue;
            }
            if (text[close] == '}')
                break;
            if (text[close] == '{') {
                fmt_error_set(error, FMT_ERROR_SYNTAX, close, SIZE_MAX, "nested fields are not supported");
                goto done;
            }
            if (text[close] == ':' && colon == SIZE_MAX)
                colon = close;
            ++close;
        }
        if (close == length) {
            fmt_error_set(error, FMT_ERROR_SYNTAX, at, SIZE_MAX, "unclosed replacement field");
            goto done;
        }
        size_t selector_end = colon == SIZE_MAX ? close : colon;
        fmt_part_t part = {.offset = at};
        if (!fmt_selector(format, &names, at + 1u, selector_end, &automatic, &mode, &part.argument, error))
            goto done;
        if (!fmt_options(format, &part, colon == SIZE_MAX ? close : colon + 1u, close, error)) {
            fmt_excel_free((fmt_excel_t *)part.options.excel);
            string_free(part.presentation);
            goto done;
        }
        if (!fmt_push(format, part, error)) {
            fmt_excel_free((fmt_excel_t *)part.options.excel);
            string_free(part.presentation);
            goto done;
        }
        at = close + 1u;
        literal = at;
    }
    ok = fmt_literal(format, literal, length - literal, error);

done:
    dictionary_destroy(names);
    return ok;
}

/* Copy a template and layout, then replace every field name with a checked argument index. */
fmt_t *fmt_compile(const char *pattern, const fmt_arg_t *arguments, size_t count, fmt_error_t *error)
{
    fmt_error_clear(error);
    if (!pattern || (count && !arguments)) {
        fmt_error_set(error, FMT_ERROR_ARGUMENT, SIZE_MAX, SIZE_MAX, "template and argument array are required");
        return NULL;
    }
    if (count > SIZE_MAX / sizeof(fmt_layout_t) || strlen(pattern) > INT_MAX) {
        fmt_error_set(error, FMT_ERROR_RANGE, SIZE_MAX, SIZE_MAX, "template or binding layout is too large");
        return NULL;
    }
    fmt_t *format = calloc(1u, sizeof(*format));
    if (!format) {
        fmt_error_set(error, FMT_ERROR_MEMORY, SIZE_MAX, SIZE_MAX, "cannot allocate compiled template");
        return NULL;
    }
    format->text = fmt_compile_text(pattern, SIZE_MAX, error);
    format->layout = count ? calloc(count, sizeof(*format->layout)) : NULL;
    format->arguments = count;
    if (!format->text)
        goto fail;
    if (count && !format->layout) {
        fmt_error_set(error, FMT_ERROR_MEMORY, SIZE_MAX, SIZE_MAX, "cannot copy UTF-8 template or binding layout");
        goto fail;
    }
    for (size_t i = 0; i < count; ++i) {
        fmt_arg_impl_t argument = fmt_arg_unpack(&arguments[i]);
        if (argument.kind <= FMT_KIND_NONE || argument.kind >= FMT_KIND_COUNT) {
            fmt_error_set(error, FMT_ERROR_TYPE, SIZE_MAX, i, "invalid argument descriptor category");
            goto fail;
        }
        format->layout[i].kind = argument.kind;
        if (argument.kind == FMT_KIND_ADAPTER) {
            const fmt_adapter_t *adapter = argument.value.adapter.type;
            if (!adapter || !adapter->render) {
                fmt_error_set(error, FMT_ERROR_TYPE, SIZE_MAX, i, "argument adapter has no renderer");
                goto fail;
            }
            format->layout[i].adapter = adapter;
        }
        if (argument.name) {
            format->layout[i].name = fmt_compile_text(argument.name, i, error);
            if (!format->layout[i].name)
                goto fail;
        }
    }
    if (!fmt_scan(format, error))
        goto fail;
    return format;

fail:
    fmt_free(format);
    return NULL;
}

/* Release all storage owned by a compiled template; borrowed argument resources are never touched. */
void fmt_free(fmt_t *format)
{
    if (!format)
        return;
    for (size_t i = 0; i < format->count; ++i) {
        fmt_excel_free((fmt_excel_t *)format->parts[i].options.excel);
        string_free(format->parts[i].presentation);
    }
    if (format->layout) {
        for (size_t i = 0; i < format->arguments; ++i)
            string_free(format->layout[i].name);
    }
    free(format->parts);
    free(format->layout);
    string_free(format->text);
    free(format);
}
