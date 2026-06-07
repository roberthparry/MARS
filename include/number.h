#ifndef NUMBER_H
#define NUMBER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdint.h>
#include "qcomplex.h"
#include "qfloat.h"

/**
 * @file number.h
 * @brief Generic numeric value cluster with by-value public handles.
 *
 * `number_t` is a fixed-size public value type that can represent several
 * numeric backends behind one uniform interface. The current implementation
 * may store exact, fixed-precision, or multiprecision numeric representations
 * internally while keeping one stable public API.
 *
 * Construction is explicit for typed inputs. `num_create_from_double()` and
 * `num_create_from_cdouble()` keep double precision; `num_create_from_qfloat()`
 * and `num_create_from_qcomplex()` keep double-double precision. String
 * parsing chooses the most suitable representation by syntax:
 *
 * - integer text -> an internal MPZ-backed exact integer
 * - `a/b` fraction text -> an internal MPQ-backed exact rational
 * - decimal / scientific real text -> an internal MPFR-backed real
 * - complex text -> an internal exact or multiprecision complex representation
 *
 * Unless a precision is specified explicitly, multiprecision construction uses
 * the current default precision (`1024` bits initially). Setter functions for
 * inexact values preserve the destination precision policy, while exact
 * setters such as `num_set_long()` and `num_set_frac()` replace the destination
 * with exact storage.
 *
 * Ownership model:
 *
 * - functions returning `number_t` return a live value
 * - callers should call `num_destroy(&value)` when finished with any such value
 * - passing `number_t` by value performs a shallow copy only
 * - shallow copies may alias the same heap-backed payload internally
 * - use `num_clone()` when an independent live copy is required
 * - `char *` strings returned by `num_to_string()` must be released with
 *   `free()`
 *
 * For immortal-backed values such as `NUM_PI`, `NUM_E`, and `NUM_HALF`,
 * `num_destroy(&value)` is safe and may be a no-op.
 *
 * Pure arithmetic helpers never mutate their by-value inputs. Functions that
 * write to an existing `number_t` take a `number_t *` destination.
 */

/**
 * @brief Fixed-size public storage for a generic numeric value.
 *
 * Callers may copy, return, and pass `number_t` by value. The storage words
 * are public only so the type has a known size; callers must treat them as
 * opaque and must not inspect or modify them directly.
 *
 * Some `number_t` values may own heap-backed payload internally, so
 * `num_destroy()` must be used to release resources when a live value is no
 * longer needed.
 */
#define NUMBER_STORAGE_WORDS 5u

typedef struct _number_t {
    uint64_t storage[NUMBER_STORAGE_WORDS];
} number_t;

typedef enum number_primality_t {
    NUMBER_PRIMALITY_UNKNOWN = -1,
    NUMBER_PRIMALITY_COMPOSITE = 0,
    NUMBER_PRIMALITY_PRIME = 1
} number_primality_t;

typedef struct number_factor_t {
    number_t prime;
    unsigned long exponent;
} number_factor_t;

typedef struct number_factors_t {
    size_t count;
    number_factor_t *items;
} number_factors_t;

#ifndef MARS_NUMBER_IMPLEMENTATION
typedef union number_inline_qfloat_bits_t {
    qfloat_t value;
    uint64_t words[2];
} number_inline_qfloat_bits_t;

typedef union number_inline_qcomplex_bits_t {
    qcomplex_t value;
    uint64_t words[4];
} number_inline_qcomplex_bits_t;

static inline uint32_t number_inline_kind(number_t number)
{
    return (uint32_t)number.storage[0];
}

static inline qfloat_t number_inline_qfloat(number_t number)
{
    number_inline_qfloat_bits_t bits;

    bits.words[0] = number.storage[1];
    bits.words[1] = number.storage[2];
    return bits.value;
}

static inline qcomplex_t number_inline_qcomplex(number_t number)
{
    number_inline_qcomplex_bits_t bits;

    bits.words[0] = number.storage[1];
    bits.words[1] = number.storage[2];
    bits.words[2] = number.storage[3];
    bits.words[3] = number.storage[4];
    return bits.value;
}

static inline qfloat_t number_inline_qcomplex_real(number_t number)
{
    number_inline_qfloat_bits_t bits;

    bits.words[0] = number.storage[1];
    bits.words[1] = number.storage[2];
    return bits.value;
}

