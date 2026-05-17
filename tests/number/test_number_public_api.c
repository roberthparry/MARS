#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_number.h"

void run_number_public_api_tests(void)
{
    printf(C_CYAN "Testing number_t as a standalone public API...\n" C_RESET);

    {
        number_t a = num_create_from_string("2");
        number_t b = num_create_from_string("3");
        number_t frac = num_create_from_string("5/6");
        number_t dec = num_create_from_string("1.25");
        number_t z = num_create_from_string("1 + 2i");

        number_t sum = num_add(a, b);
        number_t quot = num_div(a, b);
        number_t product = num_mul(sum, frac);
        number_t logged = num_log(dec);
        number_t thousand = num_create_from_long(1000);
        number_t log10_thousand = num_log10(thousand);
        number_t rooted = num_sqrt(z);
        number_t constant = NUM_PI;
        number_t i_constant = NUM_I;
        number_t cloned = num_clone(product);
        number_t pi_cloned = num_clone(constant);
        number_t i_cloned = num_clone(i_constant);

        assert_number_string("num_add(\"2\", \"3\")", sum, "5");
        assert_number_string("num_div(\"2\", \"3\")", quot, "⅔");
        assert_number_string("num_mul(\"5\", \"5/6\")", product, "²⁵⁄₆");
        assert_number_string("num_clone(\"25/6\")", cloned, "²⁵⁄₆");
        assert_number_string("num_log10(1000)", log10_thousand, "3");
        assert_number_string_prefix("NUM_PI", constant,
                                    "3.141592653589793238462643383279");
        assert_number_string_prefix("num_clone(NUM_PI)", pi_cloned,
                                    "3.141592653589793238462643383279");
        assert_number_string("num_clone(NUM_I)", i_cloned, "0 + 1i");

        ASSERT_TRUE(num_is_real(a));
        ASSERT_TRUE(num_is_real(frac));
        ASSERT_TRUE(num_is_real(dec));
        ASSERT_TRUE(!num_is_real(z));
        ASSERT_TRUE(num_is_real(logged));
        ASSERT_TRUE(!num_is_real(rooted));
        ASSERT_TRUE(num_is_exact(a));
        ASSERT_TRUE(num_is_exact(frac));
        ASSERT_TRUE(!num_is_exact(dec));
        ASSERT_NUMBER_EQ(product, cloned);

        num_destroy(&a);
        num_destroy(&b);
        num_destroy(&frac);
        num_destroy(&dec);
        num_destroy(&z);
        num_destroy(&sum);
        num_destroy(&quot);
        num_destroy(&product);
        num_destroy(&logged);
        num_destroy(&thousand);
        num_destroy(&log10_thousand);
        num_destroy(&rooted);
        num_destroy(&constant);
        num_destroy(&i_constant);
        num_destroy(&cloned);
        num_destroy(&pi_cloned);
        num_destroy(&i_cloned);
    }

    {
        num_scope_t *outer = NULL;
        num_scope_t *inner = NULL;
        number_t one_third = num_create_from_string("1/3");
        number_t one_sixth = num_create_from_string("1/6");
        number_t two = num_create_from_long(2);
        number_t five = num_create_from_long(5);
        number_t one = num_create_from_long(1);
        number_t sum;
        number_t detached;
        number_t outer_sum;
        number_t inner_sum;
        number_t inner_promoted;

        ASSERT_TRUE(!num_scope_is_active());

        outer = num_scope_enter();
        ASSERT_TRUE(num_scope_is_active());

        sum = num_add(one_third, one_sixth);
        detached = num_scope_detach(sum);
        assert_number_string("num_scope_detach(scoped 1/3 + 1/6)", detached, "½");

        num_scope_leave(&outer);
        ASSERT_TRUE(!num_scope_is_active());

        assert_number_string("detached scoped result survives leave", detached, "½");
        num_destroy(&detached);

        outer = num_scope_enter();
        outer_sum = num_add(two, five);
        inner = num_scope_enter();
        inner_sum = num_add(outer_sum, one);
        inner_promoted = num_scope_detach(inner_sum);
        num_scope_leave(&inner);

        assert_number_string("nested detached scoped result", inner_promoted, "8");

        num_destroy(&outer_sum);
        num_scope_leave(&outer);
        ASSERT_TRUE(!num_scope_is_active());

        num_destroy(&inner_promoted);
        num_destroy(&one);
        num_destroy(&five);
        num_destroy(&two);
        num_destroy(&one_third);
        num_destroy(&one_sixth);
    }

    {
        number_t a = num_create_from_string("2");
        number_t b = num_create_from_string("5/6");
        number_t c = num_add(a, b);
        char *text = num_to_string(c);

        ASSERT_NOT_NULL(text);
        printf(C_WHITE C_BOLD "README example" C_RESET "\n");
        printf("    2 + 5/6 = %s\n\n", text ? text : "(null)");
        ASSERT_TRUE(text && strcmp(text, "¹⁷⁄₆") == 0);

        free(text);
        num_destroy(&a);
        num_destroy(&b);
        num_destroy(&c);
    }
}
