/**
 * @file test_format.c
 * @brief Typed interpolation, ownership, compiled-layout and output regression tests.
 *
 * SQL-backed cases supply templates, typed JSON fixtures and expected text or error categories. Stateful C
 * assertions check one-time evaluation, borrowed lifetimes, immutable templates, custom callbacks and streams.
 * Runnable format-module README examples execute after ordinary checks.
 */
#include <errno.h>
#include <float.h>
#include <inttypes.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "string-t.h"
#include "json-t.h"
#include "qfloat-t.h"
#include "qcomplex-t.h"
#include "number-t.h"
#include "expression-t.h"
#include "matrix-t.h"
#include "datetime-t.h"
#include "equation-t.h"
#include "TeX-t.h"
#include "test_harness.h"
#include "test_database.h"
#define MARS_TEST_FORMAT_INTERNAL_ACCESS
#include "test_format_internal.h"

TEST_SUITE_CONFIG(TEST_CONFIG_GLOBAL);
TEST_SUITE_SETUP(test_format_locale_setup);
TEST_POST_SUMMARY(test_format_locale_cleanup);

static const json_t *test_property(const json_t *object, const char *name)
{
    string_t *key = string_new_with(name);
    const json_t *value = key ? json_object_get(object, key) : NULL;
    string_free(key);
    return value;
}

static const string_t *test_text_property(const json_t *object, const char *name)
{
    return json_string_value(test_property(object, name));
}

static const char *test_value(const json_t *object)
{
    return string_c_str(test_text_property(object, "value"));
}

typedef struct {
    fmt_arg_t value;
    number_t number;
    bool owns_number;
    datetime_t *datetime;
    equation_t *equation;
    expr_t *expression;
    TeX_t *notation;
} test_argument_t;

static bool test_signed(const json_t *object, test_argument_t *argument)
{
    const char *text = test_value(object);
    char *end;
    errno = 0;
    intmax_t value = strtoimax(text, &end, 10);
    argument->value = fmt_arg_signed(value);
    return end != text && !*end && errno == 0;
}

static bool test_unsigned(const json_t *object, test_argument_t *argument)
{
    const char *text = test_value(object);
    char *end;
    errno = 0;
    uintmax_t value = strtoumax(text, &end, 10);
    argument->value = fmt_arg_unsigned(value);
    return end != text && !*end && errno == 0 && *text != '-';
}

static bool test_boolean(const json_t *object, test_argument_t *argument)
{
    bool value;
    if (!json_bool_value(test_property(object, "value"), &value))
        return false;
    argument->value = fmt_arg_bool(value);
    return true;
}

static bool test_float_value(const json_t *object, test_argument_t *argument)
{
    const char *text = test_value(object);
    char *end;
    float value = strtof(text, &end);
    argument->value = fmt_arg_float(value);
    return end != text && !*end;
}

static bool test_double_value(const json_t *object, test_argument_t *argument)
{
    const char *text = test_value(object);
    char *end;
    double value = strtod(text, &end);
    argument->value = fmt_arg_double(value);
    return end != text && !*end;
}

static bool test_long_double_value(const json_t *object, test_argument_t *argument)
{
    const char *text = test_value(object);
    char *end;
    long double value = strtold(text, &end);
    argument->value = fmt_arg_long_double(value);
    return end != text && !*end;
}

static bool test_cstring(const json_t *object, test_argument_t *argument)
{
    const string_t *value = test_text_property(object, "value");
    argument->value = fmt_arg_cstring(value ? string_c_str(value) : NULL);
    return true;
}

static bool test_string(const json_t *object, test_argument_t *argument)
{
    argument->value = fmt_arg_string(test_text_property(object, "value"));
    return true;
}

static bool test_view(const json_t *object, test_argument_t *argument)
{
    const string_t *value = test_text_property(object, "value");
    argument->value = fmt_arg_view(string_view_all(value));
    return value != NULL;
}

static bool test_rune(const json_t *object, test_argument_t *argument)
{
    const string_t *value = test_text_property(object, "value");
    argument->value = fmt_arg_rune(string_at(value, 0u));
    return value != NULL;
}

static bool test_pointer(const json_t *object, test_argument_t *argument)
{
    (void)object;
    argument->value = fmt_arg_pointer(NULL);
    return true;
}

static bool test_qfloat(const json_t *object, test_argument_t *argument)
{
    const string_t *text = test_text_property(object, "value");
    if (text) {
        argument->value = fmt_arg_qfloat(qf_from_text(text));
        return true;
    }
    const string_t *high = test_text_property(object, "hi");
    const string_t *low = test_text_property(object, "lo");
    if (!high || !low)
        return false;
    char *high_end, *low_end;
    qfloat_t value = {strtod(string_c_str(high), &high_end), strtod(string_c_str(low), &low_end)};
    argument->value = fmt_arg_qfloat(value);
    return !*high_end && !*low_end;
}

