/**
 * @file number_coefficients.c
 * @brief Bounded, precision-aware coefficient reuse for native special-function series.
 *
 * Shares integer zeta values, integer logarithms and both ordinary and factorial-normalised even Bernoulli
 * coefficients between Clausen, zeta and polygamma evaluation. Direct indices avoid searches. A mutex protects lazy
 * precision upgrades; callers receive copies and never retain cache storage. Even zeta and Bernoulli coefficients share
 * the guarded recurrence from (t/2)coth(t/2), avoiding a separate integer-zeta evaluation for every coefficient.
 * Known long even-zeta and ordinary Bernoulli prefixes can instead use the exact positive tangent-number recurrence,
 * with local GMP integers and guarded conversion. Raw Bernoulli conversion omits factorials and pi powers;
 * sequential powers avoid repeated exponentiation for even zeta. Cached significands,
 * including the recurrence's inverse factorials, are limited to
 * eight MiB, with fixed index metadata; requests beyond those bounds are evaluated without retention. Shutdown releases only
 * entries that were initialised. No application policy or persistent storage enters this numeric cache.
 */
#include <limits.h>
#include <pthread.h>

#define MARS_NUMBER_INTERNAL_ACCESS
#include "number_internal.h"

enum {
    COEFFICIENT_ZETA, COEFFICIENT_BERNOULLI, COEFFICIENT_NORMALISED, COEFFICIENT_LOGARITHM,
    COEFFICIENT_CONVOLUTION, COEFFICIENT_KINDS
};
enum { COEFFICIENT_SLOTS = 4096, COEFFICIENT_MAX_BITS = 16384, COEFFICIENT_MAX_BYTES = 8 * 1024 * 1024 };

typedef struct {
    mpfr_t value;
    mpfr_prec_t precision;
} coefficient_entry_t;

static coefficient_entry_t coefficient_cache[COEFFICIENT_KINDS][COEFFICIENT_SLOTS];
static coefficient_entry_t *coefficient_owned[COEFFICIENT_KINDS * COEFFICIENT_SLOTS];
static size_t coefficient_count, coefficient_bytes;
static size_t coefficient_prefix_count;
static mpfr_prec_t coefficient_prefix_precision;
static size_t coefficient_exact_count[2];
static mpfr_prec_t coefficient_exact_precision[2];
static pthread_mutex_t coefficient_lock = PTHREAD_MUTEX_INITIALIZER;

static int coefficient_zeta(mpfr_ptr out, size_t order)
{
    if (order < 2) { mpfr_set_nan(out); return -1; }
    mpfr_zeta_ui(out, (unsigned long)order, MPFR_RNDN);
    return mpfr_number_p(out) ? 0 : -1;
}

static int coefficient_bernoulli(mpfr_ptr out, size_t index, bool normalised)
{
    if (!index || index > ULONG_MAX / 2u) { mpfr_set_nan(out); return -1; }
    unsigned long degree = (unsigned long)index * 2u;
    mpfr_prec_t work = mpfr_get_prec(out) + 64;
    mpfr_t value, scale, zeta;
    mpfr_inits2(work, value, scale, zeta, (mpfr_ptr)0);
    mpfr_const_pi(scale, MPFR_RNDN);
    mpfr_mul_2ui(scale, scale, 1u, MPFR_RNDN);
    mpfr_pow_ui(scale, scale, degree, MPFR_RNDN);
    mpfr_zeta_ui(zeta, degree, MPFR_RNDN);
    mpfr_mul_2ui(value, zeta, 1u, MPFR_RNDN);
    mpfr_div(value, value, scale, MPFR_RNDN);
    if (!normalised) {
        mpfr_fac_ui(scale, degree, MPFR_RNDN);
        mpfr_mul(value, value, scale, MPFR_RNDN);
    }
    if (!(index & 1u)) mpfr_neg(value, value, MPFR_RNDN);
    mpfr_set(out, value, MPFR_RNDN);
    mpfr_clears(value, scale, zeta, (mpfr_ptr)0);
    return mpfr_number_p(out) ? 0 : -1;
}

