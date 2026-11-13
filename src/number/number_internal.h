/**
 * @file number_internal.h
 * @brief Private numeric representations and backend contracts.
 *
 * Defines the number implementation's backend kinds, storage access and shared arithmetic helpers. Other modules
 * should use number_t or src/internal/number_internal.h rather than depend on this complete backend layout.
 *
 * This header is an implementation detail under src/, not an installed public API. Keep its consumers within the
 * documented module boundary and preserve any explicit internal-access guards.
 */

#ifndef MARS_NUMBER_INTERNAL_H
#define MARS_NUMBER_INTERNAL_H

#if !defined(MARS_NUMBER_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0)
#error "Private header requires MARS_NUMBER_INTERNAL_ACCESS."
#endif

#include <string.h>

#include <mpc.h>

#include "number-t.h"
#define MARS_SHARED_NUMBER_INTERNAL_ACCESS
#include "internal/number_internal.h"

/**
 * @brief Calculate a positive integer series denominator exactly when it fits a machine word.
 * @param base Positive integer base; series callers supply at least one.
 * @param exponent Non-negative integer exponent; zero returns one.
 * @return Exact base raised to exponent, or zero when a product would overflow unsigned long.
 * @note Thread-safe, allocation-free and logarithmic in exponent. No ownership transfers. A zero return
 * selects the caller's arbitrary-precision denominator calculation; it must never be used as a divisor.
 */
unsigned long number_series_integer_power(unsigned long base, unsigned long exponent);

/**
 * @brief Copy zeta(n) from the bounded numeric coefficient cache at the output's precision.
 * @param[out] out Caller-owned initialised MPFR destination; its precision is preserved.
 * @param[in] order Integer n, at least two.
 * @return Zero on success; minus one on an invalid order or cache-lock failure. Invalid values become NaN.
 * @note Thread-safe copies retain no borrowed cache pointers. Bounds affect retention only; larger requests
 * are evaluated without caching. The destination remains caller-owned and is cleared with mpfr_clear().
 */
int number_coefficient_zeta(mpfr_ptr out, unsigned long order);

/**
 * @brief Copy the even Bernoulli coefficient B_(2k) at the output's working precision.
 * @param[out] out Caller-owned initialised MPFR destination; its precision is preserved.
 * @param[in] index Positive k, no larger than ULONG_MAX/2.
 * @return Zero on success; minus one on invalid input or cache-lock failure, with invalid values set to NaN.
 * @note Copies are protected by the shared cache mutex. No cache ownership is transferred; clear out normally.
 * Requests exceeding the bounded cache are calculated without retention.
 */
int number_coefficient_bernoulli(mpfr_ptr out, size_t index);

/**
 * @brief Copy the factorial-normalised coefficient B_(2k)/(2k)! for Euler–Maclaurin summation.
 * @param[out] out Caller-owned initialised MPFR destination, retained at its existing precision.
 * @param[in] index Positive k, no larger than ULONG_MAX/2.
 * @return Zero on success; minus one on invalid input or cache-lock failure. Invalid values become NaN.
 * @note Uses the same thread-safe bounded retention policy as the ordinary coefficients. The caller clears
 * out with mpfr_clear(); no cache entry or borrowed pointer escapes the call.
 */
int number_coefficient_normalised(mpfr_ptr out, size_t index);

/**
 * @brief Copy a positive integer's natural logarithm at the destination precision.
 * @param[out] out Caller-owned initialised MPFR destination, whose precision is preserved.
 * @param[in] value Positive integer; zero is invalid and produces NaN.
 * @return Zero on success; minus one on invalid input or a cache-lock failure, with NaN in out.
 * Integer logarithms share the bounded thread-safe coefficient cache. Higher-precision entries supply rounded
 * copies for lower requests; oversized inputs or precisions are calculated without retention. The caller retains
 * out and clears it with mpfr_clear(); no cache storage or ownership escapes the call.
 */
int number_integer_logarithm(mpfr_ptr out, unsigned long value);

/**
 * @brief Evaluate the finite sum of n^(-s), for integers 1 <= n < endpoint, at sum's precision.
 * @param[out] sum Caller-owned initialised MPC destination, replaced with the sum.
 * @param[out] derivative Optional caller-owned initialised MPC destination, replaced with the derivative in s.
 * @param[in] argument Borrowed complex exponent s, valid for this call; must not alias either output.
 * @param[in] endpoint Exclusive positive upper bound of the finite sum.
 * @return Nothing; both outputs receive the result or the propagated non-finite arithmetic value.
 * @note The outputs must be distinct and have matching precision. Temporary factorised powers are bounded;
 * larger requests and allocation failures use direct summation. All temporary ownership stays in the call.
 * The shared sieve is initialised once, making independent calls thread-safe. This helper has no persistent state
 * depending on s and does not set an evaluation error; non-finite arithmetic propagates through the outputs.
 */
void number_zeta_finite_sum(mpc_ptr sum, mpc_ptr derivative, mpc_srcptr argument, unsigned long endpoint);