static inline qfloat_t number_inline_qcomplex_imag(number_t number)
{
    number_inline_qfloat_bits_t bits;

    bits.words[0] = number.storage[3];
    bits.words[1] = number.storage[4];
    return bits.value;
}

static inline number_t number_inline_make_qfloat(qfloat_t value)
{
    number_t number;
    number_inline_qfloat_bits_t bits;

    bits.value = value;
    number.storage[0] = 2u;
    number.storage[1] = bits.words[0];
    number.storage[2] = bits.words[1];
    return number;
}

static inline number_t number_inline_make_qcomplex_parts(qfloat_t real,
                                                         qfloat_t imag)
{
    number_t number;
    number_inline_qfloat_bits_t real_bits;
    number_inline_qfloat_bits_t imag_bits;

    real_bits.value = real;
    imag_bits.value = imag;
    number.storage[0] = 3u;
    number.storage[1] = real_bits.words[0];
    number.storage[2] = real_bits.words[1];
    number.storage[3] = imag_bits.words[0];
    number.storage[4] = imag_bits.words[1];
    return number;
}

static inline number_t number_inline_make_qcomplex(qcomplex_t value)
{
    return number_inline_make_qcomplex_parts(value.re, value.im);
}
#endif

/**
 * @brief Opaque temporary lifetime scope for heap-backed `number_t` results.
 *
 * While a scope is active, newly created heap-backed temporary results are
 * reclaimed automatically by `num_scope_leave()`, unless they are explicitly
 * detached first. This is meant for temporary-heavy internal code paths that
 * would otherwise create and destroy many short-lived `number_t` values.
 *
 * The best-performing pattern is usually:
 * - keep short-lived intermediates inside the active scope
 * - keep any rolling or returned value as an ordinary owned `number_t`
 * - detach only when a scoped result truly needs to survive the scope
 *
 * The type is intentionally opaque; callers should only use the
 * `num_scope_*` APIs or the `NUM_SCOPE(name)` helper macro.
 */
typedef struct num_scope_t num_scope_t;

/** @name Lifecycle
 * Constructors and pure transforms in this section return a live `number_t`
 * by value. Call `num_destroy(&value)` when the returned value is no longer
 * needed.
 *
 * Constructors taking `const ... *` inputs clone or otherwise capture the
 * referenced value into the returned `number_t`. They do not make the caller
 * responsible for extending the source object's lifetime.
 *
 * Returned values are owning values. Even though `number_t` itself is passed
 * by value, the returned object may still manage heap-backed internal state.
 * @{
 */
number_t num_new                   (void);
number_t num_new_with_prec_bits    (size_t precision_bits);

number_t num_create_from_long      (long value);
number_t num_create_from_frac      (long numerator, long denominator);
number_t num_create_from_double    (double value);
number_t num_create_from_cdouble   (double _Complex value);
number_t num_create_from_qfloat    (qfloat_t value);
number_t num_create_from_qcomplex  (qcomplex_t value);

/**
 * @brief Parses text into the most suitable numeric representation.
 *
 * Accepted forms include:
 *
 * - integers such as `42` or `-7`
 * - rationals of the form `a/b`, such as `5/6`
 * - decimal or scientific real values such as `32.123` or `1e-23`
 * - complex values of the forms `a + bi`, `a - bi`, or `bi`
 *
 * Examples accepted by the current parser:
 *
 * - `1 + i`
 * - `1 - i`
 * - `1e-23 + 2.3e12i`
 * - `1e-23 + (2.3e12)i`
 * - `3i`
 * - `1/2 - 3/2i`
 *
 * Examples not accepted by the current parser:
 *
 * - `1/i`
 *
 * Returns a live `number_t` by value. On parse failure, returns an invalid
 * `number_t`.
 */
number_t num_create_from_string(const char *text);

/**
 * @brief Return the canonical text spelling for a recognised shared constant.
 *
 * The returned pointer is process-lifetime static storage owned by the
 * library. It is intended for serialisation layers that need a compact,
 * stable spelling for named constants. Mathematical spellings such as `π`
 * are preferred.
 *
 * @param value  Number to inspect.
 * @return       Canonical spelling, or @c NULL if @p value is not a
 *               recognised shared constant.
 */
