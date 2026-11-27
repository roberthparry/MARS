/**
 * @file number_format_adapter.c
 * @brief Module-owned native numerical adapter for typed brace formatting.
 *
 * Captures native values in the formatter's bounded opaque payload and validates this module's presentation
 * vocabulary. Rendering delegates directly to the structured native formatter, preserving precision and borrowing
 * rules without generated format strings, a global registry or dependencies on unrelated mathematical adapters.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "format-t.h"
#include "number-t.h"
#define MARS_SHARED_NUMBER_FORMAT_INTERNAL_ACCESS
#include "internal/number_format_internal.h"
#define MARS_SHARED_QFLOAT_FORMAT_INTERNAL_ACCESS
#include "internal/qfloat_format_internal.h"
#define MARS_NUMBER_INTERNAL_ACCESS
#include "number_internal.h"

_Static_assert(sizeof(number_t) <= FMT_ADAPTER_CAPACITY, "Native value exceeds formatting adapter capacity.");

static string_t *numi_excel_integer(number_t value, const fmt_options_t *options, fmt_error_t *error)
{
    mpz_srcptr integer = number_mpz_value(number_impl_const(&value)->value.mpz);
    size_t capacity = mpz_sizeinbase(integer, 10) + 3u;
    char *buffer = malloc(capacity);
    if (!buffer)
        return NULL;
    mpz_get_str(buffer, 10, integer);
    bool negative = buffer[0] == '-';
    const char *digits = buffer + negative;
    string_t *out = fmt_excel_decimal(options, digits, (int)strlen(digits), negative, error);
    free(buffer);
    return out;
}

static string_t *numi_excel_rational(number_t value, const fmt_options_t *options, fmt_error_t *error)
{
    mpq_srcptr rational = number_mpq_value(number_impl_const(&value)->value.mpq);
    mpz_srcptr numerator = mpq_numref(rational), denominator = mpq_denref(rational);
    long point = (long)mpz_sizeinbase(numerator, 10) - (long)mpz_sizeinbase(denominator, 10) + 1;
    size_t places = fmt_excel_precision(options) + (point < 0 ? (size_t)-point : 0u);
    if (places > 100000u || point > 100000 || point < -100000) {
        if (error)
            *error = (fmt_error_t){FMT_ERROR_RANGE, SIZE_MAX, SIZE_MAX, "rational coefficient exceeds the supported range"};
        return NULL;
    }
    mpz_t scaled, quotient, remainder;
    mpz_inits(scaled, quotient, remainder, NULL);
    mpz_ui_pow_ui(scaled, 10u, (unsigned long)places);
    mpz_mul(scaled, scaled, numerator);
    mpz_abs(scaled, scaled);
    mpz_tdiv_qr(quotient, remainder, scaled, denominator);
    size_t capacity = mpz_sizeinbase(quotient, 10) + 4u;
    char *digits = malloc(capacity);
    string_t *out = NULL;
    if (digits) {
        mpz_get_str(digits, 10, quotient);
        size_t length = strlen(digits);
        int decimal_point = (int)length - (int)places;
        if (mpz_sgn(remainder)) {
            digits[length] = '1';
            digits[length + 1u] = '\0';
        }
        out = fmt_excel_decimal(options, digits, decimal_point, mpz_sgn(numerator) < 0, error);
    }
    free(digits);
    mpz_clears(scaled, quotient, remainder, NULL);
    return out;
}

static string_t *numi_excel_mpfr(number_t value, const fmt_options_t *options, fmt_error_t *error)
{
    mpfr_srcptr real = number_mpfr_value(number_impl_const(&value)->value.mpfr);
    if (!mpfr_number_p(real))
        return fmt_excel_real(options, mpfr_get_ld(real, MPFR_RNDN), error);
    if (mpfr_zero_p(real))
        return fmt_excel_decimal(options, "0", 1, mpfr_signbit(real) != 0, error);
    mpz_t coefficient;
    mpz_init(coefficient);
    mpfr_exp_t exponent = mpfr_get_z_2exp(coefficient, real);
    if (exponent < -100000 || exponent > 300000) {
        mpz_clear(coefficient);
        if (error)
            *error = (fmt_error_t){FMT_ERROR_RANGE, SIZE_MAX, SIZE_MAX, "MPFR exponent exceeds Excel rendering range"};
        return NULL;
    }
    bool negative = mpz_sgn(coefficient) < 0;
    mpz_abs(coefficient, coefficient);
    int scale = exponent < 0 ? (int)-exponent : 0;
    if (exponent >= 0) {
        mpz_mul_2exp(coefficient, coefficient, (mp_bitcnt_t)exponent);
    } else {
        mpz_t power;
        mpz_init(power);
        mpz_ui_pow_ui(power, 5u, (unsigned long)scale);
        mpz_mul(coefficient, coefficient, power);
        mpz_clear(power);
    }
    char *digits = malloc(mpz_sizeinbase(coefficient, 10) + 2u);
    string_t *out = NULL;
    if (digits) {
        mpz_get_str(digits, 10, coefficient);
        int point = (int)strlen(digits) - scale;
        fmt_options_t native = *options;
        native.precision = (int)num_get_prec_digits(value);
        out = fmt_excel_decimal(&native, digits, point, negative, error);
    }
    free(digits);
    mpz_clear(coefficient);
    return out;
}

static string_t *numi_excel_double(number_t value, const fmt_options_t *options, fmt_error_t *error)
{
    return fmt_excel_real(options, number_impl_const(&value)->value.d, error);
}

static string_t *numi_excel_qfloat(number_t value, const fmt_options_t *options, fmt_error_t *error)
{
    qfloat_t real = number_impl_const(&value)->value.qf;
    if (!isfinite(real.hi) || !isfinite(real.lo))
        return fmt_excel_real(options, qf_to_double(real), error);
    char digits[1621];
    int point;
    bool negative;
    qfi_decimal_components(real, digits, &point, &negative);
    fmt_options_t native = *options;
    native.precision = 32;
    return fmt_excel_decimal(&native, digits, point, negative, error);
}

static string_t *numi_excel_value(number_t value, const fmt_options_t *options, fmt_error_t *error);

static string_t *numi_excel_complex(number_t value, const fmt_options_t *options, fmt_error_t *error)
{
    if (!num_is_real(value)) {
        if (error)
            *error = (fmt_error_t){FMT_ERROR_TYPE, SIZE_MAX, SIZE_MAX, "Excel numeric codes require a real number"};
        return NULL;
    }
    number_t real = num_real_part(value);
    string_t *out = numi_excel_value(real, options, error);
    num_destroy(&real);
    return out;
}

static string_t *numi_excel_value(number_t value, const fmt_options_t *options, fmt_error_t *error)
{
    typedef string_t *(*render_fn)(number_t, const fmt_options_t *, fmt_error_t *);
    static const render_fn render[NUMBER_COMPLEX + 1] = {
        [NUMBER_DOUBLE]   = numi_excel_double,
        [NUMBER_QFLOAT]   = numi_excel_qfloat,
        [NUMBER_QCOMPLEX] = numi_excel_complex,
        [NUMBER_MPZ]      = numi_excel_integer,
        [NUMBER_MPQ]      = numi_excel_rational,
        [NUMBER_MPFR]     = numi_excel_mpfr,
        [NUMBER_CDOUBLE]  = numi_excel_complex,
        [NUMBER_COMPLEX]  = numi_excel_complex,
    };
    number_kind_t kind = number_impl_const(&value)->kind;
    if (kind <= NUMBER_INVALID || kind > NUMBER_COMPLEX)
        return fmt_excel_real(options, NAN, error);
    return render[kind](value, options, error);
}


static bool numi_fmt_validate(const fmt_options_t *options, fmt_error_t *error)
{
    const char *style = options->presentation;
    if (options->excel)
        return true;
    if ((*style && (style[1] || !strchr("nN", *style))) || options->alternate) {
        if (error)
            *error = (fmt_error_t){FMT_ERROR_TYPE, SIZE_MAX, SIZE_MAX, "unsupported number presentation or flags"};
        return false;
    }
    return true;
}

static string_t *numi_fmt_render(const void *data, const fmt_options_t *options, fmt_error_t *error)
{
    (void)error;
    number_t value;
    memcpy(&value, data, sizeof(value));
    if (options->excel)
        return numi_excel_value(value, options, error);
    const char *style = options->presentation;
    return numi_format_value_text(value, *style ? *style : 'n', options->precision);
}

static const fmt_adapter_t numi_fmt_adapter = {
    .validate = numi_fmt_validate,
    .render = numi_fmt_render,
    .numeric = true,
};

/* Capture native numerical data with this module's immutable formatting adapter. */
fmt_arg_t fmt_arg_number(number_t value)
{
    return fmt_arg_adapter(&numi_fmt_adapter, &value, sizeof(value));
}