static bool test_complex(const json_t *object, test_argument_t *argument)
{
    argument->value = fmt_arg_qcomplex(qc_from_text(test_text_property(object, "value")));
    return true;
}

static bool test_number(const json_t *object, test_argument_t *argument)
{
    const string_t *precision = test_text_property(object, "digits");
    size_t previous = num_get_default_prec_digits();
    if (precision)
        num_set_default_prec_digits((size_t)strtoul(string_c_str(precision), NULL, 10));
    argument->number = num_create_from_text(test_text_property(object, "value"));
    if (precision)
        num_set_default_prec_digits(previous);
    argument->owns_number = true;
    argument->value = fmt_arg_number(argument->number);
    return true;
}

static bool test_datetime(const json_t *object, test_argument_t *argument)
{
    const char *text = test_value(object);
    int year, month, day, hour = 0, minute = 0;
    double second = 0.0;
    int fields = text ? sscanf(text, "%d-%d-%dT%d:%d:%lf", &year, &month, &day, &hour, &minute, &second) : 0;
    if (fields < 3)
        return false;
    argument->datetime = datetime_init_ymdt(datetime_alloc(), (short)year, (month_t)month, (uint8_t)day,
                                            (uint8_t)hour, (uint8_t)minute, second);
    argument->value = fmt_arg_datetime(argument->datetime);
    return argument->datetime != NULL;
}

static bool test_equation(const json_t *object, test_argument_t *argument)
{
    argument->equation = equ_from_string(test_value(object));
    argument->value = fmt_arg_equation(argument->equation);
    return argument->equation != NULL;
}

static bool test_expression(const json_t *object, test_argument_t *argument)
{
    argument->expression = expr_from_string(test_value(object), NULL);
    argument->value = fmt_arg_expression(argument->expression);
    return argument->expression != NULL;
}

static bool test_notation(const json_t *object, test_argument_t *argument)
{
    argument->notation = TeX_from_expression(test_text_property(object, "value"));
    argument->value = fmt_arg_TeX(argument->notation);
    return argument->notation != NULL;
}

typedef struct {
    const char *name;
    bool (*create)(const json_t *, test_argument_t *);
} test_argument_factory_t;

static const test_argument_factory_t test_factories[] = {
    {"TeX",        test_notation},
    {"bool",       test_boolean},
    {"complex",    test_complex},
    {"datetime",   test_datetime},
    {"double",     test_double_value},
    {"equation",   test_equation},
    {"expression", test_expression},
    {"float",      test_float_value},
    {"int",        test_signed},
    {"longdouble", test_long_double_value},
    {"number",     test_number},
    {"pointer",    test_pointer},
    {"qfloat",     test_qfloat},
    {"rune",       test_rune},
    {"string",     test_string},
    {"text",       test_cstring},
    {"uint",       test_unsigned},
    {"view",       test_view},
};

static const test_argument_factory_t *test_factory(const char *name)
{
    size_t first = 0u, end = sizeof(test_factories) / sizeof(*test_factories);
    while (first < end) {
        size_t middle = first + (end - first) / 2u;
        int order = strcmp(name, test_factories[middle].name);
        if (!order)
            return &test_factories[middle];
        if (order < 0)
            end = middle;
        else
            first = middle + 1u;
    }
    return NULL;
}

