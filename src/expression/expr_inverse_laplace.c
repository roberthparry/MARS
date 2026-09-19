#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_internal.h"
#define MARS_SHARED_NUMBER_INTERNAL_ACCESS
#include "internal/number_internal.h"

/* Finite algebraic rules for causal unilateral transforms; no numerical contour integration. */
enum { inverse_degree_limit = 16, inverse_coefficient_count = inverse_degree_limit + 1 };

typedef struct {
    expr_t *coefficient[inverse_coefficient_count];
    size_t degree;
} inverse_poly_t;

typedef struct {
    expr_t *root[inverse_degree_limit];
    size_t count;
} inverse_poles_t;

static bool inverse_uses(const expr_t *f, const expr_t *s)
{
    bool used = false;
    expr_t *variable = (expr_t *)s;
    return !expr_collect_var_usage(f, 1u, &variable, &used) || used;
}

static expr_t *inverse_clean(expr_t *owned)
{
    /* Remove paired unary signs before the general product beautifier sees symbolic coefficients. */
    if (owned && owned->ops == &ops_mul && owned->a->ops == &ops_neg && owned->b->ops == &ops_neg) {
        expr_t *positive = expr_mul(owned->a->a, owned->b->a);
        expr_free(owned);
        owned = positive;
    }
    expr_t *out = owned ? expr_beautify(owned) : NULL;
    expr_free(owned);
    return out;
}

/* Only literal algebraic values establish non-zero divisors; variable bindings are never sampled. */
static bool inverse_nonzero(const expr_t *f)
{
    number_t value = NUM_ZERO;
    bool valid = expr_match_const_value(f, &value) && num_is_finite(value) && !num_is_zero(value);
    num_destroy(&value);
    return valid;
}

static void inverse_poly_clear(inverse_poly_t *p)
{
    for (size_t i = 0; i < inverse_coefficient_count; ++i) {
        expr_free(p->coefficient[i]);
        p->coefficient[i] = NULL;
    }
    p->degree = 0;
}

static void inverse_poly_trim(inverse_poly_t *p)
{
    while (p->degree && expr_const_is_zero(p->coefficient[p->degree])) {
        expr_free(p->coefficient[p->degree]);
        p->coefficient[p->degree--] = NULL;
    }
}

static bool inverse_poly_product(const inverse_poly_t *a, const inverse_poly_t *b, inverse_poly_t *out)
{
    if (a->degree + b->degree > inverse_degree_limit)
        return false;
    out->degree = a->degree + b->degree;
    for (size_t k = 0; k <= out->degree; ++k) {
        expr_t *sum = expr_const_zero();
        for (size_t i = 0; i <= a->degree && i <= k; ++i) {
            if (k - i > b->degree)
                continue;
            expr_t *term = inverse_clean(expr_mul(a->coefficient[i], b->coefficient[k - i]));
            expr_t *next = inverse_clean(expr_add(sum, term));
            expr_free(term);
            expr_free(sum);
            sum = next;
        }
        out->coefficient[k] = sum;
    }
    inverse_poly_trim(out);
    return true;
}

static bool inverse_integer_power(const expr_t *f, const expr_t **base, long *power)
{
    number_t value = NUM_ZERO;
    const expr_t *exponent = NULL;
    bool matched = expr_match_pow_const(f, base, &value);
    if (!matched && expr_match_pow_expr(f, base, &exponent))
        matched = expr_match_const_value(exponent, &value);
    long denominator = 0;
    bool valid = matched && num_get_small_rational(value, power, &denominator) && denominator == 1 &&
                 *power >= -inverse_degree_limit && *power <= inverse_degree_limit;
    num_destroy(&value);
    return valid;
}

