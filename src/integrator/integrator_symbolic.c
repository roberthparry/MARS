#include "integrator_internal.h"

static qfloat_t integral_exp_affine_box(size_t ndim, const qfloat_t *coeffs,
                                        qfloat_t constant,
                                        const qfloat_t *lo, const qfloat_t *hi,
                                        const bool *active)
{
    qfloat_t total = qf_exp(constant);

    for (size_t i = 0; i < ndim; ++i) {
        qfloat_t factor;

        if (active && !active[i]) {
            if (!qf_eq(coeffs[i], QF_ZERO))
                return QF_NAN;
            continue;
        }

        if (qf_eq(coeffs[i], QF_ZERO)) {
            factor = qf_sub(hi[i], lo[i]);
        } else {
            qfloat_t ahi = qf_mul(coeffs[i], hi[i]);
            qfloat_t alo = qf_mul(coeffs[i], lo[i]);
            factor = qf_div(qf_sub(qf_exp(ahi), qf_exp(alo)), coeffs[i]);
        }
        total = qf_mul(total, factor);
    }
    return total;
}

static qcomplex_t integral_exp_i_affine_box(size_t ndim, const qfloat_t *coeffs,
                                            qfloat_t constant,
                                            const qfloat_t *lo, const qfloat_t *hi,
                                            const bool *active)
{
    qcomplex_t total = qc_exp(qc_make(QF_ZERO, constant));

    for (size_t i = 0; i < ndim; ++i) {
        qcomplex_t factor;

        if (active && !active[i]) {
            if (!qf_eq(coeffs[i], QF_ZERO))
                return qc_make(QF_NAN, QF_NAN);
            continue;
        }

        if (qf_eq(coeffs[i], QF_ZERO)) {
            factor = qc_make(qf_sub(hi[i], lo[i]), QF_ZERO);
        } else {
            qcomplex_t num = qc_sub(qc_exp(qc_make(QF_ZERO, qf_mul(coeffs[i], hi[i]))),
                                    qc_exp(qc_make(QF_ZERO, qf_mul(coeffs[i], lo[i]))));
            factor = qc_div(num, qc_make(QF_ZERO, coeffs[i]));
        }
        total = qc_mul(total, factor);
    }
    return total;
}

static bool find_single_active_dim(size_t ndim, const bool *active, size_t *dim_out)
{
    size_t found = ndim;

    if (!dim_out)
        return false;

    for (size_t i = 0; i < ndim; ++i) {
        if (active && !active[i])
            continue;
        if (found != ndim)
            return false;
        found = i;
    }

    if (found == ndim)
        return false;

    *dim_out = found;
    return true;
}

typedef enum {
    AFFINE_POLY_SPECIAL_EXP,
    AFFINE_POLY_SPECIAL_SIN,
    AFFINE_POLY_SPECIAL_COS,
    AFFINE_POLY_SPECIAL_SINH,
    AFFINE_POLY_SPECIAL_COSH
} affine_poly_special_kind_t;

static qfloat_t eval_box_affine_poly_times_exp_common(size_t ndim, const qfloat_t *coeffs,
                                                      qfloat_t constant,
                                                      const qfloat_t *lo, const qfloat_t *hi,
                                                      const bool *active,
                                                      const qfloat_t poly[5]);

static qcomplex_t eval_box_affine_poly_times_exp_i_common(size_t ndim, const qfloat_t *coeffs,
                                                          qfloat_t constant,
                                                          const qfloat_t *lo, const qfloat_t *hi,
                                                          const bool *active,
                                                          const qfloat_t poly[5]);

static qfloat_t eval_poly_deg4_from_real_moments(const qfloat_t poly[5],
                                                 qfloat_t mean,
                                                 qfloat_t variance,
                                                 qfloat_t third_central,
                                                 qfloat_t sum_fourth_central,
                                                 qfloat_t sum_var_sq)
{
    qfloat_t value = poly[0];

    if (!qf_eq(poly[1], QF_ZERO))
        value = qf_add(value, qf_mul(poly[1], mean));
    if (!qf_eq(poly[2], QF_ZERO)) {
        qfloat_t raw2 = qf_add(qf_mul(mean, mean), variance);
        value = qf_add(value, qf_mul(poly[2], raw2));
    }
    if (!qf_eq(poly[3], QF_ZERO)) {
        qfloat_t raw3 = qf_add(qf_mul(qf_mul(mean, mean), mean),
                               qf_add(qf_mul_double(qf_mul(mean, variance), 3.0),
                                      third_central));
        value = qf_add(value, qf_mul(poly[3], raw3));
    }
    if (!qf_eq(poly[4], QF_ZERO)) {
        qfloat_t raw4 = qf_add(qf_mul(qf_mul(qf_mul(mean, mean), mean), mean),
                               qf_add(qf_mul_double(qf_mul(qf_mul(mean, mean), variance), 6.0),
                                      qf_add(qf_mul_double(qf_mul(mean, third_central), 4.0),
                                             qf_add(qf_mul_double(qf_mul(variance, variance), 3.0),
                                                    qf_sub(sum_fourth_central,
                                                           qf_mul_double(sum_var_sq, 3.0))))));
        value = qf_add(value, qf_mul(poly[4], raw4));
    }

    return value;
}

static qcomplex_t eval_poly_deg4_from_complex_moments(const qfloat_t poly[5],
                                                      qcomplex_t mean,
                                                      qcomplex_t variance,
                                                      qcomplex_t third_central,
                                                      qcomplex_t sum_fourth_central,
                                                      qcomplex_t sum_var_sq)
{
    qcomplex_t value = qc_make(poly[0], QF_ZERO);

    if (!qf_eq(poly[1], QF_ZERO))
        value = qc_add(value, qc_mul(qc_make(poly[1], QF_ZERO), mean));
    if (!qf_eq(poly[2], QF_ZERO)) {
        qcomplex_t raw2 = qc_add(qc_mul(mean, mean), variance);
        value = qc_add(value, qc_mul(qc_make(poly[2], QF_ZERO), raw2));
    }
    if (!qf_eq(poly[3], QF_ZERO)) {
        qcomplex_t raw3 = qc_add(qc_mul(qc_mul(mean, mean), mean),
                                 qc_add(qc_mul(qc_make(qf_from_double(3.0), QF_ZERO),
                                               qc_mul(mean, variance)),
                                        third_central));
        value = qc_add(value, qc_mul(qc_make(poly[3], QF_ZERO), raw3));
    }
    if (!qf_eq(poly[4], QF_ZERO)) {
        qcomplex_t raw4 = qc_add(qc_mul(qc_mul(qc_mul(mean, mean), mean), mean),
                                 qc_add(qc_mul(qc_make(qf_from_double(6.0), QF_ZERO),
                                               qc_mul(qc_mul(mean, mean), variance)),
                                        qc_add(qc_mul(qc_make(qf_from_double(4.0), QF_ZERO),
                                                      qc_mul(mean, third_central)),
                                               qc_add(qc_mul(qc_make(qf_from_double(3.0), QF_ZERO),
                                                             qc_mul(variance, variance)),
                                                      qc_sub(sum_fourth_central,
                                                             qc_mul(qc_make(qf_from_double(3.0), QF_ZERO),
                                                                    sum_var_sq))))));
        value = qc_add(value, qc_mul(qc_make(poly[4], QF_ZERO), raw4));
    }

    return value;
}

