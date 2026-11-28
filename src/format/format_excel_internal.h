/**
 * @file format_excel_internal.h
 * @brief Private Excel custom-code compiler and rendering contracts.
 *
 * Shares immutable section and token metadata between the format module's number and calendar renderers.
 * Type-owning adapters use the public coefficient and civil-component APIs in format-t.h instead.
 */
#ifndef MARS_FORMAT_EXCEL_INTERNAL_H
#define MARS_FORMAT_EXCEL_INTERNAL_H
#if !defined(MARS_FORMAT_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0)
#error "Private header requires MARS_FORMAT_INTERNAL_ACCESS."
#endif

#include "format-t.h"

/** @brief Token kinds; compiler dispatch translates source characters into this closed vocabulary. */
typedef enum {
    FX_LITERAL,        /**< Exact UTF-8 source span. */
    FX_DIGIT,          /**< Mandatory, optional or space-reserving numeric placeholder. */
    FX_DOT,            /**< Decimal or fractional-second separator. */
    FX_COMMA,          /**< Grouping or scaling directive. */
    FX_PERCENT,        /**< Percentage scaling and visible percent sign. */
    FX_EXPONENT,       /**< Scientific exponent marker and sign policy. */
    FX_SLASH,          /**< Fraction separator. */
    FX_TEXT,           /**< Borrowed text insertion. */
    FX_GENERAL,        /**< General numerical presentation. */
    FX_SPACE,          /**< Reserve one visible character. */
    FX_FILL,           /**< Repeat a visible character to the supplied field width. */
    FX_YEAR,           /**< Gregorian year field. */
    FX_MONTH,          /**< Gregorian month number or English name. */
    FX_DAY,            /**< Day number or English weekday name. */
    FX_DAY_ORDINAL,    /**< English ordinal day, optionally with a superscript suffix. */
    FX_HOUR,           /**< Hour within a day or twelve-hour period. */
    FX_MINUTE,         /**< Contextually resolved minute within an hour. */
    FX_SECOND,         /**< Second within a minute. */
    FX_AMPM,           /**< Twelve-hour period marker. */
    FX_ELAPSED_HOUR,   /**< Total elapsed hours without wrapping. */
    FX_ELAPSED_MINUTE, /**< Total elapsed minutes without wrapping. */
    FX_ELAPSED_SECOND, /**< Total elapsed seconds without wrapping. */
    FX_LONG_DATE,      /**< MARS long Gregorian date convention. */
    FX_LONG_TIME,      /**< MARS long 24-hour time convention. */
    FX_SUBSECOND,      /**< Resolved decimal separator and following fractional-second placeholders. */
    FX_INTEGER_DIGIT,  /**< Resolved decimal/scientific integer placeholder. */
    FX_FRACTION_DIGIT, /**< Resolved decimal/scientific fractional placeholder. */
    FX_KIND_COUNT      /**< Token-table size; never emitted by the compiler. */
} fmt_excel_kind_t;

/** @brief Presentation properties resolved by the lexer rather than re-read from source during rendering. */
typedef enum {
    FX_REQUIRED = 1u,         /**< A zero placeholder must display a digit. */
    FX_RESERVED = 2u,         /**< A question-mark placeholder reserves a space when omitted. */
    FX_UPPER_PERIOD = 4u,     /**< Emit upper-case AM/PM text. */
    FX_SUPERSCRIPT = 8u,      /**< Raise the ordinal suffix. */
    FX_POSITIVE_EXPONENT = 16u, /**< Include an explicit positive exponent sign. */
    FX_DATE_CONTEXT = 32u,    /**< Participates in Excel's neighbouring date-field lookup. */
    FX_HOUR_CONTEXT = 64u,    /**< An hour field makes a following short month field denote minutes. */
    FX_SECOND_CONTEXT = 128u, /**< A second field makes a preceding short month field denote minutes. */
    FX_UPPER_NAME = 256u,     /**< Upper-case English calendar names and ordinary ordinal suffixes. */
    FX_LOWER_NAME = 512u      /**< Lower-case English calendar names and ordinary ordinal suffixes. */
} fmt_excel_flags_t;

/** @brief One source span with a resolved presentation role. */
typedef struct {
    fmt_excel_kind_t kind; /**< Direct rendering category. */
    size_t start; /**< Byte offset into the owned code. */
    size_t length; /**< Byte count, or the count of repeated date letters. */
    size_t next; /**< Next token index after any compile-time fusion of placeholder runs. */
    unsigned flags; /**< Resolved fmt_excel_flags_t properties. */
} fmt_excel_token_t;

