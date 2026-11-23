/**
 * @file qfloat_lambert.c
 * @brief Real double-double entry point for MARS's defined Lambert sum.
 *
 * Positive real bases and non-positive polylogarithm orders use direct real Eulerian-polynomial arithmetic.
 * Other principal branches delegate to the native complex kernel. Both components of every double-double
 * value are retained; non-real results return NaN rather than discarding an imaginary component. Finite
 * constant-base sums of weight zero or one use their exact polynomial moments.
 */
#include <math.h>

#include "computation-t.h"
#include "qcomplex-t.h"
#include "qfloat-t.h"

static bool lambert_real_integer(qfloat_t input, double minimum, double maximum, long *value)
{
    if (!qf_eq(input, qf_floor(input)) || !qf_ge(input, qf_from_double(minimum)) ||
        !qf_le(input, qf_from_double(maximum)))
        return false;
    *value = (long)qf_to_double(input);
    return true;
}

static qfloat_t lambert_real_sum(qfloat_t a, qfloat_t q, qfloat_t z, unsigned weight, unsigned degree, long count)
{
    bool infinite = count < 0;
    qfloat_t initial = qf_eq(z, QF_ZERO) ? QF_ONE : qf_eq(z, QF_ONE) ? q : qf_pow(q, z);
    qfloat_t w = qf_mul(a, initial), sum = QF_ZERO;
    double radius = nextafter(qf_to_double(q), INFINITY);
    double z_size = nextafter(fabs(qf_to_double(z)), INFINITY);
    if (infinite && (!(radius < 1) || !isfinite(z_size)))
        return QF_NAN;
    bool stationary = !infinite && weight <= 1 && qf_eq(q, QF_ONE);
    qfloat_t moment = qf_from_double(count);
    if (stationary && weight)
        moment = qf_add(qf_mul(moment, z), qf_from_double((double)count * (count - 1) / 2));
    qfloat_t coefficients[33] = {QF_ONE};
    for (unsigned j = 1; j <= degree; ++j)
        coefficients[j] = QF_ZERO;
    for (unsigned p = 2; p <= degree; ++p)
        for (unsigned j = p - 1; j > 0; --j)
            coefficients[j] = qf_add(qf_mul(coefficients[j], qf_from_double(j + 1)),
                                      qf_mul(coefficients[j - 1], qf_from_double(p - j)));
    computation_t *control = computation_current();
    long limit = infinite ? 100000 : count;
    for (long n = 0; n < limit; ++n) {
        if (control && !computation_checkpoint(control, 1))
            return QF_NAN;
        qfloat_t coordinate = qf_add(z, qf_from_double(n));
        qfloat_t polynomial = coefficients[degree ? degree - 1 : 0];
        for (unsigned j = degree ? degree - 1 : 0; j > 0; --j)
            polynomial = qf_add(qf_mul(polynomial, w), coefficients[j - 1]);
        qfloat_t denominator = qf_pow_int(qf_sub(QF_ONE, w), (int)degree + 1);
        qfloat_t term = qf_div(qf_mul(w, polynomial), denominator);
        if (stationary)
            term = qf_mul(term, moment);
        else if (weight == 1)
            term = qf_mul(term, coordinate);
        else if (weight)
            term = qf_mul(term, qf_pow_int(coordinate, (int)weight));
        sum = qf_add(sum, term);
        if (qf_isnan(sum) || qf_isinf(sum))
            return QF_NAN;
        if (stationary || (!infinite && n + 1 == count))
            return sum;
        if (radius < 1 && n > 0) {
            double rho = nextafter(fabs(qf_to_double(w)), INFINITY);
            double distance = nextafter(n + z_size, INFINITY);
            double ratio = nextafter(radius * pow(1 + 1 / distance, weight), INFINITY);
            if (rho < 1 && ratio < 1) {
                double bound = log2(rho) + weight * log2(distance) -
                               (degree + 1) * log2(1 - rho) + log2(ratio / (1 - ratio));
                for (unsigned j = 2; j <= degree; ++j)
                    bound += log2(j);
                if (bound + 4 < -110)
                    return sum;
            }
        }
        w = qf_mul(w, q);
    }
    return QF_NAN;
}

/* Evaluate the real Lambert sum without introducing a multiprecision dependency. */
qfloat_t qf_lambert_S(qfloat_t a, qfloat_t q, qfloat_t z, qfloat_t weight, qfloat_t order, qfloat_t count)
{
    long r, s, n = -1;
    if (lambert_real_integer(weight, 0, 64, &r) && lambert_real_integer(order, -32, 0, &s) &&
        (qf_eq(count, QF_INF) || lambert_real_integer(count, 0, 100000, &n)) &&
        !qf_isnan(a) && !qf_isinf(a) && !qf_isnan(z) && !qf_isinf(z) && !qf_isinf(q) && qf_gt(q, QF_ZERO)) {
        if (!n || qf_eq(a, QF_ZERO))
            return QF_ZERO;
        return lambert_real_sum(a, q, z, (unsigned)r, (unsigned)-s, n);
    }
    qcomplex_t value = qc_lambert_S(qc_make(a, QF_ZERO), qc_make(q, QF_ZERO), qc_make(z, QF_ZERO),
                                   qc_make(weight, QF_ZERO), qc_make(order, QF_ZERO), qc_make(count, QF_ZERO));
    return qf_eq(qc_imag(value), QF_ZERO) ? qc_real(value) : QF_NAN;
}
