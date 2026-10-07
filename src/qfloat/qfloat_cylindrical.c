/**
 * @file qfloat_cylindrical.c
 * @brief Real double-double adapters for Bessel and Struve functions.
 *
 * Groups the Bessel I, K and Y and Struve H/L entry points that use guarded number-layer kernels. Each adapter
 * preserves its existing domain checks, conversion, rounding and numeric-scope cleanup. The numerical kernels
 * remain in the number module; this file does not introduce another special-function implementation.
 */

#include "number.h"

/* Round guarded modified Bessel I to double-double precision on the real domain. */
qfloat_t qf_bessel_i(qfloat_t order, qfloat_t argument)
{
    NUM_SCOPE(scope);
    if (qf_lt(argument, QF_ZERO) && !qf_eq(order, qf_floor(order)))
        return QF_NAN;
    number_t value = num_bessel_i(num_create_from_qfloat(order), num_create_from_qfloat(argument));
    return num_is_real(value) ? num_to_qfloat(value) : QF_NAN;
}

/* Use the guarded multiprecision kernel before rounding to double-double precision. */
qfloat_t qf_bessel_k(qfloat_t order, qfloat_t argument)
{
    NUM_SCOPE(scope);
    if (!qf_gt(argument, QF_ZERO))
        return QF_NAN;
    number_t value = num_bessel_k(num_create_from_qfloat(order), num_create_from_qfloat(argument));
    qfloat_t result = num_to_qfloat(value);
    num_destroy(&value);
    return result;
}

/* Round guarded Bessel Y to double-double precision on its non-negative real domain. */
qfloat_t qf_bessel_y(qfloat_t order, qfloat_t argument)
{
    NUM_SCOPE(scope);
    if (qf_lt(argument, QF_ZERO))
        return QF_NAN;
    number_t value = num_bessel_y(num_create_from_qfloat(order), num_create_from_qfloat(argument));
    qfloat_t result = num_is_real(value) ? num_to_qfloat(value) : QF_NAN;

    num_destroy(&value);
    return result;
}

/* Round guarded ordinary Struve H to double-double precision on the real domain. */
qfloat_t qf_struve_h(qfloat_t order, qfloat_t argument)
{
    NUM_SCOPE(scope);
    if (qf_lt(argument, QF_ZERO) && !qf_eq(order, qf_floor(order)))
        return QF_NAN;
    number_t value = num_struve_h(num_create_from_qfloat(order), num_create_from_qfloat(argument));
    return num_is_real(value) ? num_to_qfloat(value) : QF_NAN;
}

/* Round the guarded real modified Struve L to double-double precision. */
qfloat_t qf_struve_l(qfloat_t order, qfloat_t argument)
{
    NUM_SCOPE(scope);
    if (qf_lt(argument, QF_ZERO) && !qf_eq(order, qf_floor(order)))
        return QF_NAN;
    number_t value = num_struve_l(num_create_from_qfloat(order), num_create_from_qfloat(argument));
    return num_is_real(value) ? num_to_qfloat(value) : QF_NAN;
}
