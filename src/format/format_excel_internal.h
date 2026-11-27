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
    FX_LONG_TIME      /**< MARS long 24-hour time convention. */
} fmt_excel_kind_t;

/** @brief One source span with a resolved presentation role. */
typedef struct {
    fmt_excel_kind_t kind; /**< Direct rendering category. */
    size_t start; /**< Byte offset into the owned code. */
    size_t length; /**< Byte count, or the count of repeated date letters. */
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
    int shift; /**< Decimal scaling from percent and trailing commas. */
    size_t decimal; /**< Decimal token index, or SIZE_MAX. */
    size_t exponent; /**< Scientific exponent token index, or SIZE_MAX. */
    size_t slash; /**< Fraction slash token index, or SIZE_MAX. */
    size_t first_digit; /**< First numeric placeholder index, or SIZE_MAX. */
    size_t last_digit; /**< Last numeric placeholder index, or SIZE_MAX. */
    unsigned whole; /**< Count of integer placeholders. */
    unsigned fractional; /**< Count of decimal places. */
} fmt_excel_section_t;

/** @brief Immutable Excel custom code; template ownership includes all tokens and comparison values. */
struct _fmt_excel_t {
    char *code; /**< Owned exact UTF-8 code. */
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
 * @param code Borrowed NUL-terminated presentation.
 * @return True for a custom-code spelling; false for ordinary presentation. Retains nothing and cannot fail.
 */
bool fmt_excel_detect(const char *code);

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