const char *num_constant_name(number_t value);

/**
 * @brief Parse a recognised shared constant spelling.
 *
 * Accepts canonical mathematical spellings such as `π`, `γ`, `√2`, and
 * typeable Greek aliases such as `@pi`, `@gamma`, and `@phi`. The `@` prefix
 * is reserved for Greek aliases; non-Greek constants use ordinary names such
 * as `e`, `i`, `inf`, `ln2`, and `sqrt2`.
 *
 * @param text  Constant spelling to parse.
 * @param out   Receives a live `number_t` on success. Must not be @c NULL.
 * @return      @c true on success, @c false when @p text is not a recognised
 *              constant spelling.
 */
bool num_constant_value(const char *text, number_t *out);

/** @name Shared constants
 * Process-lifetime immutable `number_t` values.
 *
 * These constants may be copied by value:
 *
 * ```c
 * number_t x = NUM_PI;
 * ```
 *
 * Calling `num_destroy()` on a local copy is always safe. Callers must not
 * pass the global constant objects themselves by address to mutating APIs.
 * @{
 */

extern const number_t NUM_ZERO;          /**< exact 0 */
extern const number_t NUM_ONE;           /**< exact 1 */
extern const number_t NUM_NEG_ONE;       /**< exact -1 */
extern const number_t NUM_HALF;          /**< exact ½ */
extern const number_t NUM_ONE_AND_HALF;  /**< exact ³/₂ */
extern const number_t NUM_ONE_THIRD;     /**< exact ⅓ */
extern const number_t NUM_QUARTER;       /**< exact ¼ */
extern const number_t NUM_ONE_SIXTH;     /**< exact ⅙ */
extern const number_t NUM_ONE_EIGHTH;    /**< exact ⅛ */
extern const number_t NUM_ONE_TENTH;     /**< exact ¹/₁₀ */
extern const number_t NUM_TWO;           /**< exact 2 */
extern const number_t NUM_TEN;           /**< exact 10 */
extern const number_t NUM_NAN;           /**< NaN */
extern const number_t NUM_INF;           /**< +∞ */
extern const number_t NUM_NINF;          /**< -∞ */

/** @name Circular constants
 * Constants built from `pi`.
 * @{
 */
extern const number_t NUM_PI;            /**< π */
extern const number_t NUM_2PI;           /**< 2π */
extern const number_t NUM_PI_2;          /**< π/2 */
extern const number_t NUM_NEG_PI_2;      /**< -π/2 */
extern const number_t NUM_PI_4;          /**< π/4 */
extern const number_t NUM_3PI_4;         /**< 3π/4 */
extern const number_t NUM_PI_6;          /**< π/6 */
extern const number_t NUM_PI_3;          /**< π/3 */
extern const number_t NUM_2_PI;          /**< 2/π */
/** @} */

/** @name Exponential and logarithmic constants
 * Constants built from `e` and natural logarithms.
 * @{
 */
extern const number_t NUM_E;                 /**< e */
extern const number_t NUM_INV_E;             /**< 1/e */
extern const number_t NUM_NEG_INV_E;         /**< -1/e */
extern const number_t NUM_LN2;               /**< ln(2) */
extern const number_t NUM_LN10;              /**< ln(10) */
extern const number_t NUM_INVLN2;            /**< 1/ln(2) */
extern const number_t NUM_EULER_MASCHERONI;  /**< Euler-Mascheroni γ */
extern const number_t NUM_PHI;               /**< golden ratio φ */
/** @} */

/** @name Root and Gaussian-normalisation constants
 * Algebraic roots and common constants from Gaussian / gamma-function formulae.
 * @{
 */