static void test_format_check(const void *data, bool stream_case)
{
    static const char *const codes[] = {
        [FMT_OK]              = "success",
        [FMT_ERROR_ARGUMENT]  = "argument",
        [FMT_ERROR_MEMORY]    = "memory",
        [FMT_ERROR_SYNTAX]    = "syntax",
        [FMT_ERROR_MISSING]   = "missing",
        [FMT_ERROR_AMBIGUOUS] = "ambiguous",
        [FMT_ERROR_TYPE]      = "type",
        [FMT_ERROR_RANGE]     = "range",
        [FMT_ERROR_IO]        = "io",
    };
    const test_database_case_t *row = data;
    json_t *root = json_from_text(row->input);
    const string_t *pattern = test_text_property(root, "pattern");
    const json_t *values = test_property(root, "args");
    size_t count = json_array_size(values);
    fmt_arg_t *arguments = count ? calloc(count, sizeof(*arguments)) : NULL;
    test_argument_t *fixtures = count ? calloc(count, sizeof(*fixtures)) : NULL;
    bool ok = root && pattern && (!count || (arguments && fixtures));
    for (size_t i = 0; ok && i < count; ++i) {
        const json_t *object = json_array_get(values, i);
        const char *type = string_c_str(test_text_property(object, "type"));
        const test_argument_factory_t *factory = test_factory(type);
        ok = factory && factory->name && !strcmp(type, factory->name) && factory->create(object, &fixtures[i]);
        const string_t *name = test_text_property(object, "name");
        arguments[i] = fmt_named(name ? string_c_str(name) : NULL, fixtures[i].value);
    }
    fmt_error_t error = {FMT_OK, SIZE_MAX, SIZE_MAX, "fixture not loaded"};
    string_t *out = ok && !stream_case ? fmt_string_args(string_c_str(pattern), arguments, count, &error) : NULL;
    bool failure_case = string_view_equals_literal(string_view_all(row->operation), "error");
    if (!stream_case && failure_case) {
        ok = ok && !out && error.code > FMT_OK && error.code <= FMT_ERROR_IO &&
             !strcmp(codes[error.code], string_c_str(row->expected));
    } else if (!stream_case) {
        ok = ok && out && error.code == FMT_OK && string_byte_length(out) == string_byte_length(row->expected) &&
             !memcmp(string_c_str(out), string_c_str(row->expected), string_byte_length(out));
    }
    char *written = NULL;
    size_t written_length = 0u;
    FILE *stream = ok && stream_case ? open_memstream(&written, &written_length) : NULL;
    if (ok && stream_case)
        ok = stream != NULL;
    for (unsigned pass = 0; ok && stream_case && pass < 2u; ++pass) {
        int bytes = fmt_fprint_args(stream, string_c_str(pattern), arguments, count, &error);
        ok = failure_case ? bytes == -1 && error.code > FMT_OK && error.code <= FMT_ERROR_IO &&
                            !strcmp(codes[error.code], string_c_str(row->expected)) :
                            bytes >= 0 && (size_t)bytes == string_byte_length(row->expected) && error.code == FMT_OK;
    }
    if (stream) {
        ok = fclose(stream) == 0 && ok;
        size_t expected = failure_case ? 0u : string_byte_length(row->expected);
        ok = ok && written_length == expected * 2u &&
             (!expected || (!memcmp(written, string_c_str(row->expected), expected) &&
                            !memcmp(written + expected, string_c_str(row->expected), expected)));
    }
    free(written);
    if (!ok) {
        test_set_failure_detailf("%s: result=%s; error=%s at byte %zu argument %zu; expected=%s",
            string_c_str(row->name), out ? string_c_str(out) : "<NULL>", error.message,
            error.offset, error.argument, string_c_str(row->expected));
        printf("%s: got [%s], expected [%s], error %s at %zu\n", string_c_str(row->name),
               out ? string_c_str(out) : "<NULL>", string_c_str(row->expected), error.message, error.offset);
    }
    string_free(out);
    for (size_t i = 0; i < count; ++i) {
        if (fixtures && fixtures[i].owns_number)
            num_destroy(&fixtures[i].number);
        if (fixtures) {
            datetime_dealloc(fixtures[i].datetime);
            equ_free(fixtures[i].equation);
            expr_free(fixtures[i].expression);
            TeX_free(fixtures[i].notation);
        }
    }
    free(fixtures);
    free(arguments);
    json_free(root);
    TEST_ASSERT_TRUE(ok, "typed formatting agrees with its database-backed text or error reference");
}

static void test_format_case(const void *data)
{
    test_format_check(data, false);
}

static void test_stream_case(const void *data)
{
    test_format_check(data, true);
}

static void test_format_render(void)
{
    test_run_database_cases(__FILE__, __LINE__, "format.render", "tests.test_format_render", test_format_case);
}

static void test_format_errors(void)
{
    test_run_database_cases(__FILE__, __LINE__, "format.errors", "tests.test_format_errors", test_format_case);
}

static void test_format_streams(void)
{
    test_run_database_cases(__FILE__, __LINE__, "format.stream", "tests.test_format_streams", test_stream_case);
}

static void test_format_stream_errors(void)
{
    test_run_database_cases(__FILE__, __LINE__, "format.stream_errors", "tests.test_format_stream_errors", test_stream_case);
}

static void test_macro_evaluation(void)
{
    int counter = 4;
    string_t *text = fmt_string("{}", FMT(counter++));
    TEST_ASSERT_TRUE(text && counter == 5 && !strcmp(string_c_str(text), "4"), "FMT evaluates its value exactly once");
    string_free(text);
    text = fmt_string("{{literal}} and {} {}", FMT(counter), FMT(counter));
    TEST_ASSERT_TRUE(text && !strcmp(string_c_str(text), "{literal} and 5 5"), "duplicate names remain legal positionally");
    string_free(text);
    text = fmt_string("{result}", fmt_named("result", FMT(counter + 2)));
    TEST_ASSERT_TRUE(text && !strcmp(string_c_str(text), "7"), "explicit naming supports C expressions");
    string_free(text);
    const char *unit = "items";
    text = fmt_string("{answer}/{unit}", FMT(unit), FMT(answer, counter++));
    TEST_ASSERT_TRUE(text && counter == 6 && !strcmp(string_c_str(text), "5/items"),
                     "explicit FMT labels evaluate once and remove dependence on binding order");
    string_free(text);
    text = fmt_string("{NUM_ONE}", FMT(NUM_ONE));
    TEST_ASSERT_TRUE(text && !strcmp(string_c_str(text), "1"), "FMT preserves a macro argument's original name");
    string_free(text);
    text = fmt_string("plain UTF-8: Ω");
    TEST_ASSERT_TRUE(text && !strcmp(string_c_str(text), "plain UTF-8: Ω"), "zero-argument calls need no dummy binding");
    string_free(text);
    fmt_arg_t address = FMT(&counter);
    char expected[2u + sizeof(void *) * 2u + 1u];
    snprintf(expected, sizeof(expected), "%p", (void *)&counter);
    text = fmt_string("{:p}", address);
    TEST_ASSERT_TRUE(text && !strcmp(string_c_str(text), expected),
                     "ordinary object pointers select address formatting");
    string_free(text);
    size_t length = 3u;
    bool ready = true;
    long double precise = 1.0L + LDBL_EPSILON;
    text = fmt_string("{length} {ready} {precise}", FMT(length), FMT(ready), FMT(precise));
    TEST_ASSERT_TRUE(text && strstr(string_c_str(text), "3 true ") == string_c_str(text),
                     "fundamental typedefs and long double select their native constructors");
    if (LDBL_MANT_DIG > DBL_MANT_DIG)
        TEST_ASSERT_TRUE(strcmp(string_c_str(text), "3 true 1") != 0, "long double is not narrowed to double");
    string_free(text);
}