/* Existing degree-four collectors evaluate coefficients; this collector preserves symbolic parameters. */
static bool inverse_poly_collect(const expr_t *f, const expr_t *s, inverse_poly_t *out)
{
    if (!f)
        return false;
    if (!inverse_uses(f, s)) {
        out->coefficient[0] = expr_clone(f);
        return true;
    }
    if (expr_is_var(f) && f->var_id == s->var_id) {
        out->coefficient[0] = expr_const_zero();
        out->coefficient[1] = expr_const_one();
        out->degree = 1;
        return true;
    }
    inverse_poly_t a = {0}, b = {0};
    const expr_t *base = NULL;
    long power = 0;
    bool ok = false;
    if (f->ops == &ops_neg && inverse_poly_collect(f->a, s, &a)) {
        out->degree = a.degree;
        for (size_t i = 0; i <= a.degree; ++i)
            out->coefficient[i] = inverse_clean(expr_neg(a.coefficient[i]));
        ok = true;
    } else if (f->ops == &ops_div && !inverse_uses(f->b, s) && inverse_nonzero(f->b) &&
               inverse_poly_collect(f->a, s, &a)) {
        out->degree = a.degree;
        for (size_t i = 0; i <= a.degree; ++i)
            out->coefficient[i] = inverse_clean(expr_div(a.coefficient[i], f->b));
        ok = true;
    } else if ((f->ops == &ops_add || f->ops == &ops_sub || f->ops == &ops_mul) &&
               inverse_poly_collect(f->a, s, &a) && inverse_poly_collect(f->b, s, &b)) {
        if (f->ops == &ops_mul)
            ok = inverse_poly_product(&a, &b, out);
        else {
            out->degree = a.degree > b.degree ? a.degree : b.degree;
            expr_t *zero = expr_const_zero();
            for (size_t i = 0; i <= out->degree; ++i) {
                const expr_t *left = i <= a.degree ? a.coefficient[i] : zero;
                const expr_t *right = i <= b.degree ? b.coefficient[i] : zero;
                out->coefficient[i] = inverse_clean(f->ops == &ops_add ? expr_add(left, right)
                                                                                    : expr_sub(left, right));
            }
            expr_free(zero);
            inverse_poly_trim(out);
            ok = true;
        }
    } else if (inverse_integer_power(f, &base, &power) && power >= 0 &&
               inverse_poly_collect(base, s, &a)) {
        out->coefficient[0] = expr_const_one();
        ok = true;
        for (long i = 0; ok && i < power; ++i) {
            inverse_poly_t next = {0};
            ok = inverse_poly_product(out, &a, &next);
            inverse_poly_clear(out);
            *out = next;
        }
    }
    inverse_poly_clear(&a);
    inverse_poly_clear(&b);
    if (!ok)
        inverse_poly_clear(out);
    return ok;
}

static expr_t *inverse_power(const expr_t *f, size_t order)
{
    number_t exponent = num_create_from_long((long)order);
    expr_t *out = expr_pow(f, &exponent);
    num_destroy(&exponent);
    return out;
}

/* Taylor coefficients at a pole, calculated by the triangular Horner recurrence. */
static void inverse_poly_shift(const inverse_poly_t *p, const expr_t *root, inverse_poly_t *out)
{
    out->degree = p->degree;
    for (size_t i = 0; i <= p->degree; ++i)
        out->coefficient[i] = expr_const_zero();
    for (size_t k = p->degree + 1; k-- > 0;) {
        for (size_t j = p->degree; j > 0; --j) {
            expr_t *product = expr_mul(root, out->coefficient[j]);
            expr_t *next = inverse_clean(expr_add(product, out->coefficient[j - 1]));
            expr_free(product);
            expr_free(out->coefficient[j]);
            out->coefficient[j] = next;
        }
        expr_t *product = expr_mul(root, out->coefficient[0]);
        expr_t *next = inverse_clean(expr_add(product, p->coefficient[k]));
        expr_free(product);
        expr_free(out->coefficient[0]);
        out->coefficient[0] = next;
    }
}

static bool inverse_collect_poles(const expr_t *f, const expr_t *s, inverse_poles_t *poles)
{
    if (!inverse_uses(f, s))
        return inverse_nonzero(f);
    if (f->ops == &ops_neg)
        return inverse_collect_poles(f->a, s, poles);
    if (f->ops == &ops_mul)
        return inverse_collect_poles(f->a, s, poles) && inverse_collect_poles(f->b, s, poles);
    const expr_t *base = NULL;
    long power = 0;
    if (inverse_integer_power(f, &base, &power) && power > 0) {
        for (long i = 0; i < power; ++i)
            if (!inverse_collect_poles(base, s, poles))
                return false;
        return true;
    }
    inverse_poly_t p = {0};
    bool ok = inverse_poly_collect(f, s, &p) && p.degree == 1 && inverse_nonzero(p.coefficient[1]) &&
              poles->count < inverse_degree_limit;
    if (ok) {
        expr_t *negative = expr_neg(p.coefficient[0]);
        poles->root[poles->count++] = inverse_clean(expr_div(negative, p.coefficient[1]));
        expr_free(negative);
    }
    inverse_poly_clear(&p);
    return ok;
}

