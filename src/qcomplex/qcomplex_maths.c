#include <math.h>

#include "qcomplex_internal.h"

static const qfloat_t qc_faddeeva_ak[32] = {
    { .hi = 0.27768018363489788, .lo = 9.0767120701630337e-18 },
    { .hi = 0.83304055090469364, .lo = 2.7230136210489064e-17 },
    { .hi = 1.3884009181744894, .lo = 4.5383560350815163e-17 },
    { .hi = 1.9437612854442852, .lo = 6.3536984491141175e-17 },
    { .hi = 2.4991216527140812, .lo = -1.4035419629356404e-16 },
    { .hi = 3.0544820199838769, .lo = -1.2220077215323797e-16 },
    { .hi = 3.6098423872536727, .lo = -1.0404734801291188e-16 },
    { .hi = 4.1652027545234684, .lo = -8.589392387258579e-17 },
    { .hi = 4.7205631217932638, .lo = 3.7634871011780275e-16 },
    { .hi = 5.27592348906306, .lo = -4.9587075591933679e-17 },
    { .hi = 5.8312838563328553, .lo = 4.1265555839845498e-16 },
    { .hi = 6.3866442236026515, .lo = -1.3280227311281643e-17 },
    { .hi = 6.9420045908724477, .lo = -4.3921601302101817e-16 },
    { .hi = 7.497364958142243, .lo = 2.3026620969370541e-17 },
    { .hi = 8.0527253254120392, .lo = -4.0290916474036598e-16 },
    { .hi = 8.6080856926818345, .lo = 5.9333469250022725e-17 },
    { .hi = 9.1634460599516299, .lo = 5.2157610324041134e-16 },
    { .hi = 9.718806427221427, .lo = -7.9253810216945052e-16 },
    { .hi = 10.274166794491222, .lo = -3.3029546817906181e-16 },
    { .hi = 10.829527161761018, .lo = 1.3194716581132695e-16 },
    { .hi = 11.384887529030813, .lo = 5.941897998017156e-16 },
    { .hi = 11.94024789630061, .lo = -7.1992440560814635e-16 },
    { .hi = 12.495608263570405, .lo = -2.5768177161775784e-16 },
    { .hi = 13.050968630840201, .lo = 2.0456086237263117e-16 },
    { .hi = 13.606328998109996, .lo = 6.6680349636301987e-16 },
    { .hi = 14.161689365379793, .lo = -6.4731070904684198e-16 },
    { .hi = 14.717049732649588, .lo = -1.8506807505645327e-16 },
    { .hi = 15.272410099919384, .lo = 2.7717455893393539e-16 },
    { .hi = 15.827770467189179, .lo = 7.3941719292432414e-16 },
    { .hi = 16.383130834458974, .lo = 1.2016598269147127e-15 },
    { .hi = 16.93849120172877, .lo = 1.6639024609051016e-15 },
    { .hi = 17.493851568998569, .lo = -1.4265685839050107e-15 },
};

