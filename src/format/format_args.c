/**
 * @file format_args.c
 * @brief Typed interpolation bindings and thread-local formatting diagnostics.
 *
 * Constructors copy scalar data and borrow object resources. They do not render values or transfer ownership.
 * The format engine shares exact UTF-8 copying and diagnostic publication through its private interface.
 */
#include <string.h>

#include "format-t.h"
#define MARS_FORMAT_INTERNAL_ACCESS
#include "format_internal.h"

_Static_assert(sizeof(fmt_arg_impl_t) <= sizeof(fmt_arg_t), "Argument representation exceeds opaque storage.");
_Static_assert(_Alignof(fmt_arg_impl_t) <= _Alignof(fmt_arg_t), "Argument storage alignment is insufficient.");

/* Keep packing local so constructors can fold the byte copy into their returned value. */
static fmt_arg_t fmt_arg_pack(fmt_arg_impl_t value)
{
    fmt_arg_t argument = {0};
    memcpy(argument.storage, &value, sizeof(value));
    return argument;
}

fmt_arg_impl_t fmt_arg_unpack(const fmt_arg_t *argument)
{
    fmt_arg_impl_t value;
    memcpy(&value, argument->storage, sizeof(value));
    return value;
}

static _Thread_local fmt_error_t fmt_error = {FMT_OK, SIZE_MAX, SIZE_MAX, "success"};

void fmt_error_clear(fmt_error_t *error)
{
    fmt_error = (fmt_error_t){FMT_OK, SIZE_MAX, SIZE_MAX, "success"};
    if (error)
        *error = fmt_error;
}

bool fmt_error_set(fmt_error_t *error, fmt_code_t code, size_t offset, size_t argument, const char *message)
{
    fmt_error = (fmt_error_t){code, offset, argument, message};
    if (error)
        *error = fmt_error;
    return false;
}

string_t *fmt_copy_text(const char *text)
{
    string_t *copy = text ? string_new() : NULL;
    if (!text) {
        fmt_error_set(NULL, FMT_ERROR_ARGUMENT, SIZE_MAX, SIZE_MAX, "text input is NULL");
        return NULL;
    }
    if (!copy || string_append_bytes(copy, text, strlen(text)) != 0) {
        string_free(copy);
        fmt_error_set(NULL, FMT_ERROR_MEMORY, SIZE_MAX, SIZE_MAX, "cannot allocate text output");
        return NULL;
    }
    if (!string_is_valid_utf8(copy)) {
        string_free(copy);
        fmt_error_set(NULL, FMT_ERROR_ARGUMENT, SIZE_MAX, SIZE_MAX, "text input is not valid UTF-8");
        return NULL;
    }
    return copy;
}

/* Borrow the current thread's latest formatting diagnostic. */
const fmt_error_t *fmt_last_error(void)
{
    return &fmt_error;
}

/* Capture an ordinary signed scalar without allocating. */
fmt_arg_t fmt_arg_signed(intmax_t value)
{
    return fmt_arg_pack((fmt_arg_impl_t){.kind = FMT_KIND_SIGNED, .value.signed_value = value});
}

/* Capture an ordinary unsigned scalar without allocating. */
fmt_arg_t fmt_arg_unsigned(uintmax_t value)
{
    return fmt_arg_pack((fmt_arg_impl_t){.kind = FMT_KIND_UNSIGNED, .value.unsigned_value = value});
}

/* Preserve a truth value's textual default. */
fmt_arg_t fmt_arg_bool(bool value)
{
    return fmt_arg_pack((fmt_arg_impl_t){.kind = FMT_KIND_BOOL, .value.boolean = value});
}

/* Preserve the source precision of a float. */
fmt_arg_t fmt_arg_float(float value)
{
    return fmt_arg_pack((fmt_arg_impl_t){.kind = FMT_KIND_FLOAT, .value.real = value});
}

/* Preserve the source precision of a double. */
fmt_arg_t fmt_arg_double(double value)
{
    return fmt_arg_pack((fmt_arg_impl_t){.kind = FMT_KIND_DOUBLE, .value.real = value});
}

/* Preserve a long double without narrowing. */
fmt_arg_t fmt_arg_long_double(long double value)
{
    return fmt_arg_pack((fmt_arg_impl_t){.kind = FMT_KIND_LONG_DOUBLE, .value.real = value});
}

/* Borrow NUL-terminated text until the formatting call completes. */
fmt_arg_t fmt_arg_cstring(const char *value)
{
    return fmt_arg_pack((fmt_arg_impl_t){.kind = FMT_KIND_CSTRING, .value.cstring = value});
}

/* Borrow a native string without retaining it. */
fmt_arg_t fmt_arg_string(const string_t *value)
{
    return fmt_arg_pack((fmt_arg_impl_t){.kind = FMT_KIND_STRING, .value.string = value});
}

/* Copy a non-owning native text view. */
fmt_arg_t fmt_arg_view(string_view_t value)
{
    return fmt_arg_pack((fmt_arg_impl_t){.kind = FMT_KIND_VIEW, .value.view = value});
}

/* Copy a non-owning user-visible rune. */
fmt_arg_t fmt_arg_rune(rune_t value)
{
    return fmt_arg_pack((fmt_arg_impl_t){.kind = FMT_KIND_RUNE, .value.rune = value});
}

/* Store an address for display without dereferencing it. */
fmt_arg_t fmt_arg_pointer(const void *value)
{
    return fmt_arg_pack((fmt_arg_impl_t){.kind = FMT_KIND_POINTER, .value.pointer = value});
}

/* Capture a module-owned adapter and its bounded inline payload without retaining the source address. */
fmt_arg_t fmt_arg_adapter(const fmt_adapter_t *adapter, const void *value, size_t size)
{
    if (!adapter || !adapter->render || size > FMT_ADAPTER_CAPACITY || (size && !value))
        return (fmt_arg_t){0};
    fmt_arg_impl_t argument = {.kind = FMT_KIND_ADAPTER, .value.adapter.type = adapter};
    if (size)
        memcpy(argument.value.adapter.data, value, size);
    return fmt_arg_pack(argument);
}

/* Borrow a user-defined object and renderer. */
fmt_arg_t fmt_arg_custom(const char *name, const void *value, fmt_custom_fn render)
{
    return fmt_arg_pack((fmt_arg_impl_t){.name = name, .kind = FMT_KIND_CUSTOM, .value.custom = {value, render}});
}

/* Replace a descriptor's name without modifying or evaluating its payload. */
fmt_arg_t fmt_named(const char *name, fmt_arg_t value)
{
    memcpy(value.storage + offsetof(fmt_arg_impl_t, name), &name, sizeof(name));
    return value;
}