static void test_compiled_layout(void)
{
    char source[] = "{n:04d}/{n}";
    int n = 7;
    fmt_arg_t arguments[] = {FMT(n)};
    fmt_error_t error;
    fmt_t *format = fmt_compile(source, arguments, 1u, &error);
    TEST_ASSERT_TRUE(format && error.code == FMT_OK, "compile a resolved layout");
    source[0] = '!';
    n = 12;
    arguments[0] = FMT(n);
    string_t *out = fmt_render(format, arguments, 1u, &error);
    TEST_ASSERT_TRUE(out && !strcmp(string_c_str(out), "0012/12"), "compiled templates own their text and reread values");
    string_free(out);
    arguments[0] = fmt_named("other", FMT(n));
    out = fmt_render(format, arguments, 1u, &error);
    TEST_ASSERT_TRUE(!out && error.code == FMT_ERROR_TYPE && error.offset == 0u && error.argument == 0u,
                     "changed binding names produce a located layout error");
    arguments[0] = fmt_named("n", FMT(1.0));
    out = fmt_render(format, arguments, 1u, &error);
    TEST_ASSERT_TRUE(!out && error.code == FMT_ERROR_TYPE, "changed argument types are rejected");
    out = fmt_render(format, arguments, 0u, &error);
    TEST_ASSERT_TRUE(!out && error.code == FMT_ERROR_ARGUMENT, "changed argument counts are rejected");
    fmt_free(format);
    fmt_free(NULL);
}

static string_t *test_custom_point(const void *value, const fmt_options_t *options, fmt_error_t *error)
{
    if (options->presentation[0] && strcmp(options->presentation, "point") && strcmp(options->presentation, "coords")) {
        *error = (fmt_error_t){FMT_ERROR_TYPE, SIZE_MAX, SIZE_MAX, "unsupported point style"};
        return NULL;
    }
    const int *point = value;
    return fmt_string("({}, {})", FMT(point[0]), FMT(point[1]));
}

static void test_custom_and_append(void)
{
    const int point[] = {2, 3};
    fmt_arg_t argument = fmt_arg_custom("position", point, test_custom_point);
    string_builder_t *out = string_builder_new();
    TEST_ASSERT_TRUE(out && fmt_append(out, "[{position:*>10coords}]", argument) == 12,
                     "custom cores receive parsed options and the engine pads the result");
    TEST_ASSERT_TRUE(!strcmp(string_c_str(out), "[****(2, 3)]"), "custom output retains its presentation");
    string_t *before = string_clone(out);
    TEST_ASSERT_TRUE(fmt_append(out, "{missing}", argument) == -1 &&
                     !strcmp(string_c_str(out), string_c_str(before)), "format failures preserve append destinations");
    TEST_ASSERT_TRUE(fmt_append(out, "{position:invalid}", argument) == -1 && fmt_last_error()->code == FMT_ERROR_TYPE &&
                     !strcmp(string_c_str(out), string_c_str(before)), "custom failures remain atomic and diagnostic");
    TEST_ASSERT_TRUE(fmt_append(NULL, "text") == -1 && fmt_last_error()->code == FMT_ERROR_ARGUMENT,
                     "NULL append destinations are checked");
    string_free(before);
    string_builder_free(out);
    out = string_new_with("start");
    TEST_ASSERT_TRUE(out && fmt_append(out, "/{out}", FMT(out)) == 6 && !strcmp(string_c_str(out), "start/start"),
                     "staged appends support borrowing their own destination");
    string_builder_free(out);
}