static const qfloat_t qc_faddeeva_ck[32] = {
    { .hi = 0.35334045532188407, .lo = -5.1060356717009355e-18 },
    { .hi = 0.35164004979327168, .lo = -1.7896523109490442e-17 },
    { .hi = 0.3482556145708236, .lo = 3.9344395413187435e-18 },
    { .hi = 0.34321974361585256, .lo = 2.1438505844416959e-18 },
    { .hi = 0.33658093511854198, .lo = 5.1930164514014062e-18 },
    { .hi = 0.32840312443386288, .lo = -1.4351829991431114e-17 },
    { .hi = 0.31876506834915475, .lo = -2.3680555894473439e-17 },
    { .hi = 0.30775958661321068, .lo = 3.6417051543110264e-18 },
    { .hi = 0.29549266803135615, .lo = 2.4628289459045659e-17 },
    { .hi = 0.28208244973531427, .lo = 1.7021070742208289e-17 },
    { .hi = 0.26765807945804687, .lo = 2.2530497410534951e-17 },
    { .hi = 0.25235847177048698, .lo = -8.5797690063293585e-18 },
    { .hi = 0.23633097025828204, .lo = -2.0894686963217415e-19 },
    { .hi = 0.21972992852251819, .lo = -1.2392959193406266e-17 },
    { .hi = 0.20271522367016331, .lo = 1.3218712990283339e-17 },
    { .hi = 0.18545071661012946, .lo = 5.0162237656232391e-18 },
    { .hi = 0.1681026739831443, .lo = -1.4288814336266023e-18 },
    { .hi = 0.15083816692311044, .lo = -9.6313706582867086e-18 },
    { .hi = 0.1338234620707556, .lo = -1.1775274090226018e-17 },
    { .hi = 0.11722242033499172, .lo = 3.7962892016288056e-18 },
    { .hi = 0.10119491882278679, .lo = -1.7106764694884606e-18 },
    { .hi = 0.085895311135226873, .lo = -5.0653672707238615e-18 },
    { .hi = 0.071470940857959478, .lo = 4.4405939760279703e-19 },
    { .hi = 0.058060722561917584, .lo = -2.2426541532734527e-19 },
    { .hi = 0.04579380398006308, .lo = -5.4362822314392653e-20 },
    { .hi = 0.034788322244119041, .lo = -4.8767738915884548e-19 },
    { .hi = 0.025150266159410895, .lo = 5.919375636596669e-19 },
    { .hi = 0.016972455474731779, .lo = -1.6056741194047767e-18 },
    { .hi = 0.010333646977421203, .lo = -2.9123172842187257e-19 },
    { .hi = 0.0052977760224501569, .lo = -3.4709720932211063e-19 },
    { .hi = 0.0019133408000020994, .lo = 1.6662426274081609e-20 },
    { .hi = 0.00021293527138969463, .lo = -7.344430498602854e-21 },
};

static const qfloat_t qc_sqrt_2pi = {
    .hi = 2.5066282746310007,
    .lo = -1.8328579980459169e-16
};

static const qfloat_t qc_pi_cubed = {
    .hi = 31.00627668029982,
    .lo = 4.1641946985288283e-16
};

static qfloat_t qc_abs2_local(qcomplex_t z)
{
    return qf_add(qf_mul(z.re, z.re), qf_mul(z.im, z.im));
}

static qcomplex_t qc_faddeeva_inside(qcomplex_t z)
{
    const int N = 32;
    qcomplex_t sum = qcr(0.0);

    for (int k = 1; k <= N; k++) {
        qcomplex_t denom = qc_make(z.re, qf_sub(z.im, qc_faddeeva_ak[k - 1]));
        sum = qc_add(sum, qc_div(qcrf(qc_faddeeva_ck[k - 1]), denom));
    }

    // inside = 1 + (2i / sqrt(pi)) * sum
    qcomplex_t two_i_sum = qc_mul(qc_make(qf_from_double(0.0), qf_from_double(2.0)), sum);
    return qc_add(qcr(1.0), qc_div(two_i_sum, qcrf(QF_SQRT_PI)));
}

qcomplex_t qc_erf(qcomplex_t z) {
    if (qf_eq(z.im, qf_from_double(0.0)))
        return qcrf(qf_erf(z.re));
    /* Faddeeva requires Im(iz) = Re(z) >= 0; use antisymmetry erf(-z) = -erf(z) otherwise */
    if (qf_lt(z.re, qf_from_double(0.0)))
        return qc_neg(qc_erf(qc_neg(z)));
    qcomplex_t iz = qc_make(qf_neg(z.im), z.re);
    return qc_sub(qcr(1.0), qc_faddeeva_inside(iz));
}

qcomplex_t qc_erfc(qcomplex_t z)
{
    if (qf_eq(z.im, qf_from_double(0.0)))
        return qcrf(qf_erfc(z.re));
    /* Use erfc(-z) = 2 - erfc(z) to keep Re(z) >= 0 for Faddeeva */
    if (qf_lt(z.re, qf_from_double(0.0)))
        return qc_sub(qcr(2.0), qc_erfc(qc_neg(z)));
    qcomplex_t iz = qc_make(qf_neg(z.im), z.re);
    return qc_faddeeva_inside(iz);
}

