/**
 * @file test_format_core.c
 * @brief Formatting-core boundary and stateful generic-adapter regression checks.
 *
 * Includes no mathematical header or integration layer. A mutable local fixture verifies copied adapter payloads,
 * compilation-time option validation, immutable type identity and checked capture failures using public APIs only.
 */
#include <string.h>

#include "format-t.h"
#include "test_harness.h"
#define MARS_TEST_FORMAT_INTERNAL_ACCESS
#include "test_format_internal.h"

#if defined(MARS_QFLOAT_H) || defined(MARS_QCOMPLEX_H) || defined(MARS_NUMBER_H) || \
    defined(MARS_MATRIX_H) || defined(MARS_EXPRESSION_H)
#error "The core formatting interface must not include mathematical module headers."
#endif

static unsigned test_adapter_validations;

static bool test_adapter_validate(const fmt_options_t *options, fmt_error_t *error)
{
    ++test_adapter_validations;
    if (*options->presentation && strcmp(options->presentation, "units")) {
        if (error)
            *error = (fmt_error_t){FMT_ERROR_TYPE, SIZE_MAX, SIZE_MAX, "unsupported fixture presentation"};
        return false;
    }
    return true;
}

static string_t *test_adapter_render(const void *data, const fmt_options_t *options, fmt_error_t *error)
{
    (void)options;
    (void)error;
    int value;
    memcpy(&value, data, sizeof(value));
    return fmt_string("{}", FMT(value));
}

static string_t *test_adapter_failure(const void *data, const fmt_options_t *options, fmt_error_t *error)
{
    (void)data;
    (void)options;
    if (error)
        *error = (fmt_error_t){FMT_ERROR_RANGE, SIZE_MAX, SIZE_MAX, "fixture render failure"};
    return NULL;
}

void test_format_core(void)
{
    static const fmt_adapter_t first = {test_adapter_validate, test_adapter_render, true};
    static const fmt_adapter_t second = {test_adapter_validate, test_adapter_render, true};
    static const fmt_adapter_t text = {NULL, test_adapter_render, false};
    static const fmt_adapter_t failure = {NULL, test_adapter_failure, false};
    static const fmt_adapter_t missing = {NULL, NULL, false};
    int value = 7;
    fmt_arg_t argument = fmt_arg_adapter(&first, &value, sizeof(value));
    value = 19;
    test_adapter_validations = 0;
    fmt_error_t error;
    fmt_t *format = fmt_compile("{:+06units}", &argument, 1u, &error);
    TEST_ASSERT_TRUE(format && test_adapter_validations == 1u, "an adapter validates options once at compilation");
    string_t *out = fmt_render(format, &argument, 1u, &error);
    TEST_ASSERT_TRUE(out && !strcmp(string_c_str(out), "+00007"),
                     "copied adapter data survives changes to its original value and receives numeric padding");
    string_free(out);
    argument = fmt_arg_adapter(&first, &value, sizeof(value));
    out = fmt_render(format, &argument, 1u, &error);
    TEST_ASSERT_TRUE(out && !strcmp(string_c_str(out), "+00019") && test_adapter_validations == 1u,
                     "compiled rendering accepts new payloads without repeating presentation validation");
    string_free(out);
    argument = fmt_arg_adapter(&second, &value, sizeof(value));
    out = fmt_render(format, &argument, 1u, &error);
    TEST_ASSERT_TRUE(!out && error.code == FMT_ERROR_TYPE && error.argument == 0u && error.offset == 0u,
                     "a different adapter identity is rejected even when it has the same callbacks and payload size");
    string_free(out);
    fmt_free(format);
    argument = fmt_arg_adapter(&first, &value, sizeof(value));
    format = fmt_compile("a{:.2invalid}", &argument, 1u, &error);
    TEST_ASSERT_TRUE(!format && error.code == FMT_ERROR_TYPE && error.offset == 1u && error.argument == 0u,
                     "adapter validation errors acquire the field's byte offset and argument index");
    fmt_free(format);
    fmt_code_t previous = fmt_last_error()->code;
    fmt_arg_t invalid[] = {
        fmt_arg_adapter(NULL, &value, sizeof(value)),
        fmt_arg_adapter(&missing, &value, sizeof(value)),
        fmt_arg_adapter(&first, NULL, sizeof(value)),
        fmt_arg_adapter(&first, &value, FMT_ADAPTER_CAPACITY + 1u),
    };
    TEST_ASSERT_TRUE(fmt_last_error()->code == previous, "invalid captures leave thread-local diagnostics unchanged");
    for (size_t i = 0; i < sizeof(invalid) / sizeof(*invalid); ++i) {
        format = fmt_compile("{}", &invalid[i], 1u, &error);
        TEST_ASSERT_TRUE(!format && error.code == FMT_ERROR_TYPE, "invalid captures are rejected before rendering");
        fmt_free(format);
    }
    unsigned char bytes[FMT_ADAPTER_CAPACITY] = {0};
    argument = fmt_arg_adapter(&first, bytes, sizeof(bytes));
    out = fmt_string("{}", argument);
    TEST_ASSERT_TRUE(out && !strcmp(string_c_str(out), "0"), "a full-capacity adapter payload remains valid");
    string_free(out);
    argument = fmt_arg_adapter(&first, NULL, 0u);
    out = fmt_string("{}", argument);
    TEST_ASSERT_TRUE(out && !strcmp(string_c_str(out), "0"), "an empty adapter payload is zero-filled");
    string_free(out);
    argument = fmt_arg_adapter(&text, &value, sizeof(value));
    out = fmt_string("{:arbitrary}", argument);
    TEST_ASSERT_TRUE(out && !strcmp(string_c_str(out), "19"), "a NULL validator permits extension presentation names");
    string_free(out);
    out = fmt_string("{:+}", argument);
    TEST_ASSERT_TRUE(!out && fmt_last_error()->code == FMT_ERROR_TYPE, "text adapters reject numerical options");
    string_free(out);
    argument = fmt_arg_adapter(&failure, &value, sizeof(value));
    out = fmt_string("[]{value}", fmt_named("value", argument));
    TEST_ASSERT_TRUE(!out && fmt_last_error()->code == FMT_ERROR_RANGE && fmt_last_error()->offset == 2u &&
                     fmt_last_error()->argument == 0u, "adapter rendering propagates a located custom diagnostic");
    string_free(out);
}