static int coefficient_raw(mpfr_ptr out, size_t index) { return coefficient_bernoulli(out, index, false); }
static int coefficient_normalised(mpfr_ptr out, size_t index) { return coefficient_bernoulli(out, index, true); }

static int coefficient_logarithm(mpfr_ptr out, size_t index)
{
    if (!index) { mpfr_set_nan(out); return -1; }
    mpfr_log_ui(out, (unsigned long)index, MPFR_RNDN);
    return mpfr_number_p(out) ? 0 : -1;
}

/* The cache lock owns every reservation; accounting uses the actual allocation, including guard bits. */
static bool coefficient_reserve(coefficient_entry_t *entry, mpfr_prec_t precision, unsigned guard)
{
    mpfr_prec_t stored = precision + guard;
    size_t previous = entry->precision ? mpfr_custom_get_size(mpfr_get_prec(entry->value)) : 0;
    size_t bytes = mpfr_custom_get_size(stored);
    if (entry->precision >= precision && mpfr_get_prec(entry->value) >= stored) return true;
    if (bytes > previous && bytes - previous > COEFFICIENT_MAX_BYTES - coefficient_bytes) return false;
    if (!entry->precision) {
        mpfr_init2(entry->value, stored);
        coefficient_owned[coefficient_count++] = entry;
    } else mpfr_set_prec(entry->value, stored);
    coefficient_bytes = coefficient_bytes - previous + bytes;
    entry->precision = precision;
    return true;
}

/* Brent--Harvey, Fast Computation of Bernoulli, Tangent and Secant Numbers, Fig. 2 (arXiv:1108.0286).
 * The positive integer recurrence is exact; only conversion and sequential powers need arithmetic guard bits. */
