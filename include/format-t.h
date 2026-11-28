/**
 * @file format-t.h
 * @brief Typed brace formatting, named interpolation and reusable output templates.
 *
 * Use this module to format C values and text with automatic, indexed or named fields.
 * FMT(value) captures a value's type and source name once; FMT(name, value) supplies an explicit label.
 * fmt_named() accepts a binding name supplied as a C string at runtime.
 * Formatting creates string_t output, appends to strings or builders, or writes to FILE streams.
 * Compiled templates own their text and binding layout and use direct argument indices during rendering.
 *
 * Argument descriptors borrow strings, views, runes and custom objects. They never acquire or release
 * those resources; keep the referenced values alive and unchanged during a call. Primitive scalars and adapter
 * payloads are copied into fixed-size storage. Release output with string_free() and templates with fmt_free().
 * Templates are immutable and shareable between threads using independent arguments and reentrant custom
 * callbacks. Diagnostics are thread-local. Module-owned adapters provide additional types without coupling this engine
 * to their representations. Each participating type header enables its own FMT() selection without an aggregate
 * integration header.
 *
 * Excel custom codes provide positive/negative/zero/text sections, conditions, terminal colours, decimal,
 * scientific and fraction fields, and Gregorian date/time presentations. Codes are compiled once with the
 * enclosing template. English calendar names and explicit decimal/grouping conventions do not depend on the
 * host locale; locale tags retain their currency symbols without importing external locale data.
 * Native date codes use M for months and m for minutes; an explicit excel: prefix selects Excel's contextual rules.
 */
#ifndef MARS_FORMAT_H
#define MARS_FORMAT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "string-t.h"

/** @brief Immutable compiled template; release its owned text and metadata with fmt_free(). */
typedef struct _fmt_t fmt_t;

/** @brief Formatting result category; detailed failures also identify a template byte offset and argument. */
typedef enum {
    FMT_OK,              /**< Operation succeeded. */
    FMT_ERROR_ARGUMENT,  /**< Required input or an argument value is invalid. */
    FMT_ERROR_MEMORY,    /**< Required output or metadata allocation failed. */
    FMT_ERROR_SYNTAX,    /**< Template syntax or encoding is invalid. */
    FMT_ERROR_MISSING,   /**< A referenced argument or binding does not exist. */
    FMT_ERROR_AMBIGUOUS, /**< A referenced name identifies more than one binding. */
    FMT_ERROR_TYPE,      /**< A presentation or compiled layout is incompatible with the supplied type. */
    FMT_ERROR_RANGE,     /**< An index, option or encoded output exceeds its supported range. */
    FMT_ERROR_IO         /**< A stream could not accept the complete output. */
} fmt_code_t;

/** @brief Non-owning diagnostic copied to callers or returned from thread-local storage. */
typedef struct {
    fmt_code_t code; /**< FMT_OK on success, otherwise the failure category. */
    size_t offset; /**< Zero-based template byte offset, or SIZE_MAX when unrelated to a template field. */
    size_t argument; /**< Zero-based argument index, or SIZE_MAX when no argument has been identified. */
    const char *message; /**< Static diagnostic text; never free or modify it. */
} fmt_error_t;

/** @brief Immutable compiled Excel custom format, owned by its enclosing template. */
typedef struct _fmt_excel_t fmt_excel_t;

/** @brief Parsed field options supplied to native and custom renderers; all pointers are borrowed. */
typedef struct {
    char alignment; /**< '<', '>', '^' or '=', or zero for the type's default alignment. */
    char fill; /**< Single ASCII padding character, normally a space. */
    char sign; /**< '+', space or '-' (the default); applies to numerical values. */
    bool alternate; /**< Request an alternate numeric representation, for example a base prefix. */
    bool zero; /**< Request sign-aware zero padding when no alignment is specified. */
    size_t width; /**< Minimum width in user-visible string characters, at most INT_MAX. */
    int precision; /**< Precision from zero through 10000, or -1 for the type's default. */
    const char *presentation; /**< NUL-terminated presentation name, or an empty string for the default. */
    const fmt_excel_t *excel; /**< Borrowed compiled Excel custom code, or NULL for ordinary presentation. */
} fmt_options_t;

