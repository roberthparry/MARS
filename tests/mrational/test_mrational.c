#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "internal/mrational_internal.h"
#include "mint.h"
#include "mrational.h"

#include "test_harness.h"

TEST_SUITE_CONFIG(TEST_CONFIG_GLOBAL);
static int mrational_validity_equal(const void *actual, const void *expected, void *ctx);
static void mrational_validity_format(const void *value, char *buf, size_t buf_size, void *ctx);
static bool test_mrational_suite_setup(void);

static const test_validity_contract_t mrational_exact_contract =
    TEST_VALIDITY_CONTRACT("mrational-exact",
                           mrational_validity_equal,
                           mrational_validity_format,
                           NULL);

TEST_SUITE_SETUP(test_mrational_suite_setup);

#define TEST_ASSERT_MRATIONAL_EQ(actual_ptr, expected_ptr) \
    do { \
        const mrational_t *test_mr_actual__ = (actual_ptr); \
        const mrational_t *test_mr_expected__ = (expected_ptr); \
        TEST_ASSERT_VALID_NAMED("mrational-exact", \
                                &test_mr_actual__, \
                                &test_mr_expected__); \
    } while (0)

static int mrational_validity_equal(const void *actual, const void *expected, void *ctx)
{
    const mrational_t *const *got = (const mrational_t *const *)actual;
    const mrational_t *const *want = (const mrational_t *const *)expected;

    (void)ctx;
    return mr_eq(*got, *want);
}

static void mrational_validity_format(const void *value, char *buf, size_t buf_size, void *ctx)
{
    const mrational_t *const *rational = (const mrational_t *const *)value;
    char *text;

    (void)ctx;
    if (!buf || buf_size == 0) {
        return;
    }

    text = mr_to_string(*rational);
    if (!text) {
        snprintf(buf, buf_size, "<mr_to_string failed>");
        return;
    }

    snprintf(buf, buf_size, "%s", text);
    free(text);
}

static bool test_mrational_suite_setup(void)
{
    test_register_validity_checker("mrational-exact", &mrational_exact_contract);
    return TEST_REQUIRE_VALIDITY_CHECKER("mrational-exact");
}

static void assert_mr_string_label(const char *label,
                                   const mrational_t *rational,
                                   const char *expected)
{
    char *got = mr_to_string(rational);

    ASSERT_NOT_NULL(got);
    printf(C_WHITE C_BOLD "%s" C_RESET "\n", label ? label : "<unspecified>");
    printf("    expected = %s\n", expected);
    printf("    got      = %s\n\n", got);
    TEST_ASSERT_STR_EQ(got, expected);
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
    TEST_ASSERT_STR_EQ(got, expected);
    free(got);
}