static expr_t *inverse_pole_term(const expr_t *coefficient, const expr_t *root, size_t order, const expr_t *t)
{
    expr_t *argument = expr_mul(root, t);
    expr_t *exponential = expr_exp(argument);
    expr_t *power = inverse_power(t, order - 1);
    number_t factorial = num_clone(NUM_ONE);
    for (size_t i = 2; i < order; ++i) {
        number_t integer = num_create_from_long((long)i);
        number_t next = num_mul(factorial, integer);
        num_destroy(&integer);
        num_destroy(&factorial);
        factorial = next;
    }
    expr_t *divisor = expr_new_const(factorial);
    expr_t *scale = expr_div(coefficient, divisor);
    expr_t *product = expr_mul(power, exponential);
    expr_t *out = inverse_clean(expr_mul(scale, product));
    expr_free(product);
    expr_free(scale);
    expr_free(divisor);
    num_destroy(&factorial);
    expr_free(power);
    expr_free(exponential);
    expr_free(argument);
    return out;
}

/* At each distinct pole, divide Taylor series after removing its known multiplicity. */
static expr_t *inverse_partial_fractions(const inverse_poly_t *n, const inverse_poly_t *d,
                                         const inverse_poles_t *poles, const expr_t *t)
{
    bool visited[inverse_degree_limit] = {false};
    expr_t *out = expr_const_zero();
    for (size_t i = 0; i < poles->count; ++i) {
        if (visited[i])
            continue;
        size_t multiplicity = 0;
        /* The exhaustive comparison is bounded by sixteen poles, including repetitions. */
        for (size_t j = i; j < poles->count; ++j) {
            expr_t *difference = inverse_clean(expr_sub(poles->root[i], poles->root[j]));
            bool equal = expr_const_is_zero(difference);
            bool distinct = inverse_nonzero(difference);
            expr_free(difference);
            if (!equal && !distinct) {
                expr_free(out);
                return NULL;
            }
            if (equal) {
                visited[j] = true;
                ++multiplicity;
            }
        }
        inverse_poly_t numerator = {0}, denominator = {0};
        inverse_poly_shift(n, poles->root[i], &numerator);
        inverse_poly_shift(d, poles->root[i], &denominator);
        expr_t *series[inverse_degree_limit] = {0};
        bool valid = multiplicity <= d->degree && inverse_nonzero(denominator.coefficient[multiplicity]);
        for (size_t k = 0; valid && k < multiplicity; ++k) {
            expr_t *remainder = k <= n->degree ? expr_clone(numerator.coefficient[k]) : expr_const_zero();
            for (size_t j = 1; j <= k && multiplicity + j <= d->degree; ++j) {
                expr_t *term = expr_mul(denominator.coefficient[multiplicity + j], series[k - j]);
                expr_t *next = inverse_clean(expr_sub(remainder, term));
                expr_free(term);
                expr_free(remainder);
                remainder = next;
            }
            series[k] = inverse_clean(expr_div(remainder, denominator.coefficient[multiplicity]));
            expr_free(remainder);
            expr_t *term = inverse_pole_term(series[k], poles->root[i], multiplicity - k, t);
            expr_t *next = inverse_clean(expr_add(out, term));
            expr_free(term);
            expr_free(out);
            out = next;
        }
        for (size_t k = 0; k < multiplicity; ++k)
            expr_free(series[k]);
        inverse_poly_clear(&numerator);
        inverse_poly_clear(&denominator);
        if (!valid) {
            expr_free(out);
            return NULL;
        }
    }
    return out;
}

