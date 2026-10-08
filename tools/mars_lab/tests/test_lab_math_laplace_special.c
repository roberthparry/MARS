/**
 * @file test_lab_math_laplace_special.c
 * @brief Special-function Laplace regressions with independent numerical references.
 *
 * Ports test_laplace_special.py. Adaptive Simpson quadrature, defining series,
 * incomplete-gamma recurrences and bounded Clausen Fourier sums preserve every
 * original parameter grid, domain assertion and convergence threshold.
 */
#include <math.h>

#include "test_lab_math_support.h"

typedef double complex (*lab_math_special_integrand_t)(double, void *);

static double complex lab_math_special_refine(lab_math_special_integrand_t function, void *context,
                                             double a, double b, double complex fa, double complex fm,
                                             double complex fb, double complex estimate, double error, unsigned depth)
{
    double mid = (a + b) / 2;
    double complex fl = function((a + mid) / 2, context), fr = function((mid + b) / 2, context);
    double complex lo = (mid - a) * (fa + 4 * fl + fm) / 6, hi = (b - mid) * (fm + 4 * fr + fb) / 6;
    double complex delta = lo + hi - estimate;
    if (cabs(delta) <= 15 * error)
        return lo + hi + delta / 15;
    if (!lab_math_check(depth != 0, "independent special-function quadrature converged"))
        return NAN;
    return lab_math_special_refine(function, context, a, mid, fa, fl, fm, lo, error / 2, depth - 1) +
           lab_math_special_refine(function, context, mid, b, fm, fr, fb, hi, error / 2, depth - 1);
}

static double complex lab_math_special_quadrature(lab_math_special_integrand_t function, void *context,
                                                 double left, double right)
{
    double complex first = function(left, context), centre = function((left + right) / 2, context);
    double complex last = function(right, context), whole = (right - left) * (first + 4 * centre + last) / 6;
    return lab_math_special_refine(function, context, left, right, first, centre, last, whole, 2e-12, 24);
}

static double lab_math_special_ei(double x)
{
    double term = x, total = term;
    for (unsigned k = 2; k < 1000; ++k) {
        term *= x / k;
        double contribution = term / k;
        total += contribution;
        if (fabs(contribution) < 2e-16 * fmax(1, fabs(total)))
            return 0.5772156649015328606 + log(x) + total;
    }
    lab_math_check(false, "Ei defining series converged");
    return NAN;
}

static double complex lab_math_special_bessel_j(double order, double complex x)
{
    if (order < 0 && order == floor(order))
        return pow(-1, -order) * lab_math_special_bessel_j(-order, x);
    double complex term = order == 0 ? 1 : cpow(x / 2, order) / tgamma(order + 1), total = term;
    for (unsigned k = 1; k < 1000; ++k) {
        term *= -(x * x / 4) / (k * (k + order));
        total += term;
        if (cabs(term) < 2e-16 * fmax(1, cabs(total)))
            return total;
    }
    lab_math_check(false, "Bessel J defining series converged");
    return NAN;
}

static double lab_math_special_bessel_y(double x)
{
    double term = 1, harmonic = 0, correction = 0;
    for (unsigned k = 1; k < 1000; ++k) {
        term *= -(x * x / 4) / ((double)k * k);
        harmonic += 1.0 / k;
        double contribution = -harmonic * term;
        correction += contribution;
        if (fabs(contribution) < 2e-16 * fmax(1, fabs(correction)))
            return 2 / acos(-1) * ((log(x / 2) + 0.5772156649015328606) *
                                   creal(lab_math_special_bessel_j(0, x)) + correction);
    }
    lab_math_check(false, "Bessel Y defining series converged");
    return NAN;
}

static double lab_math_special_gamma(double shape, double x, bool upper)
{
    double current = shape == floor(shape) ? 1 : 0.5;
    double value = current == 1 ? (upper ? exp(-x) : -expm1(-x))
                                : sqrt(acos(-1)) * (upper ? erfc(sqrt(x)) : erf(sqrt(x)));
    while (current < shape) {
        double tail = pow(x, current) * exp(-x);
        value = current * value + (upper ? tail : -tail);
        current += 1;
    }
    return value;
}

typedef struct lab_math_special_context {
    double complex rate;
    double complex target;
    double order;
    bool upper;
    bool regularised;
    double complex (*source)(double, const struct lab_math_special_context *);
} lab_math_special_context_t;