/**
 * @brief Add the Euler–Maclaurin integral, endpoint and Bernoulli tail to a finite zeta sum.
 * @param[in,out] sum Caller-owned initialised MPC accumulator containing the finite value sum.
 * @param[in,out] derivative Optional matching MPC accumulator containing the finite derivative in s.
 * @param[in] argument Borrowed exponent s; must not alias an accumulator.
 * @param[in] endpoint Borrowed first omitted base, on the principal logarithm branch; must not alias an accumulator.
 * @param[in] target Required result precision in bits; accumulators must have at least 128 extra working bits.
 * @param[in] budget Maximum number of Bernoulli corrections, at least four.
 * @param[in] guard Additional bits required by the absolute correction stopping check.
 * @return Zero after convergence, or minus one on exhausted corrections, non-finite arithmetic or coefficient failure.
 * Failed accumulators are unspecified and must not be accepted as results. The pole at s=1 receives signed infinity.
 * Both accumulators remain caller-owned and distinct, with identical precision; clear them with mpc_clear().
 * All per-call state is local; shared coefficient copies use the bounded thread-safe cache.
 */
int number_zeta_tail(mpc_ptr sum, mpc_ptr derivative, mpc_srcptr argument, mpc_srcptr endpoint,
                     mpfr_prec_t target, size_t budget, unsigned guard);

/**
 * @brief Copy log(offset + index), reusing bounded logarithms for the most recently used exact offset.
 * @param[out] out Caller-owned initialised MPC destination, whose precision is preserved.
 * @param[in] offset Borrowed complex offset; must not alias out and remains valid throughout this call.
 * @param[in] index Non-negative integer shift.
 * @return Nothing; out receives the logarithm or the corresponding MPC non-finite value.
 * @note Thread-safe calls copy values; no cache pointer or ownership escapes. Offset changes release old entries.
 * Requests beyond retention limits, locking failures and non-finite logarithms use direct evaluation. Arithmetic
 * domain errors propagate as MPC non-finite values. The caller clears out normally with mpc_clear().
 */
void number_zeta_logarithm(mpc_ptr out, mpc_srcptr offset, unsigned long index);

/**
 * @brief Copy (offset+index)^(-exponent), retaining bounded exact-key Dirichlet powers between evaluations.
 * @param[out] out Initialised caller-owned MPC destination selecting precision; must not alias either key.
 * @param[in] offset Borrowed finite complex offset selecting the principal logarithm branch.
 * @param[in] exponent Borrowed finite complex exponent.
 * @param index Non-negative integer shift, used as a direct cache index when eligible.
 * @return Nothing; out receives the value or propagated non-finite arithmetic. Cache failures use direct evaluation.
 * The eight-MiB cache copies values under a mutex, retains at most one exact key pair and never exposes ownership.
 * Both keys remain valid throughout this call; the caller clears out normally. Independent calls are thread-safe.
 */
void number_dirichlet_power(mpc_ptr out, mpc_srcptr offset, mpc_srcptr exponent, unsigned long index);

/**
 * @brief Bound a complex modulus from above using |real| + |imag| and upward rounding.
 * @param[out] out Caller-owned initialised MPFR destination, distinct from both components of value.
 * @param[in] value Borrowed complex input, valid during this call.
 * @return Nothing; out receives the upper bound, with non-finite components propagated.
 * @note No allocation or ownership transfer occurs. NaNs and infinities propagate. Independent calls are
 * thread-safe. This conservative bound is for convergence checks, not a replacement for the mathematical modulus.
 */
void number_mpc_abs_upper(mpfr_ptr out, mpc_srcptr value);

/**
 * @brief Bound a complex modulus from below using max(|real|, |imag|) and downward rounding.
 * @param[out] out Caller-owned initialised MPFR destination, distinct from both components of value.
 * @param[in] value Borrowed complex input, valid during this call.
 * @return Nothing; out receives the lower bound, with non-finite components propagated.
 * @note No allocation or ownership transfer occurs. NaNs and infinities propagate. Independent calls are
 * thread-safe; clear out using mpfr_clear() when finished. Use this only where a conservative bound is sufficient.
 */
void number_mpc_abs_lower(mpfr_ptr out, mpc_srcptr value);

/**
 * @brief Conservatively test a scaled series term against a relative precision target using binary exponents.
 * @param[in] term_upper Caller-owned finite non-negative upper bound on the omitted term or tail.
 * @param[in] sum_lower Caller-owned finite non-negative lower bound on the absolute result scale.
 * @param[in] bits Positive required relative precision in bits.
 * @param[in] factor_bits Non-negative exponent bounding an additional multiplier by at most 2^factor_bits.
 * @return True only when the scaled upper bound is proved smaller than sum_lower times 2^(-bits).
 * A zero upper bound is accepted even with a zero scale. Invalid bounds, non-positive precision, exponent
 * overflow or an inconclusive comparison return false. No allocation, mutation or ownership transfer occurs;
 * both inputs remain valid throughout the call and independent calls are thread-safe.
 */
bool number_series_small(mpfr_srcptr term_upper, mpfr_srcptr sum_lower, mpfr_prec_t bits, unsigned factor_bits);

/**
 * @brief Refine the established real inverse-gamma branch with increasing working precision.
 * @param[in,out] value Caller-owned initialised MPFR input, replaced with its inverse-gamma value at the same precision.
 * @return Zero on success or a rejected real-domain input, minus one if checked refinement cannot converge.
 * @note Rejected inputs and refinement failures become NaN. The fixed-precision seed chooses the existing real
 * branch but never limits final accuracy. No ownership transfers; clear value with mpfr_clear(). Independent
 * calls use local temporaries and are thread-safe subject to the ordinary fixed-precision seed's contract.
 */
