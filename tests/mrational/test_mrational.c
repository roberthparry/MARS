#include <stdlib.h>
#include <string.h>

#include "mint.h"
#include "internal/mrational_internal.h"
#include "mrational.h"

#define TEST_CONFIG_MODE TEST_CONFIG_GLOBAL
#define TEST_CONFIG_MAIN
#include "test_harness.h"

static void assert_mr_string_label(const char *label,
                                   const mrational_t *rational,
                                   const char *expected)
{
    char *got = mr_to_string(rational);

    ASSERT_NOT_NULL(got);
    printf(C_WHITE C_BOLD "%s" C_RESET "\n", label ? label : "<unspecified>");
    printf("    expected = %s\n", expected);
    printf("    got      = %s\n\n", got);
    ASSERT_TRUE(strcmp(got, expected) == 0);
    free(got);
}

static void assert_mint_clone_string_label(const char *label,
                                           const mint_t *mint,
                                           const char *expected)
{
    char *got = mi_to_string(mint);

    ASSERT_NOT_NULL(got);
    printf(C_WHITE C_BOLD "%s" C_RESET "\n", label ? label : "<unspecified>");
    printf("    expected = %s\n", expected);
    printf("    got      = %s\n\n", got);
    ASSERT_TRUE(strcmp(got, expected) == 0);
    free(got);
}

void test_create_and_normalise(void)
{
    printf(C_CYAN "Testing construction and normalization...\n" C_RESET);
    mrational_t *zero = mr_new();
    mrational_t *whole = mr_create_long(-42);
    mrational_t *frac = mr_create_frac_long(6, -8);
    mrational_t *parsed = mr_create_string("  -10/20  ");

    ASSERT_NOT_NULL(zero);
    ASSERT_NOT_NULL(whole);
    ASSERT_NOT_NULL(frac);
    ASSERT_NOT_NULL(parsed);

    assert_mr_string_label("mr_new() -> 0", zero, "0");
    ASSERT_TRUE(mr_is_zero(zero));
    ASSERT_TRUE(mr_is_integer(zero));

    assert_mr_string_label("mr_create_long(-42)", whole, "-42");
    ASSERT_TRUE(mr_is_integer(whole));

    assert_mr_string_label("mr_create_frac_long(6, -8)", frac, "-3/4");
    ASSERT_TRUE(!mr_is_integer(frac));

    assert_mr_string_label("mr_create_string(\"  -10/20  \")", parsed, "-1/2");
    ASSERT_TRUE(!mr_is_integer(parsed));

    mr_free(zero);
    mr_free(whole);
    mr_free(frac);
    mr_free(parsed);
}

void test_setters_and_accessors(void)
{
    printf(C_CYAN "Testing setters and accessors...\n" C_RESET);
    mrational_t *r = mr_new();
    mint_t *src_num = mi_create_string("84");
    mint_t *src_den = mi_create_string("-126");
    const mint_t *num = NULL;
    const mint_t *den = NULL;

    ASSERT_NOT_NULL(r);
    ASSERT_NOT_NULL(src_num);
    ASSERT_NOT_NULL(src_den);
    ASSERT_EQ_INT(mr_set_frac_long(r, 14, 21), 0);
    assert_mr_string_label("mr_set_frac_long(r, 14, 21)", r, "2/3");

    num = mr_numerator(r);
    den = mr_denominator(r);
    ASSERT_NOT_NULL(num);
    ASSERT_NOT_NULL(den);
    assert_mint_clone_string_label("mr_numerator(2/3)", num, "2");
    assert_mint_clone_string_label("mr_denominator(2/3)", den, "3");
    num = den = NULL;

    ASSERT_EQ_INT(mr_set_string(r, "5"), 0);
    assert_mr_string_label("mr_set_string(r, \"5\")", r, "5");
    ASSERT_TRUE(mr_is_integer(r));

    ASSERT_EQ_INT(mr_set_mints(r, src_num, src_den), 0);
    assert_mr_string_label("mr_set_mints(r, 84, -126)", r, "-2/3");
    assert_mint_clone_string_label("source numerator unchanged", src_num, "84");
    assert_mint_clone_string_label("source denominator unchanged", src_den, "-126");

    ASSERT_EQ_INT(mr_set_string(r, "7/0"), -1);
    ASSERT_EQ_INT(mr_set_long(r, 0), 0);
    ASSERT_TRUE(mr_is_zero(r));
    ASSERT_EQ_INT(mr_set_frac_long(r, 1, 0), -1);

    mr_clear(r);
    assert_mr_string_label("mr_clear(r)", r, "0");
    mi_free(src_num);
    mi_free(src_den);
    mr_free(r);
}

void test_clone_compare_and_order(void)
{
    printf(C_CYAN "Testing clone, comparison, and ordering...\n" C_RESET);
    mrational_t *a = mr_create_frac_long(2, 3);
    mrational_t *b = mr_clone(a);
    mrational_t *c = mr_create_frac_long(3, 4);
    mrational_t *d = mr_create_frac_long(1, 3);

    ASSERT_NOT_NULL(a);
    ASSERT_NOT_NULL(b);
    ASSERT_NOT_NULL(c);
    ASSERT_NOT_NULL(d);

    ASSERT_TRUE(mr_eq(a, b));
    ASSERT_TRUE(mr_cmp(a, b) == 0);
    ASSERT_TRUE(mr_lt(a, c));
    ASSERT_TRUE(mr_le(a, c));
    ASSERT_TRUE(mr_gt(c, a));
    ASSERT_TRUE(mr_ge(c, a));

    ASSERT_EQ_INT(mr_add(b, d), 0);
    assert_mr_string_label("original a after mr_clone + mr_add(b, d)", a, "2/3");
    assert_mr_string_label("mr_add(clone(2/3), 1/3)", b, "1");

    mr_free(a);
    mr_free(b);
    mr_free(c);
    mr_free(d);
}