static double complex lab_math_special_ei_source(double t, const lab_math_special_context_t *c)
{
    return lab_math_special_ei(creal(c->rate) * t);
}

static double complex lab_math_special_j_source(double t, const lab_math_special_context_t *c)
{
    return lab_math_special_bessel_j(c->order, c->rate * t);
}

static double complex lab_math_special_y_source(double t, const lab_math_special_context_t *c)
{
    return lab_math_special_bessel_y(creal(c->rate) * t);
}

static double complex lab_math_special_gamma_source(double t, const lab_math_special_context_t *c)
{
    return lab_math_special_gamma(c->order, creal(c->rate) * t, c->upper) / (c->regularised ? tgamma(c->order) : 1);
}

static double complex lab_math_special_half_source(double t, const lab_math_special_context_t *c)
{
    return sqrt(2 / (acos(-1) * t)) * (c->order > 0 ? sin(t) : cos(t));
}

static double complex lab_math_special_moment_source(double t, const lab_math_special_context_t *c)
{
    return pow(t, c->order - 1) * cexp(-c->rate * t);
}

static double complex lab_math_special_smoothed(double u, void *opaque)
{
    if (!u)
        return 0;
    lab_math_special_context_t *context = opaque;
    double t = pow(u, 8);
    return 8 * pow(u, 7) * cexp(-context->target * t) * context->source(t, context);
}

static double complex lab_math_special_laplace(lab_math_special_context_t *context, double growth)
{
    double decay = creal(context->target) - growth;
    if (!lab_math_check(decay > 0, "positive independent quadrature damping margin"))
        return NAN;
    return lab_math_special_quadrature(lab_math_special_smoothed, context, 0, pow(40 / decay, 1.0 / 8));
}

static double complex lab_math_special_fubini(double v, void *opaque)
{
    lab_math_special_context_t *context = opaque;
    return 1 / (context->rate + context->target * v);
}

static const char *lab_math_special_literal(double complex value)
{
    return lab_math_format("(%.17g+(%.17g)*i)", creal(value), cimag(value));
}

static const json_t *lab_math_special_fields(const char *source)
{
    return lab_math_fields(source, "s", "evaluate", 40);
}

static void lab_math_special_transform(const char *operand, double complex target, double complex expected,
                                      const char *bindings)
{
    const json_t *result = lab_math_special_fields(lab_math_format("{Laplace(%s,t,s) | s=%s%s}", operand,
                                                                   lab_math_special_literal(target), bindings));
    lab_math_contains(lab_math_text(result, "function"), "laplace(", false);
    double complex actual = lab_math_number(result, "value");
    lab_math_check(isfinite(creal(actual)) && isfinite(cimag(actual)), operand);
    lab_math_check(cabs(actual - expected) <= 3e-9 * fmax(1, cabs(expected)),
                   lab_math_format("%s: actual %.17g%+.17gi expected %.17g%+.17gi", operand,
                                   creal(actual), cimag(actual), creal(expected), cimag(expected)));
}

static void test_laplace_special_integral_families(void)
{
    const double rates[] = {0.5, 1, 2};
    for (size_t r = 0; r < 3; ++r) {
        for (unsigned imaginary = 0; imaginary < 2; ++imaginary) {
            lab_math_special_context_t context = {rates[r], (2.5 + (imaginary ? 0.75 * I : 0)) * rates[r],
                                                   0, false, false, lab_math_special_ei_source};
            double complex expected = lab_math_special_quadrature(lab_math_special_fubini, &context, 0, 1);
            lab_math_special_transform(lab_math_format("E1(%g*t)", rates[r]), context.target, expected, "");
            lab_math_special_transform(lab_math_format("Ei(-%g*t)", rates[r]), context.target, -expected, "");
            lab_math_special_transform(lab_math_format("Ei(%g*t)", rates[r]), context.target,
                                        lab_math_special_laplace(&context, rates[r]), "");
        }
    }
    const double shapes[] = {0.5, 1, 1.5, 3};
    const char *gamma_names[][2] = {{"gammainc_lower", "gammainc_P"}, {"gammainc_upper", "gammainc_Q"}};
    for (size_t s = 0; s < 4; ++s) {
        for (unsigned upper = 0; upper < 2; ++upper) {
            for (unsigned regularised = 0; regularised < 2; ++regularised) {
                lab_math_special_context_t context = {1.5, 2.75 + 0.5 * I, shapes[s], upper != 0,
                                                       regularised != 0, lab_math_special_gamma_source};
                lab_math_special_transform(lab_math_format("%s(%g,1.5*t)", gamma_names[upper][regularised], shapes[s]),
                                            context.target, lab_math_special_laplace(&context, 0), "");
            }
        }
    }
}