static bool coefficient_exact_prefix(size_t count, mpfr_prec_t requested, bool raw)
{
    if (count < 32 || count > 1024 || requested < 1024 || requested > COEFFICIENT_MAX_BITS - 160) return false;
    if (pthread_mutex_lock(&coefficient_lock)) return false;
    mpfr_prec_t precision = ((requested + 63) / 64) * 64;
    if (coefficient_exact_precision[raw] >= precision) {
        bool ready = coefficient_exact_count[raw] >= count;
        if (ready || count - coefficient_exact_count[raw] <= 16) {
            pthread_mutex_unlock(&coefficient_lock);
            return ready;
        }
    }
    /* Conversion retains 96 extra bits and at most 1024 multiply/divide steps. Check the bounded cache first. */
    size_t bytes = coefficient_bytes;
    for (size_t n = 1; n <= count; ++n) {
        coefficient_entry_t *entries[] = { raw ? &coefficient_cache[COEFFICIENT_BERNOULLI][n] :
            &coefficient_cache[COEFFICIENT_NORMALISED][n], &coefficient_cache[COEFFICIENT_ZETA][2 * n] };
        for (unsigned j = 0; j < (raw ? 1u : 2u); ++j) {
            if (entries[j]->precision >= precision) continue;
            size_t previous = entries[j]->precision ? mpfr_custom_get_size(mpfr_get_prec(entries[j]->value)) : 0;
            size_t stored = mpfr_custom_get_size(precision + (j ? 0 : 96));
            bytes += stored - previous;
        }
    }
    if (bytes > COEFFICIENT_MAX_BYTES) { pthread_mutex_unlock(&coefficient_lock); return false; }
    bool ok = true;
    mpz_t tangent[1025], factorial, denominator;
    for (size_t n = 1; n <= count; ++n) mpz_init(tangent[n]);
    mpz_inits(factorial, denominator, NULL);
    mpz_set_ui(tangent[1], 1u);
    for (size_t n = 2; n <= count; ++n) mpz_mul_ui(tangent[n], tangent[n - 1], n - 1);
    for (size_t k = 2; k <= count; ++k) {
        for (size_t j = k; j <= count; ++j) {
            mpz_mul_ui(tangent[j], tangent[j], j - k + 2);
            mpz_addmul_ui(tangent[j], tangent[j - 1], j - k);
        }
    }
    mpfr_t square, power, value;
    mpfr_inits2(precision + 96, square, power, value, (mpfr_ptr)0);
    if (!raw) {
        mpfr_const_pi(square, MPFR_RNDN); mpfr_mul_2ui(square, square, 1u, MPFR_RNDN);
        mpfr_sqr(square, square, MPFR_RNDN); mpfr_set_ui(power, 1u, MPFR_RNDN);
    }
    mpz_set_ui(factorial, 1u);
    for (size_t n = 1; ok && n <= count; ++n) {
        if (!raw && n > 1) {
            mpz_mul_ui(factorial, factorial, 2 * n - 2);
            mpz_mul_ui(factorial, factorial, 2 * n - 1);
        }
        /* B_(2n)/(2n)! = (-1)^(n-1) T_n / ((2n-1)!*2^(2n)*(2^(2n)-1)).
         * The raw projection instead multiplies by 2n and omits (2n-1)!. */
        mpz_set_ui(denominator, 1u); mpz_mul_2exp(denominator, denominator, 2 * n);
        mpz_sub_ui(denominator, denominator, 1u);
        mpfr_set_z(value, tangent[n], MPFR_RNDN);
        if (raw) mpfr_mul_ui(value, value, 2 * n, MPFR_RNDN);
        else mpfr_div_z(value, value, factorial, MPFR_RNDN);
        mpfr_div_z(value, value, denominator, MPFR_RNDN);
        mpfr_div_2ui(value, value, 2 * n, MPFR_RNDN);
        if (!mpfr_number_p(value) || mpfr_zero_p(value)) { ok = false; break; }
        if (raw) {
            /* B_(2n) = (-1)^(n-1)*2n*T_n/(2^(2n)*(2^(2n)-1)). No pi or factorial is needed. */
            if (!(n & 1u)) mpfr_neg(value, value, MPFR_RNDN);
            coefficient_entry_t *entry = &coefficient_cache[COEFFICIENT_BERNOULLI][n];
            if (entry->precision <= precision) {
                ok = coefficient_reserve(entry, precision, 96);
                if (ok) mpfr_set(entry->value, value, MPFR_RNDN);
            }
            continue;
        }
        coefficient_entry_t *normalised = &coefficient_cache[COEFFICIENT_NORMALISED][n];
        if (!(n & 1u)) mpfr_neg(value, value, MPFR_RNDN);
        /* A lower precision prefetch must not overwrite an independently validated higher precision coefficient. */
        if (normalised->precision <= precision) {
            ok = coefficient_reserve(normalised, precision, 96);
            if (!ok) break;
            mpfr_set(normalised->value, value, MPFR_RNDN);
        }
        mpfr_abs(value, value, MPFR_RNDN);
        mpfr_mul(power, power, square, MPFR_RNDN);
        mpfr_mul(value, value, power, MPFR_RNDN); mpfr_div_2ui(value, value, 1u, MPFR_RNDN);
        if (!mpfr_number_p(value) || mpfr_zero_p(value)) { ok = false; break; }
        coefficient_entry_t *zeta = &coefficient_cache[COEFFICIENT_ZETA][2 * n];
        if (zeta->precision <= precision) {
            ok = coefficient_reserve(zeta, precision, 0);
            if (ok) mpfr_set(zeta->value, value, MPFR_RNDN);
        }
    }
    if (ok) {
        coefficient_exact_count[raw] = count;
        coefficient_exact_precision[raw] = precision;
    }
    mpfr_clears(square, power, value, (mpfr_ptr)0);
    mpz_clears(factorial, denominator, NULL);
    for (size_t n = 1; n <= count; ++n) mpz_clear(tangent[n]);
    pthread_mutex_unlock(&coefficient_lock);
    return ok;
}

/* Prepare a known even-zeta prefix, preserving independently validated higher-precision entries. */
bool number_coefficient_zeta_prefix(size_t count, mpfr_prec_t requested)
{
    return coefficient_exact_prefix(count, requested, false);
}

/* Prepare ordinary Bernoulli coefficients directly from exact tangent numbers, without unused zeta entries. */
bool number_coefficient_bernoulli_prepare(size_t count, mpfr_prec_t requested)
{
    return coefficient_exact_prefix(count, requested, true);
}