static qfloat_t eval_box_affine_unary_special(affine_poly_special_kind_t kind,
                                              size_t ndim,
                                              const qfloat_t *coeffs,
                                              qfloat_t constant,
                                              const qfloat_t *lo, const qfloat_t *hi,
                                              const bool *active)
{
    size_t dim;

    switch (kind) {
    case AFFINE_POLY_SPECIAL_EXP:
        return integral_exp_affine_box(ndim, coeffs, constant, lo, hi, active);
    case AFFINE_POLY_SPECIAL_SINH:
        if (find_single_active_dim(ndim, active, &dim)) {
            if (qf_eq(coeffs[dim], QF_ZERO))
                return qf_mul(qf_sinh(constant), qf_sub(hi[dim], lo[dim]));
            return qf_div(qf_sub(qf_cosh(qf_add(qf_mul(coeffs[dim], hi[dim]), constant)),
                                 qf_cosh(qf_add(qf_mul(coeffs[dim], lo[dim]), constant))),
                          coeffs[dim]);
        } else {
            qfloat_t *neg_coeffs = malloc(ndim * sizeof(*neg_coeffs));
            qfloat_t total;

            if (!neg_coeffs)
                return QF_NAN;
            for (size_t i = 0; i < ndim; ++i)
                neg_coeffs[i] = qf_neg(coeffs[i]);
            total = qf_mul_double(qf_sub(integral_exp_affine_box(ndim, coeffs, constant, lo, hi, active),
                                         integral_exp_affine_box(ndim, neg_coeffs, qf_neg(constant), lo, hi, active)),
                                  0.5);
            free(neg_coeffs);
            return total;
        }
    case AFFINE_POLY_SPECIAL_COSH:
        if (find_single_active_dim(ndim, active, &dim)) {
            if (qf_eq(coeffs[dim], QF_ZERO))
                return qf_mul(qf_cosh(constant), qf_sub(hi[dim], lo[dim]));
            return qf_div(qf_sub(qf_sinh(qf_add(qf_mul(coeffs[dim], hi[dim]), constant)),
                                 qf_sinh(qf_add(qf_mul(coeffs[dim], lo[dim]), constant))),
                          coeffs[dim]);
        } else {
            qfloat_t *neg_coeffs = malloc(ndim * sizeof(*neg_coeffs));
            qfloat_t total;

            if (!neg_coeffs)
                return QF_NAN;
            for (size_t i = 0; i < ndim; ++i)
                neg_coeffs[i] = qf_neg(coeffs[i]);
            total = qf_mul_double(qf_add(integral_exp_affine_box(ndim, coeffs, constant, lo, hi, active),
                                         integral_exp_affine_box(ndim, neg_coeffs, qf_neg(constant), lo, hi, active)),
                                  0.5);
            free(neg_coeffs);
            return total;
        }
    case AFFINE_POLY_SPECIAL_SIN:
        if (find_single_active_dim(ndim, active, &dim)) {
            if (qf_eq(coeffs[dim], QF_ZERO))
                return qf_mul(qf_sin(constant), qf_sub(hi[dim], lo[dim]));
            return qf_div(qf_sub(qf_cos(qf_add(qf_mul(coeffs[dim], lo[dim]), constant)),
                                 qf_cos(qf_add(qf_mul(coeffs[dim], hi[dim]), constant))),
                          coeffs[dim]);
        }
        return integral_exp_i_affine_box(ndim, coeffs, constant, lo, hi, active).im;
    case AFFINE_POLY_SPECIAL_COS:
        if (find_single_active_dim(ndim, active, &dim)) {
            if (qf_eq(coeffs[dim], QF_ZERO))
                return qf_mul(qf_cos(constant), qf_sub(hi[dim], lo[dim]));
            return qf_div(qf_sub(qf_sin(qf_add(qf_mul(coeffs[dim], hi[dim]), constant)),
                                 qf_sin(qf_add(qf_mul(coeffs[dim], lo[dim]), constant))),
                          coeffs[dim]);
        }
        return integral_exp_i_affine_box(ndim, coeffs, constant, lo, hi, active).re;
    }

    return QF_NAN;
}

static bool exp_weighted_affine_stats(size_t ndim, const qfloat_t *coeffs,
                                      qfloat_t constant,
                                      const qfloat_t *lo, const qfloat_t *hi,
                                      const bool *active,
                                      qfloat_t *mean_out,
                                      qfloat_t *variance_out)
{
    qfloat_t mean = constant;
    qfloat_t variance = QF_ZERO;

    if (!mean_out || !variance_out)
        return false;

    for (size_t i = 0; i < ndim; ++i) {
        qfloat_t ahi;
        qfloat_t alo;
        qfloat_t den;
        qfloat_t first_num;
        qfloat_t second_num;
        qfloat_t first_term;
        qfloat_t second_term;

        if (active && !active[i]) {
            if (!qf_eq(coeffs[i], QF_ZERO))
                return false;
            continue;
        }

        if (qf_eq(coeffs[i], QF_ZERO))
            continue;

        ahi = qf_mul(coeffs[i], hi[i]);
        alo = qf_mul(coeffs[i], lo[i]);
        den = qf_sub(qf_exp(ahi), qf_exp(alo));
        first_num = qf_sub(qf_mul(hi[i], qf_exp(ahi)),
                           qf_mul(lo[i], qf_exp(alo)));
        second_num = qf_sub(qf_mul(qf_mul(hi[i], hi[i]), qf_exp(ahi)),
                            qf_mul(qf_mul(lo[i], lo[i]), qf_exp(alo)));
        first_term = qf_div(qf_mul(coeffs[i], first_num), den);
        second_term = qf_div(qf_mul(qf_mul(coeffs[i], coeffs[i]), second_num), den);

        mean = qf_add(mean, qf_sub(first_term, QF_ONE));
        variance = qf_add(variance,
                          qf_add(second_term,
                                 qf_sub(QF_ONE, qf_mul(first_term, first_term))));
    }

    *mean_out = mean;
    *variance_out = variance;
    return true;
}

static bool exp_weighted_affine_third_moment(size_t ndim, const qfloat_t *coeffs,
                                             qfloat_t constant,
                                             const qfloat_t *lo, const qfloat_t *hi,
                                             const bool *active,
                                             qfloat_t *mean_out,
                                             qfloat_t *variance_out,
                                             qfloat_t *third_central_out)
{
    qfloat_t mean = constant;
    qfloat_t variance = QF_ZERO;
    qfloat_t third_central = QF_ZERO;

    if (!mean_out || !variance_out || !third_central_out)
        return false;

    for (size_t i = 0; i < ndim; ++i) {
        qfloat_t ahi;
        qfloat_t alo;
        qfloat_t den;
        qfloat_t first_num;
        qfloat_t second_num;
        qfloat_t third_num;
        qfloat_t first_term;
        qfloat_t second_term;
        qfloat_t third_term;
        qfloat_t raw1;
        qfloat_t raw2;
        qfloat_t raw3;

        if (active && !active[i]) {
            if (!qf_eq(coeffs[i], QF_ZERO))
                return false;
            continue;
        }

        if (qf_eq(coeffs[i], QF_ZERO))
            continue;

        ahi = qf_mul(coeffs[i], hi[i]);
        alo = qf_mul(coeffs[i], lo[i]);
        den = qf_sub(qf_exp(ahi), qf_exp(alo));
        first_num = qf_sub(qf_mul(hi[i], qf_exp(ahi)),
                           qf_mul(lo[i], qf_exp(alo)));
        second_num = qf_sub(qf_mul(qf_mul(hi[i], hi[i]), qf_exp(ahi)),
                            qf_mul(qf_mul(lo[i], lo[i]), qf_exp(alo)));
        third_num = qf_sub(qf_mul(qf_mul(qf_mul(hi[i], hi[i]), hi[i]), qf_exp(ahi)),
                           qf_mul(qf_mul(qf_mul(lo[i], lo[i]), lo[i]), qf_exp(alo)));
        first_term = qf_div(qf_mul(coeffs[i], first_num), den);
        second_term = qf_div(qf_mul(qf_mul(coeffs[i], coeffs[i]), second_num), den);
        third_term = qf_div(qf_mul(qf_mul(qf_mul(coeffs[i], coeffs[i]), coeffs[i]), third_num), den);
        raw1 = qf_sub(first_term, QF_ONE);
        raw2 = qf_add(second_term, qf_sub(qf_mul_double(first_term, -2.0), qf_from_double(-2.0)));
        raw3 = qf_add(third_term,
                      qf_add(qf_mul_double(second_term, -3.0),
                             qf_add(qf_mul_double(first_term, 6.0),
                                    qf_from_double(-6.0))));

        mean = qf_add(mean, raw1);
        variance = qf_add(variance, qf_sub(raw2, qf_mul(raw1, raw1)));
        third_central = qf_add(third_central,
                               qf_add(raw3,
                                      qf_add(qf_mul_double(qf_mul(raw2, raw1), -3.0),
                                             qf_mul_double(qf_mul(qf_mul(raw1, raw1), raw1), 2.0))));
    }

    *mean_out = mean;
    *variance_out = variance;
    *third_central_out = third_central;
    return true;
}