int number_mpfr_gammainv_mut(mpfr_t value);

/**
 * @brief Evaluate complex gamma with guarded recurrence and a bounded Stirling remainder.
 * @param[out] out Initialised caller-owned destination, selecting precision; may alias input.
 * @param[in] input Borrowed finite complex argument outside gamma's poles.
 * @param rounding Final rounding mode.
 * @return Zero on success, minus one with NaN on invalid input or exhausted working limits.
 * No ownership transfers. Local temporaries are released; coefficient-cache access is synchronised.
 */
int number_mpc_gamma(mpc_ptr out, mpc_srcptr input, mpc_rnd_t rounding);

/**
 * @brief Evaluate the analytic logarithm of gamma on the plane cut along the non-positive real axis.
 * @param[out] out Initialised caller-owned destination, selecting precision; may alias input.
 * @param[in] input Borrowed finite argument, retained throughout the call.
 * @param rounding Final rounding mode.
 * @return Zero on success, minus one with NaN at poles or when bounded refinement cannot finish.
 * Local temporaries are released and shared coefficient access is synchronised; no pointers escape.
 */
int number_mpc_lgamma(mpc_ptr out, mpc_srcptr input, mpc_rnd_t rounding);

/**
 * @brief Evaluate complex digamma using recurrence, reflection and cached asymptotic coefficients.
 * @param[out] out Initialised caller-owned destination selecting precision; may alias z.
 * @param[in] z Borrowed argument outside the digamma poles.
 * @param rnd Final rounding mode.
 * @return Zero on success or minus one if convergence or cache access fails.
 * No ownership transfers; local temporaries and synchronised coefficients permit independent calls.
 */
int number_mpc_digamma_mut(mpc_ptr out, mpc_srcptr z, mpc_rnd_t rnd);

/**
 * @brief Refine the complex inverse-gamma branch selected by its logarithmic starting estimate.
 * @param[out] out Initialised caller-owned destination selecting precision; may alias input.
 * @param[in] input Borrowed finite, non-zero complex gamma value.
 * @param rounding Final rounding mode.
 * @return Zero after a checked logarithmic residual, minus one with NaN on unresolved refinement.
 * Seeds determine the branch only. No storage escapes and independent calls are thread-safe.
 */
int number_mpc_gammainv(mpc_ptr out, mpc_srcptr input, mpc_rnd_t rounding);

/** @brief Requested incomplete-gamma quantity; the dispatcher supplies the same mode to all complex inputs. */
typedef enum {
    NUMBER_GAMMA_LOWER, /**< Unregularised lower gamma. */
    NUMBER_GAMMA_UPPER, /**< Unregularised upper gamma. */
    NUMBER_GAMMA_P,     /**< Lower gamma divided by complete gamma. */
    NUMBER_GAMMA_Q      /**< Upper gamma divided by complete gamma. */
} number_gamma_mode_t;

/**
 * @brief Evaluate the principal incomplete-gamma series without fixed-precision conversion.
 * @param[out] out Initialised caller-owned destination, selecting precision; may alias either input.
 * @param[in] order Borrowed gamma order, excluding lower-gamma poles at non-positive integers.
 * @param[in] argument Borrowed finite argument for the principal power, non-zero unless Re(order)>0.
 * @param mode Requested lower, upper or regularised value.
 * @param rounding Final rounding mode.
 * @return Zero on a tail- and cancellation-checked value; minus one with NaN outside supported working limits.
 * Local storage is released. Shared Bernoulli coefficients are accessed under their cache lock.
 */
int number_mpc_gammainc(mpc_ptr out, mpc_srcptr order, mpc_srcptr argument,
                        number_gamma_mode_t mode, mpc_rnd_t rounding);

/**
 * @brief Sum the real non-negative integer-order Legendre chi series on |z| < 0.95.
 * @param[out] out Caller-owned initialised MPFR destination, preserved at its existing precision.
 * @param[in] order Non-negative integer order.
 * @param[in] input Borrowed real argument; may alias out and must remain valid for this call.
 * @return Zero for a verified sum, one outside the direct-series region without modifying out, or minus one
 * for insufficient convergence or roundoff accuracy, setting out to NaN.
 * @note All scratch storage is local. No ownership transfers; the caller clears out normally. Independent
 * calls are thread-safe. The dispatcher retains analytic continuation through the established polylogarithms.
 */
int number_mpfr_chi_series(mpfr_ptr out, unsigned long order, mpfr_srcptr input);

/**
 * @brief Sum the complex non-negative integer-order Legendre chi series on |z| < 0.95.
 * @param[out] out Caller-owned initialised MPC destination, preserved at its existing precision.
 * @param[in] order Non-negative integer order.
 * @param[in] input Borrowed complex argument; may alias out and remains valid during this call.
 * @return Zero for a verified sum, one outside the direct-series region without modifying out, or minus one
 * on insufficient convergence or roundoff accuracy, with out set to NaN.
 * @note Local scratch values own their allocations. No pointers or ownership escape; clear out with mpc_clear().
 * Independent calls are thread-safe. The caller supplies the existing continuation outside this region.
 */
int number_mpc_chi_series(mpc_ptr out, unsigned long order, mpc_srcptr input);

