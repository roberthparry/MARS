/**
 * @file test_lab_math_support.c
 * @brief Worker execution, owned fixtures and assertions for native mathematical regressions.
 *
 * Retains each worker response and constructed string until the current test ends.
 * The arena is deliberately single-threaded, matching mandatory sequential tests.
 * Numerical parsing checks the entire output; reference quadrature is independent
 * of the native expression implementation. File access uses the public file API.
 */
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <regex.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "file.h"
#include "lab_process.h"
#include "../src/internal/lab_evaluate_internal.h"
#include "test_lab_math_support.h"

typedef struct math_resource {
    string_t *text;
    json_t *fields;
    struct math_resource *next;
} math_resource_t;

static math_resource_t *resources;

/* Includes the documented branch-verification allowance under a 50% CPU quota. */
enum { lab_math_worker_timeout_ms = 120000 };

static const char *lab_math_retain(string_t *text, json_t *fields)
{
    math_resource_t *entry = malloc(sizeof(*entry));
    if (!lab_math_check(entry != NULL, "allocate mathematical test fixture")) {
        string_free(text);
        json_free(fields);
        return "";
    }
    *entry = (math_resource_t){text, fields, resources};
    resources = entry;
    return text ? string_c_str(text) : "";
}

/* Free test-owned storage after every registered test, including failed assertions. */
void lab_math_reset(void)
{
    while (resources) {
        math_resource_t *entry = resources;
        resources = entry->next;
        string_free(entry->text);
        json_free(entry->fields);
        free(entry);
    }
}

/* Preserve protocol fields without UI rewriting or external TeX rendering. */
const json_t *lab_math_worker(const char *worker, const char *source, const char *variable, const char *action,
                             unsigned precision, int *status, const char **raw)
{
    string_t *path = lab_proc_worker_path(worker);
    string_t *digits = string_sprintf("%u", precision < 17 ? 17 : precision);
    string_t *output = NULL;
    const char *expression_args[] = {path ? string_c_str(path) : "", source, variable,
                                     digits ? string_c_str(digits) : "40", action, NULL};
    const char *equation_args[] = {path ? string_c_str(path) : "", source,
                                   digits ? string_c_str(digits) : "40", NULL};
    bool equation = !strcmp(worker, "equation_lab");
    bool ok = path && digits && lab_proc_run(equation ? equation_args : expression_args, NULL,
                                           lab_math_worker_timeout_ms, 4u * 1024u * 1024u, &output, status);
    if (!ok)
        string_printf("Mathematical worker failed: errno=%d, deadline=%d ms, source=%s\n",
                      errno, lab_math_worker_timeout_ms, source);
    lab_math_check(ok, source);
    json_t *fields = output ? lab_eval_fields(output) : NULL;
    lab_math_check(fields != NULL, "decode mathematical worker fields");
    const char *text = lab_math_retain(output, fields);
    if (raw)
        *raw = text;
    string_free(path);
    string_free(digits);
    return fields;
}

/* Standard successful mathematical evaluation. */
const json_t *lab_math_fields(const char *source, const char *variable, const char *action, unsigned precision)
{
    int status = -1;
    const char *raw = "";
    const json_t *fields = lab_math_worker("mars_lab", source, variable, action, precision, &status, &raw);
    if (status)
        string_printf("worker source: %s\n%s\n", source, raw);
    lab_math_check(status == 0, source);
    return fields;
}

/* Missing fields must fail instead of accidentally passing negative assertions. */
const char *lab_math_text(const json_t *fields, const char *key)
{
    const json_t *value = lab_eval_get(fields, key);
    lab_math_check(value && json_type(value) == JSON_STRING, key);
    return lab_eval_text(fields, key);
}

/* Allocate dynamically sized formatted input, preserving full UTF-8 text. */
const char *lab_math_format(const char *format, ...)
{
    va_list arguments;
    va_start(arguments, format);
    string_t *text = string_new();
    bool ok = text && string_append_vformat(text, format, arguments) >= 0;
    va_end(arguments);
    lab_math_check(ok, "format mathematical regression input");
    return lab_math_retain(text, NULL);
}

/* Literal replacement mirrors copied-expression operations in the original tests. */
const char *lab_math_replace(const char *text, const char *old, const char *replacement)
{
    string_t *copy = string_new_with(text);
    lab_math_check(copy && string_replace(copy, old, replacement) >= 0, "replace copied expression fragment");
    return lab_math_retain(copy, NULL);
}