static bool exp_i_weighted_affine_stats(size_t ndim, const qfloat_t *coeffs,
                                        qfloat_t constant,
                                        const qfloat_t *lo, const qfloat_t *hi,
                                        const bool *active,
                                        qcomplex_t *mean_out,
                                        qcomplex_t *variance_out)
{
    qcomplex_t mean = qc_make(constant, QF_ZERO);
    qcomplex_t variance = QC_ZERO;

    if (!mean_out || !variance_out)
        return false;

    for (size_t i = 0; i < ndim; ++i) {
        qcomplex_t exp_hi;
        qcomplex_t exp_lo;
        qcomplex_t den;
        qcomplex_t first_ratio;
        qcomplex_t second_ratio;
        qcomplex_t first_term;
        qcomplex_t second_term;

        if (active && !active[i]) {
            if (!qf_eq(coeffs[i], QF_ZERO))
                return false;
            continue;
        }

        if (qf_eq(coeffs[i], QF_ZERO))
            continue;

        exp_hi = qc_exp(qc_make(QF_ZERO, qf_mul(coeffs[i], hi[i])));
        exp_lo = qc_exp(qc_make(QF_ZERO, qf_mul(coeffs[i], lo[i])));
        den = qc_sub(exp_hi, exp_lo);
        first_ratio = qc_div(qc_sub(qc_mul(qc_make(hi[i], QF_ZERO), exp_hi),
                                    qc_mul(qc_make(lo[i], QF_ZERO), exp_lo)),
                             den);
        second_ratio = qc_div(qc_sub(qc_mul(qc_make(qf_mul(hi[i], hi[i]), QF_ZERO), exp_hi),
                                     qc_mul(qc_make(qf_mul(lo[i], lo[i]), QF_ZERO), exp_lo)),
                              den);
        first_term = qc_mul(qc_make(coeffs[i], QF_ZERO), first_ratio);
        second_term = qc_mul(qc_make(qf_mul(coeffs[i], coeffs[i]), QF_ZERO), second_ratio);

        mean = qc_add(mean, qc_add(first_term, qc_make(QF_ZERO, QF_ONE)));
        variance = qc_add(variance,
                          qc_sub(second_term,
                                 qc_add(qc_mul(first_term, first_term),
                                        qc_make(QF_ONE, QF_ZERO))));
    }

    *mean_out = mean;
    *variance_out = variance;
    return true;
}

static bool exp_i_weighted_affine_third_moment(size_t ndim, const qfloat_t *coeffs,
                                               qfloat_t constant,
                                               const qfloat_t *lo, const qfloat_t *hi,
                                               const bool *active,
                                               qcomplex_t *mean_out,
                                               qcomplex_t *variance_out,
                                               qcomplex_t *third_central_out)
{
    qcomplex_t mean = qc_make(constant, QF_ZERO);
    qcomplex_t variance = QC_ZERO;
    qcomplex_t third_central = QC_ZERO;

    if (!mean_out || !variance_out || !third_central_out)
        return false;

    for (size_t i = 0; i < ndim; ++i) {
        qcomplex_t exp_hi;
        qcomplex_t exp_lo;
        qcomplex_t den;
        qcomplex_t first_ratio;
        qcomplex_t second_ratio;
        qcomplex_t third_ratio;
        qcomplex_t first_term;
        qcomplex_t second_term;
        qcomplex_t third_term;
        qcomplex_t raw1;
        qcomplex_t raw2;
        qcomplex_t raw3;

        if (active && !active[i]) {
            if (!qf_eq(coeffs[i], QF_ZERO))
                return false;
            continue;
        }

        if (qf_eq(coeffs[i], QF_ZERO))
            continue;

        exp_hi = qc_exp(qc_make(QF_ZERO, qf_mul(coeffs[i], hi[i])));
        exp_lo = qc_exp(qc_make(QF_ZERO, qf_mul(coeffs[i], lo[i])));
        den = qc_sub(exp_hi, exp_lo);
        first_ratio = qc_div(qc_sub(qc_mul(qc_make(hi[i], QF_ZERO), exp_hi),
                                    qc_mul(qc_make(lo[i], QF_ZERO), exp_lo)),
                             den);
        second_ratio = qc_div(qc_sub(qc_mul(qc_make(qf_mul(hi[i], hi[i]), QF_ZERO), exp_hi),
                                     qc_mul(qc_make(qf_mul(lo[i], lo[i]), QF_ZERO), exp_lo)),
                              den);
        third_ratio = qc_div(qc_sub(qc_mul(qc_make(qf_mul(qf_mul(hi[i], hi[i]), hi[i]), QF_ZERO), exp_hi),
                                    qc_mul(qc_make(qf_mul(qf_mul(lo[i], lo[i]), lo[i]), QF_ZERO), exp_lo)),
                             den);
        first_term = qc_mul(qc_make(coeffs[i], QF_ZERO), first_ratio);
        second_term = qc_mul(qc_make(qf_mul(coeffs[i], coeffs[i]), QF_ZERO), second_ratio);
        third_term = qc_mul(qc_make(qf_mul(qf_mul(coeffs[i], coeffs[i]), coeffs[i]), QF_ZERO), third_ratio);
        raw1 = qc_add(first_term, qc_make(QF_ZERO, QF_ONE));
        raw2 = qc_add(second_term,
                      qc_add(qc_mul(qc_make(QF_ZERO, qf_from_double(2.0)), first_term),
                             qc_make(qf_from_double(-2.0), QF_ZERO)));
        raw3 = qc_add(third_term,
                      qc_add(qc_mul(qc_make(QF_ZERO, qf_from_double(3.0)), second_term),
                             qc_add(qc_mul(qc_make(qf_from_double(-6.0), QF_ZERO), first_term),
                                    qc_make(QF_ZERO, qf_from_double(-6.0)))));

        mean = qc_add(mean, raw1);
        variance = qc_add(variance, qc_sub(raw2, qc_mul(raw1, raw1)));
        third_central = qc_add(third_central,
                               qc_add(raw3,
                                      qc_add(qc_mul(qc_make(qf_from_double(-3.0), QF_ZERO),
                                                    qc_mul(raw2, raw1)),
                                             qc_mul(qc_make(qf_from_double(2.0), QF_ZERO),
                                                    qc_mul(qc_mul(raw1, raw1), raw1)))));
    }

    *mean_out = mean;
    *variance_out = variance;
    *third_central_out = third_central;
    return true;
}

