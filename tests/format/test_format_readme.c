/**
 * @file test_format_readme.c
 * @brief Executable README examples for the typed formatting guide.
 *
 * Keeps the guide's named, positional, string, builder, native, compiled, custom, error-handling and emoji programmes
 * synchronised with their documented output. The suite invokes these entry points only after ordinary checks.
 * Assertions verify results and ownership; output remains the same as the corresponding standalone programmes.
 */
#include <string.h>

#include "format-t.h"
#include "qfloat-t.h"
#include "qcomplex-t.h"
#include "number-t.h"
#include "datetime-t.h"
#include "timeseries-t.h"
#include "equation-t.h"
#include "TeX-t.h"
#include "test_harness.h"
#define MARS_TEST_FORMAT_INTERNAL_ACCESS
#include "test_format_internal.h"

/* README example: named automatic binding with native fixed decimal precision. */
void example_format_named(void)
{
    const char *name = "Ada";
    qfloat_t x = qf_from_double(1.25);
    int bytes = fmt_print("{name}: x={x:.3f}\n", FMT(name), FMT(x));
    TEST_ASSERT_TRUE(bytes == 13, "README named output byte count");
}

/* README example: compile a binding layout once and render changing values by direct argument index. */
void example_format_compiled(void)
{
    int count = 7;
    fmt_arg_t arguments[] = {FMT(count)};
    fmt_t *format = fmt_compile("count={count:04d}", arguments, 1u, NULL);
    count = 12;
    arguments[0] = FMT(count);
    string_t *text = fmt_render(format, arguments, 1u, NULL);
    if (text)
        fmt_print("{text}\n", FMT(text));
    TEST_ASSERT_TRUE(format && text && !strcmp(string_c_str(text), "count=0012"), "README compiled output");
    string_free(text);
    fmt_free(format);
}

/* README example: reorder named captures and bind a calculation without repeating its evaluation. */
void example_format_positions(void)
{
    static const char expected[] = "bolts: 3/3; total=21; {ready}\n";
    int quantity = 3;
    const char *item = "bolts";
    int first = fmt_print("{quantity} {item}\n", FMT(item), FMT(quantity));
    TEST_ASSERT_TRUE(first == 8, "README named output ignores capture ordering");
    int bytes = fmt_print("{item}: {quantity}/{quantity}; total={total}; {{ready}}\n",
        FMT(total, quantity * 7),
        FMT(item),
        FMT(quantity));
    TEST_ASSERT_TRUE(bytes == (int)sizeof(expected) - 1, "README repeated named fields and calculated value output");
}

/* README example: own the returned string and apply visible-character precision and numeric prefix padding. */
void example_format_string(void)
{
    const char *word = "é🙂";
    unsigned code = 42u;
    string_t *text = fmt_string("|{word:>4.1s}| {code:#06x}", FMT(word), FMT(code));
    TEST_ASSERT_TRUE(text && !strcmp(string_c_str(text), "|   é| 0x002a"), "README Unicode and hexadecimal padding");
    if (!text)
        return;
    int bytes = fmt_print("{text}\n", FMT(text));
    string_free(text);
    TEST_ASSERT_TRUE(bytes == (int)sizeof("|   é| 0x002a\n") - 1, "README owned string output");
}

/* README example: append to a builder and choose a stream for the completed message. */
void example_format_builder(void)
{
    string_builder_t *out = string_builder_new();
    TEST_ASSERT_TRUE(out != NULL, "README builder allocation");
    if (!out)
        return;
    int quantity = 3;
    int status = fmt_append(out, "items={quantity}", FMT(quantity)) < 0 ||
                 fmt_append(out, ", total={total}", FMT(total, quantity * 7)) < 0;
    if (!status)
        status = fmt_fprint(stdout, "{out}\n", FMT(out)) < 0;
    bool matches = !strcmp(string_c_str(out), "items=3, total=21");
    string_builder_free(out);
    TEST_ASSERT_TRUE(status == 0 && matches, "README builder and stream output");
}

/* README example: format native complex coordinates and an exact borrowed fraction without narrowing. */
void example_format_native(void)
{
    static const char expected[] = "z=1.25 - 2.50i; ratio=⅚\n";
    qcomplex_t z = qc_make(qf_from_double(1.25), qf_from_double(-2.5));
    number_t ratio = num_create_from_string("5/6");
    int bytes = fmt_print("z={z:.2f}; ratio={ratio}\n", FMT(z), FMT(ratio));
    num_destroy(&ratio);
    TEST_ASSERT_TRUE(bytes == (int)sizeof(expected) - 1, "README complex and exact native output");
}

typedef struct {
    int x;
    int y;
} point_t;