extern const number_t NUM_SQRT_HALF;         /**< √(1/2) */
extern const number_t NUM_SQRT2;             /**< √2 */
extern const number_t NUM_SQRT3;             /**< √3 */
extern const number_t NUM_SQRT2_OVER_TWO;    /**< √2/2 */
extern const number_t NUM_SQRT3_OVER_TWO;    /**< √3/2 */
extern const number_t NUM_SQRT_2PI;          /**< √(2π) */
extern const number_t NUM_SQRT_PI;           /**< √π */
extern const number_t NUM_SQRT_PI_OVER_TWO;  /**< √(π/2) */
extern const number_t NUM_SQRT1ONPI;         /**< 1/√π */
extern const number_t NUM_2_SQRTPI;          /**< 2/√π */
extern const number_t NUM_NEG_TWO_OVER_SQRT_PI; /**< -2/√π */
extern const number_t NUM_INV_SQRT_2PI;      /**< 1/√(2π) */
extern const number_t NUM_LOG_SQRT_2PI;      /**< log(√(2π)) */
extern const number_t NUM_LN_2PI;            /**< ln(2π) */
extern const number_t NUM_PI_SQUARED;        /**< π² */
extern const number_t NUM_2PI_CUBED;         /**< (2π)³ */
/** @} */

extern const number_t NUM_I;             /**< i */
extern const number_t NUM_NEG_I;         /**< -i */
/** @} */

/**
 * @brief Returns an owning `number_t` representing `10^exponent10`.
 *
 * Call `num_destroy(&value)` when finished with the returned value.
 */
number_t num_pow10(int exponent10);

/**
 * @brief Returns an independent live copy of a `number_t`.
 *
 * Use this when a duplicated value must later be destroyed independently from
 * the original.
 */
number_t num_clone(const number_t number);

/**
 * @brief Materialises a named constant using the current default precision policy.
 *
 * Exact constants such as `NUM_ZERO` or `NUM_HALF` are copied as exact values.
 * Inexact multiprecision constants such as `NUM_PI` or `NUM_E` are realised at
 * the library's current default working precision.
 */
number_t num_const(number_t constant);

/**
 * @brief Materialises a named constant at an explicit binary precision.
 *
 * This is the direct way to request a multiprecision constant such as `NUM_PI`
 * or `NUM_I` at a specific number of bits. Exact constants remain exact;
 * inexact multiprecision constants are rebuilt or copied at `precision_bits`.
 * Passing `0` uses the current default working precision instead.
 */
number_t num_const_prec(number_t constant, size_t precision_bits);

/**
 * @brief Materialises a named constant at an explicit decimal-significance target.
 *
 * This behaves like `num_const_prec()`, but the requested precision is given in
 * significant decimal digits instead of bits. Exact constants remain exact;
 * inexact multiprecision constants are realised at a binary precision large
 * enough to represent at least `significant_digits` decimal digits.
 */
number_t num_const_prec_digits(number_t constant, size_t significant_digits);

/**
 * @brief Releases any owned payload and resets a number to the invalid state.
 *
 * Call this exactly once for each live owning `number_t` value when it is no
 * longer needed. It is safe to call on an already-cleared value.
 *
 * Because `number_t` uses shallow by-value copies, callers must avoid destroying
 * multiple aliases of the same underlying payload unless they first made an
 * independent copy with `num_clone()`.
 */
#ifndef MARS_NUMBER_IMPLEMENTATION
void num_destroy_slow(number_t *number);
static inline void num_destroy(number_t *number)
{
    uint32_t kind;

    if (!number)
        return;
    kind = (uint32_t)number->storage[0];
    if (kind <= 3u)
        return;
    num_destroy_slow(number);
}
#else
void num_destroy_slow(number_t *number);
void num_destroy(number_t *number);
#endif
/** @} */

/** @name Temporary scopes
 * Implicit scopes for temporary heap-backed numeric results.
 *
 * Outside any active scope, constructors and arithmetic helpers behave as
 * usual and callers should release returned live values with `num_destroy()`.
 *
 * Inside an active scope, newly created heap-backed results are reclaimed
 * automatically when the scope is left. Such temporaries must not outlive the
 * scope unless they are detached from it first with `num_scope_detach()`.
 * Detaching a plain heap-backed scoped value transfers that existing value out
 * of the scope. Detaching an arena-backed scoped value materialises an
 * ordinary owning copy so it can survive scope teardown safely.
 *
 * In hot paths, scopes work best for short-lived intermediates. Rolling state
 * that is updated repeatedly should usually remain an ordinary owned value and
 * be destroyed explicitly when replaced, rather than being repeatedly detached.
 *
 * On compilers that support `__attribute__((cleanup(...)))`, `NUM_SCOPE(name)`
 * declares a local scope handle, enters it immediately, and leaves it
 * automatically on block exit.
 *
 * Scopes are nestable and must be left in last-in, first-out order.
 * @{
 */