static void test_laplace_special_bessel(void)
{
    const double orders[] = {-3, -2, -1, -0.75, -0.5, 0, 0.5, 1, 2.25}, rates[] = {0.5, 1.5};
    for (size_t n = 0; n < 9; ++n) {
        for (size_t r = 0; r < 2; ++r) {
            lab_math_special_context_t context = {rates[r], 3 + 0.5 * I, orders[n], false, false, lab_math_special_j_source};
            lab_math_special_transform(lab_math_format("BesselJ(%g,%g*t)", orders[n], rates[r]), context.target,
                                        lab_math_special_laplace(&context, 0), "");
        }
    }
    for (int sign = -1; sign <= 1; sign += 2) {
        for (int order = -1; order <= 1; order += 2) {
            lab_math_special_context_t context = {1, 0.5 + sign * 2 * I, order * 0.5, false, false, lab_math_special_half_source};
            lab_math_special_transform(lab_math_format("BesselJ(%g,t)", context.order), context.target,
                                        lab_math_special_laplace(&context, 0), "");
        }
    }
    for (unsigned r = 1; r <= 3; ++r) {
        lab_math_special_context_t context = {r * 0.5, 3 + 0.5 * I, 0, false, false, lab_math_special_y_source};
        lab_math_special_transform(lab_math_format("BesselY(0,%g*t)", r * 0.5), context.target,
                                    lab_math_special_laplace(&context, 0), "");
    }
    const double complex complex_rates[] = {0, -1, I, 1 + I};
    for (size_t i = 0; i < 4; ++i) {
        lab_math_special_context_t context = {complex_rates[i], 3 + 0.5 * I, 0, false, false, lab_math_special_j_source};
        lab_math_special_transform("BesselJ(0,a*t)", context.target, lab_math_special_laplace(&context,
            fabs(cimag(context.rate))),
                                    lab_math_format("; a=%s", lab_math_special_literal(context.rate)));
    }
}

static void test_laplace_special_parameters(void)
{
    lab_math_special_context_t context = {1, 2, 0, false, false, lab_math_special_j_source};
    lab_math_special_transform("E1(t)", 2, lab_math_special_quadrature(lab_math_special_fubini, &context, 0, 1), "");
    context.rate = 0.5;
    double complex expected = lab_math_special_quadrature(lab_math_special_fubini, &context, 0, 1);
    lab_math_special_transform("E1(t/2)", 2, expected, "");
    lab_math_special_transform("E1(c*t)", 2, expected, "; c=1/2");
    context = (lab_math_special_context_t){1.5, 3, -3, false, false, lab_math_special_j_source};
    lab_math_special_transform("BesselJ(n,c*t)", 3, lab_math_special_laplace(&context, 0), "; n=-3,c=3/2");
    context = (lab_math_special_context_t){1, 2, 0.5, false, false, lab_math_special_j_source};
    lab_math_special_transform("BesselJ(v,t)", 2, lab_math_special_laplace(&context, 0), ",v=1/2");
    context.order = 1.5;
    context.source = lab_math_special_gamma_source;
    lab_math_special_transform("gammainc_lower(v,t)", 2, lab_math_special_laplace(&context, 0), ",v=3/2");
    const double complex rates[] = {1, 1 + 0.5 * I, 0.5 + 3 * I}, targets[] = {2, 2 + 0.25 * I, 0.75 - 9 * I};
    const char *names[] = {"gammainc_lower", "gammainc_upper", "gammainc_P", "gammainc_Q"};
    for (size_t i = 0; i < 3; ++i) {
        context = (lab_math_special_context_t){rates[i], targets[i], 0.5, false, false, lab_math_special_moment_source};
        expected = lab_math_special_quadrature(lab_math_special_fubini, &context, 0, 1);
        lab_math_special_transform("E1(a*t)", targets[i], expected, lab_math_format(",a=%s", lab_math_special_literal(rates[i])));
        double complex lower = cpow(rates[i], 0.5) * lab_math_special_laplace(&context, 0) / targets[i];
        double complex upper = tgamma(0.5) / targets[i] - lower;
        const double complex values[] = {lower, upper, lower / tgamma(0.5), upper / tgamma(0.5)};
        for (size_t n = 0; n < 4; ++n)
            lab_math_special_transform(lab_math_format("%s(v,a*t)", names[n]), targets[i], values[n],
                                        lab_math_format(",v=1/2,a=%s", lab_math_special_literal(rates[i])));
    }
}

