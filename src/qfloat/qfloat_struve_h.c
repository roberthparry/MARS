#include "number.h"

/* Round guarded ordinary Struve H to double-double precision on the real domain. */
qfloat_t qf_struve_h(qfloat_t order, qfloat_t argument)
{
    NUM_SCOPE(scope);
    if (qf_lt(argument, QF_ZERO) && !qf_eq(order, qf_floor(order)))
        return QF_NAN;
    number_t value = num_struve_h(num_create_from_qfloat(order), num_create_from_qfloat(argument));
    return num_is_real(value) ? num_to_qfloat(value) : QF_NAN;
}