num_scope_t *num_scope_enter(void);
void num_scope_leave(num_scope_t **scope);
bool num_scope_is_active(void);
number_t num_scope_detach(number_t value);

#define NUM_SCOPE(name) __attribute__((cleanup(num_scope_leave))) num_scope_t *name = num_scope_enter()
/** @} */

/** @name Precision, setup, and conversion
 * Setter functions mutate an existing destination in place.
 *
 * Precision setters affect only backends where precision is meaningful.
 * Exact integer and rational representations may treat such requests as
 * metadata no-ops.
 *
 * All setters in this section return:
 * - `0` on success
 * - `-1` on invalid input or unsupported conversion
 *
 * Getter functions in this section return the requested value directly.
 * For precision getters, a result of `0` means that precision is not
 * meaningful for the current backend.
 * @{
 */

/** Default multiprecision working precision, expressed in bits or decimal digits. */
int    num_set_default_prec_bits   (size_t precision_bits);
size_t num_get_default_prec_bits   (void);
int    num_set_default_prec_digits (size_t significant_digits);
size_t num_get_default_prec_digits (void);

/** Per-value working precision, expressed in bits or decimal digits. */
int    num_set_prec_bits   (number_t *number, size_t precision_bits);
size_t num_get_prec_bits   (const number_t number);

/**
 * @brief Returns the effective working precision in bits.
 *
 * Unlike `num_get_prec_bits()`, exact backends return the precision they should
 * participate with when mixed into inexact numeric work.
 */
size_t num_get_effective_prec_bits(const number_t number);
int    num_set_prec_digits (number_t *number, size_t significant_digits);
size_t num_get_prec_digits (const number_t number);

/** In-place value replacement helpers. */
int num_set_long      (number_t *number, long value);
int num_set_frac      (number_t *number, long numerator, long denominator);
int num_set_double    (number_t *number, double value);
int num_set_cdouble   (number_t *number, double _Complex value);
int num_set_qfloat    (number_t *number, qfloat_t value);
int num_set_qcomplex  (number_t *number, qcomplex_t value);

/**
 * @brief Replaces a number by parsing the same literal forms as `num_create_from_string()`.
 *
 * Returns `0` on success and `-1` on invalid input or parse failure.
 */
int num_set_from_string(number_t *number, const char *text);

/**
 * @brief Formats a numeric value as a newly allocated string.
 *
 * Returns a newly allocated string that must be released with `free()`.
 */
char    *num_to_string (const number_t number);
/** @brief Returns the numeric value converted to `double`. */
double   num_to_double (const number_t number);
/** @brief Returns the numeric value converted to `qfloat_t`. */
qfloat_t num_to_qfloat (const number_t number);
/** @} */

/** @name Formatted output
 * `number_t` values participate in the library's formatted output helpers.
 *
 * The `%n` conversion prints a pretty human-readable representation.
 * The `%N` conversion prefers scientific notation for floating-point and
 * complex backends and otherwise falls back to the ordinary exact form.
 *
 * Width and precision follow the library's floating formatting rules when the
 * active backend is floating-point or complex. For exact integer and rational
 * backends, floating-style precision specifiers are ignored.
 *
 * Return values follow the corresponding `printf` / `snprintf` conventions.
 * @{
 */

 /** @brief `%n` / `%N` formatting helpers. `number_t` arguments are passed by value. */
int num_vsprintf(char *out, size_t out_size, const char *fmt, va_list ap);  /**< caller-provided buffer, `va_list` form */
int num_sprintf (char *out, size_t out_size, const char *fmt, ...);         /**< caller-provided buffer, variadic form */
int num_printf  (const char *fmt, ...);                                     /**< prints to standard output */
/** @} */

/** @name Queries and comparisons
 * Query helpers inspect the current stored value.
 *
 * Equality is supported across mixed backends through the generic promotion
 * rules. Ordering (`num_lt`, `num_le`, `num_gt`, `num_ge`, `num_cmp`) is only
 * meaningful for real values; if either operand is non-real, the relational
 * predicates return `false` and `num_cmp` returns `0`.
 *
 * These functions do not take ownership of their inputs and never mutate the
 * passed values.
 *
 * Predicate functions return `true` or `false`. Scalar getters return the
 * requested property directly. Comparison functions return either a boolean
 * relation result or, for `num_cmp(...)`, an integer ordering result.
 * @{
 */
