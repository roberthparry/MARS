/**
 * @file qcomplex_appell.c
 * @brief Native double-double complex entry points for all four Appell functions.
 *
 * F1 delegates to the two-variable complex Lauricella FD implementation. F2--F4 sum triangular total-degree
 * rows without growing factorials. A polynomial/geometric Pochhammer majorant
 * chooses the truncation independently of cancellation or vanishing diagonals. Finite numerator parameters and
 * zero axes restrict the summation rectangle. Outside the defining domains, only finite polynomials are evaluated.
 * Storage is bounded and local; this native fixed-precision implementation has no MPFR or MPC dependency.
 */
#include <limits.h>
#include <math.h>
#include <stdlib.h>

#include "qcomplex-t.h"

enum { QC_APPELL_MAX_DEGREE = 4096, QC_APPELL_MAX_CELLS = 1000000 };

static long appell_fixed_end(qcomplex_t z)
{
    qfloat_t real = qc_real(z);
    if (!qf_eq(qc_imag(z), QF_ZERO) || qf_gt(real, QF_ZERO) || !qf_eq(real, qf_floor(real))) return LONG_MAX;
    double value = -qf_to_double(real);
    return value > QC_APPELL_MAX_DEGREE ? QC_APPELL_MAX_DEGREE + 1L : (long)value;
}

/* Rounded binary64 values propose only an inflated stopping bound, never the computed function value. */
static bool appell_fixed_bound(qcomplex_t z, bool inverse, long double *log_constant, unsigned *degree)
{
    qcomplex_t shifted = qc_sub(z, QC_ONE);
    double distance = nextafter(qf_to_double(qf_add(qf_abs(qc_real(shifted)), qf_abs(qc_imag(shifted)))), INFINITY);
    if (inverse) distance *= 2;
    if (!isfinite(distance) || distance > 128) return false;
    unsigned exponent = (unsigned)ceil(distance);
    *degree += exponent;
    if (inverse) {
        for (unsigned j = 0; j <= exponent; ++j) {
            qcomplex_t argument = qc_add(z, qc_make(qf_from_double(j), QF_ZERO));
            double modulus = nextafter(qf_to_double(qc_abs(argument)), 0.0);
            if (!(modulus > 0)) return false;
            long double ratio = (j + 1.0L) / modulus;
            if (ratio > 1) *log_constant += log2l(ratio) + 1e-12L;
        }
    }
    return true;
}

/* Each separate-axis coefficient is shared by every total-degree row that visits that index. */
static qcomplex_t appell_fixed_axis_ratio(const qcomplex_t *z, unsigned family, unsigned index, bool first)
{
    unsigned count = family == 4 ? 6u : 7u;
    qcomplex_t j = qc_make(qf_from_double(index - 1u), QF_ZERO);
    qcomplex_t value = z[count - (first ? 2u : 1u)];
    qcomplex_t divisor = qc_make(qf_from_double(index), QF_ZERO);
    if (family == 2) {
        value = qc_mul(value, qc_add(z[first ? 1 : 2], j));
        divisor = qc_mul(divisor, qc_add(z[first ? 3 : 4], j));
    } else if (family == 3) {
        value = qc_mul(value, qc_add(z[first ? 0 : 1], j));
        value = qc_mul(value, qc_add(z[first ? 2 : 3], j));
    } else {
        divisor = qc_mul(divisor, qc_add(z[first ? 2 : 3], j));
    }
    return qc_div(value, divisor);
}

static qcomplex_t appell_fixed_common_ratio(const qcomplex_t *z, unsigned family, unsigned index)
{
    qcomplex_t j = qc_make(qf_from_double(index - 1u), QF_ZERO);
    if (family == 3) return qc_div(QC_ONE, qc_add(z[4], j));
    qcomplex_t value = qc_add(z[0], j);
    return family == 4 ? qc_mul(value, qc_add(z[1], j)) : value;
}

