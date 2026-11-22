/**
 * @file qcomplex_lambert.c
 * @brief Double-double evaluation of MARS's defined generalised Lambert sum.
 *
 * Uses principal powers and Eulerian polynomials for non-positive polylogarithm orders. Infinite sections
 * have an absolute geometric/polynomial tail bound independent of cancellation; finite sections support
 * the unit circle away from poles. All storage is bounded and local, without a multiprecision bridge.
 */
#include <math.h>
#include <stdint.h>

#include "computation-t.h"
#include "qcomplex-t.h"

static qcomplex_t lambert_power(qcomplex_t base, unsigned exponent)
{
    qcomplex_t result = QC_ONE;
    while (exponent) {
        if (exponent & 1u) result = qc_mul(result, base);
        exponent >>= 1;
        if (exponent) base = qc_mul(base, base);
    }
    return result;
}

static bool lambert_integer(qcomplex_t input, double minimum, double maximum, long *value)
{
    qfloat_t real = qc_real(input);
    if (!qf_eq(qc_imag(input), QF_ZERO) || !qf_eq(real, qf_floor(real)) ||
        !qf_ge(real, qf_from_double(minimum)) || !qf_le(real, qf_from_double(maximum))) return false;
    *value = (long)qf_to_double(real);
    return true;
}

/* Evaluate the explicitly defined Lambert sum without reducing double-double precision. */
qcomplex_t qc_lambert_S(qcomplex_t a, qcomplex_t q, qcomplex_t z, qcomplex_t weight_value,
                       qcomplex_t order_value, qcomplex_t count_value)
{
    long weight, order, count = 0;
    bool infinite = qf_eq(qc_real(count_value), QF_INF) && qf_eq(qc_imag(count_value), QF_ZERO);
    if (!lambert_integer(weight_value, 0, 64, &weight) || !lambert_integer(order_value, -32, 32, &order) ||
        (!infinite && !lambert_integer(count_value, 0, 100000, &count)) ||
        qc_isnan(a) || qc_isnan(q) || qc_isnan(z) || qc_isinf(a) || qc_isinf(q) || qc_isinf(z)) return QC_NAN;
    if ((!infinite && !count) || qc_eq(a, QC_ZERO)) return QC_ZERO;
    if (qc_eq(q, QC_ZERO)) return QC_NAN;
    qcomplex_t w = qc_mul(a, qc_exp(qc_mul(z, qc_log(q)))), sum = QC_ZERO;
    double radius = nextafter(qf_to_double(qc_abs(q)), INFINITY);
    double z_size = nextafter(qf_to_double(qc_abs(z)), INFINITY);
    if (infinite && (!(radius < 1) || !isfinite(z_size))) return QC_NAN;
    unsigned p = order < 0 ? (unsigned)-order : 0;
    qfloat_t coefficients[33] = {QF_ONE};
    for (unsigned j = 1; j <= p; ++j) coefficients[j] = QF_ZERO;
    for (unsigned m = 2; m <= p; ++m)
        for (unsigned j = m - 1; j > 0; --j)
            coefficients[j] = qf_add(qf_mul(coefficients[j], qf_from_double(j + 1)),
                                     qf_mul(coefficients[j - 1], qf_from_double(m - j)));
    size_t limit = infinite ? 100000 : (size_t)count;
    for (size_t n = 0; n < limit; ++n) {
        computation_t *control = computation_current();
        if (control && !computation_checkpoint(control, 1)) return QC_NAN;
        qcomplex_t coordinate = qc_add(z, qc_make(qf_from_double((double)n), QF_ZERO)), li;
        if (order > 0) li = qc_polylog(qc_make(qf_from_double((double)order), QF_ZERO), w);
        else {
            qcomplex_t polynomial = qc_make(coefficients[p ? p - 1 : 0], QF_ZERO);
            for (unsigned j = p ? p - 1 : 0; j > 0; --j)
                polynomial = qc_add(qc_mul(polynomial, w), qc_make(coefficients[j - 1], QF_ZERO));
            li = qc_div(qc_mul(w, polynomial), lambert_power(qc_sub(QC_ONE, w), p + 1));
        }
        sum = qc_add(sum, qc_mul(lambert_power(coordinate, (unsigned)weight), li));
        if (qc_isnan(sum) || qc_isinf(sum)) return QC_NAN;
        if (!infinite && n + 1 == (size_t)count) return sum;
        if (radius < 1 && n > 0) {
            double rho = nextafter(qf_to_double(qc_abs(w)), INFINITY);
            double distance = nextafter((double)n + z_size, INFINITY);
            double ratio = nextafter(radius * pow(1 + 1 / distance, (double)weight), INFINITY);
            if (rho < 1 && ratio < 1) {
                double log_bound = log2(rho) + weight * log2(distance) -
                    (p + 1) * log2(1 - rho) + log2(ratio / (1 - ratio));
                for (unsigned j = 2; j <= p; ++j) log_bound += log2(j);
                if (log_bound + 4 < -110) return sum;
            }
        }
        w = qc_mul(w, q);
    }
    return QC_NAN;
}