/**
 * @brief Produce the unpadded core representation of a custom argument.
 * @param[in] value Borrowed caller-owned object, possibly NULL when the callback supports it.
 * @param[in] options Borrowed parsed options, valid only during the callback; implement supported presentation options.
 * @param[out] error Optional destination for failure details; set code and message when returning NULL.
 * @return Owned text released by the formatter with string_free(), or NULL on failure.
 * @note Do not retain options or acquire ownership of value. Callbacks used concurrently must be reentrant.
 * The engine applies alignment and padding to the returned core; the callback handles precision and numeric flags.
 */
typedef string_t *(*fmt_custom_fn)(const void *value, const fmt_options_t *options, fmt_error_t *error);

/** @brief Maximum copied adapter payload in bytes; capturing values never allocates additional storage. */
#define FMT_ADAPTER_CAPACITY 40u

/**
 * @brief Immutable module-owned formatting adapter and compiled-layout type identity.
 *
 * Use one distinct static const instance per supported type. The adapter must remain alive and unchanged while
 * any descriptor or compiled template refers to it. Callbacks must be reentrant when used concurrently.
 * Rendering receives the copied payload bytes, not a pointer to the original captured value. Decode those bytes
 * with memcpy into a correctly typed local value; do not cast them to a typed pointer or retain their address.
 */
typedef struct {
    /**
     * @brief Validate a field's type-specific presentation and options without accessing its value.
     * @param options Borrowed parsed options, valid only during this call.
     * @param[out] error Optional caller-owned diagnostic; use static message text on failure.
     * @return True when supported, or false on failure; an unset failure code becomes FMT_ERROR_TYPE.
     * @note NULL accepts all presentations after the core's numeric-option checks. Retains no inputs.
     */
    bool (*validate)(const fmt_options_t *options, fmt_error_t *error);
    fmt_custom_fn render; /**< Required core renderer; returns owned text released by the engine with string_free(). */
    bool numeric; /**< Apply numerical default alignment, sign insertion and sign-aware zero padding. */
} fmt_adapter_t;

/**
 * @brief Render a decimal coefficient using a field's compiled Excel custom code.
 * @param options Borrowed options with non-NULL excel, valid throughout this call.
 * @param digits Required unsigned decimal coefficient, containing only ASCII digits and at least one digit.
 * @param point Decimal point position relative to the first coefficient digit; 123 with point 1 means 1.23.
 * @param negative Sign bit, retained for signed zero in a single numeric section.
 * @param[out] error Optional diagnostic destination; failures use static messages.
 * @return Owned text released with string_free(), or NULL for invalid input, unsupported range or allocation failure.
 * @note No numeric value is parsed or narrowed. Local scratch is released; independent calls are reentrant.
 */
string_t *fmt_excel_decimal(const fmt_options_t *options, const char *digits, int point, bool negative, fmt_error_t *error);

/**
 * @brief Render a C real using a compiled Excel custom code without narrowing its precision.
 * @param options Borrowed options with non-NULL excel.
 * @param value Copied native real, including signed zero and non-finite values.
 * @param[out] error Optional diagnostic destination.
 * @return Owned string released with string_free(), or NULL on range or allocation failure. Reentrant.
 */
string_t *fmt_excel_real(const fmt_options_t *options, long double value, fmt_error_t *error);

/**
 * @brief Render Gregorian civil components using a field's Excel date or time code.
 * @param options Borrowed options with non-NULL excel.
 * @param year Gregorian year from 1 through 9999.
 * @param month Month from 1 through 12.
 * @param day Valid day within the month.
 * @param hour Hour from 0 through 23.
 * @param minute Minute from 0 through 59.
 * @param second Seconds including a fractional part, from 0 inclusive to 60 exclusive.
 * @param[out] error Optional diagnostic destination.
 * @return Owned string released with string_free(), or NULL for invalid components or allocation failure.
 * @note Uses English calendar names and MARS conventions independently of process locale. No inputs are retained.
 */
string_t *fmt_excel_datetime(const fmt_options_t *options, int year, unsigned month, unsigned day,
                             unsigned hour, unsigned minute, double second, fmt_error_t *error);

