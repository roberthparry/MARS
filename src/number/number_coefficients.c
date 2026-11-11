/**
 * @file number_coefficients.c
 * @brief Bounded, precision-aware coefficient reuse for native special-function series.
 *
 * Shares integer zeta values, integer logarithms and both ordinary and factorial-normalised even Bernoulli
 * coefficients between Clausen, zeta and polygamma evaluation. Direct indices avoid searches. A mutex protects lazy
 * precision upgrades; callers receive copies and never retain cache storage. Cached significands are limited to
 * eight MiB, with fixed index metadata; requests beyond those bounds are evaluated without retention. Shutdown releases only
 * entries that were initialised. No application policy or persistent storage enters this numeric cache.
 */
#include <limits.h>
#include <pthread.h>

#define MARS_NUMBER_INTERNAL_ACCESS
#include "number_internal.h"

enum { COEFFICIENT_ZETA, COEFFICIENT_BERNOULLI, COEFFICIENT_NORMALISED, COEFFICIENT_LOGARITHM, COEFFICIENT_KINDS };
enum { COEFFICIENT_SLOTS = 4096, COEFFICIENT_MAX_BITS = 16384, COEFFICIENT_MAX_BYTES = 8 * 1024 * 1024 };

typedef struct {
    mpfr_t value;
    mpfr_prec_t precision;
} coefficient_entry_t;

static coefficient_entry_t coefficient_cache[COEFFICIENT_KINDS][COEFFICIENT_SLOTS];
static coefficient_entry_t *coefficient_owned[COEFFICIENT_KINDS * COEFFICIENT_SLOTS];
static size_t coefficient_count, coefficient_bytes;
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
        size_t previous = entry->precision ? mpfr_custom_get_size(entry->precision) : 0;
        size_t bytes = mpfr_custom_get_size(precision);
        if (bytes - previous > COEFFICIENT_MAX_BYTES - coefficient_bytes) {
            pthread_mutex_unlock(&coefficient_lock);
            return calculate[kind](out, index);
        }
        if (!entry->precision) {
            mpfr_init2(entry->value, precision);
            coefficient_owned[coefficient_count++] = entry;
        } else mpfr_set_prec(entry->value, precision);
        coefficient_bytes += bytes - previous;
        entry->precision = precision;
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