/* Complete the square: (A(s-r)+B)/((s-r)^2+w^2). */
static expr_t *inverse_quadratic(const inverse_poly_t *n, const inverse_poly_t *d, const expr_t *t)
{
    if (d->degree != 2 || n->degree > 1 || !inverse_nonzero(d->coefficient[2]))
        return NULL;
    expr_t *two = expr_const_long(2);
    expr_t *twice = expr_mul(two, d->coefficient[2]);
    expr_t *negative = expr_neg(d->coefficient[1]);
    expr_t *root = inverse_clean(expr_div(negative, twice));
    expr_t *constant = expr_div(d->coefficient[0], d->coefficient[2]);
    expr_t *square = expr_mul(root, root);
    expr_t *frequency_squared = inverse_clean(expr_sub(constant, square));
    expr_t *a = n->degree ? inverse_clean(expr_div(n->coefficient[1], d->coefficient[2])) : expr_const_zero();
    expr_t *offset = expr_div(n->coefficient[0], d->coefficient[2]);
    expr_t *shift = expr_mul(a, root);
    expr_t *b = inverse_clean(expr_add(offset, shift));
    expr_t *out = NULL;
    if (expr_const_is_zero(frequency_squared)) {
        expr_t *first = inverse_pole_term(a, root, 1, t);
        expr_t *second = inverse_pole_term(b, root, 2, t);
        out = inverse_clean(expr_add(first, second));
        expr_free(second);
        expr_free(first);
    } else if (inverse_nonzero(frequency_squared)) {
        number_t value = NUM_ZERO;
        bool hyperbolic = expr_match_const_value(frequency_squared, &value) && num_is_real(value) &&
                          num_cmp(value, NUM_ZERO) < 0;
        num_destroy(&value);
        expr_t *radicand = hyperbolic ? expr_neg(frequency_squared) : expr_clone(frequency_squared);
        expr_t *frequency = inverse_clean(expr_sqrt(radicand));
        expr_t *argument = expr_mul(frequency, t);
        expr_t *cosine = hyperbolic ? expr_cosh(argument) : expr_cos(argument);
        expr_t *sine = hyperbolic ? expr_sinh(argument) : expr_sin(argument);
        expr_t *ratio = expr_div(b, frequency);
        expr_t *first = expr_mul(a, cosine);
        expr_t *second = expr_mul(ratio, sine);
        expr_t *sum = expr_add(first, second);
        expr_t *exponent = expr_mul(root, t);
        expr_t *exponential = expr_exp(exponent);
        out = inverse_clean(expr_mul(exponential, sum));
        expr_free(exponential);
        expr_free(exponent);
        expr_free(sum);
        expr_free(second);
        expr_free(first);
        expr_free(ratio);
        expr_free(sine);
        expr_free(cosine);
        expr_free(argument);
        expr_free(frequency);
        expr_free(radicand);
    }
    expr_free(b);
    expr_free(shift);
    expr_free(offset);
    expr_free(a);
    expr_free(frequency_squared);
    expr_free(square);
    expr_free(constant);
    expr_free(root);
    expr_free(negative);
    expr_free(twice);
    expr_free(two);
    return out;
}

static expr_t *inverse_rational(const expr_t *numerator, const expr_t *denominator,
                                const expr_t *s, const expr_t *t)
{
    inverse_poly_t n = {0}, d = {0};
    inverse_poles_t poles = {0};
    expr_t *out = NULL;
    if (inverse_poly_collect(numerator, s, &n) && inverse_poly_collect(denominator, s, &d) &&
        n.degree < d.degree) {
        out = inverse_quadratic(&n, &d, t);
        if (!out && inverse_collect_poles(denominator, s, &poles) && poles.count == d.degree)
            out = inverse_partial_fractions(&n, &d, &poles, t);
    }
    for (size_t i = 0; i < poles.count; ++i)
        expr_free(poles.root[i]);
    inverse_poly_clear(&d);
    inverse_poly_clear(&n);
    return out;
}

static expr_t *inverse_formal(const expr_t *f, const expr_t *metadata)
{
    expr_t *out = expr_alloc(&ops_inverse_laplace);
    out->a = expr_clone(f);
    out->b = expr_clone(metadata);
    return out;
}

/* Products and reciprocals can distribute a rational numerator across several tree nodes. */
static void inverse_fraction_parts(const expr_t *f, expr_t **numerator, expr_t **denominator)
{
    const expr_t *base = NULL;
    long power = 0;
    if (f->ops == &ops_mul || f->ops == &ops_div) {
        expr_t *an = NULL, *ad = NULL, *bn = NULL, *bd = NULL;
        inverse_fraction_parts(f->a, &an, &ad);
        inverse_fraction_parts(f->b, &bn, &bd);
        *numerator = expr_mul(an, f->ops == &ops_mul ? bn : bd);
        *denominator = expr_mul(ad, f->ops == &ops_mul ? bd : bn);
        expr_free(bd);
        expr_free(bn);
        expr_free(ad);
        expr_free(an);
    } else if (inverse_integer_power(f, &base, &power) && power < 0) {
        *numerator = expr_const_one();
        *denominator = inverse_power(base, (size_t)-power);
    } else {
        *numerator = expr_clone(f);
        *denominator = expr_const_one();
    }
}