qcomplex_t qc_erfinv(qcomplex_t z)
{
    // Newton iteration: solve erf(w) = z.  Initial guess good for small |z|.
    qcomplex_t w = qc_mul(z, qcrf(qf_div(QF_SQRT_PI, qf_from_double(2.0))));

    for (int i = 0; i < 10; i++) {
        qcomplex_t f  = qc_sub(qc_erf(w), z);
        qcomplex_t fp = qc_mul(qcrf(qf_div(qf_from_double(2.0), QF_SQRT_PI)),
                               qc_exp(qc_neg(qc_mul(w, w))));
        qcomplex_t delta = qc_div(f, fp);
        w = qc_sub(w, delta);
        if (qf_lt(qc_abs2_local(delta), qf_from_double(1e-60)))
            break;
    }
    return w;
}

qcomplex_t qc_erfcinv(qcomplex_t z)
{
    // Newton iteration: solve erfc(w) = z.  Initial guess via erfinv(1-z).
    qcomplex_t w = qc_erfinv(qc_sub(qcr(1.0), z));

    for (int i = 0; i < 10; i++) {
        qcomplex_t f  = qc_sub(qc_erfc(w), z);
        qcomplex_t fp = qc_mul(qcrf(qf_div(qf_neg(qf_from_double(2.0)), QF_SQRT_PI)),
                               qc_exp(qc_neg(qc_mul(w, w))));
        qcomplex_t delta = qc_div(f, fp);
        w = qc_sub(w, delta);
        if (qf_lt(qc_abs(delta), qf_from_double(1e-30)))
            break;
    }
    return w;
}

// Lanczos coefficients (g=7, N=9)
static qfloat_t c[] = {
    { 1.00000000000000000e+00, -6.99999999999999971e-34 },
    { 6.76520368121885099e+02, -2.77349972750868088e-16 },
    { -1.25913921672240281e+03, -6.33356109449210117e-14 },
    { 7.71323428777653135e+02, -5.57539348441983815e-14 },
    { -1.76615029162140587e+02, -1.20612308688560953e-14 },
    { 1.25073432786869052e+01, -3.93560260890580655e-16 },
    { -1.38571095265720118e-01, 9.21762572455474946e-19 },
    { 9.98436957801957158e-06, -7.23679346146658305e-22 },
    { 1.50563273514931162e-07, -5.92527880744794851e-24 },
};

static qcomplex_t lanczos_sum(qcomplex_t z_minus_one)
{
    qcomplex_t sum = qcrf(c[0]);
    for (int i = 1; i < 9; i++)
        sum = qc_add(sum, qc_div(qcrf(c[i]),
                                 qc_add(z_minus_one, qcr((double)i))));
    return sum;
}

// ------------------------------------------------------------

// Complex Gamma
// ------------------------------------------------------------

qcomplex_t qc_gamma(qcomplex_t z)
{
    if (qf_eq(z.im, qf_from_double(0.0)))
        return qcrf(qf_gamma(z.re));

    if (qf_lt(z.re, qf_from_double(0.5))) {
        // Reflection: Γ(z) = π / (sin(πz) Γ(1-z))
        qcomplex_t sin_pi_z  = qc_sin(qc_mul(z, qcrf(QF_PI)));
        qcomplex_t gamma_1mz = qc_gamma(qc_sub(qcr(1.0), z));
        return qc_div(qcrf(QF_PI), qc_mul(sin_pi_z, gamma_1mz));
    }

    qcomplex_t z_minus_one = qc_sub(z, qcr(1.0));
    qcomplex_t sum         = lanczos_sum(z_minus_one);
    qcomplex_t t           = qc_add(z_minus_one, qcr(7.5));  // g + 0.5, g = 7

    return qc_mul(qc_mul(qcrf(qc_sqrt_2pi), qc_pow(t, qc_sub(z, qcr(0.5)))),
                  qc_mul(qc_exp(qc_neg(t)), sum));
}

qcomplex_t qc_lgamma(qcomplex_t z)
{
    if (qf_eq(z.im, qf_from_double(0.0)))
        return qcrf(qf_lgamma(z.re));

    if (qf_lt(z.re, qf_from_double(0.5))) {
        // Reflection: lgamma(z) = log(π) - log(sin(πz)) - lgamma(1-z)
        qcomplex_t log_sin_piz = qc_log(qc_sin(qc_mul(z, qcrf(QF_PI))));
        qcomplex_t lg_1mz      = qc_lgamma(qc_sub(qcr(1.0), z));
        return qc_sub(qc_sub(qcrf(qf_log(QF_PI)), log_sin_piz), lg_1mz);
    }

    qcomplex_t z_minus_one = qc_sub(z, qcr(1.0));
    qcomplex_t sum         = lanczos_sum(z_minus_one);
    qcomplex_t t           = qc_add(z_minus_one, qcr(7.5));
    return qc_add(
        qc_add(qcrf(QF_LOG_SQRT_2PI), qc_mul(qc_sub(z, qcr(0.5)), qc_log(t))),
        qc_add(qc_neg(t), qc_log(sum)));
}

