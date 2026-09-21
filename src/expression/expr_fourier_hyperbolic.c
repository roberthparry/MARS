#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_internal.h"

typedef struct {
    bool supported;
    bool reciprocal;
    bool singular;
} hyperbolic_family_t;

static const hyperbolic_family_t hyperbolic_families[EXPR_KIND_COUNT] = {
    [EXPR_KIND_SINH]   = {true, false, true},
    [EXPR_KIND_COSH]   = {true, false, false},
    [EXPR_KIND_SECH]   = {true, true,  false},
    [EXPR_KIND_COSECH] = {true, true,  true},
};

/* Match one hyperbolic power without composing principal powers across a branch cut.
 * In particular, cosech(u)^n and sinh(u)^(-n) have different phases for non-integral n. */
bool expr_fourier_hyperbolic_parts(const expr_t *f, const expr_t **argument, expr_t **power,
                                  expr_t **branch_power, bool *singular)
{
    *argument = NULL;
    *power = NULL;
    *branch_power = NULL;
    if (!f)
        return false;
    if (f->ops == &ops_div && expr_const_is_one(f->a)) {
        if (!expr_fourier_hyperbolic_parts(f->b, argument, power, branch_power, singular))
            return false;
        expr_t *negative = expr_neg(*power);
        expr_free(*power);
        *power = negative;
        if (*branch_power) {
            negative = expr_neg(*branch_power);
            expr_free(*branch_power);
            *branch_power = negative;
        }
        return true;
    }
    const expr_t *base = f;
    expr_t *exponent = NULL;
    if (f->ops == &ops_pow) {
        base = f->a;
        exponent = expr_clone(f->b);
    } else if (f->ops == &ops_pow_d) {
        base = f->a;
        exponent = expr_new_const(f->c);
    } else if (f->ops == &ops_sqrt) {
        base = f->a;
        exponent = expr_new_const(NUM_HALF);
    } else {
        exponent = expr_const_one();
    }
    bool absolute = base->ops == &ops_abs;
    if (absolute)
        base = base->a;
    const hyperbolic_family_t family = hyperbolic_families[base->ops->kind];
    if (!family.supported) {
        expr_free(exponent);
        return false;
    }
    *argument = base->a;
    *singular = family.singular;
    *branch_power = family.singular && !absolute ? expr_clone(exponent) : NULL;
    *power = family.reciprocal ? expr_neg(exponent) : expr_clone(exponent);
    expr_free(exponent);
    return true;
}

/* Describe ordinary convergence and branch choices separately from unsupported distributional extensions. */
const char *expr_fourier_hyperbolic_note(const expr_t *power, bool singular,
                                        const expr_t *rate, const expr_t *offset)
{
    number_t n = expr_eval((expr_t *)power), a = expr_eval((expr_t *)rate), b = expr_eval((expr_t *)offset);
    number_t real = num_real_part(n);
    bool real_affine = num_is_real(a) && num_is_finite(a) && !num_is_zero(a) &&
                       num_is_real(b) && num_is_finite(b);
    bool possible_affine = (num_is_nan(a) || (num_is_real(a) && num_is_finite(a) && !num_is_zero(a))) &&
                           (num_is_nan(b) || (num_is_real(b) && num_is_finite(b)));
    const char *note = NULL;
    if (possible_affine && num_is_finite(n) && num_gt(real, NUM_ZERO)) {
        note = real_affine
                   ? "No ordinary Fourier transform: this real-affine hyperbolic power grows exponentially at infinity "
                     "and does not define a tempered distribution."
                   : "For real non-zero scale and real offset, this hyperbolic power grows exponentially: "
                     "neither its ordinary Fourier transform nor a tempered-distribution transform exists.";
    } else if (possible_affine && singular && num_is_finite(n) && num_le(real, NUM_NEG_ONE)) {
        note = real_affine
                   ? "No ordinary Fourier transform: the hyperbolic power has a non-integrable singularity at the "
                     "real zero of sinh. A principal-value or finite-part prescription must be specified separately."
                   : "For real non-zero scale and real offset, the hyperbolic power has a non-integrable singularity. "
                     "A principal-value or finite-part prescription must be specified separately.";
    } else if (possible_affine && !num_is_zero(n)) {
        note = singular
                   ? "The beta-function formula requires real non-zero scale, real offset and -1 < Re(n) < 0 for "
                     "the effective sinh exponent. Principal powers are used on the negative half-line. "
                     "Zero exponent gives a Dirac impulse; distributional boundary cases are not inferred."
                   : "The beta-function formula requires real non-zero scale, real offset and Re(n) < 0 for "
                     "the effective cosh exponent. Zero exponent gives a Dirac impulse; distributional boundary "
                     "cases are not inferred.";
    }
    num_destroy(&real);
    num_destroy(&b);
    num_destroy(&a);
    num_destroy(&n);
    return note;
}
