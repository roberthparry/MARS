#include "number.h"

/* Round guarded Bessel Y to double-double precision on its non-negative real domain. */
qfloat_t qf_bessel_y(qfloat_t order, qfloat_t argument)
{
    NUM_SCOPE(scope);
    if (qf_lt(argument, QF_ZERO))
        return QF_NAN;
    number_t value = num_bessel_y(num_create_from_qfloat(order), num_create_from_qfloat(argument));
    return num_is_real(value) ? num_to_qfloat(value) : QF_NAN;
}