/* Collect a linear logarithmic numerator without treating its parameters as numerical samples. */
static bool inverse_log_parts(const expr_t *f, const expr_t *s, expr_t **a, expr_t **b)
{
    if (!inverse_uses(f, s)) {
        *a = expr_const_zero();
        *b = expr_clone(f);
        return true;
    }
    if ((f->ops == &ops_log || f->ops == &ops_log10) && expr_is_var(f->a) && f->a->var_id == s->var_id) {
        *a = expr_const_one();
        *b = expr_const_zero();
        if (f->ops == &ops_log10) {
            expr_t *ten = expr_const_long(10);
            expr_t *scale = expr_log(ten);
            expr_t *coefficient = inverse_clean(expr_div(*a, scale));
            expr_free(*a);
            *a = coefficient;
            expr_free(scale);
            expr_free(ten);
        }
        return true;
    }
    expr_t *left_a = NULL, *left_b = NULL, *right_a = NULL, *right_b = NULL;
    bool matched = false;
    if ((f->ops == &ops_add || f->ops == &ops_sub) &&
        inverse_log_parts(f->a, s, &left_a, &left_b) && inverse_log_parts(f->b, s, &right_a, &right_b)) {
        *a = inverse_clean(f->ops == &ops_add ? expr_add(left_a, right_a) : expr_sub(left_a, right_a));
        *b = inverse_clean(f->ops == &ops_add ? expr_add(left_b, right_b) : expr_sub(left_b, right_b));
        matched = true;
    } else if (f->ops == &ops_neg && inverse_log_parts(f->a, s, &left_a, &left_b)) {
        *a = inverse_clean(expr_neg(left_a));
        *b = inverse_clean(expr_neg(left_b));
        matched = true;
    } else if (f->ops == &ops_mul) {
        const expr_t *scalar = !inverse_uses(f->a, s) ? f->a : f->b;
        const expr_t *operand = scalar == f->a ? f->b : f->a;
        if (!inverse_uses(scalar, s) && inverse_log_parts(operand, s, &left_a, &left_b)) {
            *a = inverse_clean(expr_mul(scalar, left_a));
            *b = inverse_clean(expr_mul(scalar, left_b));
            matched = true;
        }
    } else if (f->ops == &ops_div && !inverse_uses(f->b, s) && inverse_nonzero(f->b) &&
               inverse_log_parts(f->a, s, &left_a, &left_b)) {
        *a = inverse_clean(expr_div(left_a, f->b));
        *b = inverse_clean(expr_div(left_b, f->b));
        matched = true;
    }
    expr_free(right_b);
    expr_free(right_a);
    expr_free(left_b);
    expr_free(left_a);
    return matched;
}

/* L^-1{(A ln(s)+B)/s} = B-A(gamma+ln(t)), for positive time. */
static expr_t *inverse_logarithm(const expr_t *numerator, const expr_t *denominator,
                               const expr_t *s, const expr_t *t)
{
    inverse_poly_t d = {0};
    expr_t *a = NULL, *b = NULL, *out = NULL;
    if (inverse_poly_collect(denominator, s, &d) && d.degree == 1 &&
        expr_const_is_zero(d.coefficient[0]) && inverse_nonzero(d.coefficient[1]) &&
        inverse_log_parts(numerator, s, &a, &b) && !expr_const_is_zero(a)) {
        expr_t *gamma = expr_new_const(NUM_EULER_MASCHERONI);
        expr_t *logarithm = expr_log(t);
        expr_t *sum = expr_add(gamma, logarithm);
        expr_t *scaled = expr_mul(a, sum);
        expr_t *difference = expr_sub(b, scaled);
        out = inverse_clean(expr_div(difference, d.coefficient[1]));
        expr_free(difference);
        expr_free(scaled);
        expr_free(sum);
        expr_free(logarithm);
        expr_free(gamma);
    }
    expr_free(b);
    expr_free(a);
    inverse_poly_clear(&d);
    return out;
}

