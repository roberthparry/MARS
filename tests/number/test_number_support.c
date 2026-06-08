#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_number.h"

static int number_validity_equal(const void *actual,
                                 const void *expected,
                                 void *ctx)
{
    (void)ctx;
    return num_eq(*(const number_t *)actual, *(const number_t *)expected);
}

static int number_validity_format(const void *value,
                                  string_t *out,
                                  void *ctx)
{
    string_t *text;

    (void)ctx;
    if (!out)
        return -1;

    text = num_to_string(*(const number_t *)value);
    if (!text)
        return string_append_cstr(out, "<num_to_string failed>");

    if (string_append_string(out, text) != 0) {
        string_free(text);
        return -1;
    }
    string_free(text);
    return 0;
}

const test_validity_contract_t *number_validity_contract_exact(void)
{
    static const test_validity_contract_t contract =
        TEST_VALIDITY_CONTRACT("number-exact",
                               number_validity_equal,
                               number_validity_format,
                               NULL);

    return &contract;
}

void assert_number_string(const char *label,
                          number_t number,
                          const char *expected_text)
{
    string_t *got;

    got = num_to_string(number);
    ASSERT_NOT_NULL(got);
    printf(C_WHITE C_BOLD "%s" C_RESET "\n", label ? label : "<unspecified>");
    printf("    expected = %s\n", expected_text);
    printf("    got      = %s\n\n", string_c_str(got));
    ASSERT_TRUE(strcmp(string_c_str(got), expected_text) == 0);
    string_free(got);
}

void assert_number_string_prefix(const char *label,
                                 number_t number,
                                 const char *expected_prefix)
{
    char *got;
    size_t prefix_len;
    char format[32];
    int written;

    prefix_len = strlen(expected_prefix);
    snprintf(format, sizeof(format), "%%.%zun", prefix_len);
    written = num_sprintf(NULL, 0u, format, number);
    ASSERT_TRUE(written >= 0);
    got = malloc((size_t)written + 1u);
    ASSERT_NOT_NULL(got);
    ASSERT_EQ_INT(num_sprintf(got, (size_t)written + 1u, format, number),
                  written);
    printf(C_WHITE C_BOLD "%s" C_RESET "\n", label ? label : "<unspecified>");
    printf("    prefix   = %s\n", expected_prefix);
    printf("    got      = %s\n\n", got);
    ASSERT_TRUE(strncmp(got, expected_prefix, prefix_len) == 0);
    free(got);
}