void test_arithmetic(void)
{
    printf(C_CYAN "Testing arithmetic operations...\n" C_RESET);
    mrational_t *a = mr_create_frac_long(1, 2);
    mrational_t *b = mr_create_frac_long(1, 3);
    mrational_t *zero = mr_create_long(0);

    ASSERT_NOT_NULL(a);
    ASSERT_NOT_NULL(b);
    ASSERT_NOT_NULL(zero);

    ASSERT_EQ_INT(mr_add(a, b), 0);
    assert_mr_string_label("mr_add(1/2, 1/3)", a, "5/6");

    ASSERT_EQ_INT(mr_sub(a, b), 0);
    assert_mr_string_label("mr_sub(5/6, 1/3)", a, "1/2");

    ASSERT_EQ_INT(mr_mul(a, b), 0);
    assert_mr_string_label("mr_mul(1/2, 1/3)", a, "1/6");

    ASSERT_EQ_INT(mr_div(a, b), 0);
    assert_mr_string_label("mr_div(1/6, 1/3)", a, "1/2");

    ASSERT_EQ_INT(mr_neg(a), 0);
    assert_mr_string_label("mr_neg(1/2)", a, "-1/2");

    ASSERT_EQ_INT(mr_abs(a), 0);
    assert_mr_string_label("mr_abs(-1/2)", a, "1/2");

    ASSERT_EQ_INT(mr_inv(a), 0);
    assert_mr_string_label("mr_inv(1/2)", a, "2");

    ASSERT_EQ_INT(mr_inv(zero), -1);

    mr_free(a);
    mr_free(b);
    mr_free(zero);
}

void test_large_values(void)
{
    printf(C_CYAN "Testing large-value rationals...\n" C_RESET);
    mrational_t *a = mr_create_string("18446744073709551616/3");
    mrational_t *b = mr_create_string("5/7");
    mint_t *num = mi_create_string("18446744073709551616");
    mint_t *den = mi_create_string("3");
    mrational_t *c = mr_create_mints(num, den);

    ASSERT_NOT_NULL(a);
    ASSERT_NOT_NULL(b);
    ASSERT_NOT_NULL(num);
    ASSERT_NOT_NULL(den);
    ASSERT_NOT_NULL(c);

    ASSERT_EQ_INT(mr_mul(a, b), 0);
    assert_mr_string_label("mr_mul(18446744073709551616/3, 5/7)",
                           a,
                           "92233720368547758080/21");

    ASSERT_EQ_INT(mr_div(a, b), 0);
    assert_mr_string_label("mr_div(previous, 5/7)", a, "18446744073709551616/3");
    assert_mr_string_label("mr_create_mints(18446744073709551616, 3)",
                           c,
                           "18446744073709551616/3");

    mr_free(a);
    mr_free(b);
    mr_free(c);
    mi_free(num);
    mi_free(den);
}

void test_bernoulli_accessors(void)
{
    printf(C_CYAN "Testing Bernoulli term accessors...\n" C_RESET);
    const mrational_t *b1 = mr_bernoulli_even_term(1u);
    const mrational_t *b2 = mr_bernoulli_even_term(2u);
    const mrational_t *blast = mr_bernoulli_even_term(mr_bernoulli_even_term_count());

    ASSERT_TRUE(mr_bernoulli_even_term_count() >= 100u);
    ASSERT_NOT_NULL(b1);
    ASSERT_NOT_NULL(b2);
    ASSERT_NOT_NULL(blast);
    ASSERT_TRUE(mr_bernoulli_even_term(0u) == NULL);
    ASSERT_TRUE(mr_bernoulli_even_term(mr_bernoulli_even_term_count() + 1u) == NULL);

    assert_mr_string_label("mr_bernoulli_even_term(1)", b1, "1/6");
    assert_mr_string_label("mr_bernoulli_even_term(2)", b2, "-1/30");
}

static int readme_example(void)
{
    mrational_t *a = mr_create_frac_long(2, 3);
    mrational_t *b = mr_create_string("5/4");
    char *text;

    if (!a || !b) {
        mr_free(a);
        mr_free(b);
        return 1;
    }

    if (mr_mul(a, b) != 0) {
        mr_free(a);
        mr_free(b);
        return 1;
    }

    text = mr_to_string(a);
    if (!text) {
        mr_free(a);
        mr_free(b);
        return 1;
    }

    printf("(2/3) * (5/4) = %s\n", text);

    free(text);
    mr_free(a);
    mr_free(b);
    return 0;
}

void test_readme_examples(void)
{
    printf(C_YELLOW "\nRunning README example...\n" C_RESET);
    ASSERT_EQ_INT(readme_example(), 0);
}

int tests_main(void)
{
    RUN_TEST_CASE(test_create_and_normalise);
    RUN_TEST_CASE(test_setters_and_accessors);
    RUN_TEST_CASE(test_clone_compare_and_order);
    RUN_TEST_CASE(test_arithmetic);
    RUN_TEST_CASE(test_large_values);
    RUN_TEST_CASE(test_bernoulli_accessors);
    RUN_TEST_CASE(test_readme_examples);
    return 0;
}
