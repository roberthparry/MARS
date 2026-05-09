#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_number.h"

void run_number_parse_tests(void)
{
    printf(C_CYAN "Testing number construction and parsing...\n" C_RESET);

    {
        number_t integer = num_create_string("42");
        number_t fraction = num_create_string("5/6");
        number_t decimal = num_create_string("32.123");
        number_t complex_value = num_create_string("1 + 2i");
        number_t direct = num_create_double(1.25);
        char *text;

        assert_number_string("num_create_string(\"42\")", integer, "42");
        assert_number_string("num_create_string(\"5/6\")", fraction, "5/6");

        text = num_to_string(decimal);
        ASSERT_NOT_NULL(text);
        printf(C_WHITE C_BOLD "num_create_string(\"32.123\")" C_RESET "\n");
        printf("    selected = multiprecision real\n");
        printf("    got      = %s\n\n", text);
        ASSERT_TRUE(num_is_real(decimal));
        ASSERT_TRUE(!num_is_exact(decimal));
        free(text);

        ASSERT_TRUE(!num_is_real(complex_value));

        text = num_to_string(direct);
        ASSERT_NOT_NULL(text);
        printf(C_WHITE C_BOLD "num_create_double(1.25)" C_RESET "\n");
        printf("    expected = 1.25\n");
        printf("    got      = %s\n\n", text);
        ASSERT_TRUE(strcmp(text, "1.25") == 0);
        free(text);

        num_clear(&integer);
        num_clear(&fraction);
        num_clear(&decimal);
        num_clear(&complex_value);
        num_clear(&direct);
    }
}