/* Scoped allocation services implemented by number_core.c. */
void *number_scope_mem_alloc(size_t size, size_t align);
void *number_scope_mem_calloc(size_t count, size_t size, size_t align);
void *number_scope_mem_alloc_heap(size_t size, size_t align);
void *number_scope_mem_calloc_heap(size_t count, size_t size, size_t align);
void *number_scope_mem_realloc(void *ptr, size_t size, size_t align);
void number_scope_mem_free(void *ptr);
bool number_scope_mem_is_arena_ptr(const void *ptr);

typedef enum number_kind_t {
    NUMBER_INVALID,
    NUMBER_DOUBLE,
    NUMBER_QFLOAT,
    NUMBER_QCOMPLEX,
    NUMBER_MPZ,
    NUMBER_MPQ,
    NUMBER_MPFR,
    NUMBER_CDOUBLE,
    NUMBER_COMPLEX
} number_kind_t;

typedef enum number_math_family_t {
    NUMBER_MATH_INVALID,
    NUMBER_MATH_QREAL,
    NUMBER_MATH_QCOMPLEX,
    NUMBER_MATH_MPFR,
    NUMBER_MATH_COMPLEX
} number_math_family_t;

/*
 * Representation vtable: payload lifetime, conversion, predicates and the
 * primitive same-backend operations needed throughout the number module.
 * Higher mathematical functions are dispatched by number_maths.c according
 * to math_family; they are deliberately not repeated in every backend vtable.
 */
typedef struct number_vtable_t {
    number_kind_t kind;
    number_math_family_t math_family;
    bool exact;
    bool is_complex;
    void (*destroy_payload)(number_t *number);
    void *(*scope_payload)(const number_t *number);
    void (*destroy_scope_payload)(void *payload);
    number_t *(*clone)(const number_t *number);
    number_t *(*const_prec)(const number_t *number, size_t precision_bits);
    string_t *(*to_text)(const number_t *number);
    bool (*is_immortal)(const number_t *number);
    bool (*immortal_id)(const number_t *number, number_const_id_t *id_out);
    bool (*is_real)(const number_t *number);
    bool (*is_zero)(const number_t *number);
    bool (*is_one)(const number_t *number);
    bool (*is_finite)(const number_t *number);
    bool (*is_nan)(const number_t *number);
    bool (*is_inf)(const number_t *number);
    bool (*eq_same)(const number_t *a, const number_t *b);
    bool (*eq_same_tol)(const number_t *a, const number_t *b);
    int (*cmp_same)(const number_t *a, const number_t *b);
    number_t (*const_like)(const number_t *like, number_const_id_t id);
    number_t (*imag_const_like)(const number_t *like, number_const_id_t id);
    string_t *(*format_inexact_text)(const number_t *number, bool scientific, int precision);
    int (*set_precision)(number_t *number, size_t precision_bits);
    size_t (*get_precision)(const number_t *number);
    size_t (*get_effective_precision)(const number_t *number);
    long (*get_exponent2)(const number_t *number);
    double (*to_double)(const number_t *number);
    qfloat_t (*to_qfloat)(const number_t *number);
    bool (*is_integer)(const number_t *number);
    size_t (*get_mantissa_bits)(const number_t *number);
    bool (*get_mantissa_u64)(const number_t *number, uint64_t *out);
    int (*sign)(const number_t *number);
    number_t *(*neg)(const number_t *number);
    number_t *(*abs_value)(const number_t *number);
    number_t *(*inv)(const number_t *number);
    number_t *(*conj_value)(const number_t *number);
    number_t *(*real_part)(const number_t *number);
    number_t *(*imag_part)(const number_t *number);
    number_t *(*arg_value)(const number_t *number);
    number_t *(*pow_int)(const number_t *number, int exponent);
    number_t *(*mul_pow10_value)(const number_t *number, int exponent10);
    number_t *(*ldexp_value)(const number_t *number, int exponent2);
    number_t *(*floor_value)(const number_t *number);
    int (*sincos_value)(const number_t *number, number_t *sin_out, number_t *cos_out);
    int (*sinhcosh_value)(const number_t *number, number_t *sinh_out, number_t *cosh_out);
    number_t *(*add_same)(const number_t *a, const number_t *b);
    number_t *(*sub_same)(const number_t *a, const number_t *b);
    number_t *(*mul_same)(const number_t *a, const number_t *b);
    number_t *(*div_same)(const number_t *a, const number_t *b);
    number_t *(*exp_same)(const number_t *number);
    number_t *(*log_same)(const number_t *number);
    number_t *(*sqrt_same)(const number_t *number);
} number_vtable_t;

typedef number_t *(*number_coerce_fn)(const number_t *number);

typedef struct number_cdouble_t {
    double _Complex value;
    int constant_id;
    bool immortal;
} number_cdouble_t;

typedef struct {
    number_kind_t kind;
    union {
        double d;
        number_cdouble_t cd;
        qfloat_t qf;
        qcomplex_t qc;
        struct _number_mpz_t *mpz;
        struct _number_mpq_t *mpq;
        struct _number_mpfr_t *mpfr;
        struct _complex_t *cx;
    } value;
} number_private_t;

typedef struct _complex_t {
    int constant_id;
    size_t precision_bits;
    number_t real;
    number_t imag;
    bool mpc_cache_valid;
    mpc_t mpc_cache;
} complex_t;