/* b_n = 2n*c_n - sum_(j=1)^(n-1) b_(n-j)*c_j, with c_n = 1/(4^n*(2n+1)!).
 * Successive omitted magnitudes have ratio below 16/((2j+2)(2j+3)); hence their tail is at most four terms.
 * Keeping 48 extra relative bits in that tail and 96 arithmetic guard bits covers accumulation across 4096 slots.
 * Small convolution products need fewer relative bits, but retain the same absolute precision as the accumulator. */
static bool coefficient_bernoulli_prefix(size_t index, mpfr_prec_t precision)
{
    mpfr_prec_t work = precision + 96;
    mpfr_t term, magnitude, bound;
    mpfr_init2(term, work);
    mpfr_inits2(64, magnitude, bound, (mpfr_ptr)0);
    bool ok = true;
    if (coefficient_prefix_precision < precision) {
        coefficient_prefix_precision = precision;
        coefficient_prefix_count = 0;
    } else coefficient_prefix_precision = precision;
    /* Ordinary sequential extensions start at the next direct index; a precision change revalidates the prefix. */
    for (size_t n = coefficient_prefix_count + 1; ok && n <= index; ++n) {
        coefficient_entry_t *value = &coefficient_cache[COEFFICIENT_NORMALISED][n];
        coefficient_entry_t *factor = &coefficient_cache[COEFFICIENT_CONVOLUTION][n];
        if (factor->precision < precision || mpfr_get_prec(factor->value) < work) {
            ok = coefficient_reserve(factor, precision, 96);
            if (!ok) break;
            if (n == 1) mpfr_set_ui(factor->value, 1u, MPFR_RNDN);
            else mpfr_set(factor->value, coefficient_cache[COEFFICIENT_CONVOLUTION][n - 1].value, MPFR_RNDN);
            mpfr_div_ui(factor->value, factor->value, 4ul * (2ul * n) * (2ul * n + 1u), MPFR_RNDN);
        }
        if (value->precision >= precision && mpfr_get_prec(value->value) >= work) {
            coefficient_prefix_count = n;
            continue;
        }
        ok = coefficient_reserve(value, precision, 96);
        if (!ok) break;
        mpfr_mul_ui(value->value, factor->value, 2ul * n, MPFR_RNDN);
        for (size_t j = 1; j < n; ++j) {
            mpfr_srcptr left = coefficient_cache[COEFFICIENT_NORMALISED][n - j].value;
            mpfr_srcptr right = coefficient_cache[COEFFICIENT_CONVOLUTION][j].value;
            mpfr_prec_t product_precision = work;
            if (j > 1 && !mpfr_zero_p(value->value)) {
                /* Each shortened product loses less than 2^(accumulator exponent-work-16) absolutely,
                 * leaving ample room within the existing 96 guard bits across all 4096 coefficients. */
                mpfr_exp_t relative = mpfr_get_exp(left) + mpfr_get_exp(right) - mpfr_get_exp(value->value);
                if (relative < -16) product_precision = relative < 128 - work ? 128 : work + relative + 16;
            }
            if (mpfr_get_prec(term) != product_precision) mpfr_set_prec(term, product_precision);
            mpfr_mul(term, left, right, MPFR_RNDN);
            mpfr_sub(value->value, value->value, term, MPFR_RNDN);
            if (j < 4) continue;
            mpfr_abs(magnitude, term, MPFR_RNDU); mpfr_abs(bound, value->value, MPFR_RNDD);
            if (number_series_small(magnitude, bound, precision, 48)) break;
        }
        if (!mpfr_number_p(value->value) || mpfr_sgn(value->value) != (n & 1u ? 1 : -1)) {
            ok = coefficient_bernoulli(value->value, n, true) == 0;
        }
        if (ok) coefficient_prefix_count = n;
    }
    mpfr_clears(term, magnitude, bound, (mpfr_ptr)0);
    return ok;
}