qcomplex_t qc_digamma(qcomplex_t z)
{
    if (qf_eq(z.im, qf_from_double(0.0)))
        return qcrf(qf_digamma(z.re));

    if (qf_lt(z.re, qf_from_double(0.5))) {
        // Reflection: ψ(z) = ψ(1-z) - π cot(πz)
        qcomplex_t pi_z = qc_mul(z, qcrf(QF_PI));
        qcomplex_t cotpz = qc_div(qc_cos(pi_z), qc_sin(pi_z));
        return qc_sub(qc_digamma(qc_sub(qcr(1.0), z)), qc_mul(qcrf(QF_PI), cotpz));
    }

    // Shift upward until |z| >= 10
    qcomplex_t psi = qcr(0.0);
    qcomplex_t zz  = z;
    while (qf_lt(qc_abs(zz), qf_from_double(10.0))) {
        psi = qc_sub(psi, qc_div(qcr(1.0), zz));
        zz  = qc_add(zz, qcr(1.0));
    }

    // Asymptotic: ψ(z) ≈ log(z) - 1/(2z) - Σ B_{2k}/(2k z^{2k})
    qcomplex_t invz   = qc_div(qcr(1.0), zz);
    qcomplex_t result = qc_sub(qc_log(zz), qc_mul(qcr(0.5), invz));

    qfloat_t B2  = qf_div(qf_from_double( 1.0), qf_from_double( 6.0));
    qfloat_t B4  = qf_div(qf_from_double(-1.0), qf_from_double(30.0));
    qfloat_t B6  = qf_div(qf_from_double( 1.0), qf_from_double(42.0));
    qfloat_t B8  = qf_div(qf_from_double(-1.0), qf_from_double(30.0));
    qfloat_t B10 = qf_div(qf_from_double( 5.0), qf_from_double(66.0));

    qcomplex_t z2  = qc_mul(invz, invz);
    qcomplex_t z4  = qc_mul(z2, z2);
    qcomplex_t z6  = qc_mul(z4, z2);
    qcomplex_t z8  = qc_mul(z6, z2);
    qcomplex_t z10 = qc_mul(z8, z2);

    result = qc_sub(result, qc_mul(qcrf(B2),  qc_mul(qcr(0.5),      z2)));
    result = qc_sub(result, qc_mul(qcrf(B4),  qc_mul(qcr(0.25),     z4)));
    result = qc_sub(result, qc_mul(qcrf(B6),  qc_mul(qcr(1.0/6.0),  z6)));
    result = qc_sub(result, qc_mul(qcrf(B8),  qc_mul(qcr(0.125),    z8)));
    result = qc_sub(result, qc_mul(qcrf(B10), qc_mul(qcr(0.1),      z10)));

    return qc_add(result, psi);
}

