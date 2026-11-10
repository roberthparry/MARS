/**
 * @file qcomplex_error.c
 * @brief Complex double-double error functions through the guarded native number kernel.
 *
 * Preserves direct real double-double evaluation while using checked multiprecision series and inverse iteration
 * for complex arguments. No browser or application policy enters these library adapters. Each result is rounded
 * to the fixed qcomplex precision after native evaluation, and temporary number values remain scope-owned.
 */
#include "number-t.h"
#include "qcomplex-t.h"

static qcomplex_t error_adapt(qcomplex_t z, number_t (*function)(number_t), qfloat_t (*real_function)(qfloat_t))
{
    if (qf_eq(z.im, QF_ZERO)) return qc_make(real_function(z.re), QF_ZERO);
    NUM_SCOPE(scope);
    number_t value = function(num_create_from_qcomplex(z));
    return num_is_finite(value) ? qc_make(num_to_qfloat(num_real_part(value)), num_to_qfloat(num_imag_part(value))) : QC_NAN;
}

/* Evaluate complex erf with the guarded entire series and round to double-double. */
qcomplex_t qc_erf(qcomplex_t z) { return error_adapt(z, num_erf, qf_erf); }

/* Evaluate complex erfc while preserving its complementary cancellation guards. */
qcomplex_t qc_erfc(qcomplex_t z) { return error_adapt(z, num_erfc, qf_erfc); }

/* Refine and verify inverse erf before rounding to fixed complex precision. */
qcomplex_t qc_erfinv(qcomplex_t z) { return error_adapt(z, num_erfinv, qf_erfinv); }

/* Refine and verify inverse erfc using the corrected complex forward calculation. */
qcomplex_t qc_erfcinv(qcomplex_t z) { return error_adapt(z, num_erfcinv, qf_erfcinv); }
