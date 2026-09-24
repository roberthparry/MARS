#include "number.h"

/* Use the guarded multiprecision kernel before rounding to double-double precision. */
qfloat_t qf_bessel_k(qfloat_t order, qfloat_t argument)
{
    NUM_SCOPE(scope);
    if (!qf_gt(argument, QF_ZERO))
        return QF_NAN;
    number_t value = num_bessel_k(num_create_from_qfloat(order), num_create_from_qfloat(argument));
    return num_to_qfloat(value);
}