static void test_native_objects(void)
{
    number_t exact = num_create_from_string("123456789012345678901234567890123456789/7");
    string_t *expected = num_to_string(exact);
    string_t *out = fmt_string("{exact}", FMT(exact));
    TEST_ASSERT_TRUE(out && expected && !strcmp(string_c_str(out), string_c_str(expected)),
                     "exact number payloads retain their native representation");
    string_free(out);
    string_free(expected);
    num_destroy(&exact);
    matrix_t *matrix = mat_from_string("(1,2;3,4)");
    expr_t *expression = expr_from_string("a*x + b", NULL);
    TEST_ASSERT_TRUE(matrix && expression, "construct live native object fixtures");
    expected = mat_to_text(matrix, MAT_STRING_LATEX);
    out = fmt_string("{matrix:latex}", FMT(matrix));
    TEST_ASSERT_TRUE(out && expected && !strcmp(string_c_str(out), string_c_str(expected)),
                     "matrix formatting delegates to its native TeX renderer");
    string_free(out);
    string_free(expected);
    expected = mat_to_text(matrix, MAT_STRING_LAYOUT_PRETTY);
    out = fmt_string("{matrix:pretty_layout}", FMT(matrix));
    TEST_ASSERT_TRUE(out && expected && !strcmp(string_c_str(out), string_c_str(expected)),
                     "matrix layout retains the legacy pretty presentation");
    string_free(out);
    string_free(expected);
    expected = mat_to_text(matrix, MAT_STRING_LAYOUT_SCIENTIFIC);
    out = fmt_string("{matrix:scientific_layout}", FMT(matrix));
    TEST_ASSERT_TRUE(out && expected && !strcmp(string_c_str(out), string_c_str(expected)),
                     "matrix layout retains the legacy scientific presentation");
    string_free(out);
    string_free(expected);
    expected = expr_to_text(expression, style_FUNCTION);
    out = fmt_string("{expression:function}", FMT(expression));
    TEST_ASSERT_TRUE(out && expected && !strcmp(string_c_str(out), string_c_str(expected)),
                     "expression formatting preserves native algebra and bindings");
    string_free(out);
    string_free(expected);
    mat_free(matrix);
    expr_free(expression);
}

static void test_native_adapter_layout(void)
{
    qfloat_t value = qf_from_double(1.25);
    fmt_arg_t argument = FMT(value);
    value = qf_from_double(2.5);
    fmt_t *format = fmt_compile("{value:+08.2f}", &argument, 1u, NULL);
    string_t *out = fmt_render(format, &argument, 1u, NULL);
    TEST_ASSERT_TRUE(out && !strcmp(string_c_str(out), "+0001.25"), "native adapter capture copies scalar components");
    string_free(out);
    argument = FMT(value);
    out = fmt_render(format, &argument, 1u, NULL);
    TEST_ASSERT_TRUE(out && !strcmp(string_c_str(out), "+0002.50"), "native scalar payloads update in a compiled layout");
    string_free(out);
    argument = fmt_named("value", fmt_arg_qcomplex(QC_ONE));
    out = fmt_render(format, &argument, 1u, NULL);
    TEST_ASSERT_TRUE(!out && fmt_last_error()->code == FMT_ERROR_TYPE,
                     "a compiled native layout rejects a different mathematical adapter");
    string_free(out);
    fmt_free(format);
    out = fmt_string("{}", fmt_arg_matrix(NULL));
    TEST_ASSERT_TRUE(!out && fmt_last_error()->code == FMT_ERROR_ARGUMENT, "the matrix adapter checks NULL values");
    string_free(out);
    out = fmt_string("{}", fmt_arg_expression(NULL));
    TEST_ASSERT_TRUE(!out && fmt_last_error()->code == FMT_ERROR_ARGUMENT, "the expression adapter checks NULL values");
    string_free(out);
}

static void test_stream_and_invalid_text(void)
{
    FILE *stream = tmpfile();
    TEST_ASSERT_TRUE(stream != NULL, "create a disposable stream");
    TEST_ASSERT_TRUE(fmt_fprint(stream, "{missing}") == -1 && ftell(stream) == 0L,
                     "format failures write nothing to streams");
    const char *word = "Ω";
    TEST_ASSERT_TRUE(fmt_fprint(stream, "{}", FMT(word)) == 2 && ftell(stream) == 2L,
                     "stream counts measure encoded bytes");
    rewind(stream);
    char bytes[3] = {0};
    TEST_ASSERT_TRUE(fread(bytes, 1u, 2u, stream) == 2u && !strcmp(bytes, word), "stream UTF-8 is preserved");
    fclose(stream);
    TEST_ASSERT_TRUE(fmt_fprint(NULL, "text") == -1 && fmt_last_error()->code == FMT_ERROR_ARGUMENT,
                     "NULL streams are checked");
    const char invalid[] = {'x', (char)0x80, '\0'};
    fmt_error_t error;
    fmt_t *format = fmt_compile(invalid, NULL, 0u, &error);
    TEST_ASSERT_TRUE(!format && error.code == FMT_ERROR_SYNTAX, "invalid UTF-8 templates are rejected");
    fmt_arg_t invalid_name = fmt_named(invalid, FMT(1));
    format = fmt_compile("{}", &invalid_name, 1u, &error);
    TEST_ASSERT_TRUE(!format && error.code == FMT_ERROR_ARGUMENT, "invalid UTF-8 binding names are rejected");
    string_t *out = fmt_string("{}", FMT(invalid));
    TEST_ASSERT_TRUE(!out && fmt_last_error()->code == FMT_ERROR_ARGUMENT, "invalid UTF-8 arguments are rejected");
    string_t *raw = string_new();
    TEST_ASSERT_TRUE(raw && string_append_bytes(raw, invalid, sizeof(invalid) - 1u) == 0, "prepare raw UTF-8 input");
    out = fmt_string("{:.1s}", FMT(raw));
    TEST_ASSERT_TRUE(!out && fmt_last_error()->code == FMT_ERROR_ARGUMENT,
                     "native strings are validated before character-based truncation");
    string_free(raw);
    out = fmt_string("{:c}", FMT(0));
    TEST_ASSERT_TRUE(out && string_byte_length(out) == 1u && string_c_str(out)[0] == '\0',
                     "embedded NUL output is counted rather than lost");
    string_free(out);
}