bool   num_is_exact        (const number_t number);
bool   num_is_real         (const number_t number);
bool   num_is_integer      (const number_t number);
bool   num_is_finite       (const number_t number);
bool   num_is_nan          (const number_t number);
bool   num_is_inf          (const number_t number);
bool   num_is_zero         (const number_t number);
bool   num_is_one          (const number_t number);

short  num_get_sign         (const number_t number);
long   num_get_exponent2    (const number_t number);
size_t num_get_mantissa_bits(const number_t number);
bool   num_get_mantissa_u64 (const number_t number, uint64_t *out);
int    num_sign             (const number_t number);

bool   num_eq               (const number_t a, const number_t b);
bool   num_lt               (const number_t a, const number_t b);
bool   num_le               (const number_t a, const number_t b);
bool   num_gt               (const number_t a, const number_t b);
bool   num_ge               (const number_t a, const number_t b);
int    num_cmp              (const number_t a, const number_t b);
/** @} */

/** @name Core arithmetic
 * Every function in this section returns an owning `number_t` by value. Call
 * `num_destroy(&result)` when finished with it.
 *
 * Binary operations may promote operands internally to a common backend before
 * evaluating the operation.
 *
 * Input values are passed by value but are treated as borrowed read-only
 * handles. Implementations must not clear or otherwise destroy them.
 * @{
 */
number_t num_neg          (const number_t number);
number_t num_abs          (const number_t number);
number_t num_inv          (const number_t number);
number_t num_conj         (const number_t number);
number_t num_real_part    (const number_t number);
number_t num_imag_part    (const number_t number);
number_t num_arg          (const number_t number);
#ifndef MARS_NUMBER_IMPLEMENTATION
number_t num_add_slow     (const number_t a, const number_t b);
number_t num_sub_slow     (const number_t a, const number_t b);
number_t num_mul_slow     (const number_t a, const number_t b);
number_t num_div_slow     (const number_t a, const number_t b);

static inline number_t num_add(const number_t a, const number_t b)
{
    uint32_t kind = number_inline_kind(a);

    if (kind == number_inline_kind(b)) {
        if (kind == 2u)
            return number_inline_make_qfloat(qf_add(number_inline_qfloat(a),
                                                    number_inline_qfloat(b)));
        if (kind == 3u)
            return number_inline_make_qcomplex_parts(
                qf_add(number_inline_qcomplex_real(a), number_inline_qcomplex_real(b)),
                qf_add(number_inline_qcomplex_imag(a), number_inline_qcomplex_imag(b)));
    }
    return num_add_slow(a, b);
}

static inline number_t num_sub(const number_t a, const number_t b)
{
    uint32_t kind = number_inline_kind(a);

    if (kind == number_inline_kind(b)) {
        if (kind == 2u)
            return number_inline_make_qfloat(qf_sub(number_inline_qfloat(a),
                                                    number_inline_qfloat(b)));
        if (kind == 3u)
            return number_inline_make_qcomplex_parts(
                qf_sub(number_inline_qcomplex_real(a), number_inline_qcomplex_real(b)),
                qf_sub(number_inline_qcomplex_imag(a), number_inline_qcomplex_imag(b)));
    }
    return num_sub_slow(a, b);
}

static inline number_t num_mul(const number_t a, const number_t b)
{
    uint32_t kind = number_inline_kind(a);

    if (kind == number_inline_kind(b)) {
        if (kind == 2u)
            return number_inline_make_qfloat(qf_mul(number_inline_qfloat(a),
                                                    number_inline_qfloat(b)));
        if (kind == 3u) {
            qfloat_t ar = number_inline_qcomplex_real(a);
            qfloat_t ai = number_inline_qcomplex_imag(a);
            qfloat_t br = number_inline_qcomplex_real(b);
            qfloat_t bi = number_inline_qcomplex_imag(b);

            return number_inline_make_qcomplex_parts(
                qf_sub(qf_mul(ar, br), qf_mul(ai, bi)),
                qf_add(qf_mul(ar, bi), qf_mul(ai, br)));
        }
    }
    return num_mul_slow(a, b);
}