/** @brief One of at most four independently selected Excel format sections. */
typedef struct {
    size_t first; /**< First token index. */
    size_t count; /**< Number of tokens. */
    int condition; /**< Zero or comparison code: 1 <, 2 <=, 3 =, 4 <>, 5 >=, 6 >. */
    char *threshold; /**< Owned unsigned normalised comparison digits, or NULL. */
    int threshold_point; /**< Decimal point position of the comparison value. */
    bool threshold_negative; /**< Comparison value sign. */
    char colour[32]; /**< Optional ANSI SGR prefix, including its terminating NUL. */
    bool date; /**< Contains calendar or elapsed-time fields. */
    bool ampm; /**< Uses twelve-hour time. */
    bool general; /**< Contains the General keyword. */
    bool text; /**< Contains a text insertion placeholder. */
    bool grouping; /**< Contains a grouping comma before the last integer placeholder. */
    bool has_seconds; /**< Contains seconds; omitted seconds must not round another date or time field. */
    bool name_case_set; /**< A native calendar-name case directive was encountered during compilation. */
    unsigned name_case; /**< Current compile-time name case, copied into subsequent tokens. */
    int shift; /**< Decimal scaling from percent and trailing commas. */
    size_t decimal; /**< Decimal token index, or SIZE_MAX. */
    size_t exponent; /**< Scientific exponent token index, or SIZE_MAX. */
    size_t slash; /**< Fraction slash token index, or SIZE_MAX. */
    size_t first_digit; /**< First numeric placeholder index, or SIZE_MAX. */
    size_t last_digit; /**< Last numeric placeholder index, or SIZE_MAX. */
    unsigned whole; /**< Count of integer placeholders. */
    unsigned fractional; /**< Count of decimal places. */
    unsigned minimum_fraction; /**< Position after the last mandatory fractional placeholder. */
    unsigned exponent_digits; /**< Mandatory exponent digit count, resolved once during compilation. */
    unsigned second_precision; /**< Maximum fractional-second precision, resolved once during compilation. */
} fmt_excel_section_t;

/** @brief Immutable Excel custom code; template ownership includes all tokens and comparison values. */
struct _fmt_excel_t {
    char *code; /**< Owned exact UTF-8 code. */
    size_t length; /**< Owned code's byte length, captured once. */
    fmt_excel_token_t *tokens; /**< Owned contiguous token array. */
    size_t count; /**< Initialised token count. */
    unsigned sections; /**< Section count from one through four. */
    bool excel_compatibility; /**< Explicit excel: mode retains case-insensitive, contextual month/minute rules. */
    fmt_excel_section_t section[4]; /**< Directly indexed section metadata. */
};

/** @brief Call-local position of a repeat-fill field, measured before terminal colour is added. */
typedef struct {
    const fmt_excel_token_t *token; /**< Borrowed repeat character, or NULL when absent. */
    size_t offset; /**< Byte insertion position within the uncoloured output. */
} fmt_excel_fill_t;

/**
 * @brief Distinguish Excel codes from ordinary native presentation names.
 * @param code Borrowed UTF-8 presentation span; need not be NUL-terminated.
 * @param length Exact byte count of the presentation span.
 * @return True for a custom-code spelling; false for ordinary presentation. Retains nothing and cannot fail.
 */
bool fmt_excel_detect(const char *code, size_t length);

/**
 * @brief Fold one ASCII byte without consulting the process locale.
 * @param byte Input byte; non-ASCII bytes are unchanged.
 * @return Lower-case ASCII byte or the original byte. Allocates nothing and cannot fail.
 */
static inline unsigned char fmt_excel_lower(unsigned char byte) {
    return byte >= 'A' && byte <= 'Z' ? (unsigned char)(byte + ('a' - 'A')) : byte;
}

/**
 * @brief Compare ASCII text case-insensitively within a bounded prefix.
 * @param a Borrowed text with length readable bytes or an earlier NUL terminator.
 * @param b Borrowed text with the same readable-byte contract.
 * @param length Maximum byte count to compare.
 * @return Zero for an equal prefix, otherwise a signed byte difference. Retains nothing and is reentrant.
 */
int fmt_excel_ascii_compare(const char *a, const char *b, size_t length);

/**
 * @brief Tokenise the next section through directly indexed byte handlers.
 * @param[in,out] plan Owned-in-progress plan with enough token capacity for its source bytes.
 * @param[in,out] section Current section receiving tokens and presentation properties.
 * @param[in,out] position Required byte cursor, left at a semicolon or the terminating NUL on success.
 * @param[out] error Optional diagnostic destination.
 * @return True on success; false on syntax or allocation failure. Retains no extra resources beyond plan ownership.
 */
bool fmt_excel_lex_section(fmt_excel_t *plan, fmt_excel_section_t *section, size_t *position, fmt_error_t *error);

