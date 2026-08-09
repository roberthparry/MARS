#include <mpfr.h>

#include "qfloat.h"
#include "internal/lommel_mpfr.h"

enum {
    QF_LOMMEL_MPFR_PRECISION = 128
};

static void qf_lommel_set_mpfr(mpfr_ptr out, qfloat_t value)
{
    mpfr_set_d(out, value.hi, MPFR_RNDN);
    mpfr_add_d(out, out, value.lo, MPFR_RNDN);
}

static qfloat_t qf_lommel_from_mpfr(mpfr_srcptr value)
{
    mpfr_t remainder;
    double hi;
    double lo;

    if (mpfr_nan_p(value))
        return QF_NAN;
    if (mpfr_inf_p(value))
        return mpfr_sgn(value) < 0 ? QF_NINF : QF_INF;

    hi = mpfr_get_d(value, MPFR_RNDN);
    mpfr_init2(remainder, QF_LOMMEL_MPFR_PRECISION);
    mpfr_set(remainder, value, MPFR_RNDN);
    mpfr_sub_d(remainder, remainder, hi, MPFR_RNDN);
    lo = mpfr_get_d(remainder, MPFR_RNDN);
    mpfr_clear(remainder);
    return qf_inline_renorm(hi, lo);
}

static qfloat_t qf_lommel_apply(
    qfloat_t mu, qfloat_t nu, qfloat_t argument,
    int (*operation)(mpfr_ptr, mpfr_srcptr, mpfr_srcptr, mpfr_srcptr,
                     mpfr_rnd_t))
{
    mpfr_t mp_mu;
    mpfr_t mp_nu;
    mpfr_t mp_argument;
    mpfr_t result;
    qfloat_t out;

    mpfr_inits2(QF_LOMMEL_MPFR_PRECISION,
                mp_mu, mp_nu, mp_argument, result, (mpfr_ptr)0);
    qf_lommel_set_mpfr(mp_mu, mu);
    qf_lommel_set_mpfr(mp_nu, nu);
    qf_lommel_set_mpfr(mp_argument, argument);
    if (!operation || operation(result, mp_mu, mp_nu,
                                mp_argument, MPFR_RNDN) != 0)
        mpfr_set_nan(result);
    out = qf_lommel_from_mpfr(result);
    mpfr_clears(mp_mu, mp_nu, mp_argument, result, (mpfr_ptr)0);
    return out;
}

qfloat_t qf_lommel_s(qfloat_t mu, qfloat_t nu, qfloat_t argument)
{
    return qf_lommel_apply(mu, nu, argument, mars_mpfr_lommel_s);
}

qfloat_t qf_lommel_s_derivative(qfloat_t mu, qfloat_t nu,
                                qfloat_t argument)
{
    return qf_lommel_apply(mu, nu, argument,
                           mars_mpfr_lommel_s_derivative);
}