static bool exp_weighted_affine_fourth_moment(size_t ndim, const qfloat_t *coeffs,
                                              qfloat_t constant,
                                              const qfloat_t *lo, const qfloat_t *hi,
                                              const bool *active,
                                              qfloat_t *mean_out,
                                              qfloat_t *variance_out,
                                              qfloat_t *third_central_out,
                                              qfloat_t *sum_fourth_central_out,
                                              qfloat_t *sum_var_sq_out)
{
    qfloat_t mean = constant;
    qfloat_t variance = QF_ZERO;
    qfloat_t third_central = QF_ZERO;
    qfloat_t sum_fourth_central = QF_ZERO;
    qfloat_t sum_var_sq = QF_ZERO;

    if (!mean_out || !variance_out || !third_central_out ||
        !sum_fourth_central_out || !sum_var_sq_out)
        return false;

    for (size_t i = 0; i < ndim; ++i) {
        qfloat_t ahi;
        qfloat_t alo;
        qfloat_t den;
        qfloat_t first_num;
        qfloat_t second_num;
        qfloat_t third_num;
        qfloat_t fourth_num;
        qfloat_t first_term;
        qfloat_t second_term;
        qfloat_t third_term;
        qfloat_t fourth_term;
        qfloat_t raw1;
        qfloat_t raw2;
        qfloat_t raw3;
        qfloat_t raw4;
        qfloat_t var_i;
        qfloat_t third_i;
        qfloat_t fourth_i;

        if (active && !active[i]) {
            if (!qf_eq(coeffs[i], QF_ZERO))
                return false;
            continue;
        }

        if (qf_eq(coeffs[i], QF_ZERO))
            continue;

        ahi = qf_mul(coeffs[i], hi[i]);
        alo = qf_mul(coeffs[i], lo[i]);
        den = qf_sub(qf_exp(ahi), qf_exp(alo));
        first_num = qf_sub(qf_mul(hi[i], qf_exp(ahi)),
                           qf_mul(lo[i], qf_exp(alo)));
        second_num = qf_sub(qf_mul(qf_mul(hi[i], hi[i]), qf_exp(ahi)),
                            qf_mul(qf_mul(lo[i], lo[i]), qf_exp(alo)));
        third_num = qf_sub(qf_mul(qf_mul(qf_mul(hi[i], hi[i]), hi[i]), qf_exp(ahi)),
                           qf_mul(qf_mul(qf_mul(lo[i], lo[i]), lo[i]), qf_exp(alo)));
        fourth_num = qf_sub(qf_mul(qf_mul(qf_mul(qf_mul(hi[i], hi[i]), hi[i]), hi[i]), qf_exp(ahi)),
                            qf_mul(qf_mul(qf_mul(qf_mul(lo[i], lo[i]), lo[i]), lo[i]), qf_exp(alo)));
        first_term = qf_div(qf_mul(coeffs[i], first_num), den);
        second_term = qf_div(qf_mul(qf_mul(coeffs[i], coeffs[i]), second_num), den);
        third_term = qf_div(qf_mul(qf_mul(qf_mul(coeffs[i], coeffs[i]), coeffs[i]), third_num), den);
        fourth_term = qf_div(qf_mul(qf_mul(qf_mul(qf_mul(coeffs[i], coeffs[i]), coeffs[i]), coeffs[i]), fourth_num), den);
        raw1 = qf_sub(first_term, QF_ONE);
        raw2 = qf_add(second_term, qf_sub(qf_mul_double(first_term, -2.0), qf_from_double(-2.0)));
        raw3 = qf_add(third_term,
                      qf_add(qf_mul_double(second_term, -3.0),
                             qf_add(qf_mul_double(first_term, 6.0), qf_from_double(-6.0))));
        raw4 = qf_add(fourth_term,
                      qf_add(qf_mul_double(third_term, -4.0),
                             qf_add(qf_mul_double(second_term, 12.0),
                                    qf_add(qf_mul_double(first_term, -24.0),
                                           qf_from_double(24.0)))));
        var_i = qf_sub(raw2, qf_mul(raw1, raw1));
        third_i = qf_add(raw3,
                         qf_add(qf_mul_double(qf_mul(raw2, raw1), -3.0),
                                qf_mul_double(qf_mul(qf_mul(raw1, raw1), raw1), 2.0)));
        fourth_i = qf_add(raw4,
                          qf_add(qf_mul_double(qf_mul(raw3, raw1), -4.0),
                                 qf_add(qf_mul_double(qf_mul(raw2, qf_mul(raw1, raw1)), 6.0),
                                        qf_mul_double(qf_mul(qf_mul(raw1, raw1),
                                                             qf_mul(raw1, raw1)), -3.0))));

        mean = qf_add(mean, raw1);
        variance = qf_add(variance, var_i);
        third_central = qf_add(third_central, third_i);
        sum_fourth_central = qf_add(sum_fourth_central, fourth_i);
        sum_var_sq = qf_add(sum_var_sq, qf_mul(var_i, var_i));
    }

    *mean_out = mean;
    *variance_out = variance;
    *third_central_out = third_central;
    *sum_fourth_central_out = sum_fourth_central;
    *sum_var_sq_out = sum_var_sq;
    return true;
}

static bool exp_i_weighted_affine_fourth_moment(size_t ndim, const qfloat_t *coeffs,
                                                qfloat_t constant,
                                                const qfloat_t *lo, const qfloat_t *hi,
                                                const bool *active,
                                                qcomplex_t *mean_out,
                                                qcomplex_t *variance_out,
                                                qcomplex_t *third_central_out,
                                                qcomplex_t *sum_fourth_central_out,
                                                qcomplex_t *sum_var_sq_out)
{
    qcomplex_t mean = qc_make(constant, QF_ZERO);
    qcomplex_t variance = QC_ZERO;
    qcomplex_t third_central = QC_ZERO;
    qcomplex_t sum_fourth_central = QC_ZERO;
    qcomplex_t sum_var_sq = QC_ZERO;

    if (!mean_out || !variance_out || !third_central_out ||
        !sum_fourth_central_out || !sum_var_sq_out)
        return false;

    for (size_t i = 0; i < ndim; ++i) {
        qcomplex_t exp_hi;
        qcomplex_t exp_lo;
        qcomplex_t den;
        qcomplex_t first_ratio;
        qcomplex_t second_ratio;
        qcomplex_t third_ratio;
        qcomplex_t fourth_ratio;
        qcomplex_t first_term;
        qcomplex_t second_term;
        qcomplex_t third_term;
        qcomplex_t fourth_term;
        qcomplex_t raw1;
        qcomplex_t raw2;
        qcomplex_t raw3;
        qcomplex_t raw4;
        qcomplex_t var_i;
        qcomplex_t third_i;
        qcomplex_t fourth_i;

        if (active && !active[i]) {
            if (!qf_eq(coeffs[i], QF_ZERO))
                return false;
            continue;
        }

        if (qf_eq(coeffs[i], QF_ZERO))
            continue;

        exp_hi = qc_exp(qc_make(QF_ZERO, qf_mul(coeffs[i], hi[i])));
        exp_lo = qc_exp(qc_make(QF_ZERO, qf_mul(coeffs[i], lo[i])));
        den = qc_sub(exp_hi, exp_lo);
        first_ratio = qc_div(qc_sub(qc_mul(qc_make(hi[i], QF_ZERO), exp_hi),
                                    qc_mul(qc_make(lo[i], QF_ZERO), exp_lo)),
                             den);
        second_ratio = qc_div(qc_sub(qc_mul(qc_make(qf_mul(hi[i], hi[i]), QF_ZERO), exp_hi),
                                     qc_mul(qc_make(qf_mul(lo[i], lo[i]), QF_ZERO), exp_lo)),
                              den);
        third_ratio = qc_div(qc_sub(qc_mul(qc_make(qf_mul(qf_mul(hi[i], hi[i]), hi[i]), QF_ZERO), exp_hi),
                                    qc_mul(qc_make(qf_mul(qf_mul(lo[i], lo[i]), lo[i]), QF_ZERO), exp_lo)),
                             den);
        fourth_ratio = qc_div(qc_sub(qc_mul(qc_make(qf_mul(qf_mul(qf_mul(hi[i], hi[i]), hi[i]), hi[i]), QF_ZERO), exp_hi),
                                     qc_mul(qc_make(qf_mul(qf_mul(qf_mul(lo[i], lo[i]), lo[i]), lo[i]), QF_ZERO), exp_lo)),
                              den);
        first_term = qc_mul(qc_make(coeffs[i], QF_ZERO), first_ratio);
        second_term = qc_mul(qc_make(qf_mul(coeffs[i], coeffs[i]), QF_ZERO), second_ratio);
        third_term = qc_mul(qc_make(qf_mul(qf_mul(coeffs[i], coeffs[i]), coeffs[i]), QF_ZERO), third_ratio);
        fourth_term = qc_mul(qc_make(qf_mul(qf_mul(qf_mul(coeffs[i], coeffs[i]), coeffs[i]), coeffs[i]), QF_ZERO), fourth_ratio);
        raw1 = qc_add(first_term, qc_make(QF_ZERO, QF_ONE));
        raw2 = qc_add(second_term,
                      qc_add(qc_mul(qc_make(QF_ZERO, qf_from_double(2.0)), first_term),
                             qc_make(qf_from_double(-2.0), QF_ZERO)));
        raw3 = qc_add(third_term,
                      qc_add(qc_mul(qc_make(QF_ZERO, qf_from_double(3.0)), second_term),
                             qc_add(qc_mul(qc_make(qf_from_double(-6.0), QF_ZERO), first_term),
                                    qc_make(QF_ZERO, qf_from_double(-6.0)))));
        raw4 = qc_add(fourth_term,
                      qc_add(qc_mul(qc_make(QF_ZERO, qf_from_double(4.0)), third_term),
                             qc_add(qc_mul(qc_make(qf_from_double(-12.0), QF_ZERO), second_term),
                                    qc_add(qc_mul(qc_make(QF_ZERO, qf_from_double(-24.0)), first_term),
                                           qc_make(qf_from_double(24.0), QF_ZERO)))));
        var_i = qc_sub(raw2, qc_mul(raw1, raw1));
        third_i = qc_add(raw3,
                         qc_add(qc_mul(qc_make(qf_from_double(-3.0), QF_ZERO), qc_mul(raw2, raw1)),
                                qc_mul(qc_make(qf_from_double(2.0), QF_ZERO), qc_mul(qc_mul(raw1, raw1), raw1))));
        fourth_i = qc_add(raw4,
                          qc_add(qc_mul(qc_make(qf_from_double(-4.0), QF_ZERO), qc_mul(raw3, raw1)),
                                 qc_add(qc_mul(qc_make(qf_from_double(6.0), QF_ZERO),
                                               qc_mul(raw2, qc_mul(raw1, raw1))),
                                        qc_mul(qc_make(qf_from_double(-3.0), QF_ZERO),
                                               qc_mul(qc_mul(raw1, raw1),
                                                      qc_mul(raw1, raw1))))));

        mean = qc_add(mean, raw1);
        variance = qc_add(variance, var_i);
        third_central = qc_add(third_central, third_i);
        sum_fourth_central = qc_add(sum_fourth_central, fourth_i);
        sum_var_sq = qc_add(sum_var_sq, qc_mul(var_i, var_i));
    }

    *mean_out = mean;
    *variance_out = variance;
    *third_central_out = third_central;
    *sum_fourth_central_out = sum_fourth_central;
    *sum_var_sq_out = sum_var_sq;
    return true;
}