static ssize_t test_reject_write(void *cookie, const char *bytes, size_t count)
{
    (void)cookie;
    (void)bytes;
    (void)count;
    errno = EIO;
    return 0;
}

static void test_stream_failure(void)
{
    cookie_io_functions_t functions = {.write = test_reject_write};
    FILE *stream = fopencookie(NULL, "w", functions);
    TEST_ASSERT_TRUE(stream && setvbuf(stream, NULL, _IONBF, 0u) == 0, "create a deterministic failing stream");
    int result = fmt_fprint(stream, "output {}", FMT(42));
    bool ok = result == -1 && fmt_last_error()->code == FMT_ERROR_IO && ferror(stream);
    fclose(stream);
    TEST_ASSERT_TRUE(ok, "failed stream writes report checked I/O errors");
}

typedef struct {
    const fmt_t *format;
    bool passed;
} test_format_thread_t;

static void *test_format_worker(void *data)
{
    test_format_thread_t *worker = data;
    int n = 23;
    fmt_arg_t arguments[] = {FMT(n)};
    string_t *out = fmt_render(worker->format, arguments, 1u, NULL);
    worker->passed = out && !strcmp(string_c_str(out), "0023") && fmt_last_error()->code == FMT_OK;
    string_free(out);
    FILE *stream = tmpfile();
    bool printed = stream && fmt_fprint(stream, "{n:04d}", FMT(n)) == 4 &&
                   fmt_fprint(stream, "{n:04d}", FMT(n)) == 4;
    if (stream) {
        rewind(stream);
        char bytes[9] = {0};
        printed = printed && fread(bytes, 1u, 8u, stream) == 8u && !strcmp(bytes, "00230023");
        fclose(stream);
    }
    worker->passed = worker->passed && printed;
    out = fmt_string("{missing}");
    worker->passed = worker->passed && !out && fmt_last_error()->code == FMT_ERROR_MISSING;
    return NULL;
}

static void test_thread_local_diagnostics(void)
{
    int n = 1;
    fmt_arg_t arguments[] = {FMT(n)};
    fmt_t *format = fmt_compile("{n:04d}", arguments, 1u, NULL);
    TEST_ASSERT_TRUE(format != NULL, "compile a shareable immutable template");
    test_format_thread_t worker = {format, false};
    pthread_t thread;
    TEST_ASSERT_TRUE(pthread_create(&thread, NULL, test_format_worker, &worker) == 0, "start one rendering worker");
    TEST_ASSERT_TRUE(pthread_join(thread, NULL) == 0 && worker.passed, "render across threads with independent errors");
    TEST_ASSERT_TRUE(fmt_last_error()->code == FMT_OK, "worker failures do not replace this thread's diagnostic");
    fmt_free(format);
}

static void test_stream_plan_reuse(void)
{
    char pattern[48] = "{value:d}";
    char name[16] = "value";
    char *written = NULL;
    size_t length = 0u;
    FILE *stream = open_memstream(&written, &length);
    TEST_ASSERT_TRUE(stream != NULL, "create a stream for mutable template and layout checks");
    if (!stream)
        return;
    bool ok = fmt_fprint(stream, pattern, fmt_named(name, FMT(12))) == 2;
    strcpy(pattern, "{value:#x}");
    ok = ok && fmt_fprint(stream, pattern, fmt_named(name, FMT(31))) == 4;
    strcpy(pattern, "{label}");
    strcpy(name, "label");
    ok = ok && fmt_fprint(stream, pattern, fmt_named(name, FMT("Ada"))) == 3;
    ok = ok && fmt_fprint(stream, pattern, fmt_named(name, FMT(42))) == 2;
    strcpy(name, "other");
    ok = ok && fmt_fprint(stream, pattern, fmt_named(name, FMT(99))) == -1 &&
         fmt_last_error()->code == FMT_ERROR_MISSING;
    ok = fclose(stream) == 0 && ok && length == 11u && !strcmp(written, "120x1fAda42");
    free(written);
    TEST_ASSERT_TRUE(ok, "stream plan reuse compares current template text, binding names and value categories");
}