/* Strip only the test fixture's binding envelope. */
const char *lab_math_algebra(const json_t *fields)
{
    string_t *text = string_new_with(lab_math_text(fields, "expression"));
    string_offset_t end = string_find(text, " | ");
    size_t start = string_starts_with(text, "{ ") ? 2 : 0;
    size_t length = end < 0 ? string_byte_length(text) : (size_t)end;
    string_t *body = string_substr(text, start, length >= start ? length - start : 0);
    string_free(text);
    return lab_math_retain(body, NULL);
}

/* Locate the primitive right-hand side without interpreting mathematics. */
const char *lab_math_after(const char *text, const char *delimiter)
{
    string_t *input = string_new_with(text), *separator = string_new_with(delimiter);
    string_offset_t position = string_find_string(input, separator);
    lab_math_check(position >= 0, delimiter);
    size_t start = position < 0 ? string_byte_length(input) : (size_t)position + string_byte_length(separator);
    string_t *tail = string_substr(input, start, string_byte_length(input) - start);
    string_free(separator);
    string_free(input);
    return lab_math_retain(tail, NULL);
}

/* Read documentation/source fixtures using the public file abstraction. */
const char *lab_math_read(const char *path)
{
    file_t *file = file_new_cstr(path);
    string_t *text = file ? file_read_all_text(file) : NULL;
    lab_math_check(text != NULL, path);
    file_free(file);
    return lab_math_retain(text, NULL);
}

/* Execute generated Function text, preserving its stdout and checking the exit. */
const char *lab_math_programme(const char *source, unsigned precision)
{
    string_t *path = lab_proc_worker_path("ophelia");
    string_t *input = string_new_with(source);
    const char *digits = lab_math_format("%u", precision);
    const char *args[] = {path ? string_c_str(path) : "", digits, NULL};
    string_t *output = NULL;
    int status = -1;
    bool ok = path && input && lab_proc_run_input(args, NULL, input, lab_math_worker_timeout_ms,
                                                4u * 1024u * 1024u, &output, &status);
    if (!ok)
        string_printf("Mathematical function worker failed: errno=%d, deadline=%d ms, source=%s\n",
                      errno, lab_math_worker_timeout_ms, source);
    lab_math_check(ok && status == 0, source);
    string_free(input);
    string_free(path);
    if (output)
        string_trim(output);
    return lab_math_retain(output, NULL);
}

static bool lab_math_decimal(string_cursor_t *cursor, double *value)
{
    int sign = string_cursor_consume(cursor, "-") ? -1 : 1;
    if (sign > 0)
        string_cursor_consume(cursor, "+");
    if (string_cursor_consume(cursor, "NAN") || string_cursor_consume(cursor, "nan")) {
        *value = NAN;
        return true;
    }
    if (string_cursor_consume(cursor, "INF") || string_cursor_consume(cursor, "inf")) {
        *value = sign * INFINITY;
        return true;
    }
    long double mantissa = 0;
    int fractional = 0;
    bool dot = false, digit = false;
    unsigned char byte = 0;
    while (string_cursor_peek_ascii(cursor, &byte)) {
        if (byte >= '0' && byte <= '9') {
            mantissa = mantissa * 10 + byte - '0';
            fractional += dot;
            digit = true;
            string_cursor_next(cursor);
        } else if (byte == '.' && !dot) {
            dot = true;
            string_cursor_next(cursor);
        } else {
            break;
        }
    }
    if (!digit)
        return false;
    int exponent = 0;
    if (string_cursor_consume(cursor, "e") || string_cursor_consume(cursor, "E")) {
        int exponent_sign = string_cursor_consume(cursor, "-") ? -1 : 1;
        if (exponent_sign > 0)
            string_cursor_consume(cursor, "+");
        bool exponent_digit = false;
        while (string_cursor_peek_ascii(cursor, &byte) && byte >= '0' && byte <= '9') {
            if (exponent < 100000)
                exponent = exponent * 10 + byte - '0';
            exponent_digit = true;
            string_cursor_next(cursor);
        }
        if (!exponent_digit)
            return false;
        exponent *= exponent_sign;
    }
    *value = (double)(sign * mantissa * powl(10, exponent - fractional));
    return true;
}