qcomplex_t qc_trigamma(qcomplex_t z)
{
    if (qf_eq(z.im, qf_from_double(0.0)))
        return qcrf(qf_trigamma(z.re));

    if (qf_lt(z.re, qf_from_double(0.5))) {
        // Reflection: ψ₁(z) = π² csc²(πz) - ψ₁(1-z)
        qcomplex_t pi_z  = qc_mul(z, qcrf(QF_PI));
        qcomplex_t csc   = qc_div(qcr(1.0), qc_sin(pi_z));
        qcomplex_t term  = qc_mul(qcrf(qf_mul(QF_PI, QF_PI)), qc_mul(csc, csc));
        return qc_sub(term, qc_trigamma(qc_sub(qcr(1.0), z)));
    }

    // Recurrence upward until |z| >= 10
    qcomplex_t accum = qcr(0.0);
    qcomplex_t zz    = z;
    while (qf_lt(qc_abs(zz), qf_from_double(10.0))) {
        qcomplex_t invz = qc_div(qcr(1.0), zz);
        accum = qc_add(accum, qc_mul(invz, invz));
        zz    = qc_add(zz, qcr(1.0));
    }

    // Asymptotic: ψ₁(z) ≈ 1/z + 1/(2z²) + Σ B_{2k}/z^{2k+1}
    qcomplex_t invz  = qc_div(qcr(1.0), zz);
    qcomplex_t invz2 = qc_mul(invz, invz);
    qcomplex_t result = qc_add(invz, qc_mul(qcr(0.5), invz2));

    qfloat_t B2 = qf_div(qf_from_double( 1.0), qf_from_double( 6.0));
    qfloat_t B4 = qf_div(qf_from_double(-1.0), qf_from_double(30.0));
    qfloat_t B6 = qf_div(qf_from_double( 1.0), qf_from_double(42.0));
    qfloat_t B8 = qf_div(qf_from_double(-1.0), qf_from_double(30.0));

    qcomplex_t z3 = qc_mul(invz2, invz);
    qcomplex_t z5 = qc_mul(z3, invz2);
    qcomplex_t z7 = qc_mul(z5, invz2);
    qcomplex_t z9 = qc_mul(z7, invz2);

    result = qc_add(result, qc_mul(qcrf(B2), z3));
    result = qc_add(result, qc_mul(qcrf(B4), z5));
    result = qc_add(result, qc_mul(qcrf(B6), z7));
    result = qc_add(result, qc_mul(qcrf(B8), z9));

    return qc_add(result, accum);
}

qcomplex_t qc_tetragamma(qcomplex_t z)
{
    if (qf_eq(z.im, qf_from_double(0.0)))
        return qcrf(qf_tetragamma(z.re));

    if (qf_lt(z.re, qf_from_double(0.5))) {
        // Reflection: ψ₂(z) = ψ₂(1-z) + 2π³ csc²(πz) cot(πz)
        qcomplex_t pi_z   = qc_mul(z, qcrf(QF_PI));
        qcomplex_t sin_pz = qc_sin(pi_z);
        qcomplex_t csc    = qc_div(qcr(1.0), sin_pz);
        qcomplex_t csc2   = qc_mul(csc, csc);
        qcomplex_t cot_pz = qc_div(qc_cos(pi_z), sin_pz);
        qcomplex_t term = qc_mul(qcrf(qf_mul_double(qc_pi_cubed, 2.0)),
                                 qc_mul(csc2, cot_pz));
        return qc_add(qc_tetragamma(qc_sub(qcr(1.0), z)), term);
    }

    // Recurrence upward until |z| >= 10
    qcomplex_t accum = qcr(0.0);
    qcomplex_t zz    = z;
    while (qf_lt(qc_abs(zz), qf_from_double(10.0))) {
        qcomplex_t invz  = qc_div(qcr(1.0), zz);
        qcomplex_t invz3 = qc_mul(invz, qc_mul(invz, invz));
        accum = qc_add(accum, qc_mul(qcr(2.0), invz3));
        zz    = qc_add(zz, qcr(1.0));
    }

    // Asymptotic: ψ₂(z) ≈ 1/z² + 1/z³ + Σ B_{2k}/z^{2k+2}
    qcomplex_t invz  = qc_div(qcr(1.0), zz);
    qcomplex_t invz2 = qc_mul(invz, invz);
    qcomplex_t result = qc_add(invz2, qc_mul(invz2, invz));

    qfloat_t B2 = qf_div(qf_from_double( 1.0), qf_from_double( 6.0));
    qfloat_t B4 = qf_div(qf_from_double(-1.0), qf_from_double(30.0));
    qfloat_t B6 = qf_div(qf_from_double( 1.0), qf_from_double(42.0));
    qfloat_t B8 = qf_div(qf_from_double(-1.0), qf_from_double(30.0));

    qcomplex_t z4  = qc_mul(invz2, invz2);
    qcomplex_t z6  = qc_mul(z4, invz2);
    qcomplex_t z8  = qc_mul(z6, invz2);
    qcomplex_t z10 = qc_mul(z8, invz2);

    result = qc_add(result, qc_mul(qcrf(B2), z4));
    result = qc_add(result, qc_mul(qcrf(B4), z6));
    result = qc_add(result, qc_mul(qcrf(B6), z8));
    result = qc_add(result, qc_mul(qcrf(B8), z10));

    return qc_add(result, accum);
}