/**
 * @brief Find the coefficient precision needed by a compiled Excel field.
 * @param options Borrowed options, possibly NULL or without an Excel plan.
 * @return Decimal digit budget including guard digits, between 64 and 10064; 64 without a plan.
 * @note This is an adapter conversion budget, not a limit on exact integer input. Allocates nothing and is reentrant.
 */
size_t fmt_excel_precision(const fmt_options_t *options);

/**
 * @brief Opaque by-value binding; construct with FMT() or fmt_arg_*() and rename with fmt_named().
 *
 * Descriptors can be copied, returned and stored in arrays without allocation or release. Referenced resources
 * remain borrowed. The fixed-size storage permits these value semantics; its bytes are private implementation
 * data and must not be inspected, modified or serialised. A zero-initialised descriptor is invalid.
 */
typedef struct _fmt_arg_t fmt_arg_t;

/** @brief Sized public storage for an otherwise private binding representation. */
struct _fmt_arg_t {
    _Alignas(max_align_t) unsigned char storage[64]; /**< Opaque storage; use only the public constructors. */
};

/** @brief Capture an unnamed signed integer. @param value Copied integer. @return Non-owning by-value descriptor. */
fmt_arg_t fmt_arg_signed(intmax_t value);

/** @brief Capture an unnamed unsigned integer. @param value Copied integer. @return Non-owning by-value descriptor. */
fmt_arg_t fmt_arg_unsigned(uintmax_t value);

/** @brief Capture an unnamed truth value. @param value Copied truth value. @return Non-owning by-value descriptor. */
fmt_arg_t fmt_arg_bool(bool value);

/** @brief Capture an unnamed float. @param value Copied scalar. @return Descriptor retaining float default precision. */
fmt_arg_t fmt_arg_float(float value);

/** @brief Capture an unnamed double. @param value Copied scalar. @return Descriptor retaining double default precision. */
fmt_arg_t fmt_arg_double(double value);

/** @brief Capture an unnamed long double. @param value Copied scalar. @return Descriptor retaining its full precision. */
fmt_arg_t fmt_arg_long_double(long double value);

/** @brief Borrow C text. @param value Borrowed UTF-8 text or NULL. @return Descriptor valid while value remains alive. */
fmt_arg_t fmt_arg_cstring(const char *value);

/** @brief Borrow a string. @param value Borrowed string or NULL. @return Descriptor valid while value remains alive. */
fmt_arg_t fmt_arg_string(const string_t *value);

/** @brief Borrow a view. @param value Borrowed view. @return Descriptor valid while its backing text is unchanged. */
fmt_arg_t fmt_arg_view(string_view_t value);

/** @brief Borrow a rune. @param value Borrowed rune. @return Descriptor valid while its backing text is unchanged. */
fmt_arg_t fmt_arg_rune(rune_t value);

/** @brief Capture an address. @param value Pointer or NULL, never dereferenced. @return Non-owning address descriptor. */
fmt_arg_t fmt_arg_pointer(const void *value);

/**
 * @brief Copy an extension value into an opaque binding with a module-owned adapter.
 * @param adapter Borrowed immutable adapter with a renderer; must outlive the descriptor and compiled templates.
 * @param value Borrowed source bytes, copied before return; required when size is non-zero.
 * @param size Payload byte count, at most FMT_ADAPTER_CAPACITY; unused storage is zero-filled.
 * @return Unnamed allocation-free descriptor, or an invalid zero descriptor for an oversized payload, missing
 * source bytes, NULL adapter or missing renderer. Does not change the thread-local diagnostic.
 * @note Copied pointers and shallow handles retain their original borrowing rules; no resources are acquired.
 */
fmt_arg_t fmt_arg_adapter(const fmt_adapter_t *adapter, const void *value, size_t size);

/**
 * @brief Borrow a custom object with its renderer.
 * @param name Borrowed binding name or NULL; retained only in the descriptor.
 * @param value Borrowed object passed unchanged to render.
 * @param render Callback required when the argument is rendered; NULL causes a checked error.
 * @return Non-owning by-value descriptor, valid while its name, object and callback remain available.
 */
fmt_arg_t fmt_arg_custom(const char *name, const void *value, fmt_custom_fn render);

