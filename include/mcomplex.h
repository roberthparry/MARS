#ifndef MCOMPLEX_H
#define MCOMPLEX_H

#include <stdbool.h>
#include <stddef.h>
#include <stdarg.h>

#include "mfloat.h"
#include "qcomplex.h"

/**
 * @file mcomplex.h
 * @brief Opaque multiprecision complex type backed by two mfloat values.
 */

typedef struct _mcomplex_t mcomplex_t;

extern const mcomplex_t * const MC_ZERO;
extern const mcomplex_t * const MC_ONE;
extern const mcomplex_t * const MC_HALF;
extern const mcomplex_t * const MC_TENTH;
extern const mcomplex_t * const MC_TEN;
extern const mcomplex_t * const MC_PI;
extern const mcomplex_t * const MC_E;
extern const mcomplex_t * const MC_EULER_MASCHERONI;
extern const mcomplex_t * const MC_SQRT2;
extern const mcomplex_t * const MC_SQRT_PI;
extern const mcomplex_t * const MC_NAN;
extern const mcomplex_t * const MC_INF;
extern const mcomplex_t * const MC_NINF;
extern const mcomplex_t * const MC_I;

mcomplex_t *mc_new(void);
mcomplex_t *mc_new_prec(size_t precision_bits);
mcomplex_t *mc_create(const mfloat_t *real, const mfloat_t *imag);
mcomplex_t *mc_create_long(long real);
mcomplex_t *mc_create_qcomplex(qcomplex_t value);
mcomplex_t *mc_create_string(const char *text);
mcomplex_t *mc_clone(const mcomplex_t *mcomplex);
void mc_free(mcomplex_t *mcomplex);
void mc_clear(mcomplex_t *mcomplex);

int mc_set_precision(mcomplex_t *mcomplex, size_t precision_bits);
size_t mc_get_precision(const mcomplex_t *mcomplex);
int mc_set_precision_digits(mcomplex_t *mcomplex, size_t significant_digits);
size_t mc_get_precision_digits(const mcomplex_t *mcomplex);
int mc_set(mcomplex_t *mcomplex, const mfloat_t *real, const mfloat_t *imag);
int mc_set_qcomplex(mcomplex_t *mcomplex, qcomplex_t value);
int mc_set_string(mcomplex_t *mcomplex, const char *text);
char *mc_to_string(const mcomplex_t *mcomplex);
qcomplex_t mc_to_qcomplex(const mcomplex_t *mcomplex);
/* %mz prints complex values in fixed format; %MZ prints scientific format.
   Width and precision apply to the real and imaginary parts individually. */
int mc_vsprintf(char *out, size_t out_size, const char *fmt, va_list ap);
int mc_sprintf(char *out, size_t out_size, const char *fmt, ...);
int mc_printf(const char *fmt, ...);

const mfloat_t *mc_real(const mcomplex_t *mcomplex);
const mfloat_t *mc_imag(const mcomplex_t *mcomplex);

bool mc_is_zero(const mcomplex_t *mcomplex);
bool mc_eq(const mcomplex_t *a, const mcomplex_t *b);
bool mc_isnan(const mcomplex_t *mcomplex);
bool mc_isinf(const mcomplex_t *mcomplex);
bool mc_isposinf(const mcomplex_t *mcomplex);
bool mc_isneginf(const mcomplex_t *mcomplex);

int mc_abs(mcomplex_t *mcomplex);
int mc_neg(mcomplex_t *mcomplex);
int mc_conj(mcomplex_t *mcomplex);
int mc_add(mcomplex_t *mcomplex, const mcomplex_t *other);
int mc_sub(mcomplex_t *mcomplex, const mcomplex_t *other);
int mc_mul(mcomplex_t *mcomplex, const mcomplex_t *other);
int mc_div(mcomplex_t *mcomplex, const mcomplex_t *other);
int mc_inv(mcomplex_t *mcomplex);
int mc_pow_int(mcomplex_t *mcomplex, int exponent);
int mc_pow(mcomplex_t *mcomplex, const mcomplex_t *exponent);
int mc_ldexp(mcomplex_t *mcomplex, int exponent2);
int mc_sqrt(mcomplex_t *mcomplex);
int mc_floor(mcomplex_t *mcomplex);
int mc_hypot(mcomplex_t *mcomplex, const mcomplex_t *other);
int mc_exp(mcomplex_t *mcomplex);
int mc_log(mcomplex_t *mcomplex);
int mc_sin(mcomplex_t *mcomplex);
int mc_cos(mcomplex_t *mcomplex);
int mc_tan(mcomplex_t *mcomplex);
int mc_atan(mcomplex_t *mcomplex);
int mc_atan2(mcomplex_t *mcomplex, const mcomplex_t *other);
int mc_asin(mcomplex_t *mcomplex);
int mc_acos(mcomplex_t *mcomplex);
int mc_sinh(mcomplex_t *mcomplex);
int mc_cosh(mcomplex_t *mcomplex);
int mc_tanh(mcomplex_t *mcomplex);
int mc_asinh(mcomplex_t *mcomplex);
int mc_acosh(mcomplex_t *mcomplex);
int mc_atanh(mcomplex_t *mcomplex);
int mc_gamma(mcomplex_t *mcomplex);
int mc_erf(mcomplex_t *mcomplex);
int mc_erfc(mcomplex_t *mcomplex);
int mc_erfinv(mcomplex_t *mcomplex);
int mc_erfcinv(mcomplex_t *mcomplex);
int mc_lgamma(mcomplex_t *mcomplex);
int mc_digamma(mcomplex_t *mcomplex);
int mc_trigamma(mcomplex_t *mcomplex);
int mc_tetragamma(mcomplex_t *mcomplex);
int mc_gammainv(mcomplex_t *mcomplex);
int mc_lambert_w0(mcomplex_t *mcomplex);
int mc_lambert_wm1(mcomplex_t *mcomplex);
int mc_beta(mcomplex_t *mcomplex, const mcomplex_t *other);
int mc_logbeta(mcomplex_t *mcomplex, const mcomplex_t *other);
int mc_binomial(mcomplex_t *mcomplex, const mcomplex_t *other);
int mc_beta_pdf(mcomplex_t *mcomplex, const mcomplex_t *a, const mcomplex_t *b);
int mc_logbeta_pdf(mcomplex_t *mcomplex, const mcomplex_t *a, const mcomplex_t *b);
int mc_normal_pdf(mcomplex_t *mcomplex);
int mc_normal_cdf(mcomplex_t *mcomplex);
int mc_normal_logpdf(mcomplex_t *mcomplex);
int mc_productlog(mcomplex_t *mcomplex);
int mc_gammainc_lower(mcomplex_t *mcomplex, const mcomplex_t *other);
int mc_gammainc_upper(mcomplex_t *mcomplex, const mcomplex_t *other);
int mc_gammainc_P(mcomplex_t *mcomplex, const mcomplex_t *other);
int mc_gammainc_Q(mcomplex_t *mcomplex, const mcomplex_t *other);
int mc_ei(mcomplex_t *mcomplex);
int mc_e1(mcomplex_t *mcomplex);

#endif