static string_t *test_recursive_stream(const void *data, const fmt_options_t *options, fmt_error_t *error)
{
    (void)options;
    (void)error;
    FILE *stream = *(FILE *const *)data;
    for (unsigned i = 0; i < 32u; ++i) {
        char pattern[48];
        snprintf(pattern, sizeof(pattern), "inner %u: {}", i);
        if (fmt_fprint(stream, pattern, FMT(i)) < 0)
            return NULL;
    }
    return string_new_with("payload");
}

static void test_stream_recursive_callbacks(void)
{
    char *written = NULL;
    size_t length = 0u;
    FILE *stream = open_memstream(&written, &length);
    FILE *inner = tmpfile();
    bool ok = stream && inner;
    if (ok)
        ok = fmt_fprint(stream, "before {value} after", fmt_arg_custom("value", &inner, test_recursive_stream)) == 20;
    if (stream)
        ok = fclose(stream) == 0 && ok;
    if (inner)
        ok = fclose(inner) == 0 && ok;
    ok = ok && length == 20u && !strcmp(written, "before payload after");
    free(written);
    TEST_ASSERT_TRUE(ok, "recursive stream callbacks cannot evict an active outer template");
}

static void test_stream_storage_boundaries(void)
{
    char text[4097], pattern[600];
    memset(text, 'x', sizeof(text) - 1u);
    text[sizeof(text) - 1u] = '\0';
    memset(pattern, 'p', sizeof(pattern) - 4u);
    memcpy(pattern + sizeof(pattern) - 4u, "{}", 3u);
    char *written = NULL;
    size_t length = 0u;
    FILE *stream = open_memstream(&written, &length);
    bool ok = stream && fmt_fprint(stream, "{}", FMT(text)) == 4096;
    if (ok)
        ok = fmt_fprint(stream, pattern, FMT(7)) == 597;
    if (stream)
        ok = fclose(stream) == 0 && ok;
    ok = ok && length == 4693u && !memcmp(written, text, 4096u) && written[length - 1u] == '7';
    for (size_t i = 4096u; ok && i + 1u < length; ++i)
        ok = written[i] == 'p';
    free(written);
    TEST_ASSERT_TRUE(ok, "large outputs grow safely and large templates use temporary plans");
}

static string_t *test_stream_adapter_render(const void *value, const fmt_options_t *options, fmt_error_t *error)
{
    (void)value;
    (void)options;
    (void)error;
    return string_new_with("payload");
}

static bool test_stream_adapter_reject(const fmt_options_t *options, fmt_error_t *error)
{
    (void)options;
    *error = (fmt_error_t){FMT_ERROR_TYPE, SIZE_MAX, SIZE_MAX, "replacement adapter rejects this presentation"};
    return false;
}

static void test_stream_adapter_lifetimes(void)
{
    fmt_adapter_t adapter = {.render = test_stream_adapter_render};
    int value = 0;
    char *written = NULL;
    size_t length = 0u;
    FILE *stream = open_memstream(&written, &length);
    bool ok = stream && fmt_fprint(stream, "{:tag}", fmt_arg_adapter(&adapter, &value, sizeof(value))) == 7;
    /* The first one-shot call's borrowed adapter lifetime has ended; the same storage may hold another adapter. */
    adapter.validate = test_stream_adapter_reject;
    if (ok)
        ok = fmt_fprint(stream, "{:tag}", fmt_arg_adapter(&adapter, &value, sizeof(value))) == -1 &&
             fmt_last_error()->code == FMT_ERROR_TYPE;
    if (stream)
        ok = fclose(stream) == 0 && ok;
    ok = ok && length == 7u && !strcmp(written, "payload");
    free(written);
    TEST_ASSERT_TRUE(ok, "one-shot stream calls do not extend borrowed adapter lifetimes across calls");
}

static void test_excel_compiled(void)
{
    string_t *pattern = string_new_with("{value:0.00;[Red](0.00);\"zero\"}");
    double value = 1.25;
    fmt_arg_t arguments[] = {FMT(value)};
    fmt_t *format = fmt_compile(string_c_str(pattern), arguments, 1u, NULL);
    string_free(pattern);
    string_t *positive = fmt_render(format, arguments, 1u, NULL);
    value = -2.5;
    arguments[0] = FMT(value);
    string_t *negative = fmt_render(format, arguments, 1u, NULL);
    value = -0.0;
    arguments[0] = FMT(value);
    string_t *zero = fmt_render(format, arguments, 1u, NULL);
    TEST_ASSERT_TRUE(positive && negative && zero && !strcmp(string_c_str(positive), "1.25") &&
                     !strcmp(string_c_str(negative), "\033[31m(2.50)\033[0m") && !strcmp(string_c_str(zero), "zero"),
                     "compiled Excel sections own their code and select changing values without recompilation");
    string_free(positive);
    string_free(negative);
    string_free(zero);
    fmt_free(format);
    const datetime_t *date = NULL;
    const equation_t *equation = NULL;
    const TeX_t *notation = NULL;
    string_t *out = fmt_string("{date}", FMT(date));
    TEST_ASSERT_TRUE(!out && fmt_last_error()->code == FMT_ERROR_ARGUMENT, "NULL datetime is a checked argument error");
    string_free(out);
    out = fmt_string("{equation}", FMT(equation));
    TEST_ASSERT_TRUE(!out && fmt_last_error()->code == FMT_ERROR_ARGUMENT, "NULL equation is a checked argument error");
    string_free(out);
    out = fmt_string("{notation}", FMT(notation));
    TEST_ASSERT_TRUE(!out && fmt_last_error()->code == FMT_ERROR_ARGUMENT, "NULL notation is a checked argument error");
    string_free(out);
}