static int coefficient_copy(mpfr_ptr out, size_t index, unsigned kind)
{
    static int (*const calculate[])(mpfr_ptr, size_t) = {
        coefficient_zeta, coefficient_raw, coefficient_normalised, coefficient_logarithm
    };
    mpfr_prec_t requested = mpfr_get_prec(out);
    if (index >= COEFFICIENT_SLOTS || requested > COEFFICIENT_MAX_BITS)
        return calculate[kind](out, index);
    if (pthread_mutex_lock(&coefficient_lock)) { mpfr_set_nan(out); return -1; }
    coefficient_entry_t *entry = &coefficient_cache[kind][index];
    int status = 0;
    if (entry->precision < requested) {
        mpfr_prec_t precision = ((requested + 63) / 64) * 64;
        bool bernoulli = kind == COEFFICIENT_BERNOULLI || kind == COEFFICIENT_NORMALISED;
        bool even_zeta = kind == COEFFICIENT_ZETA && index >= 2 && !(index & 1u);
        size_t degree = even_zeta ? index / 2u : index;
        /* A distant isolated zeta request must not construct an otherwise unused Bernoulli prefix. */
        even_zeta = even_zeta && (degree <= 16 ||
            (coefficient_prefix_precision >= precision && degree <= coefficient_prefix_count + 16));
        if ((bernoulli || even_zeta) && degree && precision <= COEFFICIENT_MAX_BITS - 96 &&
            coefficient_bernoulli_prefix(degree, precision)) {
            coefficient_entry_t *normalised = &coefficient_cache[COEFFICIENT_NORMALISED][degree];
            if (kind == COEFFICIENT_NORMALISED) {
                mpfr_set(out, normalised->value, MPFR_RNDN);
                pthread_mutex_unlock(&coefficient_lock);
                return 0;
            }
            mpfr_t value;
            mpfr_init2(value, precision + 96);
            if (even_zeta) {
                /* zeta(2n) = |B_(2n)/(2n)!|*(2*pi)^(2n)/2, with the prefix's 96 guard bits. */
                mpfr_const_pi(value, MPFR_RNDN);
                mpfr_mul_2ui(value, value, 1u, MPFR_RNDN);
                mpfr_pow_ui(value, value, (unsigned long)index, MPFR_RNDN);
                mpfr_div_2ui(value, value, 1u, MPFR_RNDN);
            } else mpfr_fac_ui(value, 2ul * index, MPFR_RNDN);
            mpfr_mul(value, value, normalised->value, MPFR_RNDN);
            if (even_zeta) mpfr_abs(value, value, MPFR_RNDN);
            mpfr_set(out, value, MPFR_RNDN);
            if (coefficient_reserve(entry, precision, 0)) mpfr_set(entry->value, value, MPFR_RNDN);
            mpfr_clear(value);
            pthread_mutex_unlock(&coefficient_lock);
            return 0;
        }
        if (!coefficient_reserve(entry, precision, 0)) {
            pthread_mutex_unlock(&coefficient_lock);
            return calculate[kind](out, index);
        }
        status = calculate[kind](entry->value, index);
    }
    if (!status && mpfr_number_p(entry->value)) mpfr_set(out, entry->value, MPFR_RNDN);
    else { mpfr_set_nan(out); status = -1; }
    pthread_mutex_unlock(&coefficient_lock);
    return status;
}

/* Copy an integer zeta coefficient at the caller's working precision. */
int number_coefficient_zeta(mpfr_ptr out, unsigned long order)
{
    return coefficient_copy(out, order, COEFFICIENT_ZETA);
}

/* Copy an ordinary even Bernoulli coefficient without factorial normalisation. */
int number_coefficient_bernoulli(mpfr_ptr out, size_t index)
{
    return coefficient_copy(out, index, COEFFICIENT_BERNOULLI);
}

/* Copy B_(2k)/(2k)! directly for Euler–Maclaurin corrections. */
int number_coefficient_normalised(mpfr_ptr out, size_t index)
{
    return coefficient_copy(out, index, COEFFICIENT_NORMALISED);
}

/* Copy a positive integer's logarithm, retaining only bounded precision-aware coefficients. */
int number_integer_logarithm(mpfr_ptr out, unsigned long value)
{
    return coefficient_copy(out, value, COEFFICIENT_LOGARITHM);
}

static void __attribute__((destructor)) coefficient_shutdown(void)
{
    /* Releasing each owned MPFR allocation once is necessarily proportional to the initialised entries. */
    for (size_t i = 0; i < coefficient_count; ++i) mpfr_clear(coefficient_owned[i]->value);
}
