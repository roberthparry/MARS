/**
 * @file test_lab_math_round_trips.c
 * @brief Complete copied-formula transform-family regression grid.
 *
 * Ports test_transform_round_trips.py, preserving both Fourier directions,
 * every Laplace source family, symbolic orders and distributional qualifications.
 * Distribution tests compare expressions without assigning impulse values.
 */
#include <math.h>
#include <string.h>

#include "test_lab_math_support.h"

typedef struct {
    const char *name;
    const char *source;
} round_trip_source_t;

static const round_trip_source_t fourier_sources[] = {
    {"zero", "0"},
    {"constant", "1"},
    {"linear", "t"},
    {"step", "step(t)"},
    {"polynomial", "t^3"},
    {"gaussian", "exp(-t^2)"},
    {"shifted_gaussian", "exp(-2*t^2+t+1)"},
    {"rectangle", "rect(t)"},
    {"shifted_rectangle", "rect(2*t-1)"},
    {"triangle", "tri(t)"},
    {"sinc", "sinc(t)"},
    {"sinc_squared", "sinc(t)^2"},
    {"circle_profile", "circ(t)"},
    {"absolute_exponential", "exp(-2*abs(t))"},
    {"causal_exponential", "exp(-2*t)*step(t)"},
    {"reversed_exponential", "exp(2*t)*step(-t)"},
    {"sech", "sech(t)"},
    {"tanh", "tanh(t)"},
    {"csch", "csch(t)"},
    {"coth", "coth(t)"},
    {"arctangent", "atan(t)"},
    {"inverse_hyperbolic_sine", "asinh(t)"},
    {"inverse_hyperbolic_cosine", "acosh(t)"},
    {"inverse_hyperbolic_tangent", "atanh(t)"},
    {"arcsine", "asin(t)"},
    {"arccosine", "acos(t)"},
    {"vertical_gamma", "gamma(1+i*t)"},
    {"cosh_power", "cosh(t)^(-1/2)"},
    {"sinh_power", "sinh(t)^(-1/2)"},
    {"absolute_sinh_power", "abs(sinh(t))^(-1/2)"},
    {"cosine", "cos(2*t+1)"},
    {"sine", "sin(2*t+1)"},
    {"harmonic_exponential", "exp(2*i*t)"},
    {"bessel_zero", "J_0(t)"},
    {"bessel_integer", "bessel_j(3,t)"},
    {"logarithm", "ln(abs(t))"},
    {"hermite_gaussian", "exp(-t^2/2)*HermiteH(3,t)"},
    {"weighted_gaussian", "t*exp(-t^2)"},
    {"modulated_gaussian", "exp(i*t)*exp(-t^2)"},
    {"chebyshev_window", "Tn(3,t)*rect(t/2)/sqrt(1-t^2)"},
    {"convolution", "convolve(exp(-t^2),exp(-t^2),t)"},
};