/**
 * @brief Assign an explicit name to a typed value, including values supplied as C expressions.
 * @param name Borrowed NUL-terminated name, or NULL for an unnamed positional value.
 * @param value Copied descriptor; referenced data remains borrowed.
 * @return Renamed descriptor; no allocation, failure, evaluation or ownership transfer occurs.
 */
fmt_arg_t fmt_named(const char *name, fmt_arg_t value);

/** @brief Internal real constructor selection; the qfloat header enables its own association. */
#define MARS_FMT_SELECT_QF_(value, next) next

/** @brief Internal complex constructor selection; the qcomplex header enables its own association. */
#define MARS_FMT_SELECT_QC_(value, next) next

/** @brief Internal numerical constructor selection; the number header enables its own association. */
#define MARS_FMT_SELECT_NUM_(value, next) next

/** @brief Internal matrix constructor selection; the matrix header enables its own association. */
#define MARS_FMT_SELECT_MAT_(value, next) next

/** @brief Internal expression constructor selection; the expression header enables its own association. */
#define MARS_FMT_SELECT_EXPR_(value, next) next

/* Preserve an owning-header association registered before this formatting interface was included. */
#ifndef MARS_FMT_SELECT_DATETIME_
/** @brief Internal datetime constructor selection, registered by datetime-t.h in either include order. */
#define MARS_FMT_SELECT_DATETIME_(value, next) next
#endif

/** @brief Internal equation constructor selection, enabled by equation-t.h. */
#define MARS_FMT_SELECT_EQU_(value, next) next

/** @brief Internal notation constructor selection, enabled by TeX-t.h. */
#define MARS_FMT_SELECT_TeX_(value, next) next

/* Preserve an owning-header association registered before this interface. */
#ifndef MARS_FMT_SELECT_TS_
/** @brief Internal time-series constructor selection, registered by timeseries-t.h. */
#define MARS_FMT_SELECT_TS_(value, next) next
#endif

/** @brief Compose the enabled constructor selections; inactive hooks add no type or runtime dependency. */
#define MARS_FMT_EXTENDED_(value) \
    MARS_FMT_SELECT_QF_(value, MARS_FMT_SELECT_QC_(value, MARS_FMT_SELECT_NUM_(value, \
    MARS_FMT_SELECT_MAT_(value, MARS_FMT_SELECT_EXPR_(value, MARS_FMT_SELECT_DATETIME_(value, \
    MARS_FMT_SELECT_EQU_(value, MARS_FMT_SELECT_TeX_(value, MARS_FMT_SELECT_TS_(value, fmt_arg_pointer)))))))))

/**
 * @brief Select a descriptor constructor for a supported C expression.
 * @param value Supported expression, evaluated once by its selected constructor.
 * @return Non-owning descriptor; unsupported C types produce a compiler diagnostic.
 */
#define MARS_FMT_VALUE_(value) _Generic((value), \
    char: fmt_arg_signed, signed char: fmt_arg_signed, short: fmt_arg_signed, int: fmt_arg_signed, \
    long: fmt_arg_signed, long long: fmt_arg_signed, unsigned char: fmt_arg_unsigned, \
    unsigned short: fmt_arg_unsigned, unsigned int: fmt_arg_unsigned, unsigned long: fmt_arg_unsigned, \
    unsigned long long: fmt_arg_unsigned, bool: fmt_arg_bool, float: fmt_arg_float, double: fmt_arg_double, \
    long double: fmt_arg_long_double, char *: fmt_arg_cstring, const char *: fmt_arg_cstring, \
    string_t *: fmt_arg_string, const string_t *: fmt_arg_string, string_view_t: fmt_arg_view, rune_t: fmt_arg_rune, \
    void *: fmt_arg_pointer, const void *: fmt_arg_pointer, default: MARS_FMT_EXTENDED_(value))((value))

/**
 * @brief Capture an expression using its original source spelling as the binding name.
 * @param name Stringified source spelling supplied by FMT(), borrowed for the descriptor lifetime.
 * @param value Supported expression, evaluated once.
 * @return Non-owning named descriptor with the original expression's type.
 */
#define MARS_FMT_CAPTURE_(name, value) fmt_named((name), MARS_FMT_VALUE_(value))