typedef enum number_binary_op_t { NUMBER_OP_ADD, NUMBER_OP_SUB, NUMBER_OP_MUL, NUMBER_OP_DIV } number_binary_op_t;

typedef struct _number_mpfr_t {
    number_const_id_t constant_id;
    bool immortal;
    bool initialised;
    mpfr_t value;
} number_mpfr_t;

typedef struct _number_mpz_t {
    number_const_id_t constant_id;
    bool immortal;
    bool initialised;
    mpz_t value;
} number_mpz_t;

typedef struct _number_mpq_t {
    number_const_id_t constant_id;
    bool immortal;
    bool initialised;
    mpq_t value;
} number_mpq_t;

extern const number_math_family_t number_math_family_binary_table[][NUMBER_MATH_COMPLEX + 1];
extern const number_kind_t number_math_family_target_kind_table[];
/* Concrete backend vtables (defined in number_vtables.c). */
extern const number_vtable_t number_double_vt;
extern const number_vtable_t number_qfloat_vt;
extern const number_vtable_t number_qcomplex_vt;
extern const number_vtable_t number_mpz_vt;
extern const number_vtable_t number_mpq_vt;
extern const number_vtable_t number_mpfr_vt;
extern const number_vtable_t number_cdouble_vt;
extern const number_vtable_t number_complex_vt;
/* Backend vtable registry (defined in number_vtables.c). */
extern const number_vtable_t *const number_dispatch[];
extern const size_t number_dispatch_count;
extern size_t number_default_precision_bits;

number_t *number_wrap_complex(complex_t *value);
number_t number_take_mpz(number_mpz_t *value);
number_t number_take_mpq(number_mpq_t *value);
number_t number_take_mpfr(number_mpfr_t *value);
number_t number_complex_component_from_number(const number_t *value, size_t precision_bits);
complex_t *number_complex_create(number_t real, number_t imag);
complex_t *number_complex_clone(const complex_t *value);
complex_t *number_complex_create_from_qcomplex(qcomplex_t value, size_t precision_bits);
const number_t *number_complex_real_ref(const complex_t *value);
const number_t *number_complex_imag_ref(const complex_t *value);
number_const_id_t number_complex_const_id(const complex_t *value);

static inline number_private_t *number_impl(number_t *number) { return (number_private_t *)number; }

static inline const number_private_t *number_impl_const(const number_t *number) {
    return (const number_private_t *)number;
}

static inline bool number_is_valid_value(const number_t *number) {
    return number != NULL && number_impl_const(number)->kind != NUMBER_INVALID;
}

const number_vtable_t *number_vt(const number_t *number);
number_kind_t number_kind_value(const number_t *number);
bool number_same_kind_value(const number_t *a, const number_t *b);
number_math_family_t number_math_family_value(const number_t *number);
number_math_family_t number_math_family_binary(number_math_family_t a, number_math_family_t b);

static inline number_kind_t number_math_family_target_kind(number_math_family_t family) {
    return (unsigned)family <= NUMBER_MATH_COMPLEX ? number_math_family_target_kind_table[family] : NUMBER_INVALID;
}

qfloat_t number_value_to_qfloat(const number_t *number);
qcomplex_t number_value_to_qcomplex(const number_t *number);

number_t number_invalid(void);
bool num_is_immortal(number_t number);
bool num_get_small_rational(number_t number, long *numerator, long *denominator);

/**
 * @brief Extract an exact numerator and positive square root of a non-unit square denominator.
 * @param number Borrowed rational value; approximate values are rejected.
 * @param numerator Initialised owned output; unchanged on failure.
 * @param denominator_root Distinct initialised owned output; unchanged on failure.
 * @return True on success. Release both successful outputs with num_destroy.
 */
bool num_extract_square_denominator(number_t number, number_t *numerator, number_t *denominator_root);

bool num_is_inexact_real_backend(number_t number);
number_t num_as_inexact_real_prec(number_t number, size_t precision_bits);
number_t number_take(number_t *boxed_number);
number_t *number_box_value(number_t value);
char *number_strdup(const char *text);
char *number_cstring_from_text(string_t *text);
void number_box_free(number_t *number);
void number_assign(number_t *dst, number_t value);
void number_scope_register_value(const number_t *number);

/**
 * @brief Transfer an internal temporary into the caller's numerical scope.
 * @param scope Address of the active internal scope; cleared after leaving it.
 * @param result Result owned by that scope, to preserve across its cleanup.
 * @return Live result registered with the caller's scope, or explicitly owned if none is active.
 */
number_t number_scope_result(num_scope_t **scope, number_t result);

int number_scope_unregister_value(const number_t *number);
num_scope_t *number_scope_suspend(void);
void number_scope_resume(num_scope_t *scope);
void num_scope_resume_cleanup(num_scope_t **scope);

#define NUM_SCOPE_SUSPEND(name)                                                                                        \
    __attribute__((cleanup(num_scope_resume_cleanup))) num_scope_t *(name) = number_scope_suspend()