static qfloat_t eval_box_affine_poly_times_exp_common(size_t ndim, const qfloat_t *coeffs,
                                                      qfloat_t constant,
                                                      const qfloat_t *lo, const qfloat_t *hi,
                                                      const bool *active,
                                                      const qfloat_t poly[5])
{
    qfloat_t total = integral_exp_affine_box(ndim, coeffs, constant, lo, hi, active);
    qfloat_t mean;
    qfloat_t variance;
    qfloat_t third_central = QF_ZERO;
    qfloat_t sum_fourth_central = QF_ZERO;
    qfloat_t sum_var_sq = QF_ZERO;

    if (qf_isnan(total))
        return QF_NAN;
    if (!qf_eq(poly[4], QF_ZERO)) {
        if (!exp_weighted_affine_fourth_moment(ndim, coeffs, constant, lo, hi, active,
                                               &mean, &variance, &third_central,
                                               &sum_fourth_central, &sum_var_sq))
            return QF_NAN;
    } else if (!qf_eq(poly[3], QF_ZERO)) {
        if (!exp_weighted_affine_third_moment(ndim, coeffs, constant, lo, hi, active,
                                              &mean, &variance, &third_central))
            return QF_NAN;
    } else if (!qf_eq(poly[2], QF_ZERO) || !qf_eq(poly[1], QF_ZERO)) {
        if (!exp_weighted_affine_stats(ndim, coeffs, constant, lo, hi, active,
                                       &mean, &variance))
            return QF_NAN;
    }

    return qf_mul(total,
                  eval_poly_deg4_from_real_moments(poly, mean, variance,
                                                   third_central,
                                                   sum_fourth_central,
                                                   sum_var_sq));
}

static qcomplex_t eval_box_affine_poly_times_exp_i_common(size_t ndim, const qfloat_t *coeffs,
                                                          qfloat_t constant,
                                                          const qfloat_t *lo, const qfloat_t *hi,
                                                          const bool *active,
                                                          const qfloat_t poly[5])
{
    qcomplex_t total = integral_exp_i_affine_box(ndim, coeffs, constant, lo, hi, active);
    qcomplex_t mean;
    qcomplex_t variance;
    qcomplex_t third_central = QC_ZERO;
    qcomplex_t sum_fourth_central = QC_ZERO;
    qcomplex_t sum_var_sq = QC_ZERO;

    if (qc_isnan(total))
        return qc_make(QF_NAN, QF_NAN);
    if (!qf_eq(poly[4], QF_ZERO)) {
        if (!exp_i_weighted_affine_fourth_moment(ndim, coeffs, constant, lo, hi, active,
                                                 &mean, &variance, &third_central,
                                                 &sum_fourth_central, &sum_var_sq))
            return qc_make(QF_NAN, QF_NAN);
    } else if (!qf_eq(poly[3], QF_ZERO)) {
        if (!exp_i_weighted_affine_third_moment(ndim, coeffs, constant, lo, hi, active,
                                                &mean, &variance, &third_central))
            return qc_make(QF_NAN, QF_NAN);
    } else if (!qf_eq(poly[2], QF_ZERO) || !qf_eq(poly[1], QF_ZERO)) {
        if (!exp_i_weighted_affine_stats(ndim, coeffs, constant, lo, hi, active,
                                         &mean, &variance))
            return qc_make(QF_NAN, QF_NAN);
    }

    return qc_mul(total,
                  eval_poly_deg4_from_complex_moments(poly, mean, variance,
                                                      third_central,
                                                      sum_fourth_central,
                                                      sum_var_sq));
}

static void negate_odd_poly_deg4(const qfloat_t in[5], qfloat_t out[5])
{
    out[0] = in[0];
    out[1] = qf_neg(in[1]);
    out[2] = in[2];
    out[3] = qf_neg(in[3]);
    out[4] = in[4];
}

static qfloat_t eval_box_affine_poly_deg4_times_special(affine_poly_special_kind_t kind,
                                                        size_t ndim,
                                                        const qfloat_t *coeffs,
                                                        qfloat_t constant,
                                                        const qfloat_t *lo, const qfloat_t *hi,
                                                        const bool *active,
                                                        const qfloat_t poly[5])
{
    switch (kind) {
    case AFFINE_POLY_SPECIAL_EXP:
        return eval_box_affine_poly_times_exp_common(ndim, coeffs, constant, lo, hi, active, poly);
    case AFFINE_POLY_SPECIAL_SIN:
        return eval_box_affine_poly_times_exp_i_common(ndim, coeffs, constant, lo, hi, active, poly).im;
    case AFFINE_POLY_SPECIAL_COS:
        return eval_box_affine_poly_times_exp_i_common(ndim, coeffs, constant, lo, hi, active, poly).re;
    case AFFINE_POLY_SPECIAL_SINH:
    case AFFINE_POLY_SPECIAL_COSH: {
        qfloat_t *neg_coeffs = malloc(ndim * sizeof(*neg_coeffs));
        qfloat_t neg_poly[5];
        qfloat_t pos;
        qfloat_t neg;

        if (!neg_coeffs)
            return QF_NAN;

        for (size_t i = 0; i < ndim; ++i)
            neg_coeffs[i] = qf_neg(coeffs[i]);
        negate_odd_poly_deg4(poly, neg_poly);

        pos = eval_box_affine_poly_times_exp_common(ndim, coeffs, constant, lo, hi, active, poly);
        neg = eval_box_affine_poly_times_exp_common(ndim, neg_coeffs, qf_neg(constant), lo, hi, active, neg_poly);
        free(neg_coeffs);
        return qf_mul_double(kind == AFFINE_POLY_SPECIAL_SINH ? qf_sub(pos, neg)
                                                              : qf_add(pos, neg),
                             0.5);
    }
    }

    return QF_NAN;
}