qcomplex_t qc_gammainv(qcomplex_t z)
{
    if (qf_eq(z.im, qf_from_double(0.0)))
        return qcrf(qf_gammainv(z.re));

    if (qc_isnan(z) || (qf_eq(z.re, QF_ZERO) && qf_eq(z.im, QF_ZERO)))
        return qc_make(QF_NAN, QF_NAN);

    qcomplex_t logz = qc_log(z);
    qcomplex_t w;
    w = qc_add(qcr(1.5), logz);

    for (int i = 0; i < 20; i++) {
        qcomplex_t delta = qc_div(qc_sub(qc_lgamma(w), logz), qc_digamma(w));
        w = qc_sub(w, delta);
        if (qf_lt(qc_abs(delta), qf_from_double(1e-30)))
            break;
    }
    return w;
}

qcomplex_t qc_beta(qcomplex_t a, qcomplex_t b)
{
    if (qf_eq(a.im, qf_from_double(0.0)) && qf_eq(b.im, qf_from_double(0.0)))
        return qcrf(qf_beta(a.re, b.re));

    return qc_exp(qc_logbeta(a, b));
}

qcomplex_t qc_logbeta(qcomplex_t a, qcomplex_t b)
{
    if (qf_eq(a.im, qf_from_double(0.0)) && qf_eq(b.im, qf_from_double(0.0)))
        return qcrf(qf_logbeta(a.re, b.re));

    return qc_sub(qc_add(qc_lgamma(a), qc_lgamma(b)), qc_lgamma(qc_add(a, b)));
}

qcomplex_t qc_binomial(qcomplex_t a, qcomplex_t b)
{
    if (qf_eq(a.im, qf_from_double(0.0)) && qf_eq(b.im, qf_from_double(0.0)))
        return qcrf(qf_binomial(a.re, b.re));

    // C(a,b) = Γ(a+1) / (Γ(b+1) Γ(a-b+1))
    qcomplex_t a1   = qc_add(a, qcr(1.0));
    qcomplex_t b1   = qc_add(b, qcr(1.0));
    qcomplex_t amb1 = qc_add(qc_sub(a, b), qcr(1.0));
    return qc_div(qc_gamma(a1), qc_mul(qc_gamma(b1), qc_gamma(amb1)));
}

qcomplex_t qc_beta_pdf(qcomplex_t x, qcomplex_t a, qcomplex_t b)
{
    if (qf_eq(x.im, qf_from_double(0.0)) && qf_eq(a.im, qf_from_double(0.0)) &&
        qf_eq(b.im, qf_from_double(0.0)))
        return qcrf(qf_beta_pdf(x.re, a.re, b.re));

    // f(x; a, b) = x^(a-1) * (1-x)^(b-1) / B(a,b)
    qcomplex_t one_minus_x = qc_sub(qcr(1.0), x);
    qcomplex_t num = qc_mul(qc_pow(x,           qc_sub(a, qcr(1.0))),
                            qc_pow(one_minus_x, qc_sub(b, qcr(1.0))));
    return qc_div(num, qc_beta(a, b));
}

qcomplex_t qc_logbeta_pdf(qcomplex_t x, qcomplex_t a, qcomplex_t b)
{
    if (qf_eq(x.im, qf_from_double(0.0)) && qf_eq(a.im, qf_from_double(0.0)) &&
        qf_eq(b.im, qf_from_double(0.0)))
        return qcrf(qf_logbeta_pdf(x.re, a.re, b.re));

    // log f(x; a,b) = (a-1)log(x) + (b-1)log(1-x) - log B(a,b)
    return qc_sub(qc_add(qc_mul(qc_sub(a, qcr(1.0)), qc_log(x)),
                         qc_mul(qc_sub(b, qcr(1.0)), qc_log(qc_sub(qcr(1.0), x)))),
                  qc_logbeta(a, b));
}

qcomplex_t qc_normal_pdf(qcomplex_t z)
{
    if (qf_eq(z.im, qf_from_double(0.0)))
        return qcrf(qf_normal_pdf(z.re));

    // φ(z) = exp(-z²/2) / sqrt(2π)
    return qc_mul(qcrf(QF_INV_SQRT_2PI),
                  qc_exp(qc_mul(qcr(-0.5), qc_mul(z, z))));
}