/**
 * @brief Read a coefficient digit with implicit zero extension.
 * @param digits Borrowed ASCII coefficient containing length readable digits.
 * @param length Coefficient digit count.
 * @param index Signed position relative to the coefficient's first digit.
 * @return The indexed digit, or ASCII zero outside the coefficient. No ownership transfers or failures occur.
 */
static inline char fmt_excel_digit_at(const char *digits, size_t length, int index) {
    return index >= 0 && (size_t)index < length ? digits[index] : '0';
}

/**
 * @brief Round a mutable coefficient to nearest with halves away from zero.
 * @param[in,out] digits Caller-owned NUL-terminated coefficient, modified in place.
 * @param[in,out] length Required coefficient length, updated after rounding.
 * @param[in,out] point Required decimal point position, adjusted for a carry.
 * @param keep Number of significant coefficient digits to retain; may be negative for a small fixed-point value.
 * @note Allocates nothing, retains no storage and cannot fail for a valid coefficient buffer.
 */
void fmt_excel_round(char *digits, size_t *length, int *point, int keep);

/**
 * @brief Append an unsigned decimal integer with a minimum digit width.
 * @param[in,out] out Borrowed output string.
 * @param value Integer to append.
 * @param minimum Minimum digit count, bounded by the internal 32-digit scratch capacity.
 * @return True on success, false on allocation failure. Retains no inputs and transfers no ownership.
 */
bool fmt_excel_append_unsigned(string_t *out, uint64_t value, size_t minimum);

/**
 * @brief Render fixed or scientific fields through the compiled token dispatch table.
 * @param[in,out] out Borrowed destination.
 * @param plan Borrowed immutable source and tokens.
 * @param section Borrowed resolved numeric section.
 * @param[in,out] digits Mutable coefficient scratch, rounded in place and never retained.
 * @param length Coefficient digit count.
 * @param point Decimal point position before section scaling.
 * @param[in,out] fill Required call-local repeat-fill position.
 * @return True on success, false on output allocation failure. Transfers no ownership and is reentrant.
 */
bool fmt_excel_fixed(string_t *out, const fmt_excel_t *plan, const fmt_excel_section_t *section,
                     char *digits, size_t length, int point, fmt_excel_fill_t *fill);

/**
 * @brief Compile a complete custom code and validate its bounded section grammar.
 * @param code Borrowed UTF-8 code, copied before return.
 * @param[out] error Optional diagnostic destination.
 * @return Owned plan released with fmt_excel_free(), or NULL on syntax, range or allocation failure. Reentrant.
 */
fmt_excel_t *fmt_excel_compile(const char *code, fmt_error_t *error);

/** @brief Release a compiled plan. @param plan Owned plan or NULL; invalid after return. */
void fmt_excel_free(fmt_excel_t *plan);

/**
 * @brief Select and render a text section without interpreting input text as a format.
 * @param options Borrowed field options with an Excel plan.
 * @param value Borrowed UTF-8 value.
 * @return Owned string released with string_free(), or NULL on allocation failure. No inputs are retained.
 */
string_t *fmt_excel_text(const fmt_options_t *options, const string_t *value);

/**
 * @brief Append one literal, percent, spacing or repeat-fill token to an output.
 * @param[in,out] out Borrowed destination.
 * @param plan Borrowed source plan.
 * @param token Borrowed resolved token.
 * @param[in,out] fill Required call-local repeat position, initially zero-initialised.
 * @return True on success, false on allocation failure. Repeat fill is resolved by the field-width renderer.
 */
bool fmt_excel_literal(string_t *out, const fmt_excel_t *plan, const fmt_excel_token_t *token, fmt_excel_fill_t *fill);

/**
 * @brief Resolve repeat fill and apply a selected section's colour to owned rendered text.
 * @param text Owned uncoloured text, consumed even on failure.
 * @param plan Borrowed source plan.
 * @param section Borrowed section with an optional ANSI prefix.
 * @param options Borrowed options supplying the available field width.
 * @param fill Call-local repeat position, or a zero-initialised record without repeat fill.
 * @return Owned coloured text released with string_free(), or NULL on allocation failure. Reentrant.
 */
string_t *fmt_excel_finish(string_t *text, const fmt_excel_t *plan, const fmt_excel_section_t *section,
                           const fmt_options_t *options, fmt_excel_fill_t fill);

/**
 * @brief Render a serial value as an Excel calendar or elapsed time.
 * @param options Borrowed options with the compiled code and available field width.
 * @param section Borrowed selected section.
 * @param serial Finite serial day value, using the 1900 date system.
 * @param[out] error Optional diagnostic destination.
 * @return Owned uncoloured text released with string_free(), or NULL on range or allocation failure. Reentrant.
 */
string_t *fmt_excel_calendar(const fmt_options_t *options, const fmt_excel_section_t *section,
                            long double serial, fmt_error_t *error);

#endif