static bool uniform_affine_box_stats_deg4(size_t ndim,
                                          const qfloat_t *coeffs,
                                          qfloat_t constant,
                                          const qfloat_t *lo, const qfloat_t *hi,
                                          const bool *active,
                                          qfloat_t *volume_out,
                                          qfloat_t *mean_out,
                                          qfloat_t *variance_out,
                                          qfloat_t *sum_fourth_central_out,
                                          qfloat_t *sum_var_sq_out)
{
    qfloat_t mean = constant;
    qfloat_t variance = QF_ZERO;
    qfloat_t sum_fourth_central = QF_ZERO;
    qfloat_t sum_var_sq = QF_ZERO;
    qfloat_t volume = QF_ONE;
    qfloat_t two = qf_from_double(2.0);
    qfloat_t twelve = qf_from_double(12.0);
    qfloat_t eighty = qf_from_double(80.0);

    if (!volume_out || !mean_out || !variance_out ||
        !sum_fourth_central_out || !sum_var_sq_out)
        return false;

    for (size_t i = 0; i < ndim; ++i) {
        qfloat_t len;

        if (active && !active[i]) {
            if (!qf_eq(coeffs[i], QF_ZERO))
                return false;
            continue;
        }

        len = qf_sub(hi[i], lo[i]);
        volume = qf_mul(volume, len);

        if (!qf_eq(coeffs[i], QF_ZERO)) {
            qfloat_t midpoint = qf_div(qf_add(lo[i], hi[i]), two);
            qfloat_t coeff_sq = qf_mul(coeffs[i], coeffs[i]);
            qfloat_t len_sq = qf_mul(len, len);
            qfloat_t var_i = qf_div(qf_mul(coeff_sq, len_sq), twelve);
            qfloat_t fourth_i = qf_div(qf_mul(qf_mul(coeff_sq, coeff_sq),
                                              qf_mul(len_sq, len_sq)),
                                       eighty);

            mean = qf_add(mean, qf_mul(coeffs[i], midpoint));
            variance = qf_add(variance, var_i);
            sum_var_sq = qf_add(sum_var_sq, qf_mul(var_i, var_i));
            sum_fourth_central = qf_add(sum_fourth_central, fourth_i);
        }
    }

    *volume_out = volume;
    *mean_out = mean;
    *variance_out = variance;
    *sum_fourth_central_out = sum_fourth_central;
    *sum_var_sq_out = sum_var_sq;
    return true;
}

static qfloat_t eval_box_affine_poly_deg4(size_t ndim,
                                          const qfloat_t *coeffs,
                                          qfloat_t constant,
                                          const qfloat_t *lo, const qfloat_t *hi,
                                          const bool *active,
                                          const qfloat_t poly[5])
{
    qfloat_t volume;
    qfloat_t mean;
    qfloat_t variance;
    qfloat_t sum_fourth_central;
    qfloat_t sum_var_sq;

    if (!uniform_affine_box_stats_deg4(ndim, coeffs, constant, lo, hi, active,
                                       &volume, &mean, &variance,
                                       &sum_fourth_central, &sum_var_sq))
        return QF_NAN;

    return qf_mul(volume,
                  eval_poly_deg4_from_real_moments(poly, mean, variance, QF_ZERO,
                                                   sum_fourth_central, sum_var_sq));
}

static const struct {
    dv_pattern_unary_affine_kind_t unary_kind;
    affine_poly_special_kind_t special_kind;
} affine_special_kinds[] = {
    { DV_PATTERN_UNARY_EXP,  AFFINE_POLY_SPECIAL_EXP  },
    { DV_PATTERN_UNARY_SIN,  AFFINE_POLY_SPECIAL_SIN  },
    { DV_PATTERN_UNARY_COS,  AFFINE_POLY_SPECIAL_COS  },
    { DV_PATTERN_UNARY_SINH, AFFINE_POLY_SPECIAL_SINH },
    { DV_PATTERN_UNARY_COSH, AFFINE_POLY_SPECIAL_COSH }
};

typedef enum {
    SYMBOLIC_PLAN_NONE = 0,
    SYMBOLIC_PLAN_CONST,
    SYMBOLIC_PLAN_AFFINE_POLY_SPECIAL,
    SYMBOLIC_PLAN_AFFINE_POLY,
    SYMBOLIC_PLAN_AFFINE_UNARY_SPECIAL,
    SYMBOLIC_PLAN_SCALED,
    SYMBOLIC_PLAN_ADD_SUB,
    SYMBOLIC_PLAN_MUL
} symbolic_plan_kind_t;

typedef struct {
    symbolic_plan_kind_t kind;
    qfloat_t scalar;
    qfloat_t constant;
    qfloat_t poly[5];
    affine_poly_special_kind_t special_kind;
    const dval_t *left;
    const dval_t *right;
    const dval_t *base;
    bool is_sub;
} symbolic_plan_t;

typedef struct {
    const dval_t *left;
    const dval_t *right;
    bool *used_left;
    bool *used_right;
} symbolic_binary_product_plan_t;

typedef struct {
    size_t ngroups;
    const dval_t **group_exprs;
    bool *group_owned;
    bool **group_used;
} symbolic_separable_product_plan_t;

static qfloat_t box_volume(size_t ndim, const qfloat_t *lo, const qfloat_t *hi,
                           const bool *active)
{
    qfloat_t v = QF_ONE;
    for (size_t i = 0; i < ndim; ++i)
        if (!active || active[i])
            v = qf_mul(v, qf_sub(hi[i], lo[i]));
    return v;
}

static bool try_eval_special_combination(const dval_t *expr,
                                         size_t ndim,
                                         dval_t *const *vars,
                                         const qfloat_t *lo,
                                         const qfloat_t *hi,
                                         const bool *active,
                                         int depth,
                                         qfloat_t *out);

static void symbolic_plan_reset(symbolic_plan_t *plan)
{
    if (!plan)
        return;
    plan->kind = SYMBOLIC_PLAN_NONE;
    plan->left = NULL;
    plan->right = NULL;
    plan->base = NULL;
    plan->is_sub = false;
    plan->scalar = QF_ZERO;
    plan->constant = QF_ZERO;
    plan->special_kind = AFFINE_POLY_SPECIAL_EXP;
    for (size_t i = 0; i < 5; ++i)
        plan->poly[i] = QF_ZERO;
}

static void free_symbolic_binary_product_plan(symbolic_binary_product_plan_t *plan)
{
    if (!plan)
        return;
    free(plan->used_left);
    free(plan->used_right);
    plan->used_left = NULL;
    plan->used_right = NULL;
    plan->left = NULL;
    plan->right = NULL;
}

static void free_symbolic_separable_product_plan(symbolic_separable_product_plan_t *plan)
{
    if (!plan)
        return;

    if (plan->group_exprs && plan->group_owned) {
        for (size_t i = 0; i < plan->ngroups; ++i) {
            if (plan->group_owned[i] && plan->group_exprs[i])
                dv_free((dval_t *)plan->group_exprs[i]);
        }
    }

    if (plan->group_used) {
        for (size_t i = 0; i < plan->ngroups; ++i)
            free(plan->group_used[i]);
    }

    free(plan->group_exprs);
    free(plan->group_owned);
    free(plan->group_used);

    plan->ngroups = 0;
    plan->group_exprs = NULL;
    plan->group_owned = NULL;
    plan->group_used = NULL;
}

