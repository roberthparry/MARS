#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_internal.h"
#define MARS_SHARED_NUMBER_INTERNAL_ACCESS
#include "internal/number_internal.h"

/* Build the inverse-hyperbolic pairs with native Bessel and Struve functions. */
expr_t *expr_laplace_invhyper_formula(const expr_t *s, const expr_t *rate, bool cosine, bool negative)
{
    expr_t *z = expr_div(s, rate);
    expr_t *zero = expr_const_zero();
    expr_t *one = expr_const_one();
    expr_t *two = expr_const_long(2);
    expr_t *pi = expr_new_named_const(NUM_PI, "@pi");
    expr_t *struve = cosine ? expr_struve_l(zero, z) : expr_struve_h(zero, z);
    expr_t *denominator = expr_mul(two, s);
    expr_t *out;
    if (cosine) {
        /* acosh(0)=i*pi/2; its derivative below the branch point contributes -i(I0-L0).
         * A negative rate reverses that companion, but not the positive real K0 tail. */
        expr_t *bessel_i = expr_bessel_i(zero, z);
        expr_t *difference = expr_sub(bessel_i, struve);
        expr_t *companion = negative ? expr_add(one, difference) : expr_sub(one, difference);
        expr_t *imaginary = expr_new_const(NUM_I);
        expr_t *i_pi = expr_mul(imaginary, pi);
        expr_t *numerator = expr_mul(i_pi, companion);
        expr_t *branch = expr_div(numerator, denominator);
        expr_t *bessel_k = expr_bessel_k(zero, z);
        expr_t *tail = expr_div(bessel_k, s);
        out = expr_add(tail, branch);
        expr_free(tail);
        expr_free(bessel_k);
        expr_free(branch);
        expr_free(numerator);
        expr_free(i_pi);
        expr_free(imaginary);
        expr_free(companion);
        expr_free(difference);
        expr_free(bessel_i);
    } else {
        /* Both native functions support the complex right half-plane Re(s)>0. */
        expr_t *bessel_y = expr_bessel_y(zero, z);
        expr_t *difference = expr_sub(struve, bessel_y);
        expr_t *numerator = expr_mul(pi, difference);
        expr_t *positive = expr_div(numerator, denominator);
        out = negative ? expr_neg(positive) : expr_clone(positive);
        expr_free(positive);
        expr_free(numerator);
        expr_free(difference);
        expr_free(bessel_y);
    }
    expr_free(denominator);
    expr_free(struve);
    expr_free(pi);
    expr_free(two);
    expr_free(one);
    expr_free(zero);
    expr_free(z);
    return out;
}

/* Circular companions use MARS's real-cut values asin(x)=pi/2+i*acosh(x), x>1. */
expr_t *expr_laplace_invcircular_formula(const expr_t *s, const expr_t *rate, bool cosine, bool negative)
{
    expr_t *z = expr_div(s, rate);
    expr_t *zero = expr_const_zero();
    expr_t *one = expr_const_one();
    expr_t *two = expr_const_long(2);
    expr_t *pi = expr_new_named_const(NUM_PI, "@pi");
    expr_t *imaginary = expr_new_const(NUM_I);
    expr_t *bessel_i = expr_bessel_i(zero, z);
    expr_t *struve_l = expr_struve_l(zero, z);
    expr_t *difference = expr_sub(bessel_i, struve_l);
    expr_t *signed_difference = negative ? expr_neg(difference) : expr_clone(difference);
    expr_t *companion = cosine ? expr_sub(one, signed_difference) : expr_clone(signed_difference);
    expr_t *numerator = expr_mul(pi, companion);
    expr_t *denominator = expr_mul(two, s);
    expr_t *interior = expr_div(numerator, denominator);
    expr_t *bessel_k = expr_bessel_k(zero, z);
    expr_t *i_k = expr_mul(imaginary, bessel_k);
    expr_t *tail = expr_div(i_k, s);
    expr_t *out = cosine ? expr_sub(interior, tail) : expr_add(interior, tail);
    expr_free(tail);
    expr_free(i_k);
    expr_free(bessel_k);
    expr_free(interior);
    expr_free(denominator);
    expr_free(numerator);
    expr_free(companion);
    expr_free(signed_difference);
    expr_free(difference);
    expr_free(struve_l);
    expr_free(bessel_i);
    expr_free(imaginary);
    expr_free(pi);
    expr_free(two);
    expr_free(one);
    expr_free(zero);
    expr_free(z);
    return out;
}
