#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_number.h"

void assert_number_string(const char *label,
                          number_t number,
                          const char *expected_text)
{
    char *got;

    got = num_to_string(number);
    ASSERT_NOT_NULL(got);
    printf(C_WHITE C_BOLD "%s" C_RESET "\n", label ? label : "<unspecified>");
    printf("    expected = %s\n", expected_text);
    printf("    got      = %s\n\n", got);
    ASSERT_TRUE(strcmp(got, expected_text) == 0);
    free(got);
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