static const char *lab_math_special_compact(const char *text)
{
    string_t *input = string_new_with(text), *output = string_new();
    string_cursor_t *cursor = string_cursor_new(input);
    while (cursor && !string_cursor_done(cursor)) {
        string_cursor_skip_spaces(cursor);
        if (string_cursor_done(cursor))
            break;
        string_append_rune(output, string_cursor_peek(cursor));
        string_cursor_next(cursor);
    }
    const char *copy = lab_math_format("%s", string_c_str(output));
    string_cursor_free(cursor);
    string_free(output);
    string_free(input);
    return copy;
}

static void test_laplace_special_guards(void)
{
    const char *operands[] = {"BesselJ(v,t)", "gammainc_lower(v,t)", "gammainc_upper(v,t)", "gammainc_P(v,t)", "gammainc_Q(v,t)"};
    for (size_t i = 0; i < 5; ++i) {
        const json_t *result = lab_math_special_fields(lab_math_format("Laplace(%s,t,s)", operands[i]));
        const char *compact = lab_math_special_compact(lab_math_text(result, "function"));
        lab_math_contains(compact, "laplace(", false);
        lab_math_contains(compact, i ? "realpart(v)>0" : "realpart(v)>-1", true);
        lab_math_contains(compact, "realpart(s)>0", true);
    }
    lab_math_equal(lab_math_text(lab_math_special_fields("{Laplace(BesselJ(v,t),t,s) | s=2,v=-1}"), "value"), "NAN");
    lab_math_equal(lab_math_text(lab_math_special_fields("{Laplace(gammainc_lower(v,t),t,s) | s=2,v=0}"), "value"), "NAN");
    const char *rates[] = {"E1(a*t)", "gammainc_lower(v,a*t)", "gammainc_upper(v,a*t)", "gammainc_P(v,a*t)", "gammainc_Q(v,a*t)"};
    for (size_t i = 0; i < 5; ++i) {
        const json_t *result = lab_math_special_fields(lab_math_format("Laplace(%s,t,s)", rates[i]));
        const char *compact = lab_math_special_compact(lab_math_text(result, "function"));
        lab_math_contains(compact, "laplace(", false);
        lab_math_contains(compact, "realpart(a)>0", true);
        lab_math_contains(compact, "realpart(s)>0", true);
        if (i)
            lab_math_contains(compact, "realpart(v)>0", true);
    }
    const json_t *result = lab_math_special_fields("{Laplace(E1(a*t),t,s) | s=?; a=?}");
    lab_math_contains(lab_math_text(result, "function"), "laplace(", false);
    lab_math_contains(lab_math_special_compact(lab_math_text(result, "function")), "realpart(a)>0", true);
    const char *invalid_operands[] = {"E1(a*t)", "gammainc_lower(1/2,a*t)", "gammainc_Q(2,a*t)"};
    const char *invalid_rates[] = {"0", "-1", "i", "-1+i"};
    for (size_t i = 0; i < 3; ++i) {
        for (size_t j = 0; j < 4; ++j)
            lab_math_equal(lab_math_text(lab_math_special_fields(lab_math_format("{Laplace(%s,t,s) | s=2,a=%s}",
                                                                                  invalid_operands[i],
                                                                                      invalid_rates[j])), "value"), "NAN");
    }
}