static inline number_t num_div(const number_t a, const number_t b)
{
    uint32_t kind = number_inline_kind(a);

    if (kind == number_inline_kind(b)) {
        if (kind == 2u) {
            qfloat_t av = number_inline_qfloat(a);
            qfloat_t bv = number_inline_qfloat(b);

            return number_inline_make_qfloat(qf_div(av, bv));
        }
        if (kind == 3u) {
            qfloat_t ar = number_inline_qcomplex_real(a);
            qfloat_t ai = number_inline_qcomplex_imag(a);
            qfloat_t br = number_inline_qcomplex_real(b);
            qfloat_t bi = number_inline_qcomplex_imag(b);
            qfloat_t denom = qf_add(qf_mul(br, br), qf_mul(bi, bi));

            return number_inline_make_qcomplex_parts(
                qf_div(qf_add(qf_mul(ar, br), qf_mul(ai, bi)), denom),
                qf_div(qf_sub(qf_mul(ai, br), qf_mul(ar, bi)), denom));
        }
    }
    return num_div_slow(a, b);
}
#else
number_t num_add          (const number_t a, const number_t b);
number_t num_sub          (const number_t a, const number_t b);
number_t num_mul          (const number_t a, const number_t b);
number_t num_div          (const number_t a, const number_t b);
number_t num_add_slow     (const number_t a, const number_t b);
number_t num_sub_slow     (const number_t a, const number_t b);
number_t num_mul_slow     (const number_t a, const number_t b);
number_t num_div_slow     (const number_t a, const number_t b);
#endif
number_t num_add_long     (const number_t number, long value);
number_t num_mul_long     (const number_t number, long value);
number_t num_pow          (const number_t base, const number_t exponent);
number_t num_pow_int      (const number_t base, int exponent);
number_t num_ldexp        (const number_t number, int exponent2);
/** @} */

/** @name Exact integer and number-theory helpers
 * These helpers accept exact integer `number_t` inputs. Value-returning
 * helpers return `NUM_NAN` when given unsupported inputs such as fractions,
 * inexact values, negative factorial arguments, or zero moduli.
 * @{
 */
number_t num_factorial(unsigned long n);
number_t num_fibonacci(unsigned long n);
/**
 * @brief Returns the integer partition count `p(n)` exactly.
 *
 * `p(n)` counts the ways to write non-negative integer `n` as a sum of
 * positive integers, ignoring order. Negative exact integers return `0`;
 * unsupported inputs, including fractions and inexact values, return
 * `NUM_NAN`.
 */
number_t num_partition(const number_t number);
number_t num_isqrt(const number_t number);

number_t num_gcd(const number_t a, const number_t b);
number_t num_lcm(const number_t a, const number_t b);
number_t num_mod(const number_t number, const number_t modulus);
int      num_divmod(const number_t number,
                    const number_t divisor,
                    number_t *quotient,
                    number_t *remainder);
int      num_gcdext(const number_t a,
                    const number_t b,
                    number_t *gcd_out,
                    number_t *x_out,
                    number_t *y_out);

number_t num_powmod(const number_t base,
                    const number_t exponent,
                    const number_t modulus);
number_t num_modinv(const number_t number, const number_t modulus);

bool               num_is_prime(const number_t number);
number_primality_t num_prove_prime(const number_t number);
number_t           num_next_prime(const number_t number);
number_t           num_prev_prime(const number_t number);
number_factors_t  *num_factors(const number_t number);
void               num_factors_free(number_factors_t *factors);

size_t   num_bit_length(const number_t number);
bool     num_test_bit(const number_t number, size_t bit_index);
number_t num_set_bit(const number_t number, size_t bit_index);
number_t num_clear_bit(const number_t number, size_t bit_index);
number_t num_bit_not(const number_t number);
number_t num_bit_and(const number_t a, const number_t b);
number_t num_bit_or(const number_t a, const number_t b);
number_t num_bit_xor(const number_t a, const number_t b);
number_t num_shl(const number_t number, long bits);
number_t num_shr(const number_t number, long bits);
/** @} */