void test_create_and_normalise(void)
{
    printf(C_CYAN "Testing construction and normalization...\n" C_RESET);
    mrational_t *zero = mr_new();
    mrational_t *whole = mr_create_long(-42);
    mrational_t *frac = mr_create_frac_long(6, -8);
    mrational_t *parsed = mr_create_string("  -10/20  ");
    mrational_t *parsed_general = mr_create_string("355/113");
    mrational_t *parsed_unicode = mr_create_string("³⁵⁵⁄₁₁₃");

    ASSERT_NOT_NULL(zero);
    ASSERT_NOT_NULL(whole);
    ASSERT_NOT_NULL(frac);
    ASSERT_NOT_NULL(parsed);
    ASSERT_NOT_NULL(parsed_general);
    ASSERT_NOT_NULL(parsed_unicode);

    assert_mr_string_label("mr_new() -> 0", zero, "0");
    ASSERT_TRUE(mr_is_zero(zero));
    ASSERT_TRUE(mr_is_integer(zero));

    assert_mr_string_label("mr_create_long(-42)", whole, "-42");
    ASSERT_TRUE(mr_is_integer(whole));

    assert_mr_string_label("mr_create_frac_long(6, -8)", frac, "-¾");
    ASSERT_TRUE(!mr_is_integer(frac));

    assert_mr_string_label("mr_create_string(\"  -10/20  \")", parsed, "-½");
    ASSERT_TRUE(!mr_is_integer(parsed));

    assert_mr_string_label("mr_create_string(\"355/113\")",
                           parsed_general,
                           "³⁵⁵⁄₁₁₃");
    ASSERT_TRUE(!mr_is_integer(parsed_general));

    assert_mr_string_label("mr_create_string(\"³⁵⁵⁄₁₁₃\")",
                           parsed_unicode,
                           "³⁵⁵⁄₁₁₃");
    ASSERT_TRUE(!mr_is_integer(parsed_unicode));

    mr_free(zero);
    mr_free(whole);
    mr_free(frac);
    mr_free(parsed);
    mr_free(parsed_general);
    mr_free(parsed_unicode);
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
    assert_mr_string_label("mr_set_frac_long(r, 14, 21)", r, "⅔");

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
    assert_mr_string_label("mr_set_mints(r, 84, -126)", r, "-⅔");
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

    TEST_ASSERT_MRATIONAL_EQ(a, b);
    ASSERT_TRUE(mr_cmp(a, b) == 0);
    ASSERT_TRUE(mr_lt(a, c));
    ASSERT_TRUE(mr_le(a, c));
    ASSERT_TRUE(mr_gt(c, a));
    ASSERT_TRUE(mr_ge(c, a));

    ASSERT_EQ_INT(mr_add(b, d), 0);
    assert_mr_string_label("original a after mr_clone + mr_add(b, d)", a, "⅔");
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
    assert_mr_string_label("mr_add(1/2, 1/3)", a, "⅚");

    ASSERT_EQ_INT(mr_sub(a, b), 0);
    assert_mr_string_label("mr_sub(5/6, 1/3)", a, "½");

    ASSERT_EQ_INT(mr_mul(a, b), 0);
    assert_mr_string_label("mr_mul(1/2, 1/3)", a, "⅙");

    ASSERT_EQ_INT(mr_div(a, b), 0);
    assert_mr_string_label("mr_div(1/6, 1/3)", a, "½");

    ASSERT_EQ_INT(mr_neg(a), 0);
    assert_mr_string_label("mr_neg(1/2)", a, "-½");

    ASSERT_EQ_INT(mr_abs(a), 0);
    assert_mr_string_label("mr_abs(-1/2)", a, "½");

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
                           "⁹²²³³⁷²⁰³⁶⁸⁵⁴⁷⁷⁵⁸⁰⁸⁰⁄₂₁");

    ASSERT_EQ_INT(mr_div(a, b), 0);
    assert_mr_string_label("mr_div(previous, 5/7)",
                           a,
                           "¹⁸⁴⁴⁶⁷⁴⁴⁰⁷³⁷⁰⁹⁵⁵¹⁶¹⁶⁄₃");
    assert_mr_string_label("mr_create_mints(18446744073709551616, 3)",
                           c,
                           "¹⁸⁴⁴⁶⁷⁴⁴⁰⁷³⁷⁰⁹⁵⁵¹⁶¹⁶⁄₃");

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

    assert_mr_string_label("mr_bernoulli_even_term(1)", b1, "⅙");
    assert_mr_string_label("mr_bernoulli_even_term(2)", b2, "-¹⁄₃₀");
}

static void example_mrational_basic(void)
{
    mrational_t *a = mr_create_frac_long(2, 3);
    mrational_t *b = mr_create_string("5/4");
    char *text;

    if (!a || !b) {
        mr_free(a);
        mr_free(b);
        test_mark_failure(__FILE__, __LINE__, "failed to allocate README example inputs");
        return;
    }

    if (mr_mul(a, b) != 0) {
        mr_free(a);
        mr_free(b);
        test_mark_failure(__FILE__, __LINE__, "failed to multiply README example rationals");
        return;
    }

    text = mr_to_string(a);
    if (!text) {
        mr_free(a);
        mr_free(b);
        test_mark_failure(__FILE__, __LINE__, "failed to format README example rational");
        return;
    }

    printf("(2/3) * (5/4) = %s\n", text);

    free(text);
    mr_free(a);
    mr_free(b);
}

int tests_main(void)
{
    TEST_SECTION("Construction and Access");
    TEST_RUN_CASE(test_create_and_normalise, NULL);
    TEST_RUN_CASE(test_setters_and_accessors, NULL);

    TEST_SECTION("Arithmetic and Ordering");
    TEST_RUN_CASE(test_clone_compare_and_order, NULL);
    TEST_RUN_CASE(test_arithmetic, NULL);
    TEST_RUN_CASE(test_large_values, NULL);

    TEST_SECTION("Bernoulli");
    TEST_RUN_CASE(test_bernoulli_accessors, NULL);

    TEST_SECTION("README Output Examples");
    printf(C_YELLOW "\nRunning README example...\n" C_RESET);
    TEST_RUN_OUTPUT_TAGS(example_mrational_basic, "mrational,readme,output");
    return TESTS_EXIT_CODE();
}