static string_t *point_format(const void *value, const fmt_options_t *options, fmt_error_t *error)
{
    if (*options->presentation || options->precision >= 0 || options->sign != '-' ||
        options->alternate || options->zero || options->alignment == '=') {
        if (error)
            *error = (fmt_error_t){FMT_ERROR_TYPE, SIZE_MAX, SIZE_MAX, "unsupported point options"};
        return NULL;
    }
    const point_t *point = value;
    return fmt_string("({x}, {y})", FMT(x, point->x), FMT(y, point->y));
}

/* README example: borrow a custom object, return owned core text and let the formatter apply field width. */
void example_format_custom(void)
{
    const point_t point = {2, 3};
    fmt_arg_t position = fmt_arg_custom("position", &point, point_format);
    int bytes = fmt_print("point={position:>10}\n", position);
    TEST_ASSERT_TRUE(bytes == (int)sizeof("point=    (2, 3)\n") - 1, "README custom object output");
}

/* README example: copy the diagnostic before another formatting call and preserve a failed append's destination. */
void example_format_error(void)
{
    string_t *out = string_new_with("unchanged");
    TEST_ASSERT_TRUE(out != NULL, "README error destination allocation");
    if (!out)
        return;
    int appended = fmt_append(out, " {missing}", FMT(7));
    fmt_error_t failure = *fmt_last_error();
    int bytes = fmt_print("{message}; destination={out}\n", FMT(message, failure.message), FMT(out));
    int status = appended != -1 || failure.code != FMT_ERROR_MISSING || bytes < 0;
    bool matches = !strcmp(string_c_str(out), "unchanged");
    string_free(out);
    TEST_ASSERT_TRUE(status == 0 && matches, "README failed append preserves its text and reports the saved error");
    TEST_ASSERT_TRUE(bytes == (int)sizeof("unknown named binding; destination=unchanged\n") - 1,
                     "README saved diagnostic output");
}

/* README example: print an emoji as UTF-8 text and as an encoded Unicode scalar value. */
void example_format_emoji(void)
{
    static const char expected[] = "Text: 🙂\nCharacter: 🙂\n";
    const char *emoji = "🙂";
    unsigned code_point = 0x1F642u;
    int bytes = fmt_print("Text: {emoji}\nCharacter: {code_point:c}\n", FMT(emoji), FMT(code_point));
    TEST_ASSERT_TRUE(bytes == (int)sizeof(expected) - 1, "README emoji text and scalar output byte count");
}

/* README example: owned_string output with native typed bindings and ordinary ownership. */
void example_format_owned_string(void)
{
    string_t *greeting = string_new_with("Hello, Ada 🙂");
    if (!greeting)
        return;
    int bytes = fmt_print("{greeting}\n", FMT(greeting));
    string_free(greeting);
    TEST_ASSERT_TRUE(bytes == 16, "README owned_string output byte count");
}

/* README example: datetime output with native typed bindings and ordinary ownership. */
void example_format_datetime(void)
{
    datetime_t *when = datetime_init_ymdt(datetime_alloc(), 2026, DT_June, 10, 13, 5, 6.125);
    if (!when)
        return;
    int bytes = fmt_print("{when:dddd, do^ MMMM yyyy}\n{when:yyyy-MM-dd hh:mm:ss.000}\n{when:h:mm:ss "
                          "AM/PM}\n{when:h:mm:ssam/pm}\n",
        FMT(when));
    datetime_dealloc(when);
    TEST_ASSERT_TRUE(bytes == (int)sizeof("Wednesday, 10ᵗʰ June 2026\n2026-06-10 13:05:06.125\n"
                                          "1:05:06 PM\n1:05:06pm\n") - 1,
                     "README datetime output byte count");
}

/* README example: native English name and ordinal case controls share the typed date parser. */
void example_format_datetime_case(void)
{
    datetime_t *dt = datetime_init_ymd(datetime_alloc(), 2026, DT_June, 10);
    if (!dt)
        return;
    int bytes = fmt_print("{dt:[upper]dddd do MMMM yyyy}\n{dt:[lower]ddd MMM yyyy}\n{dt:[upper]ddd [title]do "
                          "MMMM yyyy}\n",
        FMT(dt));
    datetime_dealloc(dt);
    TEST_ASSERT_TRUE(bytes == (int)sizeof("WEDNESDAY 10TH JUNE 2026\nwed jun 2026\nWED 10th June 2026\n") - 1,
                     "README date case output byte count");
}