/**
 * @brief Capture an explicitly labelled expression without evaluating the label.
 * @param name Stringified label supplied by FMT(), borrowed for the descriptor lifetime.
 * @param label Original label tokens, discarded without evaluation or requiring a declared C variable.
 * @param value Supported expression, evaluated once.
 * @return Non-owning named descriptor with the supplied expression's type.
 */
#define MARS_FMT_BIND_(name, label, value) fmt_named((name), MARS_FMT_VALUE_(value))

/**
 * @brief Select the one-expression or explicit-label capture form without inspecting the value's tokens.
 * @param form Selected internal capture macro.
 * @param ... Remaining form candidates, discarded.
 * @return The selected capture macro; no C expression is evaluated.
 */
#define MARS_FMT_FORM_(form, ...) form

/**
 * @brief Capture a supported value once, using either its source name or an explicit label.
 * @param name_or_value A value expression for FMT(value), or an unevaluated label token for FMT(name, value).
 * @param ... Optional supported C value or native value enabled by including its owning type header.
 * @return Non-owning fmt_arg_t; unsupported C types produce a compiler diagnostic.
 * @note Use FMT(name, expression) for calculated values and fmt_named() for runtime C-string names. Original
 * label spelling is preserved without macro expansion. These macros provide C type selection and name capture.
 */