static const round_trip_source_t laplace_sources[] = {
    {"zero", "0"},
    {"constant", "1"},
    {"linear", "t"},
    {"polynomial", "t^3"},
    {"square_root", "sqrt(t)"},
    {"cube_root", "cubrt(t)"},
    {"fractional_power", "t^(2/3)"},
    {"exponential", "exp(-2*t)"},
    {"sine", "sin(2*t+1)"},
    {"cosine", "cos(2*t+1)"},
    {"sinh", "sinh(2*t+1)"},
    {"cosh", "cosh(2*t+1)"},
    {"versine", "versin(t)"},
    {"vercosine", "vercos(t)"},
    {"coversine", "coversin(t)"},
    {"covercosine", "covercos(t)"},
    {"haversine", "haversin(t)"},
    {"havercosine", "havercos(t)"},
    {"hacoversine", "hacoversin(t)"},
    {"hacovercosine", "hacovercos(t)"},
    {"logarithm", "ln(t)"},
    {"shifted_logarithm", "ln(2*t+1)"},
    {"decimal_logarithm", "log10(t)"},
    {"absolute_affine", "abs(2*t-1)"},
    {"floor", "floor(2*t)"},
    {"ceiling", "ceil(2*t)"},
    {"tanh", "tanh(2*t)"},
    {"sech", "sech(2*t)"},
    {"arctangent", "atan(2*t)"},
    {"arccotangent", "acot(2*t)"},
    {"inverse_sinh", "asinh(2*t)"},
    {"inverse_tanh", "atanh(2*t)"},
    {"gaussian", "exp(-t^2)"},
    {"shifted_gaussian", "exp(-2*t^2+t+1)"},
    {"error_function", "erf(t)"},
    {"shifted_error_function", "erf(2*t+1)"},
    {"complementary_error_function", "erfc(t)"},
    {"normal_density", "normal_pdf(t)"},
    {"normal_distribution", "normal_cdf(t)"},
    {"normal_log_density", "normal_logpdf(t)"},
    {"exponential_integral_e1", "E1(2*t)"},
    {"exponential_integral_ei_positive", "Ei(2*t)"},
    {"exponential_integral_ei_negative", "Ei(-2*t)"},
    {"lower_gamma", "gammainc_lower(3/2,2*t)"},
    {"upper_gamma", "gammainc_upper(3/2,2*t)"},
    {"regularised_lower_gamma", "gammainc_P(3/2,2*t)"},
    {"regularised_upper_gamma", "gammainc_Q(3/2,2*t)"},
    {"bessel_zero", "J_0(t)"},
    {"bessel_integer", "bessel_j(3,2*t)"},
    {"bessel_fractional", "bessel_j(1/2,t)"},
    {"bessel_second_kind", "bessel_y(0,2*t)"},
    {"clausen", "clausen2(t)"},
    {"conjugation", "conj((1+i)*t)"},
    {"real_part", "((1+i)*t+2+conj((1+i)*t+2))/2"},
    {"sine_power", "sin(t)^4"},
    {"cosine_power", "cos(t)^4"},
    {"weighted_sine", "t*sin(t)"},
    {"weighted_gaussian", "t*exp(-t^2)"},
    {"modulated_error_function", "exp(-t)*erf(t)"},
    {"causal_convolution", "causal_convolve(t,t,t)"},
};

static const json_t *lab_math_round_trips_fields(const char *source, const char *variable)
{
    return lab_math_fields(source, variable, "evaluate", 40);
}

static const char *lab_math_round_trips_algebra(const json_t *result)
{
    string_t *text = string_new_with(lab_math_text(result, "unbound"));
    string_cursor_t *cursor = string_cursor_new(text);
    int depth = 0;
    while (cursor && !string_cursor_done(cursor)) {
        if (!depth && string_cursor_match(cursor, " where ("))
            break;
        if (string_cursor_match(cursor, "(") || string_cursor_match(cursor, "["))
            ++depth;
        else if (string_cursor_match(cursor, ")") || string_cursor_match(cursor, "]"))
            --depth;
        string_cursor_next(cursor);
    }
    string_t *body = cursor ? string_cursor_slice_between(0, string_cursor_position(cursor), cursor) : NULL;
    const char *copy = lab_math_format("%s", body ? string_c_str(body) : "");
    lab_math_check(body != NULL, "extract locally qualified transform algebra");
    string_free(body);
    string_cursor_free(cursor);
    string_free(text);
    return copy;
}

static double complex lab_math_round_trips_value(const char *source, double point)
{
    return lab_math_number(lab_math_round_trips_fields(lab_math_format("{%s | t=%.17g}", source, point), "t"), "value");
}

static void lab_math_round_trips_resolved(const json_t *result)
{
    lab_math_contains(lab_math_text(result, "function"), "fourier(", false);
    lab_math_contains(lab_math_text(result, "function"), "laplace(", false);
}

static void test_transform_family_round_trips(void)
{
    const char *forward[] = {"Fourier", "InverseFourier", "Laplace"};
    const char *inverse[] = {"InverseFourier", "Fourier", "InverseLaplace"};
    for (size_t direction = 0; direction < 3; ++direction) {
        const round_trip_source_t *sources = direction == 2 ? laplace_sources : fourier_sources;
        size_t count = direction == 2 ? sizeof(laplace_sources) / sizeof(*laplace_sources)
                                      : sizeof(fourier_sources) / sizeof(*fourier_sources);
        const char *frequency = direction == 2 ? "s" : "ω";
        const double points[] = {direction == 2 ? 0.3 : -0.3, direction == 2 ? 0.7 : 0.3, 1.3};
        for (size_t i = 0; i < count; ++i) {
            string_printf("round trip %s/%s\n", forward[direction], sources[i].name);
            const json_t *transformed = lab_math_round_trips_fields(lab_math_format("%s(%s,t,%s)", forward[direction],
                                                              sources[i].source, frequency), frequency);
            lab_math_round_trips_resolved(transformed);
            const json_t *restored = lab_math_round_trips_fields(lab_math_format("%s(%s,%s,t)", inverse[direction],
                                                           lab_math_round_trips_algebra(transformed), frequency), "t");
            lab_math_round_trips_resolved(restored);
            for (size_t p = 0; p < 3; ++p) {
                double complex expected = lab_math_round_trips_value(sources[i].source, points[p]);
                double complex actual = lab_math_round_trips_value(lab_math_round_trips_algebra(restored), points[p]);
                lab_math_check(isfinite(cabs(actual)), lab_math_text(restored, "expression"));
                lab_math_close(actual, expected, 1e-10 * (1 + cabs(expected)));
            }
            lab_math_reset();
        }
    }
}