/* README example: native time-series inline, pretty and CSV styles share one borrowed binding. */
void example_format_timeseries(void)
{
    const double values[] = {1.0, 2.0, 3.0};
    timeseries_t *series = ts_new_from_doubles(values, 3u);
    if (!series)
        return;
    int bytes = fmt_print("Inline: {series}\nPretty:\n{series:pretty}CSV:\n{series:csv}", FMT(series));
    ts_free(series);
    TEST_ASSERT_TRUE(bytes == (int)sizeof("Inline: 1; 2; 3\nPretty:\n1\n2\n3\nCSV:\ndate,value\n,1\n,2\n,3\n") - 1,
                     "README time-series output byte count");
}

/* README example: six catalogue locales with translated date order and grammatical month forms. */
void example_format_locales(void)
{
    datetime_t *dt = datetime_init_ymd(datetime_alloc(), 2026, DT_June, 10);
    if (!dt)
        return;
    int bytes = fmt_print("France: {dt:fr_FR}\nSpain: {dt:es_ES}\nWales: {dt:cy_GB}\nGreece: "
                          "{dt:el_GR}\nRussia: {dt:ru_RU}\nJapan: {dt:ja_JP}\n",
        FMT(dt));
    datetime_dealloc(dt);
    TEST_ASSERT_TRUE(bytes == (int)sizeof("France: 10 juin 2026\nSpain: 10 de junio de 2026\n"
        "Wales: 10 Mehefin 2026\nGreece: 10 Ιουνίου 2026\nRussia: 10 июня 2026 г.\nJapan: 2026年6月10日\n") - 1,
        "README multilingual output byte count");
}

/* README example: first-day suffix and native Greek and Japanese Lingua presentations. */
void example_datetime_lingua_new_year(void)
{
    datetime_t *dt = datetime_init_ymd(datetime_alloc(), 2026, DT_January, 1);
    if (!dt)
        return;
    int bytes = fmt_print("France: {dt:fr_FR}\nGreece: {dt:el_GR}\nJapan: {dt:ja_JP}\n", FMT(dt));
    datetime_dealloc(dt);
    TEST_ASSERT_TRUE(bytes == (int)sizeof("France: 1ᵉʳ janvier 2026\nGreece: 1 Ιανουαρίου 2026\n"
                                        "Japan: 2026年1月1日\n") - 1,
                     "README New Year Lingua output byte count");
}

/* README example: symbolic output with native typed bindings and ordinary ownership. */
void example_format_symbolic(void)
{
    expr_t *expression = expr_from_string("x^2 + 1", NULL);
    equation_t *equation = equ_from_string("x^2 = 4");
    TeX_builder_t *builder = TeX_builder_new();
    TeX_symbol(builder, "alpha");
    TeX_integer(builder, 2);
    TeX_frac(builder);
    TeX_t *notation = TeX_builder_finish(builder);
    int bytes = -1;
    if (expression && equation && notation)
        bytes = fmt_print("Expression: {expression:unbound}\nEquation: {equation:unbound}\nExpression TeX: "
                          "{expression:latex}\nEquation TeX: {equation:latex}\nTeX: {notation}\n",
            FMT(expression),
            FMT(equation),
            FMT(notation));
    TeX_free(notation);
    TeX_builder_free(builder);
    equ_free(equation);
    expr_free(expression);
    TEST_ASSERT_TRUE(bytes == 182, "README symbolic output byte count");
}

/* README example: excel output with native typed bindings and ordinary ownership. */
void example_format_excel(void)
{
    double positive = 1234.5;
    double negative = -1234.5;
    double zero = 0.0;
    double negative_zero = -0.0;
    double fraction = 1.25;
    number_t exact = num_create_from_string("123456789012345678901234567890");
    int bytes = fmt_print("{positive:+#,##0.00;(#,##0.00);\"zero\"}\n{negative:+#,##0.00;(#,##0.00);\"zero\"}\n{"
                          "zero:+#,##0.00;(#,##0.00);\"zero\"}\nSigned zero: {negative_zero:0.00}\nFraction: "
                          "{fraction:# ?/?}\nExact: {exact:#,##0}\n",
        FMT(positive),
        FMT(negative),
        FMT(zero),
        FMT(negative_zero),
        FMT(fraction),
        FMT(exact));
    num_destroy(&exact);
    TEST_ASSERT_TRUE(bytes == 108, "README excel output byte count");
}

/* README example: quote literal date text and use case-sensitive month and ordinal-day fields. */
void example_format_date_literals(void)
{
    datetime_t *when = datetime_init_ymd(datetime_alloc(), 2026, DT_February, 10);
    if (!when)
        return;
    int bytes = fmt_print("{when:do \"day of month\" MM}\n{when:do^ \"day of month\" MM}\n", FMT(when));
    datetime_dealloc(when);
    TEST_ASSERT_TRUE(bytes == (int)sizeof("10th day of month 02\n10ᵗʰ day of month 02\n") - 1,
                     "README date literals and case-sensitive month output");
}