static bool append_product_factor(const dval_t *expr,
                                  const dval_t ***factors_io,
                                  size_t *count_io,
                                  size_t *cap_io)
{
    const dval_t *left = NULL;
    const dval_t *right = NULL;
    const dval_t **grown;
    size_t new_cap;

    if (!expr || !factors_io || !count_io || !cap_io)
        return false;

    if (dv_match_mul_expr(expr, &left, &right))
        return append_product_factor(left, factors_io, count_io, cap_io) &&
               append_product_factor(right, factors_io, count_io, cap_io);

    if (*count_io == *cap_io) {
        new_cap = (*cap_io == 0) ? 4 : (*cap_io * 2);
        grown = realloc((void *)*factors_io, new_cap * sizeof(**factors_io));
        if (!grown)
            return false;
        *factors_io = grown;
        *cap_io = new_cap;
    }

    (*factors_io)[(*count_io)++] = expr;
    return true;
}

static bool usage_masks_overlap(size_t ndim, const bool *lhs, const bool *rhs)
{
    for (size_t i = 0; i < ndim; ++i) {
        if (lhs[i] && rhs[i])
            return true;
    }
    return false;
}

static bool build_direct_affine_plan(const dval_t *expr,
                                     size_t ndim,
                                     dval_t *const *vars,
                                     symbolic_plan_t *plan,
                                     qfloat_t *coeffs_out)
{
    qfloat_t constant;
    qfloat_t poly[5];

    if (!expr || !plan || !coeffs_out)
        return false;

    for (size_t i = 0; i < sizeof(affine_special_kinds) / sizeof(affine_special_kinds[0]); ++i) {
        if (!dv_match_affine_poly_deg4_times_unary_affine_kind(expr,
                                                               affine_special_kinds[i].unary_kind,
                                                               ndim, vars, poly, &constant,
                                                               coeffs_out))
            continue;
        plan->kind = SYMBOLIC_PLAN_AFFINE_POLY_SPECIAL;
        plan->constant = constant;
        plan->special_kind = affine_special_kinds[i].special_kind;
        for (size_t j = 0; j < 5; ++j)
            plan->poly[j] = poly[j];
        return true;
    }

    if (dv_match_affine_poly_deg4(expr, ndim, vars, poly, &constant, coeffs_out)) {
        plan->kind = SYMBOLIC_PLAN_AFFINE_POLY;
        plan->constant = constant;
        for (size_t j = 0; j < 5; ++j)
            plan->poly[j] = poly[j];
        return true;
    }

    for (size_t i = 0; i < sizeof(affine_special_kinds) / sizeof(affine_special_kinds[0]); ++i) {
        if (!dv_match_unary_affine_kind(expr, affine_special_kinds[i].unary_kind,
                                        ndim, vars, &constant, coeffs_out))
            continue;
        plan->kind = SYMBOLIC_PLAN_AFFINE_UNARY_SPECIAL;
        plan->constant = constant;
        plan->special_kind = affine_special_kinds[i].special_kind;
        return true;
    }

    return false;
}

static bool eval_direct_affine_plan(const symbolic_plan_t *plan,
                                    size_t ndim,
                                    const qfloat_t *coeffs,
                                    const qfloat_t *lo,
                                    const qfloat_t *hi,
                                    const bool *active,
                                    qfloat_t *out)
{
    if (!plan || !coeffs || !out)
        return false;

    switch (plan->kind) {
    case SYMBOLIC_PLAN_AFFINE_POLY_SPECIAL:
        *out = eval_box_affine_poly_deg4_times_special(plan->special_kind, ndim, coeffs,
                                                       plan->constant, lo, hi, active,
                                                       plan->poly);
        return !qf_isnan(*out);
    case SYMBOLIC_PLAN_AFFINE_POLY:
        *out = eval_box_affine_poly_deg4(ndim, coeffs, plan->constant, lo, hi, active,
                                         plan->poly);
        return !qf_isnan(*out);
    case SYMBOLIC_PLAN_AFFINE_UNARY_SPECIAL:
        *out = eval_box_affine_unary_special(plan->special_kind, ndim, coeffs,
                                             plan->constant, lo, hi, active);
        return !qf_isnan(*out);
    default:
        return false;
    }
}

static bool build_symbolic_plan(const dval_t *expr,
                                size_t ndim,
                                dval_t *const *vars,
                                symbolic_plan_t *plan,
                                qfloat_t *coeffs_out)
{
    qfloat_t constant;
    qfloat_t scale;
    const dval_t *base;
    const dval_t *left;
    const dval_t *right;
    bool is_sub;

    if (!expr || !plan)
        return false;

    symbolic_plan_reset(plan);

    if (dv_match_const_value(expr, &constant)) {
        plan->kind = SYMBOLIC_PLAN_CONST;
        plan->scalar = constant;
        return true;
    }

    if (coeffs_out && build_direct_affine_plan(expr, ndim, vars, plan, coeffs_out))
        return true;

    if (dv_match_scaled_expr(expr, &scale, &base)) {
        plan->kind = SYMBOLIC_PLAN_SCALED;
        plan->scalar = scale;
        plan->base = base;
        return true;
    }

    if (dv_match_add_sub_expr(expr, &left, &right, &is_sub)) {
        plan->kind = SYMBOLIC_PLAN_ADD_SUB;
        plan->left = left;
        plan->right = right;
        plan->is_sub = is_sub;
        return true;
    }

    if (dv_match_mul_expr(expr, &left, &right)) {
        plan->kind = SYMBOLIC_PLAN_MUL;
        plan->left = left;
        plan->right = right;
        return true;
    }

    return false;
}

static bool build_binary_product_plan(const dval_t *left,
                                      const dval_t *right,
                                      size_t ndim,
                                      dval_t *const *vars,
                                      const bool *active,
                                      symbolic_binary_product_plan_t *plan)
{
    bool disjoint = true;

    if (!left || !right || !plan)
        return false;

    plan->left = left;
    plan->right = right;
    plan->used_left = malloc(ndim * sizeof(*plan->used_left));
    plan->used_right = malloc(ndim * sizeof(*plan->used_right));
    if ((ndim > 0) && (!plan->used_left || !plan->used_right)) {
        free_symbolic_binary_product_plan(plan);
        return false;
    }
    if (!dv_collect_var_usage(left, ndim, vars, plan->used_left) ||
        !dv_collect_var_usage(right, ndim, vars, plan->used_right)) {
        free_symbolic_binary_product_plan(plan);
        return false;
    }
    for (size_t i = 0; i < ndim; ++i) {
        if (active && !active[i]) {
            plan->used_left[i] = false;
            plan->used_right[i] = false;
        }
        if (plan->used_left[i] && plan->used_right[i]) {
            disjoint = false;
            break;
        }
    }
    if (!disjoint) {
        free_symbolic_binary_product_plan(plan);
        return false;
    }

    return true;
}

static bool eval_binary_product_plan(const symbolic_binary_product_plan_t *plan,
                                     size_t ndim,
                                     dval_t *const *vars,
                                     const qfloat_t *lo,
                                     const qfloat_t *hi,
                                     int depth,
                                     qfloat_t *out)
{
    qfloat_t left_v;
    qfloat_t right_v;

    if (!plan || !plan->left || !plan->right || !plan->used_left || !plan->used_right || !out)
        return false;

    if (!try_eval_special_combination(plan->left, ndim, vars, lo, hi, plan->used_left,
                                      depth + 1, &left_v) ||
        !try_eval_special_combination(plan->right, ndim, vars, lo, hi, plan->used_right,
                                      depth + 1, &right_v))
        return false;

    *out = qf_mul(left_v, right_v);
    return true;
}