static void test_transform_symbolic_power_round_trips(void)
{
    const char *forward[] = {"Fourier", "InverseFourier", "Laplace"};
    const char *inverse[] = {"InverseFourier", "Fourier", "InverseLaplace"};
    for (size_t direction = 0; direction < 3; ++direction) {
        const char *frequency = direction == 2 ? "s" : "ω";
        const double orders[] = {direction == 2 ? -1.0 / 3 : 0, direction == 2 ? 0 : 1,
                                  direction == 2 ? 2.0 / 3 : 3, direction == 2 ? 3 : 33};
        const double points[] = {direction == 2 ? 0.3 : -0.3, 0.7};
        const json_t *spectrum = lab_math_round_trips_fields(lab_math_format("%s(t^n,t,%s)", forward[direction],
            frequency), frequency);
        const json_t *restored = lab_math_round_trips_fields(lab_math_format("%s(%s,%s,t)", inverse[direction],
            lab_math_round_trips_algebra(spectrum), frequency), "t");
        lab_math_round_trips_resolved(restored);
        for (size_t n = 0; n < 4; ++n) {
            for (size_t p = 0; p < 2; ++p) {
                const json_t *result = lab_math_round_trips_fields(lab_math_format("{%s | t=%.17g; n=%.17g}",
                                                              lab_math_round_trips_algebra(restored), points[p], orders[n]), "t");
                double expected = pow(points[p], orders[n]);
                lab_math_close(lab_math_number(result, "value"), expected, 1e-10 * (1 + fabs(expected)));
            }
        }
    }
}

static void test_transform_distribution_round_trips(void)
{
    const char *cases[][2] = {
        {"delta(t)", "δ(t)"}, {"delta(2*t-1)", "½·δ(t - 0.5)"},
        {"Derivative(delta(t),3)", "Derivative(δ(t), 3)"},
        {"Derivative(delta(t),n)", "Derivative(δ(t), n)"},
        {"finite_part(1/abs(t))", "(1/|t| : finite part)"},
    };
    const char *operators[] = {"Fourier", "InverseFourier"};
    for (size_t direction = 0; direction < 2; ++direction) {
        for (size_t i = 0; i < 5; ++i) {
            const json_t *spectrum = lab_math_round_trips_fields(lab_math_format("%s(%s,t,ω)", operators[direction],
                cases[i][0]), "ω");
            const json_t *recovered = lab_math_round_trips_fields(lab_math_format("%s(%s,ω,t)", operators[1 -
                direction], lab_math_round_trips_algebra(spectrum)), "t");
            lab_math_equal(lab_math_round_trips_algebra(recovered), cases[i][1]);
            lab_math_equal(lab_math_round_trips_algebra(lab_math_round_trips_fields(lab_math_text(recovered,
                "expression"), "t")), cases[i][1]);
            if (i == 3)
                lab_math_contains(lab_math_text(recovered, "expression"), "n ∈ ℤ≥0", true);
        }
    }
    const char *spectra[] = {"1/(2+i*ω)", "1/(2-i*ω)"};
    const double points[] = {-0.3, 0.3};
    for (size_t i = 0; i < 2; ++i) {
        const char *source = lab_math_format("InverseFourier(%s,ω,t)", spectra[i]);
        const json_t *restored = lab_math_round_trips_fields(source, "t");
        for (size_t p = 0; p < 2; ++p)
            lab_math_close(lab_math_round_trips_value(lab_math_round_trips_algebra(restored), points[p]),
                lab_math_round_trips_value(source, points[p]), 1e-12);
    }
}

/* Register the complete generated-family grid and explicit round-trip regressions. */
void test_lab_math_round_trips_cases(void)
{
    TEST_RUN_IN_GROUP(test_transform_family_round_trips, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_transform_symbolic_power_round_trips, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_transform_distribution_round_trips, tests, NULL);
    lab_math_reset();
}