string_t *number_format_double_text(const number_t *number, bool scientific, int precision);
string_t *number_format_cdouble_text(const number_t *number, bool scientific, int precision);
string_t *number_format_qfloat_text(const number_t *number, bool scientific, int precision);
string_t *number_format_qcomplex_text(const number_t *number, bool scientific, int precision);
string_t *number_format_mpfr_text(const number_t *number, bool scientific, int precision);
string_t *number_format_complex_text(const number_t *number, bool scientific, int precision);
char *number_to_cstring(const number_t *number);
number_t *number_wrap_double(double value);
number_t *number_wrap_cdouble(double _Complex value);
number_t *number_wrap_qfloat(qfloat_t value);
number_t *number_wrap_qcomplex(qcomplex_t value);
number_t *number_wrap_mpz(number_mpz_t *value);
number_t *number_wrap_mpq(number_mpq_t *value);
number_t *number_wrap_mpfr(number_mpfr_t *value);
number_t *number_wrap_complex_parts(number_t real, number_t imag);
number_mpz_t *number_mpz_from_const_id(number_const_id_t id);
number_mpz_t *number_mpz_new(void);
number_mpz_t *number_mpz_from_long(long value);
number_mpz_t *number_mpz_from_mpz(mpz_srcptr value);
number_mpz_t *number_mpz_from_text(const string_t *text);
number_mpz_t *number_mpz_clone(const number_mpz_t *value);
void number_mpz_free(number_mpz_t *value);
int number_mpz_ensure(const number_mpz_t *value);
mpz_srcptr number_mpz_value(const number_mpz_t *value);
string_t *number_mpz_to_text(const number_mpz_t *value);
bool number_mpz_get_long(const number_mpz_t *value, long *out);
size_t number_mpz_bit_length(const number_mpz_t *value);
int number_mpz_cmp(const number_mpz_t *a, const number_mpz_t *b);
bool number_mpz_is_zero(const number_mpz_t *value);
bool number_mpz_is_negative(const number_mpz_t *value);
number_mpq_t *number_mpq_from_const_id(number_const_id_t id);
number_mpq_t *number_mpq_new(void);
number_mpq_t *number_mpq_from_frac_long(long numerator, long denominator);
number_mpq_t *number_mpq_from_mpq(mpq_srcptr value);
number_mpq_t *number_mpq_from_text(const string_t *text);
number_mpq_t *number_mpq_clone(const number_mpq_t *value);
void number_mpq_free(number_mpq_t *value);
int number_mpq_ensure(const number_mpq_t *value);
mpq_srcptr number_mpq_value(const number_mpq_t *value);
string_t *number_mpq_to_text(const number_mpq_t *value);
bool number_mpq_get_small_fraction(const number_mpq_t *value, long *numerator, long *denominator);
int number_mpq_cmp(const number_mpq_t *a, const number_mpq_t *b);
bool number_mpq_is_zero(const number_mpq_t *value);
bool number_mpq_is_one(const number_mpq_t *value);
bool number_mpq_is_integer(const number_mpq_t *value);
bool number_mpq_is_negative(const number_mpq_t *value);
number_mpfr_t *number_mpfr_new_prec(size_t precision_bits);
number_mpfr_t *number_mpfr_from_const_id(number_const_id_t id, size_t precision_bits);
number_mpfr_t *number_mpfr_from_double(double value, size_t precision_bits);
number_mpfr_t *number_mpfr_from_qfloat(qfloat_t value, size_t precision_bits);
number_mpfr_t *number_mpfr_from_mpfr(mpfr_srcptr value, size_t precision_bits);
number_mpfr_t *number_mpfr_from_text(const string_t *text, size_t precision_bits);
number_mpfr_t *number_mpfr_clone(const number_mpfr_t *value);
void number_mpfr_free(number_mpfr_t *value);
int number_mpfr_ensure(const number_mpfr_t *value, size_t precision_bits);
mpfr_srcptr number_mpfr_value(const number_mpfr_t *value);
const complex_t *number_complex_value(const number_t *number);
int number_complex_get_mpc(mpc_t out, const complex_t *value, size_t precision_bits);
number_t *number_wrap_complex_mpc(mpc_srcptr value, size_t precision_bits);
void number_complex_clear_mpc_cache(complex_t *value);
int number_complex_set_mpc_cache_from_mpc(complex_t *value, mpc_srcptr source, size_t precision_bits);
bool number_eq_same_tol_with_precision(const number_t *a, const number_t *b, size_t precision_bits);
number_kind_t number_common_kind(const number_t *a, const number_t *b, number_binary_op_t op);
number_t *number_coerce(const number_t *number, number_kind_t target_kind);
bool number_matches_value(const number_t *reference, const number_t *target);
qfloat_t number_const_qfloat(number_const_id_t id);
qcomplex_t number_const_qcomplex(number_const_id_t id);
number_t number_const_mpfr_exact(number_const_id_t id);
bool number_const_has_double(number_const_id_t id);
double number_const_double_value(number_const_id_t id);
bool number_const_has_cdouble(number_const_id_t id);
double _Complex number_const_cdouble_value(number_const_id_t id);
bool number_const_has_ldexp(number_const_id_t id);
int number_const_ldexp_value(number_const_id_t id);
void number_constants_shutdown(void);

/** @brief Borrow an exact SI-valued physical constant's integer-ratio definition, or NULL. */
const char *number_physical_exact_source(number_const_id_t id);