qcomplex_t qc_normal_cdf(qcomplex_t z)
{
    if (qf_eq(z.im, qf_from_double(0.0)))
        return qcrf(qf_normal_cdf(z.re));

    // Φ(z) = 0.5 * (1 + erf(z / sqrt(2)))
    qfloat_t sqrt2 = qf_sqrt(qf_from_double(2.0));
    return qc_mul(qcr(0.5), qc_add(qcr(1.0), qc_erf(qc_div(z, qcrf(sqrt2)))));
}

qcomplex_t qc_normal_logpdf(qcomplex_t z)
{
    if (qf_eq(z.im, qf_from_double(0.0)))
        return qcrf(qf_normal_logpdf(z.re));

    // log φ(z) = -z²/2 - log(2π)/2
    return qc_sub(qc_mul(qcr(-0.5), qc_mul(z, z)),
                  qcrf(QF_LOG_SQRT_2PI));
}

static qcomplex_t qc_lambert_w_series_guess(qcomplex_t z, int branch)
{
    qcomplex_t ez = qc_mul(qcrf(QF_E), z);
    qcomplex_t p = qc_sqrt(qc_mul(qcr(2.0), qc_add(qcr(1.0), ez)));
    qcomplex_t p2 = qc_mul(p, p);
    qcomplex_t p3 = qc_mul(p2, p);
    qcomplex_t sign_p = (branch == -1) ? qc_neg(p) : p;
    qcomplex_t sign_p3 = (branch == -1) ? qc_neg(p3) : p3;
    qcomplex_t w = qcr(-1.0);

    w = qc_add(w, sign_p);
    w = qc_sub(w, qc_mul(qcr(1.0 / 3.0), p2));
    w = qc_add(w, qc_mul(qcr(11.0 / 72.0), sign_p3));
    return w;
}

static qcomplex_t qc_lambert_w_asymptotic_guess(qcomplex_t z, int branch)
{
    qfloat_t two_pi = qf_mul_double(QF_PI, 2.0);
    qcomplex_t L1 = qc_log(z);
    L1.im = qf_add(L1.im, qf_mul_double(two_pi, (double)branch));

    if (qc_abs(L1).hi == 0.0)
        return L1;

    qcomplex_t L2 = qc_log(L1);
    return qc_add(qc_sub(L1, L2), qc_div(L2, L1));
}

static qcomplex_t qc_lambert_wm1_complex(qcomplex_t z)
{
    qfloat_t zero_tol = qf_from_double(1e-30);

    if (qc_isnan(z))
        return qc_make(QF_NAN, QF_NAN);

    if (qf_eq(z.im, qf_from_double(0.0)))
        return qcrf(qf_lambert_wm1(z.re));

    if (qf_eq(z.re, QF_ZERO) && qf_eq(z.im, QF_ZERO))
        return qc_make(QF_NINF, QF_NAN);

    qcomplex_t branch_probe = qc_add(qc_mul(qcrf(QF_E), z), qcr(1.0));
    qcomplex_t w = qf_lt(qc_abs(branch_probe), qf_from_double(0.25))
        ? qc_lambert_w_series_guess(z, -1)
        : qc_lambert_w_asymptotic_guess(z, -1);

    for (int i = 0; i < 60; i++) {
        qcomplex_t ew = qc_exp(w);
        qcomplex_t wew = qc_mul(w, ew);
        qcomplex_t f = qc_sub(wew, z);
        qcomplex_t wp1 = qc_add(w, qcr(1.0));
        qcomplex_t denom;

        if (qf_lt(qc_abs(wp1), zero_tol)) {
            denom = qc_mul(ew, qcr(1.0));
        } else {
            qcomplex_t halley_corr = qc_div(qc_mul(qc_add(w, qcr(2.0)), f),
                                            qc_mul(qcr(2.0), wp1));
            denom = qc_sub(qc_mul(ew, wp1), halley_corr);
        }

        qcomplex_t delta = qc_div(f, denom);
        w = qc_sub(w, delta);

        if (qf_lt(qc_abs(delta), zero_tol))
            break;
    }

    return w;
}

qcomplex_t qc_lambert_wm1(qcomplex_t z)
{
    return qc_lambert_wm1_complex(z);
}