#define FMT(name_or_value, ...) \
    MARS_FMT_FORM_(__VA_OPT__(MARS_FMT_BIND_,) MARS_FMT_CAPTURE_)( \
        #name_or_value, name_or_value __VA_OPT__(,) __VA_ARGS__)

/**
 * @brief Compile a template and resolve its fields against a typed argument layout.
 * @param pattern Borrowed NUL-terminated UTF-8 template; braces escape as {{ and }}.
 * @param arguments Borrowed argument array, or NULL only when count is zero; values are not retained.
 * @param count Number of arguments in the layout.
 * @param[out] error Optional diagnostic destination, also reflected by fmt_last_error().
 * @return Owned immutable template released with fmt_free(), or NULL on syntax, layout, range or allocation failure.
 * @note Copies referenced binding names and types. Automatic {} fields cannot mix with explicit numeric indices.
 * Named fields may accompany either mode. Width and precision are literal numbers, not nested replacement fields.
 */
fmt_t *fmt_compile(const char *pattern, const fmt_arg_t *arguments, size_t count, fmt_error_t *error);

/**
 * @brief Release a compiled template and its owned text and metadata.
 * @param format Owned template, invalid after return; NULL is accepted. Never release during another thread's render.
 */
void fmt_free(fmt_t *format);

/**
 * @brief Render a compiled template using direct argument indices.
 * @param format Borrowed immutable template, valid throughout the call.
 * @param arguments Borrowed values with the names, types and ordering used at compilation; not retained or modified.
 * @param count Argument count, matching the compiled layout.
 * @param[out] error Optional diagnostic destination, also reflected by fmt_last_error().
 * @return Owned string released with string_free(), or NULL on changed layout, rendering or allocation failure.
 * @note Rendering performs no template parsing or name lookup. Borrowed objects and callbacks must be safe for any
 * concurrent use; each call owns its output and uses thread-local diagnostics.
 */
string_t *fmt_render(const fmt_t *format, const fmt_arg_t *arguments, size_t count, fmt_error_t *error);

/**
 * @brief Compile and render one template into a new string.
 * @param pattern Borrowed UTF-8 template.
 * @param arguments Borrowed typed bindings, or NULL when count is zero; values are evaluated before this call.
 * @param count Number of bindings.
 * @param[out] error Optional diagnostic destination, also reflected by fmt_last_error().
 * @return Owned string released with string_free(), or NULL on compilation or rendering failure; no inputs are retained.
 */
string_t *fmt_string_args(const char *pattern, const fmt_arg_t *arguments, size_t count, fmt_error_t *error);

/**
 * @brief Append formatted text atomically to an existing string or string_builder_t.
 * @param[in,out] out Required borrowed destination, unchanged if formatting fails.
 * @param pattern Borrowed UTF-8 template.
 * @param arguments Borrowed typed bindings, or NULL when count is zero.
 * @param count Number of bindings.
 * @param[out] error Optional diagnostic destination, also reflected by fmt_last_error().
 * @return Encoded byte count appended, or -1 on failure or output exceeding INT_MAX. Retains no inputs.
 */
int fmt_append_args(string_t *out, const char *pattern, const fmt_arg_t *arguments, size_t count, fmt_error_t *error);

/**
 * @brief Write a complete formatted result to a stream after successful rendering.
 * @param stream Required borrowed stream; not closed or retained.
 * @param pattern Borrowed UTF-8 template.
 * @param arguments Borrowed typed bindings, or NULL when count is zero.
 * @param count Number of bindings.
 * @param[out] error Optional diagnostic destination, also reflected by fmt_last_error().
 * @return Encoded byte count written, or -1 on failure or output exceeding INT_MAX. I/O failure may leave partial output.
 * @note Formatting errors write nothing. Concurrent stream use follows the stream's ordinary locking rules.
 */
int fmt_fprint_args(FILE *stream, const char *pattern, const fmt_arg_t *arguments, size_t count, fmt_error_t *error);

/**
 * @brief Read the latest formatting diagnostic on the current thread.
 * @return Borrowed thread-local record, valid until the next compile, render or output operation on this thread.
 * Never free it; copy the record if it must survive later calls. Constructors and fmt_free() do not change it.
 */
const fmt_error_t *fmt_last_error(void);

/** @brief Internal typed argument-array construction; its leading sentinel permits calls with no bindings. */
#define MARS_FMT_ARGUMENTS_(...) ((const fmt_arg_t[]){{.storage = {0}}, __VA_ARGS__} + 1)

/** @brief Internal argument count; sizeof does not evaluate any binding expressions a second time. */
#define MARS_FMT_COUNT_(...) (sizeof((const fmt_arg_t[]){{.storage = {0}}, __VA_ARGS__}) / sizeof(fmt_arg_t) - 1u)

/**
 * @brief Create owned text from a template and zero or more FMT()/explicit descriptors.
 * @param pattern Borrowed template, evaluated once.
 * @param ... Typed bindings, each evaluated once; resources remain borrowed for the call.
 * @return Owned string released with string_free(), or NULL; inspect fmt_last_error() on failure.
 */
#define fmt_string(pattern, ...) \
    fmt_string_args((pattern), MARS_FMT_ARGUMENTS_(__VA_ARGS__), MARS_FMT_COUNT_(__VA_ARGS__), NULL)

/**
 * @brief Append a template and zero or more typed bindings to a string or builder.
 * @param out Borrowed destination, evaluated once.
 * @param pattern Borrowed template, evaluated once.
 * @param ... Typed bindings, evaluated once each.
 * @return Bytes appended or -1; inspect fmt_last_error(). Formatting failure preserves the destination.
 */
#define fmt_append(out, pattern, ...) \
    fmt_append_args((out), (pattern), MARS_FMT_ARGUMENTS_(__VA_ARGS__), MARS_FMT_COUNT_(__VA_ARGS__), NULL)

/**
 * @brief Print a template and zero or more typed bindings to a FILE stream.
 * @param stream Borrowed stream, evaluated once.
 * @param pattern Borrowed template, evaluated once.
 * @param ... Typed bindings, evaluated once each.
 * @return Bytes written or -1; inspect fmt_last_error(). I/O failure may leave partial output.
 */
#define fmt_fprint(stream, pattern, ...) \
    fmt_fprint_args((stream), (pattern), MARS_FMT_ARGUMENTS_(__VA_ARGS__), MARS_FMT_COUNT_(__VA_ARGS__), NULL)

/**
 * @brief Print a template and zero or more typed bindings to stdout.
 * @param pattern Borrowed template, evaluated once; include a newline explicitly when required.
 * @param ... Typed bindings, evaluated once each.
 * @return Bytes written or -1; inspect fmt_last_error(). I/O failure may leave partial output.
 */
#define fmt_print(pattern, ...) \
    fmt_fprint_args(stdout, (pattern), MARS_FMT_ARGUMENTS_(__VA_ARGS__), MARS_FMT_COUNT_(__VA_ARGS__), NULL)

#endif