/** @brief Clear lazy physical-constant storage at the normal numeric shutdown boundary. */
void number_physical_constants_shutdown(void);
number_t number_create_exact_mpfr_long_prec(long value, size_t precision_bits);
number_t number_create_exact_mpfr_dyadic_prec(long numerator, int exponent2, size_t precision_bits);
number_t number_const_return_like(const number_t *like, number_const_id_t id);
number_t number_neg_const_return_like(const number_t *like, number_const_id_t id);
number_t number_imag_const_return_like(const number_t *like, number_const_id_t id);
number_t number_const_like(const number_t *like, number_const_id_t id);

/* Widest cylindrical input precision, treating exact complex components like exact real inputs. */
size_t number_cylindrical_precision(number_t order, number_t argument);

/**
 * @brief Evaluate principal Bessel J through the shared guarded complex cylinder series.
 * @param[in] order Borrowed real or complex order, retained at its effective precision.
 * @param[in] argument Borrowed argument; the negative real axis uses its upper-bank value.
 * @return Owned value released with num_destroy(), or NaN outside the supported range or on unresolved cancellation.
 * Inputs remain caller-owned. Magnitudes above 1000 or work precision above 65536 bits are unsupported.
 */
number_t number_bessel_J_complex(number_t order, number_t argument);

/**
 * @brief Evaluate complex erf using a guarded entire series, or MPFR on the real axis.
 * @param[out] out Initialised complex destination whose precision selects accuracy; may alias input.
 * @param[in] input Borrowed argument, valid throughout the call.
 * @param[in] rounding Final output rounding mode.
 * @return Zero after resolved evaluation, or -1 with NaN for an unsupported range or exhausted guard.
 * All MPC storage remains caller-owned; complex magnitudes above 140 or work precision above 65536 are unsupported.
 */
int number_mpc_erf(mpc_ptr out, mpc_srcptr input, mpc_rnd_t rounding);

/**
 * @brief Evaluate complex erfc with extra precision to preserve complementary cancellation.
 * @param[out] out Initialised destination selecting accuracy; may alias input.
 * @param[in] input Borrowed argument; real values use MPFR's complementary implementation directly.
 * @param[in] rounding Final output rounding mode.
 * @return Zero on resolved evaluation, or -1 with NaN outside the supported complex range or numerical guards.
 * Inputs remain caller-owned. The same work and magnitude limits apply as for number_mpc_erf().
 */
int number_mpc_erfc(mpc_ptr out, mpc_srcptr input, mpc_rnd_t rounding);

/**
 * @brief Evaluate the Gaussian cumulative distribution as erfc(-input/sqrt(2))/2.
 * @param[out] out Initialised destination selecting accuracy; may alias input.
 * @param[in] input Caller-owned argument, valid throughout this call.
 * @param[in] rounding Final output rounding mode.
 * @return Zero on resolved evaluation; -1 with NaN on an exhausted error-function guard.
 * The caller retains and clears both MPC operands. No shared mutable state is used.
 */
int number_mpc_normal_cdf(mpc_ptr out, mpc_srcptr input, mpc_rnd_t rounding);

/**
 * @brief Evaluate the analytic Gaussian log density with guarded complex arithmetic.
 * @param[out] out Initialised destination selecting accuracy; may alias input.
 * @param[in] input Caller-owned argument, valid throughout this call.
 * @param[in] rounding Final output rounding mode.
 * @return Zero; non-finite arithmetic follows MPC's NaN/infinity conventions.
 * The caller retains and clears both operands. No shared mutable state is used.
 */
int number_mpc_normal_logpdf(mpc_ptr out, mpc_srcptr input, mpc_rnd_t rounding);

/**
 * @brief Evaluate the Gaussian density by exponentiating its guarded analytic log density.
 * @param[out] out Initialised destination selecting accuracy; may alias input.
 * @param[in] input Caller-owned argument, valid throughout this call.
 * @param[in] rounding Final output rounding mode.
 * @return Zero; non-finite arithmetic follows MPC's NaN/infinity conventions.
 * The caller retains and clears both operands. No shared mutable state is used.
 */
int number_mpc_normal_pdf(mpc_ptr out, mpc_srcptr input, mpc_rnd_t rounding);

/**
 * @brief Refine inverse erf and verify the defining residual at the destination's precision.
 * @param[out] out Initialised destination, allowed to alias input.
 * @param[in] input Borrowed value; real endpoints -1 and 1 return their signed infinite limits.
 * @param[in] rounding Final output rounding mode.
 * @return Zero after convergence or an exact endpoint; -1 with NaN on unresolved iteration or unsupported work size.
 * No storage escapes and the caller retains every operand. Approximate seeds never limit final precision.
 */
int number_mpc_erfinv(mpc_ptr out, mpc_srcptr input, mpc_rnd_t rounding);

/**
 * @brief Refine inverse erfc directly and check its complementary residual without narrowing precision.
 * @param[out] out Initialised destination, allowed to alias input.
 * @param[in] input Borrowed value; real endpoints 0 and 2 return their signed infinite limits.
 * @param[in] rounding Final output rounding mode.
 * @return Zero after convergence or an endpoint; -1 with NaN on unsupported work or unresolved iteration.
 * All MPC operands remain caller-owned and valid throughout the call.
 */
int number_mpc_erfcinv(mpc_ptr out, mpc_srcptr input, mpc_rnd_t rounding);