qcomplex_t qc_productlog(qcomplex_t z)
{
    if (qf_eq(z.im, qf_from_double(0.0)))
        return qcrf(qf_productlog(z.re));

    /* Halley iteration on the principal branch: w e^w = z */
    qcomplex_t w = qc_log(z);
    for (int i = 0; i < 40; i++) {
        qcomplex_t ew    = qc_exp(w);
        qcomplex_t wew   = qc_mul(w, ew);
        qcomplex_t f     = qc_sub(wew, z);
        qcomplex_t wp1   = qc_add(w, qcr(1.0));
        qcomplex_t f1    = qc_mul(ew, wp1);
        qcomplex_t f2    = qc_mul(ew, qc_add(w, qcr(2.0)));
        qcomplex_t corr2 = qc_mul(qcr(0.5), qc_mul(qc_div(f, f1), f2));
        qcomplex_t delta = qc_div(f, qc_sub(f1, corr2));
        w = qc_sub(w, delta);
        if (qf_lt(qc_abs(delta), qf_from_double(1e-30)))
            break;
    }
    return w;
}

static qcomplex_t qc_gammainc_lower_series(qcomplex_t s, qcomplex_t x)
{
    qfloat_t tol = qf_from_double(1e-30);

    qcomplex_t term = qc_div(qcr(1.0), s);
    qcomplex_t sum  = term;

    for (int i = 1; i < 10000; i++) {
        term = qc_mul(term, qc_div(x, qc_add(s, qcr((double)i))));
        sum  = qc_add(sum, term);
        if (qf_lt(qc_abs(term), qf_mul(tol, qc_abs(sum))))
            break;
    }

    return qc_mul(qc_mul(qc_pow(x, s), qc_exp(qc_neg(x))), sum);
}

qcomplex_t qc_gammainc_lower(qcomplex_t s, qcomplex_t x)
{
    if (qf_eq(s.im, qf_from_double(0.0)) && qf_eq(x.im, qf_from_double(0.0)))
        return qcrf(qf_gammainc_lower(s.re, x.re));

    return qc_gammainc_lower_series(s, x);
}

qcomplex_t qc_gammainc_upper(qcomplex_t s, qcomplex_t x)
{
    if (qf_eq(s.im, qf_from_double(0.0)) && qf_eq(x.im, qf_from_double(0.0)))
        return qcrf(qf_gammainc_upper(s.re, x.re));

    return qc_sub(qc_gamma(s), qc_gammainc_lower_series(s, x));
}

qcomplex_t qc_gammainc_P(qcomplex_t s, qcomplex_t x)
{
    if (qf_eq(s.im, qf_from_double(0.0)) && qf_eq(x.im, qf_from_double(0.0)))
        return qcrf(qf_gammainc_P(s.re, x.re));

    return qc_div(qc_gammainc_lower_series(s, x), qc_gamma(s));
}

qcomplex_t qc_gammainc_Q(qcomplex_t s, qcomplex_t x)
{
    if (qf_eq(s.im, qf_from_double(0.0)) && qf_eq(x.im, qf_from_double(0.0)))
        return qcrf(qf_gammainc_Q(s.re, x.re));

    return qc_sub(qcr(1.0), qc_gammainc_P(s, x));
}

qcomplex_t qc_ei(qcomplex_t z)
{
    if (qf_eq(z.im, qf_from_double(0.0)))
        return qcrf(qf_ei(z.re));

    // Ei(z) = γ + log(z) + Σ_{k=1}^∞ z^k / (k × k!)
    qfloat_t tol  = qf_from_double(1e-30);
    qcomplex_t sum = qc_add(qcrf(QF_EULER_MASCHERONI), qc_log(z));

    qfloat_t   fact = qf_from_double(1.0);  // fact = k!
    qcomplex_t term = z;                     // term = z^k

    for (int k = 1; k < 10000; k++) {
        qcomplex_t add = qc_div(term, qcrf(qf_mul(qf_from_double((double)k), fact)));
        sum = qc_add(sum, add);
        if (qf_lt(qc_abs(add), qf_mul(tol, qc_abs(sum))))
            break;
        fact = qf_mul(fact, qf_from_double((double)(k + 1)));
        term = qc_mul(term, z);
    }
    return sum;
}

qcomplex_t qc_e1(qcomplex_t z)
{
    if (qf_eq(z.im, qf_from_double(0.0)))
        return qcrf(qf_e1(z.re));

    return qc_neg(qc_ei(qc_neg(z)));
}
