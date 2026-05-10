#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_number.h"

void run_number_parse_tests(void)
{
    printf(C_CYAN "Testing number construction and parsing...\n" C_RESET);

    {
        number_t integer = num_create_from_string("42");
        number_t fraction = num_create_from_string("5/6");
        number_t decimal = num_create_from_string("32.123");
        number_t complex_value = num_create_from_string("1 + 2i");
        number_t unit_imag = num_create_from_string("1 + i");
        number_t rational_complex = num_create_from_string("1/2 - 3/2i");
        number_t paren_imag = num_create_from_string("1e-23 + (2.3e12)i");
        number_t plain_imag = num_create_from_string("1e-23 + 2.3e12i");
        number_t direct = num_create_from_double(1.25);
        number_t set_value = num_new();
        char *text;

        assert_number_string("num_create_from_string(\"42\")", integer, "42");
        assert_number_string("num_create_from_string(\"5/6\")", fraction, "5/6");

        text = num_to_string(decimal);
        ASSERT_NOT_NULL(text);
        printf(C_WHITE C_BOLD "num_create_from_string(\"32.123\")" C_RESET "\n");
        printf("    selected = multiprecision real\n");
        printf("    got      = %s\n\n", text);
        ASSERT_TRUE(num_is_real(decimal));
        ASSERT_TRUE(!num_is_exact(decimal));
        free(text);

        ASSERT_TRUE(!num_is_real(complex_value));
        assert_number_string("num_create_from_string(\"1 + i\")", unit_imag, "1 + 1i");
        assert_number_string("num_create_from_string(\"1/2 - 3/2i\")",
            rational_complex, "0.5 - 1.5i");
        ASSERT_TRUE(num_eq(paren_imag, plain_imag));
        ASSERT_EQ_INT(num_set_from_string(&set_value, "1 - i"), 0);
        assert_number_string("num_set_from_string(\"1 - i\")", set_value, "1 - 1i");

        text = num_to_string(direct);
        ASSERT_NOT_NULL(text);
        printf(C_WHITE C_BOLD "num_create_from_double(1.25)" C_RESET "\n");
        printf("    expected = 1.25\n");
        printf("    got      = %s\n\n", text);
        ASSERT_TRUE(strcmp(text, "1.25") == 0);
        free(text);

        num_destroy(&integer);
        num_destroy(&fraction);
        num_destroy(&decimal);
        num_destroy(&complex_value);
        num_destroy(&unit_imag);
        num_destroy(&rational_complex);
        num_destroy(&paren_imag);
        num_destroy(&plain_imag);
        num_destroy(&direct);
        num_destroy(&set_value);
    }
}