static double complex lab_math_special_clausen(unsigned order, double rate, double complex target)
{
    double magnitude = fabs(rate);
    unsigned exponent = order % 2 ? order + 1 : order;
    double coefficient = order % 2 ? 2 * cabs(target) / (magnitude * magnitude * (order + 1)) : 2 / (magnitude * order);
    size_t count = (size_t)fmax(8, fmax(ceil(sqrt(2) * cabs(target) / magnitude), ceil(pow(coefficient / 1e-10, 1.0 /
        exponent))));
    while (coefficient / pow((double)count, exponent) > 1e-10)
        ++count;
    lab_math_check(coefficient / pow((double)count, exponent) <= 1e-10, "Clausen Fourier-series tail bound");
    /* Long-double compensated accumulation replaces Python's separate math.fsum calls. */
    long double complex total = 0, correction = 0;
    for (size_t n = 1; n <= count; ++n) {
        double complex denominator = target * target + pow(magnitude * (double)n, 2);
        double complex term = order % 2 ? target / (pow((double)n, order) * denominator)
                                        : rate / (pow((double)n, order - 1) * denominator);
        long double complex adjusted = (long double complex)term - correction;
        long double complex next = total + adjusted;
        correction = (next - total) - adjusted;
        total = next;
    }
    return (double complex)total;
}

static double complex lab_math_special_clausen_one_integrand(double u, void *opaque)
{
    if (!u)
        return 0;
    lab_math_special_context_t *context = opaque;
    double half = acos(-1) / cabs(context->rate), time = half * pow(u, 4);
    double value = -log(2 * sin(cabs(context->rate) * time / 2));
    double complex weights = cexp(-context->target * time) + cexp(-context->target * (2 * half - time));
    return 4 * half * pow(u, 3) * weights * value;
}

static void test_laplace_special_clausen(void)
{
    for (unsigned order = 2; order <= 32; ++order)
        lab_math_special_transform(lab_math_format("Cl(%u,t)", order), 2 + 0.75 * I,
                                    lab_math_special_clausen(order, 1, 2 + 0.75 * I), "");
    const double rates[] = {0.5, 1, -1, -2};
    const double complex targets[] = {0.25, 2 + 0.75 * I};
    for (size_t r = 0; r < 4; ++r) {
        for (size_t s = 0; s < 2; ++s) {
            lab_math_special_context_t context = {rates[r], targets[s], 1, false, false, NULL};
            double complex expected = lab_math_special_quadrature(lab_math_special_clausen_one_integrand, &context, 0, 1) /
                                      (1 - cexp(-targets[s] * 2 * acos(-1) / fabs(rates[r])));
            lab_math_special_transform(lab_math_format("Cl(1,%g*t)", rates[r]), targets[s], expected, "");
        }
    }
    const unsigned orders[] = {2, 3, 4, 31, 32};
    const double scales[] = {0.5, -0.5, 1.5, -1.5};
    for (size_t n = 0; n < 5; ++n) {
        for (size_t r = 0; r < 4; ++r)
            lab_math_special_transform(lab_math_format("Cl(%u,%g*t)", orders[n], scales[r]), 2.75 - 0.5 * I,
                                        lab_math_special_clausen(orders[n], scales[r], 2.75 - 0.5 * I), "");
    }
    const char *aliases[] = {"clausen2", "Cl2", "Cl₂", "clausen"};
    double complex target = 1.5 + 0.5 * I, expected = lab_math_special_clausen(2, 1, target);
    for (size_t i = 0; i < 4; ++i)
        lab_math_special_transform(lab_math_format("%s(t)", aliases[i]), target, expected, "");
    expected = lab_math_special_clausen(2, -0.5, target);
    lab_math_special_transform("clausen2(-t/2)", target, expected, "");
    lab_math_special_transform("Cl(n,a*t)", target, expected, "; n=2,a=-1/2");
    lab_math_special_transform("Cl(n,a*t)", target, lab_math_special_clausen(5, -0.5, target), "; n=5,a=-1/2");
    const unsigned high[] = {9, 16, 31, 32};
    for (size_t i = 0; i < 4; ++i) {
        const json_t *result = lab_math_special_fields(lab_math_format("Laplace(Cl(%u,t),t,s)", high[i]));
        lab_math_contains(lab_math_text(result, "function"), "laplace(", false);
        lab_math_contains(lab_math_text(result, "function"), "sum(", true);
        string_t *function = string_new_with(lab_math_text(result, "function"));
        lab_math_check(string_length(function) < 2000, "compact high-order Clausen function");
        string_free(function);
    }
    target = 2 + 0.75 * I;
    lab_math_special_transform("Cl(31,j*t)", target, lab_math_special_clausen(31, 1.5, target), "; j=3/2");
    const json_t *result = lab_math_special_fields(lab_math_format("{Laplace(Cl(32,t),t,j) | j=%s}",
        lab_math_special_literal(target)));
    lab_math_contains(lab_math_text(result, "function"), "laplace(", false);
    expected = lab_math_special_clausen(32, 1, target);
    lab_math_check(cabs(lab_math_number(result, "value") - expected) <= 3e-9 * fmax(1, cabs(expected)),
        "Clausen target-index hygiene");
}

