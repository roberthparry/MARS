#include <stdlib.h>
#include <string.h>

#include "mint.h"
#include "internal/mrational_internal.h"
#include "mrational.h"

#define TEST_CONFIG_MODE TEST_CONFIG_GLOBAL
#define TEST_CONFIG_MAIN
#include "test_harness.h"

static void assert_mr_string(const mrational_t *rational, const char *expected)
{
    char *got = mr_to_string(rational);

    ASSERT_NOT_NULL(got);
    printf("    expected = %s\n", expected);
    printf("    got      = %s\n", got);
    ASSERT_TRUE(strcmp(got, expected) == 0);
    free(got);
}

static void assert_mint_clone_string(const mint_t *mint, const char *expected)
{
    char *got = mi_to_string(mint);

    ASSERT_NOT_NULL(got);
    printf("    expected = %s\n", expected);
    printf("    got      = %s\n", got);
    ASSERT_TRUE(strcmp(got, expected) == 0);
    free(got);
}

void test_create_and_normalise(void)
{
    mrational_t *zero = mr_new();
    mrational_t *whole = mr_create_long(-42);
    mrational_t *frac = mr_create_frac_long(6, -8);
    mrational_t *parsed = mr_create_string("  -10/20  ");

    ASSERT_NOT_NULL(zero);
    ASSERT_NOT_NULL(whole);
    ASSERT_NOT_NULL(frac);
    ASSERT_NOT_NULL(parsed);

    assert_mr_string(zero, "0");
    ASSERT_TRUE(mr_is_zero(zero));
    ASSERT_TRUE(mr_is_integer(zero));

    assert_mr_string(whole, "-42");
    ASSERT_TRUE(mr_is_integer(whole));

    assert_mr_string(frac, "-3/4");
    ASSERT_TRUE(!mr_is_integer(frac));

    assert_mr_string(parsed, "-1/2");
    ASSERT_TRUE(!mr_is_integer(parsed));

    mr_free(zero);
    mr_free(whole);
    mr_free(frac);
    mr_free(parsed);
}

void test_setters_and_accessors(void)
{
    mrational_t *r = mr_new();
    mint_t *src_num = mi_create_string("84");
    mint_t *src_den = mi_create_string("-126");
    const mint_t *num = NULL;
    const mint_t *den = NULL;

    ASSERT_NOT_NULL(r);
    ASSERT_NOT_NULL(src_num);
    ASSERT_NOT_NULL(src_den);
    ASSERT_EQ_INT(mr_set_frac_long(r, 14, 21), 0);
    assert_mr_string(r, "2/3");

    num = mr_numerator(r);
    den = mr_denominator(r);
    ASSERT_NOT_NULL(num);
    ASSERT_NOT_NULL(den);
    assert_mint_clone_string(num, "2");
    assert_mint_clone_string(den, "3");
    num = den = NULL;

    ASSERT_EQ_INT(mr_set_string(r, "5"), 0);
    assert_mr_string(r, "5");
    ASSERT_TRUE(mr_is_integer(r));

    ASSERT_EQ_INT(mr_set_mints(r, src_num, src_den), 0);
    assert_mr_string(r, "-2/3");
    assert_mint_clone_string(src_num, "84");
    assert_mint_clone_string(src_den, "-126");

    ASSERT_EQ_INT(mr_set_string(r, "7/0"), -1);
    ASSERT_EQ_INT(mr_set_long(r, 0), 0);
    ASSERT_TRUE(mr_is_zero(r));
    ASSERT_EQ_INT(mr_set_frac_long(r, 1, 0), -1);

    mr_clear(r);
    assert_mr_string(r, "0");
    mi_free(src_num);
    mi_free(src_den);
    mr_free(r);
}

void test_clone_compare_and_order(void)
{
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
    assert_mr_string(a, "2/3");
    assert_mr_string(b, "1");

    mr_free(a);
    mr_free(b);
    mr_free(c);
    mr_free(d);
}

void test_arithmetic(void)
{
    mrational_t *a = mr_create_frac_long(1, 2);
    mrational_t *b = mr_create_frac_long(1, 3);
    mrational_t *zero = mr_create_long(0);

    ASSERT_NOT_NULL(a);
    ASSERT_NOT_NULL(b);
    ASSERT_NOT_NULL(zero);

    ASSERT_EQ_INT(mr_add(a, b), 0);
    assert_mr_string(a, "5/6");

    ASSERT_EQ_INT(mr_sub(a, b), 0);
    assert_mr_string(a, "1/2");

    ASSERT_EQ_INT(mr_mul(a, b), 0);
    assert_mr_string(a, "1/6");

    ASSERT_EQ_INT(mr_div(a, b), 0);
    assert_mr_string(a, "1/2");

    ASSERT_EQ_INT(mr_neg(a), 0);
    assert_mr_string(a, "-1/2");

    ASSERT_EQ_INT(mr_abs(a), 0);
    assert_mr_string(a, "1/2");

    ASSERT_EQ_INT(mr_inv(a), 0);
    assert_mr_string(a, "2");

    ASSERT_EQ_INT(mr_inv(zero), -1);

    mr_free(a);
    mr_free(b);
    mr_free(zero);
}

void test_large_values(void)
{
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
    assert_mr_string(a, "92233720368547758080/21");

    ASSERT_EQ_INT(mr_div(a, b), 0);
    assert_mr_string(a, "18446744073709551616/3");
    assert_mr_string(c, "18446744073709551616/3");

    mr_free(a);
    mr_free(b);
    mr_free(c);
    mi_free(num);
    mi_free(den);
}

void test_bernoulli_accessors(void)
{
    const mrational_t *b1 = mr_bernoulli_even_term(1u);
    const mrational_t *b2 = mr_bernoulli_even_term(2u);
    const mrational_t *blast = mr_bernoulli_even_term(mr_bernoulli_even_term_count());

    ASSERT_TRUE(mr_bernoulli_even_term_count() >= 100u);
    ASSERT_NOT_NULL(b1);
    ASSERT_NOT_NULL(b2);
    ASSERT_NOT_NULL(blast);
    ASSERT_TRUE(mr_bernoulli_even_term(0u) == NULL);
    ASSERT_TRUE(mr_bernoulli_even_term(mr_bernoulli_even_term_count() + 1u) == NULL);

    assert_mr_string(b1, "1/6");
    assert_mr_string(b2, "-1/30");
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