static qcomplex_t appell_fixed(unsigned family, const qcomplex_t *z)
{
    unsigned count = family == 4 ? 6u : 7u, lower = family == 2 ? 3u : family == 3 ? 4u : 2u;
    unsigned lower_count = family == 3 ? 1u : 2u;
    for (unsigned j = 0; j < count; ++j) if (qc_isnan(z[j]) || qc_isinf(z[j])) return QC_NAN;
    long last = LONG_MAX, m_last = LONG_MAX, n_last = LONG_MAX;
    if (qc_eq(z[count - 2], QC_ZERO)) m_last = 0;
    if (qc_eq(z[count - 1], QC_ZERO)) n_last = 0;
    unsigned degree = family == 3 ? 1u : 0u;
    long double log_constant = 0;
    for (unsigned j = 0; j < lower; ++j) {
        long end = appell_fixed_end(z[j]);
        if (family == 4 || (family == 2 && j == 0)) { if (end < last) last = end; }
        else {
            long *axis = (family == 2 ? j == 1 : j == 0 || j == 2) ? &m_last : &n_last;
            if (end < *axis) *axis = end;
        }
        if (!appell_fixed_bound(z[j], false, &log_constant, &degree)) return QC_NAN;
    }
    for (unsigned j = lower; j < lower + lower_count; ++j) {
        if (appell_fixed_end(z[j]) != LONG_MAX || !appell_fixed_bound(z[j], true, &log_constant, &degree))
            return QC_NAN;
    }
    if (m_last != LONG_MAX && n_last != LONG_MAX && m_last + n_last < last) last = m_last + n_last;
    double x = nextafter(qf_to_double(qc_abs(z[count - 2])), INFINITY);
    double y = nextafter(qf_to_double(qc_abs(z[count - 1])), INFINITY);
    long double radius = family == 2 ? (long double)x + y : family == 3 ? fmax(x, y) :
        (sqrtl(x) + sqrtl(y)) * (sqrtl(x) + sqrtl(y));
    radius *= 1.0L + 1e-12L;
    unsigned limit = 0;
    if (last != LONG_MAX) {
        if (last > QC_APPELL_MAX_DEGREE) return QC_NAN;
        limit = (unsigned)last;
    } else {
        if (!(radius < 1)) return QC_NAN;
        long double log_radius = log2l(radius);
        for (; limit < QC_APPELL_MAX_DEGREE; ++limit) {
            long double ratio = radius * powl((limit + 3.0L) / (limit + 2.0L), degree);
            if (ratio < 1 && log_constant + degree * log2l(limit + 2.0L) +
                (limit + 1.0L) * log_radius - log2l(1.0L - ratio) < -112) break;
        }
        if (limit == QC_APPELL_MAX_DEGREE) return QC_NAN;
    }
    size_t width = m_last < (long)limit ? (size_t)m_last + 1u : (size_t)limit + 1u;
    qcomplex_t *row = calloc(width, sizeof *row), *next = calloc(width, sizeof *next);
    qcomplex_t *axis = calloc(width, sizeof *axis);
    if (!row || !next || !axis) { free(axis); free(row); free(next); return QC_NAN; }
    for (size_t j = 1; j < width; ++j) axis[j] = appell_fixed_axis_ratio(z, family, (unsigned)j, true);
    row[0] = QC_ONE;
    qcomplex_t sum = QC_ONE;
    qfloat_t absolute = QF_ONE;
    size_t cells = 0;
    bool ok = true;
    for (unsigned k = 1; k <= limit; ++k) {
        unsigned low = k > (unsigned long)n_last ? k - (unsigned)n_last : 0u;
        unsigned high = k < width ? k : (unsigned)width - 1u;
        if (low > high || (cells += high - low + 1u) > QC_APPELL_MAX_CELLS) { ok = false; break; }
        if (low) next[low - 1u] = QC_ZERO;
        qcomplex_t common = appell_fixed_common_ratio(z, family, k);
        for (unsigned m = low; m <= high; ++m) {
            qcomplex_t ratio = m ? axis[m] : appell_fixed_axis_ratio(z, family, k, false);
            next[m] = qc_mul(qc_mul(row[m ? m - 1u : 0], ratio), common);
            sum = qc_add(sum, next[m]);
            /* The component L1 norm bounds the modulus without a square root for every term. */
            absolute = qf_add(absolute, qf_add(qf_abs(qc_real(next[m])), qf_abs(qc_imag(next[m]))));
        }
        qcomplex_t *swap = row; row = next; next = swap;
    }
    if (qc_isnan(sum) || qc_isinf(sum) ||
        (!qc_eq(sum, QC_ZERO) && qf_lt(qc_abs(sum), qf_mul(absolute, qf_from_double(1e-8))))) ok = false;
    free(axis); free(next); free(row);
    return ok ? sum : QC_NAN;
}

/* Evaluate the first Appell function as the two-variable complex Lauricella FD member. */
qcomplex_t qc_appell_F1(qcomplex_t a, qcomplex_t b1, qcomplex_t b2, qcomplex_t c, qcomplex_t x, qcomplex_t y)
{
    const qcomplex_t b[2] = {b1, b2};
    const qcomplex_t variables[2] = {x, y};

    return qc_lauricella_F(a, b, c, variables, 2u);
}

/* Evaluate Appell F2 using native double-double complex arithmetic. */
qcomplex_t qc_appell_F2(qcomplex_t a, qcomplex_t b1, qcomplex_t b2, qcomplex_t c1, qcomplex_t c2,
                        qcomplex_t x, qcomplex_t y)
{
    const qcomplex_t z[] = {a, b1, b2, c1, c2, x, y};
    return appell_fixed(2, z);
}

/* Evaluate Appell F3 using its distinct pair of upper parameter lists. */
qcomplex_t qc_appell_F3(qcomplex_t a1, qcomplex_t a2, qcomplex_t b1, qcomplex_t b2, qcomplex_t c,
                        qcomplex_t x, qcomplex_t y)
{
    const qcomplex_t z[] = {a1, a2, b1, b2, c, x, y};
    return appell_fixed(3, z);
}

/* Evaluate Appell F4 in its square-root convergence region or as a terminating polynomial. */
qcomplex_t qc_appell_F4(qcomplex_t a, qcomplex_t b, qcomplex_t c1, qcomplex_t c2, qcomplex_t x, qcomplex_t y)
{
    const qcomplex_t z[] = {a, b, c1, c2, x, y};
    return appell_fixed(4, z);
}