int tests_main(void)
{
    TEST_RUN_IN_GROUP(test_format_locale_snapshot, tests, "format,locale,ownership");
    TEST_RUN_IN_GROUP(test_format_locale_catalogue, tests, "format,locale,catalogue");
    TEST_RUN_IN_GROUP(test_format_locale_cache, tests, "format,locale,cache");
    TEST_RUN_IN_GROUP(test_format_core, tests, "format,core,adapter");
    TEST_RUN_IN_GROUP(test_format_TeX_header, tests, "format,headers,TeX");
    TEST_RUN_IN_GROUP(test_format_equation_header, tests, "format,headers,equation");
    TEST_RUN_IN_GROUP(test_format_datetime_header, tests, "format,headers,datetime");
    TEST_RUN_IN_GROUP(test_format_datetime_format_first, tests, "format,headers,datetime");
    TEST_RUN_IN_GROUP(test_format_qfloat_header, tests, "format,headers,qfloat");
    TEST_RUN_IN_GROUP(test_format_qcomplex_header, tests, "format,headers,qcomplex");
    TEST_RUN_IN_GROUP(test_format_number_header, tests, "format,headers,number");
    TEST_RUN_IN_GROUP(test_format_matrix_header, tests, "format,headers,matrix");
    TEST_RUN_IN_GROUP(test_format_expression_header, tests, "format,headers,expression");
    TEST_RUN_IN_GROUP(test_format_render, tests, "format,reference");
    TEST_RUN_IN_GROUP(test_format_errors, tests, "format,errors");
    TEST_RUN_IN_GROUP(test_macro_evaluation, tests, "format,types");
    TEST_RUN_IN_GROUP(test_compiled_layout, tests, "format,compiled");
    TEST_RUN_IN_GROUP(test_custom_and_append, tests, "format,custom,atomic");
    TEST_RUN_IN_GROUP(test_native_objects, tests, "format,native");
    TEST_RUN_IN_GROUP(test_native_adapter_layout, tests, "format,native,compiled");
    TEST_RUN_IN_GROUP(test_stream_and_invalid_text, tests, "format,streams,utf8");
    TEST_RUN_IN_GROUP(test_stream_failure, tests, "format,streams,errors");
    TEST_RUN_IN_GROUP(test_thread_local_diagnostics, tests, "format,threads");
    TEST_RUN_IN_GROUP(test_stream_plan_reuse, tests, "format,streams,cache");
    TEST_RUN_IN_GROUP(test_stream_recursive_callbacks, tests, "format,streams,callbacks");
    TEST_RUN_IN_GROUP(test_stream_storage_boundaries, tests, "format,streams,boundaries");
    TEST_RUN_IN_GROUP(test_stream_adapter_lifetimes, tests, "format,streams,adapters");
    TEST_RUN_IN_GROUP(test_format_streams, tests, "format,streams,database");
    TEST_RUN_IN_GROUP(test_format_stream_errors, tests, "format,streams,database,errors");
    TEST_RUN_IN_GROUP(test_excel_compiled, tests, "format,excel,compiled,lifetime");
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_format_named, readme_examples, "format,readme,output");
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_format_compiled, readme_examples, "format,readme,output");
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_format_positions, readme_examples, "format,readme,output");
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_format_string, readme_examples, "format,readme,output");
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_format_builder, readme_examples, "format,readme,output");
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_format_native, readme_examples, "format,readme,output");
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_format_custom, readme_examples, "format,readme,output");
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_format_error, readme_examples, "format,readme,output");
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_format_emoji, readme_examples, "format,readme,output");
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_format_owned_string, readme_examples, "format,readme,output");
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_format_datetime, readme_examples, "format,readme,output");
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_format_locales, readme_examples, "format,readme,locale,output");
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_datetime_lingua_new_year, readme_examples, "datetime,readme,locale,output");
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_format_symbolic, readme_examples, "format,readme,output");
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_format_excel, readme_examples, "format,readme,output");
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_format_date_literals, readme_examples, "format,readme,output");
    return TESTS_EXIT_CODE();
}
