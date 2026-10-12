/**
 * @file test_number_physical.c
 * @brief Immortal physical constants, exact component extraction and precision regressions.
 *
 * Exercises the public numeric API at double and multiprecision settings,
 * including safe destruction and explicit precision independent of the default.
 */
#include <math.h>
#include <string.h>

#include "test_number.h"

void run_number_physical_constant_tests(void)
{
    static const struct {
        const number_t *value;
        const char *numerator;
        const char *denominator;
    } cases[] = {
        {&NUM_SPEED_OF_LIGHT, "299792458", "1"},
        {&NUM_ELEMENTARY_CHARGE, "1602176634", "10000000000000000000000000000"},
        {&NUM_PLANCK, "662607015", "1000000000000000000000000000000000000000000"},
        {&NUM_BOLTZMANN, "1380649", "100000000000000000000000000000"},
        {&NUM_AVOGADRO, "602214076000000000000000", "1"},
        {&NUM_STANDARD_GRAVITY, "196133", "20000"},
        {&NUM_ASTRONOMICAL_UNIT, "149597870700", "1"},
        {&NUM_JULIAN_YEAR, "31557600", "1"},
    };
    size_t saved = num_get_default_prec_bits();
    ASSERT_EQ_INT(num_set_default_prec_bits(53), 0);
    for (size_t i = 0; i < sizeof(cases) / sizeof(*cases); ++i) {
        number_t constant = *cases[i].value;
        ASSERT_TRUE(num_is_exact(constant) && num_is_finite(constant));
        number_t top = {0}, bottom = {0};
        ASSERT_TRUE(num_get_fraction_parts(constant, &top, &bottom));
        number_t expected_top = num_create_from_string(cases[i].numerator);
        number_t expected_bottom = num_create_from_string(cases[i].denominator);
        number_t left = num_mul(top, expected_bottom);
        number_t right = num_mul(bottom, expected_top);
        ASSERT_TRUE(num_is_exact(top) && num_is_exact(bottom));
        ASSERT_NUMBER_EQ(left, right);
        ASSERT_TRUE(isfinite(num_to_double(constant)) && num_to_double(constant) > 0);
        ASSERT_TRUE(!qf_isnan(num_to_qfloat(constant)));
        number_t precise = num_const_prec(constant, 512);
        ASSERT_TRUE(num_is_exact(precise));
        ASSERT_NUMBER_EQ(precise, constant);
        number_t reduced = num_const_prec(precise, 53);
        ASSERT_TRUE(num_is_exact(reduced) || num_get_prec_bits(reduced) == 53);
        number_t copy = num_clone(constant);
        num_destroy(&copy);
        num_destroy(&constant);
        ASSERT_TRUE(num_is_exact(*cases[i].value) && num_is_finite(*cases[i].value));
        num_destroy(&precise);
        num_destroy(&reduced);
        num_destroy(&left);
        num_destroy(&right);
        num_destroy(&expected_top);
        num_destroy(&expected_bottom);
        num_destroy(&top);
        num_destroy(&bottom);
    }
    number_t low = num_const(NUM_ELEMENTARY_CHARGE);
    ASSERT_TRUE(!num_is_exact(low) && num_get_prec_bits(low) == 53);
    ASSERT_EQ_INT(num_set_default_prec_bits(512), 0);
    number_t explicit_low = num_const_prec(NUM_PLANCK, 53);
    number_t high = num_const(NUM_ELEMENTARY_CHARGE);
    ASSERT_TRUE(!num_is_exact(explicit_low) && num_get_prec_bits(explicit_low) == 53);
    ASSERT_TRUE(num_is_exact(high));
    number_t top = NUM_TWO, bottom = NUM_TEN;
    ASSERT_TRUE(!num_get_fraction_parts(low, &top, &bottom));
    ASSERT_NUMBER_EQ(top, NUM_TWO);
    ASSERT_NUMBER_EQ(bottom, NUM_TEN);
    ASSERT_TRUE(!num_get_fraction_parts(NUM_I, &top, &bottom));
    ASSERT_TRUE(!num_get_fraction_parts(NUM_ONE, &top, &top));
    ASSERT_TRUE(!num_get_fraction_parts(NUM_ONE, NULL, &bottom));
    ASSERT_TRUE(num_get_fraction_parts(NUM_NEG_ONE, &top, &bottom));
    ASSERT_NUMBER_EQ(top, NUM_NEG_ONE);
    ASSERT_NUMBER_EQ(bottom, NUM_ONE);
    num_destroy(&top);
    num_destroy(&bottom);
    num_destroy(&high);
    num_destroy(&explicit_low);
    num_destroy(&low);
    ASSERT_EQ_INT(num_set_default_prec_bits(saved), 0);
}
