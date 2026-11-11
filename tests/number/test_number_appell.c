/**
 * @file test_number_appell.c
 * @brief Stateful aliasing checks for the number module's real and complex Appell kernels.
 *
 * Each of the six input slots becomes the output in turn, for compact dyadic and full-precision parameters.
 * Independent product identities supply references with extra guard bits. This white-box fixture belongs to
 * the number module; ordinary data-driven Appell identities and failure cases remain in test_number.sql.
 */
#include <stdio.h>

#include "number-t.h"
#define MARS_NUMBER_INTERNAL_ACCESS
#include "number/number_internal.h"
#include "test_harness.h"
#include "test_number.h"

static bool appell_real_aliases(mpfr_prec_t bits, bool compact)
{
    const double inputs[] = {0.5, 0.75, 1.25, 0.5, 0.25, 0.375};
    mpfr_t values[6], reference, left, right, exponent, error, bound;
    for (size_t i = 0; i < 6; ++i) mpfr_init2(values[i], bits);
    mpfr_inits2(bits + 64, reference, left, right, exponent, error, bound, (mpfr_ptr)0);
    bool ok = true;
    for (size_t alias = 0; alias < 6 && ok; ++alias) {
        for (size_t i = 0; i < 6; ++i) mpfr_set_d(values[i], inputs[i], MPFR_RNDN);
        if (!compact) {
            mpfr_set_str(values[0], "0.7", 10, MPFR_RNDN);
            mpfr_set(values[3], values[0], MPFR_RNDN);
        }
        mpfr_ui_sub(left, 1u, values[4], MPFR_RNDN);
        mpfr_neg(exponent, values[1], MPFR_RNDN);
        mpfr_pow(left, left, exponent, MPFR_RNDN);
        mpfr_ui_sub(right, 1u, values[5], MPFR_RNDN);
        mpfr_neg(exponent, values[2], MPFR_RNDN);
        mpfr_pow(right, right, exponent, MPFR_RNDN);
        mpfr_mul(reference, left, right, MPFR_RNDN);
        int status = number_mpfr_appell_f1(values[alias], values[0], values[1], values[2],
                                          values[3], values[4], values[5]);
        mpfr_sub(error, values[alias], reference, MPFR_RNDU);
        mpfr_abs(error, error, MPFR_RNDU);
        mpfr_abs(bound, reference, MPFR_RNDD);
        mpfr_mul_2si(bound, bound, -bits + 8, MPFR_RNDD);
        ok = !status && mpfr_number_p(error) && mpfr_lessequal_p(error, bound);
        if (!ok) printf("Appell real alias %zu failed at %ld bits, compact=%d\n", alias, (long)bits, compact);
    }
    mpfr_clears(reference, left, right, exponent, error, bound, (mpfr_ptr)0);
    for (size_t i = 0; i < 6; ++i) mpfr_clear(values[i]);
    return ok;
}

static bool appell_complex_aliases(mpfr_prec_t bits, bool compact)
{
    const double inputs[] = {0.5, 0.75, 1.25, 0.5, 0.25, 0.375};
    mpc_t values[6], reference, left, right, exponent, difference;
    for (size_t i = 0; i < 6; ++i) mpc_init2(values[i], bits);
    mpc_init2(reference, bits + 64); mpc_init2(left, bits + 64); mpc_init2(right, bits + 64);
    mpc_init2(exponent, bits + 64); mpc_init2(difference, bits + 64);
    mpfr_t error, bound;
    mpfr_inits2(bits + 64, error, bound, (mpfr_ptr)0);
    bool ok = true;
    for (size_t alias = 0; alias < 6 && ok; ++alias) {
        for (size_t i = 0; i < 6; ++i) mpc_set_d_d(values[i], inputs[i], 0.125, MPC_RNDNN);
        if (!compact) {
            mpfr_set_str(mpc_realref(values[0]), "0.7", 10, MPFR_RNDN);
            mpfr_set_str(mpc_imagref(values[0]), "0.1", 10, MPFR_RNDN);
            mpc_set(values[3], values[0], MPC_RNDNN);
        }
        mpc_ui_sub(left, 1u, values[4], MPC_RNDNN);
        mpc_neg(exponent, values[1], MPC_RNDNN);
        mpc_pow(left, left, exponent, MPC_RNDNN);
        mpc_ui_sub(right, 1u, values[5], MPC_RNDNN);
        mpc_neg(exponent, values[2], MPC_RNDNN);
        mpc_pow(right, right, exponent, MPC_RNDNN);
        mpc_mul(reference, left, right, MPC_RNDNN);
        int status = number_mpc_appell_f1(values[alias], values[0], values[1], values[2],
                                         values[3], values[4], values[5], MPC_RNDNN);
        mpc_sub(difference, values[alias], reference, MPC_RNDNN);
        mpc_abs(error, difference, MPFR_RNDU);
        mpc_abs(bound, reference, MPFR_RNDD);
        mpfr_mul_2si(bound, bound, -bits + 8, MPFR_RNDD);
        ok = !status && mpfr_number_p(error) && mpfr_lessequal_p(error, bound);
        if (!ok) printf("Appell complex alias %zu failed at %ld bits, compact=%d\n", alias, (long)bits, compact);
    }
    mpfr_clears(error, bound, (mpfr_ptr)0);
    mpc_clear(reference); mpc_clear(left); mpc_clear(right); mpc_clear(exponent); mpc_clear(difference);
    for (size_t i = 0; i < 6; ++i) mpc_clear(values[i]);
    return ok;
}

/* Verify every supported output alias against references independent of the Appell recurrence. */
void run_number_appell_alias_tests(void)
{
    const mpfr_prec_t precisions[] = {256, 1024};
    bool ok = true;
    for (size_t i = 0; i < sizeof precisions / sizeof *precisions; ++i) {
        ok = appell_real_aliases(precisions[i], true) && ok;
        ok = appell_real_aliases(precisions[i], false) && ok;
        ok = appell_complex_aliases(precisions[i], true) && ok;
        ok = appell_complex_aliases(precisions[i], false) && ok;
    }
    ASSERT_TRUE(ok);
}
