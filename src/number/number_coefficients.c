/**
 * @file number_coefficients.c
 * @brief Bounded, precision-aware coefficient reuse for native special-function series.
 *
 * Shares integer zeta values, integer logarithms and both ordinary and factorial-normalised even Bernoulli
 * coefficients between Clausen, zeta and polygamma evaluation. Direct indices avoid searches. A mutex protects lazy
 * precision upgrades; callers receive copies and never retain cache storage. Even Bernoulli coefficients share
 * the guarded recurrence from (t/2)coth(t/2), avoiding a separate integer-zeta evaluation for every coefficient.
 * Cached significands, including the recurrence's inverse factorials, are limited to
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
    if (coefficient_prefix_precision != precision) {
        coefficient_prefix_precision = precision;
        coefficient_prefix_count = 0;
    }
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
        if (bernoulli && index && precision <= COEFFICIENT_MAX_BITS - 96 &&
            coefficient_bernoulli_prefix(index, precision)) {
            coefficient_entry_t *normalised = &coefficient_cache[COEFFICIENT_NORMALISED][index];
            if (kind == COEFFICIENT_NORMALISED) {
                mpfr_set(out, normalised->value, MPFR_RNDN);
                pthread_mutex_unlock(&coefficient_lock);
                return 0;
            }
            mpfr_t value;
            mpfr_init2(value, precision + 96);
            mpfr_fac_ui(value, 2ul * index, MPFR_RNDN);
            mpfr_mul(value, value, normalised->value, MPFR_RNDN);
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