static void test_laplace_special_unsupported(void)
{
    const char *clausen[] = {"Cl(1,t)", "clausen2(t)", "Cl(3,t)", "Cl(32,-2*t)"}, *targets[] = {"0", "-1", "i"};
    for (size_t i = 0; i < 4; ++i) {
        const json_t *result = lab_math_special_fields(lab_math_format("Laplace(%s,t,s)", clausen[i]));
        lab_math_contains(lab_math_text(result, "function"), "laplace(", false);
        lab_math_contains(lab_math_special_compact(lab_math_text(result, "function")), "realpart(s)>0", true);
        for (size_t j = 0; j < 3; ++j)
            lab_math_equal(lab_math_text(lab_math_special_fields(lab_math_format("{Laplace(%s,t,s) | s=%s}", clausen[i],
                targets[j])), "value"), "NAN");
    }
    const char *unsupported[] = {"Cl(33,t)", "Cl(3/2,t)", "Cl(n,t)", "Cl(t,t)", "Cl(3,a*t)", "clausen2(a*t)",
                                 "clausen2(i*t)", "Cl(3,(1+i)*t)", "clausen2(t+1)", "E1(-t)", "E1(i*t)", "Ei(i*t)",
                                 "E1(t+1)", "Ei(t^2)", "BesselJ(-3/2,t)", "BesselJ(1/2,-t)", "BesselJ(1,i*t)",
                                 "BesselJ(t,t)", "BesselY(1,t)", "BesselY(-1,t)", "BesselY(v,t)", "gammainc_lower(t,t)",
                                 "gammainc_upper(-1/2,t)", "gammainc_P(2,-t)", "gammainc_Q(2,t+1)", "BesselJ(1,c*t)",
                                 "Ei(c*t)", "E1((1+i)*t)", "gammainc_P(2,(1+i)*t)"};
    for (size_t i = 0; i < sizeof(unsupported) / sizeof(*unsupported); ++i)
        lab_math_contains(lab_math_text(lab_math_special_fields(lab_math_format("Laplace(%s,t,s)", unsupported[i])),
            "function"), "laplace(", true);
    const char *free_parameters[] = {"{Laplace(Cl(n,t),t,s) | s=2,n=3}", "{Laplace(clausen2(a*t),t,s) | s=2,a=1}",
                                     "{Laplace(Ei(c*t),t,s) | s=2,c=1}"};
    for (size_t i = 0; i < 3; ++i)
        lab_math_contains(lab_math_text(lab_math_special_fields(free_parameters[i]), "function"), "laplace(", true);
    const char *boundary[][2] = {{"E1(t)", "0"}, {"E1(t)", "-1/2"}, {"Ei(-t)", "0"}, {"Ei(2*t)", "2"}, {"Ei(2*t)", "1"},
                                {"gammainc_upper(2,t)", "0"}, {"gammainc_Q(2,t)", "-1/2"}, {"BesselJ(1,t)", "-1"},
                                    {"BesselY(0,t)", "0"}};
    for (size_t i = 0; i < 9; ++i)
        lab_math_equal(lab_math_text(lab_math_special_fields(lab_math_format("{Laplace(%s,t,s) | s=%s}", boundary[i][0],
            boundary[i][1])), "value"), "NAN");
}

/* Register every special-function Laplace regression; this original suite has no README examples. */
void test_lab_math_laplace_special_cases(void)
{
    TEST_RUN_IN_GROUP(test_laplace_special_integral_families, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_laplace_special_bessel, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_laplace_special_parameters, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_laplace_special_guards, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_laplace_special_clausen, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_laplace_special_unsupported, tests, NULL);
    lab_math_reset();
}