static expr_t *inverse_rule(const expr_t *f, const expr_t *metadata)
{
    const expr_t *s = metadata->a;
    const expr_t *t = metadata->b->a;
    if (expr_const_is_zero(f))
        return expr_const_zero();
    expr_t *out = NULL;
    if (f->ops == &ops_div || f->ops == &ops_mul) {
        expr_t *numerator = NULL, *denominator = NULL;
        inverse_fraction_parts(f, &numerator, &denominator);
        out = inverse_logarithm(numerator, denominator, s, t);
        if (!out)
            out = inverse_rational(numerator, denominator, s, t);
        expr_free(denominator);
        expr_free(numerator);
    }
    const expr_t *base = NULL;
    long power = 0;
    if (!out && inverse_integer_power(f, &base, &power) && power < 0) {
        expr_t *one = expr_const_one();
        expr_t *denominator = inverse_power(base, (size_t)-power);
        out = inverse_rational(one, denominator, s, t);
        expr_free(denominator);
        expr_free(one);
    }
    if (out)
        return out;
    if (f->ops == &ops_add || f->ops == &ops_sub) {
        expr_t *a = inverse_rule(f->a, metadata);
        expr_t *b = inverse_rule(f->b, metadata);
        if (a || b) {
            if (!a)
                a = inverse_formal(f->a, metadata);
            if (!b)
                b = inverse_formal(f->b, metadata);
            out = f->ops == &ops_add ? expr_add(a, b) : expr_sub(a, b);
        }
        expr_free(a);
        expr_free(b);
    } else if (f->ops == &ops_neg) {
        expr_t *a = inverse_rule(f->a, metadata);
        if (a)
            out = expr_neg(a);
        expr_free(a);
    } else if (f->ops == &ops_mul) {
        const expr_t *scalar = !inverse_uses(f->a, s) ? f->a : f->b;
        const expr_t *operand = scalar == f->a ? f->b : f->a;
        if (!inverse_uses(scalar, s)) {
            expr_t *a = inverse_rule(operand, metadata);
            if (a)
                out = expr_mul(scalar, a);
            expr_free(a);
        }
    } else if (f->ops == &ops_div && !inverse_uses(f->b, s) && inverse_nonzero(f->b)) {
        expr_t *a = inverse_rule(f->a, metadata);
        if (a)
            out = expr_div(a, f->b);
        expr_free(a);
    }
    return out;
}

/* Return a native causal formula, or NULL when no supported rule applies. */
expr_t *expr_inverse_laplace_result(const expr_t *transform)
{
    if (!transform || transform->ops != &ops_inverse_laplace || !transform->a || !transform->b ||
        !expr_is_var(transform->b->a) || !transform->b->b || !transform->b->b->a)
        return NULL;
    return inverse_clean(inverse_rule(transform->a, transform->b));
}

static number_t inverse_laplace_eval(expr_t *transform)
{
    expr_t *formula = expr_inverse_laplace_result(transform);
    number_t out = formula ? expr_eval(formula) : num_clone(NUM_NAN);
    expr_free(formula);
    return out;
}

static expr_t *inverse_laplace_simplify(const expr_t *expr, expr_t *a, expr_t *b)
{
    expr_t *out = expr_inverse_laplace_result(expr);
    if (!out)
        return expr_simplify_passthrough(expr, a, b);
    expr_free(a);
    expr_free(b);
    return out;
}

static expr_t *inverse_laplace_deriv(expr_t *transform)
{
    expr_t *wrt = (expr_t *)expr_current_wrt_internal();
    return wrt ? expr_new_formal_derivative(transform, 1u, &wrt) : NULL;
}

const expr_ops_t ops_inverse_laplace = {
    .eval = inverse_laplace_eval, .deriv = inverse_laplace_deriv, .reverse = expr_reverse_not_differentiable,
    .kind = EXPR_KIND_INVERSE_LAPLACE, .arity = EXPR_OP_BINARY, .diff_kind = EXPR_DIFF_SMOOTH,
    .expression_name = "ℒ⁻¹", .function_name = "InverseLaplace", .TeX_name = "\\mathcal{L}^{-1}",
    .simplify = inverse_laplace_simplify,
};