/** @name Elementary functions
 * Functions returning `number_t` in this section return an owning value by
 * value. Call `num_destroy(&result)` when finished with it.
 *
 * If a backend does not support an operation natively, the implementation may
 * promote to a richer floating or complex backend first.
 * @{
 */
number_t num_exp      (const number_t number);
number_t num_log      (const number_t number);
number_t num_log10    (const number_t number);
number_t num_sqrt     (const number_t number);
number_t num_sqr      (const number_t number);
number_t num_floor    (const number_t number);
number_t num_ceil     (const number_t number);
number_t num_mul_pow10(const number_t number, int exponent10);
number_t num_hypot    (const number_t a, const number_t b);
/**
 * @brief Computes `sin(x)` and `cos(x)` into caller-provided outputs.
 *
 * `sin_out` and `cos_out` must point to fresh or already-cleared `number_t`
 * objects, such as values returned by `num_new()`. This function does not
 * release any existing live contents in those outputs; callers remain
 * responsible for clearing them first if needed.
 */
int      num_sincos   (const number_t x, number_t *sin_out, number_t *cos_out);
number_t num_sin      (const number_t number);
number_t num_cos      (const number_t number);
number_t num_tan      (const number_t number);
number_t num_sec      (const number_t number);
number_t num_cosec    (const number_t number);
number_t num_cot      (const number_t number);
number_t num_atan     (const number_t number);
number_t num_atan2    (const number_t y, const number_t x);
number_t num_asin     (const number_t number);
number_t num_acos     (const number_t number);
number_t num_asec     (const number_t number);
number_t num_acosec   (const number_t number);
number_t num_acot     (const number_t number);
number_t num_sinh     (const number_t number);
number_t num_cosh     (const number_t number);
/**
 * @brief Computes `sinh(x)` and `cosh(x)` into caller-provided outputs.
 *
 * `sinh_out` and `cosh_out` must point to fresh or already-cleared `number_t`
 * objects, such as values returned by `num_new()`. This function does not
 * release any existing live contents in those outputs; callers remain
 * responsible for destroying them first if needed.
 */
int      num_sinhcosh (const number_t x, number_t *sinh_out, number_t *cosh_out);
number_t num_tanh     (const number_t number);
number_t num_sech     (const number_t number);
number_t num_cosech   (const number_t number);
number_t num_coth     (const number_t number);
number_t num_asinh    (const number_t number);
number_t num_acosh    (const number_t number);
number_t num_atanh    (const number_t number);
number_t num_asech    (const number_t number);
number_t num_acosech  (const number_t number);
number_t num_acoth    (const number_t number);
/** @} */

/** @name Special functions
 * Every function in this section returns an owning `number_t` by value. Call
 * `num_destroy(&result)` when finished with it.
 *
 * Special functions typically promote to the richest available backend needed
 * to evaluate the requested function.
 * @{
 */
number_t num_gamma        (const number_t number);
number_t num_lgamma       (const number_t number);
number_t num_digamma      (const number_t number);
number_t num_trigamma     (const number_t number);
number_t num_tetragamma   (const number_t number);
number_t num_polygamma    (unsigned int order, const number_t number);
number_t num_gammainv     (const number_t number);
number_t num_erf          (const number_t number);
number_t num_erfc         (const number_t number);
number_t num_erfinv       (const number_t number);
number_t num_erfcinv      (const number_t number);
number_t num_lambert_w0   (const number_t number);
number_t num_lambert_wm1  (const number_t number);
number_t num_beta         (const number_t a, const number_t b);
number_t num_logbeta      (const number_t a, const number_t b);
number_t num_binomial     (const number_t n, const number_t k);
number_t num_beta_pdf     (const number_t x, const number_t a, const number_t b);
number_t num_logbeta_pdf  (const number_t x, const number_t a, const number_t b);
number_t num_normal_pdf   (const number_t number);
number_t num_normal_cdf   (const number_t number);
number_t num_normal_logpdf(const number_t number);
number_t num_productlog   (const number_t number);
number_t num_gammainc_lower(const number_t s, const number_t x);
number_t num_gammainc_upper(const number_t s, const number_t x);
number_t num_gammainc_P   (const number_t s, const number_t x);
number_t num_gammainc_Q   (const number_t s, const number_t x);
number_t num_ei           (const number_t number);
number_t num_e1           (const number_t number);
/** @} */

#endif