/* Parse complete numerical output through the public UTF-8 cursor interface. */
double complex lab_math_parse_number(const char *text)
{
    string_t *normal = string_new_with(text);
    if (!lab_math_check(normal != NULL, "allocate numerical input"))
        return NAN;
    string_replace(normal, "−", "-");
    string_replace(normal, " ", "");
    string_trim(normal);
    string_cursor_t *cursor = string_cursor_new(normal);
    double complex result = NAN;
    bool ok = cursor != NULL;
    if (ok && (string_cursor_consume(cursor, "i") || string_cursor_consume(cursor, "+i"))) {
        result = I;
    } else if (ok && string_cursor_consume(cursor, "-i")) {
        result = -I;
    } else if (ok) {
        double first = 0;
        ok = lab_math_decimal(cursor, &first);
        result = first;
        if (ok && string_cursor_consume(cursor, "i")) {
            result = first * I;
        } else if (ok && !string_cursor_done(cursor)) {
            double second = 0;
            if (string_cursor_consume(cursor, "+i")) {
                second = 1;
            } else if (string_cursor_consume(cursor, "-i")) {
                second = -1;
            } else {
                ok = (string_cursor_match(cursor, "+") || string_cursor_match(cursor, "-")) &&
                     lab_math_decimal(cursor, &second) && string_cursor_consume(cursor, "i");
            }
            result = first + second * I;
        }
    }
    ok = ok && string_cursor_done(cursor);
    lab_math_check(ok, text);
    string_cursor_free(cursor);
    string_free(normal);
    return ok ? result : NAN;
}

/* Numerical field convenience for complex and real checks. */
double complex lab_math_number(const json_t *fields, const char *key)
{
    return lab_math_parse_number(lab_math_text(fields, key));
}

/* Presentation comparisons remain explicit harness assertions. */
void lab_math_equal(const char *actual, const char *expected)
{
    test_assert_cstr_eq(actual, expected, __FILE__, __LINE__);
}

/* Record assertion failures while allowing fixture cleanup and remaining cases. */
bool lab_math_check(bool condition, const char *detail)
{
    return test_assert_true(condition, __FILE__, __LINE__, detail);
}

/* Check exact literal fragments, including Unicode symbols and TeX escapes. */
void lab_math_contains(const char *actual, const char *fragment, bool present)
{
    bool found = strstr(actual, fragment) != NULL;
    TEST_ASSERT_TRUE(found == present, lab_math_format("%s fragment [%s] in [%s]", present ? "required" : "forbidden",
                                                     fragment, actual));
}

/* POSIX expressions used here are translated explicitly from the Python originals. */
void lab_math_regex(const char *actual, const char *pattern, bool present)
{
    regex_t expression;
    int status = regcomp(&expression, pattern, REG_EXTENDED | REG_NEWLINE);
    if (!lab_math_check(status == 0, pattern))
        return;
    int result = regexec(&expression, actual, 0, NULL, 0);
    TEST_ASSERT_TRUE(result == 0 || result == REG_NOMATCH, pattern);
    TEST_ASSERT_TRUE((result == 0) == present, lab_math_format("pattern [%s] in [%s]", pattern, actual));
    regfree(&expression);
}

/* Reject NaN explicitly through the strict comparison. */
void lab_math_close(double complex actual, double complex expected, double tolerance)
{
    double error = cabs(actual - expected);
    TEST_ASSERT_TRUE(error < tolerance, lab_math_format("error %.17g < %.17g; actual %.17g%+.17gi expected %.17g%+.17gi",
                                                       error, tolerance, creal(actual), cimag(actual),
                                                       creal(expected), cimag(expected)));
}

/* Match decimal-place checks at the half-unit rounding threshold. */
void lab_math_places(double complex actual, double complex expected, unsigned places)
{
    double tolerance = 0.5 * pow(10.0, -(double)places);
    TEST_ASSERT_TRUE(actual == expected || cabs(actual - expected) <= tolerance,
                     lab_math_format("%u-place comparison: actual %.17g%+.17gi expected %.17g%+.17gi",
                                     places, creal(actual), cimag(actual), creal(expected), cimag(expected)));
}

/* Independent composite Simpson references retain their original interval count. */
double complex lab_math_simpson(double complex (*function)(double, void *), void *context,
                                double left, double right, size_t steps)
{
    if (!lab_math_check(steps && !(steps % 2), "positive even Simpson interval count"))
        return NAN;
    double step = (right - left) / (double)steps;
    double complex total = function(left, context) + function(right, context);
    for (size_t i = 1; i < steps; ++i)
        total += (i % 2 ? 4 : 2) * function(left + (double)i * step, context);
    return total * step / 3;
}