/**
 * @brief Replace a real MPFR operand with its precision-preserving inverse erf value.
 * @param[in,out] value Caller-owned initialised operand in [-1,1], overwritten only at its own precision.
 * @return Zero on convergence or an infinite endpoint; -1 with NaN outside the real domain or on failure.
 * The caller remains responsible for clearing value; no shared state or retained pointers are used.
 */
int number_mpfr_erfinv_mut(mpfr_t value);

/**
 * @brief Replace a real MPFR operand with its precision-preserving inverse erfc value.
 * @param[in,out] value Caller-owned initialised operand in [0,2], retaining its precision.
 * @return Zero on convergence or an infinite endpoint; -1 with NaN outside the domain or on unresolved iteration.
 * The caller remains responsible for clearing value; no shared mutable storage is used.
 */
int number_mpfr_erfcinv_mut(mpfr_t value);

/**
 * @brief Evaluate inverse erf for a binary64 argument using a checked libc refinement.
 * @param value Real argument in [-1,1]; endpoints return signed infinity.
 * @return Inverse value, or NaN outside the real domain. No allocation or shared state is retained.
 */
double number_double_erfinv(double value);

/**
 * @brief Evaluate inverse erfc for binary64, preserving subnormal tail probabilities.
 * @param value Real probability in [0,2]; endpoints return signed infinity.
 * @return Inverse value, or NaN outside the real domain. Extreme tails use local MPFR temporaries,
 * which are released before return; independent calls are thread-safe.
 */
double number_double_erfcinv(double value);

/**
 * @brief Evaluate principal complex Lommel s with a guarded power series.
 * @param[out] out Initialised destination, selecting target precision; may alias any input.
 * @param[in] mu Borrowed exponent parameter.
 * @param[in] nu Borrowed Bessel order parameter.
 * @param[in] argument Borrowed argument; the principal power uses the upper bank on the negative real axis.
 * @param[in] rounding Final output rounding mode.
 * @return Zero after convergence; -1 with NaN on poles, unsupported arguments or exhausted accuracy guards.
 * The caller retains and releases all MPC operands; no shared mutable storage is used.
 */
int number_mpc_lommel_S(mpc_ptr out, mpc_srcptr mu, mpc_srcptr nu, mpc_srcptr argument, mpc_rnd_t rounding);

/**
 * @brief Evaluate the argument derivative of principal complex Lommel s using the same convergent series.
 * @param[out] out Initialised destination, selecting target precision; may alias an input.
 * @param[in] mu Borrowed exponent parameter. @param[in] nu Borrowed Bessel order parameter.
 * @param[in] argument Borrowed principal-branch argument.
 * @param[in] rounding Final output rounding mode.
 * @return Zero after convergence; -1 with NaN at undefined limits, poles or exhausted numerical guards.
 * All operands remain caller-owned and valid for the duration of the call.
 */
int number_mpc_lommel_S_derivative(mpc_ptr out, mpc_srcptr mu, mpc_srcptr nu, mpc_srcptr argument,
                                   mpc_rnd_t rounding);

/**
 * @brief Evaluate the convergent Appell F1 series using a total-degree coefficient recurrence.
 * @param[out] out Initialised real destination; its precision selects the requested accuracy and it may alias an input.
 * @param[in] a Borrowed first upper parameter. @param[in] b1 Borrowed first variable's upper parameter.
 * @param[in] b2 Borrowed second variable's upper parameter. @param[in] c Borrowed lower parameter.
 * @param[in] x Borrowed first argument with absolute value below 0.95.
 * @param[in] y Borrowed second argument with absolute value below 0.95.
 * @return Zero after convergence, or -1 with NaN on an unsupported domain, singular denominator or exhausted guard.
 * Evaluation stays in real arithmetic. Shifted parameters preserve every input bit and permitted integer shift;
 * coefficient recurrences are periodically refreshed and directed exponent checks can only delay acceptance.
 * No ownership transfers; callers initialise and clear their own MPFR values. No shared mutable state is used.
 */
int number_mpfr_appell_F1(mpfr_ptr out, mpfr_srcptr a, mpfr_srcptr b1, mpfr_srcptr b2, mpfr_srcptr c,
                          mpfr_srcptr x, mpfr_srcptr y);

/**
 * @brief Evaluate complex Appell F1 at the destination's precision without nested summation.
 * @param[out] out Initialised complex destination; may alias any input.
 * @param[in] a Borrowed first upper parameter. @param[in] b1 Borrowed first variable's upper parameter.
 * @param[in] b2 Borrowed second variable's upper parameter. @param[in] c Borrowed lower parameter.
 * @param[in] x Borrowed first argument with modulus below 0.95.
 * @param[in] y Borrowed second argument with modulus below 0.95.
 * @param[in] rounding Final output rounding mode; guard calculations use nearest rounding.
 * @return Zero after convergence, or -1 with NaN on an unsupported domain, pole or exhausted numerical guard.
 * Compact dyadic parameters use exact quadratic ratio coefficients with a real denominator; other parameters
 * retain complex division. No input or output precision is narrowed by the compact representation.
 * All operands remain caller-owned and valid throughout the call; no internal storage escapes.
 */
int number_mpc_appell_F1(mpc_ptr out, mpc_srcptr a, mpc_srcptr b1, mpc_srcptr b2, mpc_srcptr c,
                         mpc_srcptr x, mpc_srcptr y, mpc_rnd_t rounding);

#endif