static bool build_separable_product_plan(const dval_t *expr,
                                         size_t ndim,
                                         dval_t *const *vars,
                                         const bool *active,
                                         symbolic_separable_product_plan_t *plan)
{
    const dval_t **factors = NULL;
    bool **factor_used = NULL;
    size_t *component_ids = NULL;
    size_t factor_count = 0;
    size_t factor_cap = 0;
    size_t component_count = 0;
    bool matched = false;

    if (!expr || !plan)
        return false;

    plan->ngroups = 0;
    plan->group_exprs = NULL;
    plan->group_owned = NULL;
    plan->group_used = NULL;

    if (!append_product_factor(expr, &factors, &factor_count, &factor_cap) || factor_count < 2)
        goto cleanup;

    factor_used = calloc(factor_count, sizeof(*factor_used));
    component_ids = malloc(factor_count * sizeof(*component_ids));
    if (!factor_used || !component_ids)
        goto cleanup;

    for (size_t i = 0; i < factor_count; ++i) {
        factor_used[i] = malloc(ndim * sizeof(*factor_used[i]));
        if ((ndim > 0) && !factor_used[i])
            goto cleanup;
        if (!dv_collect_var_usage(factors[i], ndim, vars, factor_used[i]))
            goto cleanup;
        for (size_t j = 0; j < ndim; ++j) {
            if (active && !active[j])
                factor_used[i][j] = false;
        }
        component_ids[i] = SIZE_MAX;
    }

    for (size_t i = 0; i < factor_count; ++i) {
        if (component_ids[i] != SIZE_MAX)
            continue;
        component_ids[i] = component_count;
        for (size_t changed = 1; changed != 0;) {
            changed = 0;
            for (size_t j = 0; j < factor_count; ++j) {
                if (component_ids[j] == component_count)
                    continue;
                for (size_t k = 0; k < factor_count; ++k) {
                    if (component_ids[k] != component_count)
                        continue;
                    if (usage_masks_overlap(ndim, factor_used[j], factor_used[k])) {
                        component_ids[j] = component_count;
                        changed = 1;
                        break;
                    }
                }
            }
        }
        ++component_count;
    }

    if (component_count < 2)
        goto cleanup;

    plan->group_exprs = calloc(component_count, sizeof(*plan->group_exprs));
    plan->group_owned = calloc(component_count, sizeof(*plan->group_owned));
    plan->group_used = calloc(component_count, sizeof(*plan->group_used));
    if (!plan->group_exprs || !plan->group_owned || !plan->group_used)
        goto cleanup;

    for (size_t c = 0; c < component_count; ++c) {
        dval_t *group_expr = NULL;
        bool owned = false;

        plan->group_used[c] = calloc(ndim, sizeof(*plan->group_used[c]));
        if ((ndim > 0) && !plan->group_used[c])
            goto cleanup;

        for (size_t i = 0; i < factor_count; ++i) {
            dval_t *next_expr;

            if (component_ids[i] != c)
                continue;

            for (size_t j = 0; j < ndim; ++j)
                plan->group_used[c][j] = plan->group_used[c][j] || factor_used[i][j];

            if (!group_expr) {
                group_expr = (dval_t *)factors[i];
                continue;
            }

            next_expr = dv_mul(group_expr, (dval_t *)factors[i]);
            if (!next_expr) {
                if (owned)
                    dv_free(group_expr);
                goto cleanup;
            }
            if (owned)
                dv_free(group_expr);
            group_expr = next_expr;
            owned = true;
        }

        if (!group_expr)
            goto cleanup;

        plan->group_exprs[c] = group_expr;
        plan->group_owned[c] = owned;
    }

    plan->ngroups = component_count;
    matched = true;

cleanup:
    if (!matched)
        free_symbolic_separable_product_plan(plan);
    if (factor_used) {
        for (size_t i = 0; i < factor_count; ++i)
            free(factor_used[i]);
    }
    free(factor_used);
    free(component_ids);
    free((void *)factors);
    return matched;
}

static bool eval_separable_product_plan(const symbolic_separable_product_plan_t *plan,
                                        size_t ndim,
                                        dval_t *const *vars,
                                        const qfloat_t *lo,
                                        const qfloat_t *hi,
                                        int depth,
                                        qfloat_t *out)
{
    qfloat_t total = QF_ONE;
    qfloat_t term;

    if (!plan || !plan->group_exprs || !plan->group_used || plan->ngroups < 2 || !out)
        return false;

    for (size_t i = 0; i < plan->ngroups; ++i) {
        if (!try_eval_special_combination(plan->group_exprs[i], ndim, vars, lo, hi,
                                          plan->group_used[i], depth + 1, &term))
            return false;
        total = qf_mul(total, term);
    }

    *out = total;
    return true;
}

static bool try_eval_special_combination(const dval_t *expr,
                                         size_t ndim,
                                         dval_t *const *vars,
                                         const qfloat_t *lo,
                                         const qfloat_t *hi,
                                         const bool *active,
                                         int depth,
                                         qfloat_t *out)
{
    symbolic_plan_t plan;
    qfloat_t *coeffs;
    qfloat_t left_v;
    qfloat_t right_v;

    if (!expr || !out || depth > 32)
        return false;

    coeffs = malloc(ndim * sizeof(*coeffs));
    if ((ndim > 0) && !coeffs)
        return false;

    if (!build_symbolic_plan(expr, ndim, vars, &plan, coeffs)) {
        free(coeffs);
        return false;
    }

    switch (plan.kind) {
    case SYMBOLIC_PLAN_CONST:
        *out = qf_mul(plan.scalar, box_volume(ndim, lo, hi, active));
        free(coeffs);
        return true;
    case SYMBOLIC_PLAN_AFFINE_POLY_SPECIAL:
    case SYMBOLIC_PLAN_AFFINE_POLY:
    case SYMBOLIC_PLAN_AFFINE_UNARY_SPECIAL: {
        bool ok = eval_direct_affine_plan(&plan, ndim, coeffs, lo, hi, active, out);
        free(coeffs);
        return ok;
    }
    case SYMBOLIC_PLAN_SCALED: {
        bool ok = try_eval_special_combination(plan.base, ndim, vars, lo, hi, active,
                                               depth + 1, &left_v);
        free(coeffs);
        if (!ok)
            return false;
        *out = qf_mul(plan.scalar, left_v);
        return true;
    }
    case SYMBOLIC_PLAN_ADD_SUB: {
        bool ok_left = try_eval_special_combination(plan.left, ndim, vars, lo, hi, active,
                                                    depth + 1, &left_v);
        bool ok_right = ok_left &&
                        try_eval_special_combination(plan.right, ndim, vars, lo, hi, active,
                                                     depth + 1, &right_v);
        free(coeffs);
        if (!ok_right)
            return false;
        *out = plan.is_sub ? qf_sub(left_v, right_v) : qf_add(left_v, right_v);
        return true;
    }
    case SYMBOLIC_PLAN_MUL: {
        symbolic_separable_product_plan_t separable_plan = {0};
        symbolic_binary_product_plan_t binary_plan = {0};
        bool ok = false;

        if (build_separable_product_plan(expr, ndim, vars, active, &separable_plan))
            ok = eval_separable_product_plan(&separable_plan, ndim, vars, lo, hi, depth, out);
        else if (build_binary_product_plan(plan.left, plan.right, ndim, vars, active,
                                           &binary_plan))
            ok = eval_binary_product_plan(&binary_plan, ndim, vars, lo, hi, depth, out);

        free_symbolic_separable_product_plan(&separable_plan);
        free_symbolic_binary_product_plan(&binary_plan);
        free(coeffs);
        return ok;
    }
    case SYMBOLIC_PLAN_NONE:
    default:
        free(coeffs);
        return false;
    }
}

int try_integral_multi_special_affine(integrator_t *ig, dval_t *expr,
                                      size_t ndim, dval_t *const *vars,
                                      const qfloat_t *lo, const qfloat_t *hi,
                                      qfloat_t *result, qfloat_t *error_est)
{
    qfloat_t total;

    if (!try_eval_special_combination(expr, ndim, vars, lo, hi, NULL, 0, &total))
        return 0;
    if (qf_isnan(total))
        return -1;
    ig->last_intervals = 1;
    *result = total;
    if (error_est)
        *error_est = QF_ZERO;
    return 1;
}
