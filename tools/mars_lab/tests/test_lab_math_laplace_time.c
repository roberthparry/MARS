/**
 * @file test_lab_math_laplace_time.c
 * @brief Unilateral time-integral and translation regression ports.
 *
 * Preserves test_laplace_time_integrals.py and test_laplace_time_translations.py,
 * including history specialisation, copied primitives, unsupported cases and
 * exact README output. These suites exercise prebuilt workers sequentially.
 */
#include <math.h>
#include <string.h>

#include "test_lab_math_support.h"

static const json_t *lab_math_laplace_time_fields(const char *source)
{
    return lab_math_fields(source, "s", "evaluate", 40);
}

static void lab_math_laplace_time_contains(const json_t *result, const char *key, const char *fragment, bool present)
{
    lab_math_contains(lab_math_text(result, key), fragment, present);
}

static void lab_math_laplace_time_same(const char *left, const char *right, const char *key)
{
    lab_math_equal(lab_math_text(lab_math_laplace_time_fields(left), key),
        lab_math_text(lab_math_laplace_time_fields(right), key));
}

static void lab_math_laplace_time_integral_round_trip(const json_t *result)
{
    const json_t *copy = lab_math_laplace_time_fields(lab_math_text(result, "unbound"));
    if (strstr(lab_math_text(result, "tex"), "chosen antiderivative")) {
        lab_math_laplace_time_contains(copy, "tex", "F\\left(0\\right)", true);
        lab_math_laplace_time_contains(copy, "tex", "F'(x)=f\\left(x\\right)", true);
        lab_math_laplace_time_contains(copy, "function", "integral(f(x), x, 0)", true);
        lab_math_laplace_time_contains(copy, "function", "laplace(f(t), t, s)", true);
    } else {
        const char *original = lab_math_replace(lab_math_replace(lab_math_text(result, "tex"), "\\mkern-2mu ", ""), "\\,", "");
        const char *copied = lab_math_replace(lab_math_replace(lab_math_text(copy, "tex"), "\\mkern-2mu ", ""), "\\,", "");
        lab_math_equal(original, copied);
    }
}

static void test_laplace_time_integrals(void)
{
    const json_t *expected = lab_math_laplace_time_fields("@L{integral(f(x),x,0,t)}");
    lab_math_equal(lab_math_text(expected, "unbound"), "ℒ(f(t), t, s)/s");
    const char *zero_based[] = {"@L{@S_0^t f(x) dx}", "@L{@S^t_0 f(x) dx}", "@L{∫_0^t f(x) dx}"};
    for (size_t i = 0; i < 3; ++i) {
        const json_t *result = lab_math_laplace_time_fields(zero_based[i]);
        lab_math_equal(lab_math_text(result, "tex"), lab_math_text(expected, "tex"));
        lab_math_laplace_time_contains(result, "function", "const f", false);
        lab_math_laplace_time_contains(result, "unbound", "where ()", false);
    }
    const json_t *result = lab_math_laplace_time_fields("@L{@S^t f(x) dx}");
    lab_math_laplace_time_contains(result, "function", "laplace(f(t), t, s)", true);
    lab_math_laplace_time_contains(result, "function", "integral(f(x), x, 0)", true);
    lab_math_laplace_time_contains(result, "unbound", "integral_meta", false);
    lab_math_laplace_time_contains(result, "tex", "F\\left(0\\right)", true);
    lab_math_laplace_time_contains(result, "tex", "F'(x)=f\\left(x\\right)", true);
    lab_math_laplace_time_contains(result, "tex", "chosen antiderivative", true);
    lab_math_laplace_time_contains(result, "tex", "\\int^{0}", false);
    lab_math_laplace_time_same("@L{@S^t f(x) dx}", "@L{integral(f(x),x,t)}", "tex");
    lab_math_laplace_time_integral_round_trip(result);
    const char *capture[] = {"@L{@S^t f(x) dx + F}", "@L{@S_0^t f(x) dx}",
                             "@L{@S_2^t f(x) dx}", "@L{@S^t f(x) dx + @S^t g(x) dx}"};
    for (size_t i = 0; i < 4; ++i) {
        result = lab_math_laplace_time_fields(capture[i]);
        lab_math_laplace_time_contains(result, "tex", "chosen antiderivative", false);
        if (!i)
            lab_math_laplace_time_contains(result, "tex", "\\int^{0}", true);
    }
    result = lab_math_laplace_time_fields("@L{@S_2^t f(x) dx}");
    lab_math_laplace_time_contains(result, "function", "integral(f(x), x, 2, 0)", true);
    lab_math_laplace_time_integral_round_trip(result);
    result = lab_math_laplace_time_fields("@L{@S^t f(x) dx + C}");
    lab_math_laplace_time_contains(result, "function", "const C", true);
    lab_math_laplace_time_contains(result, "function", "integral(f(x), x, 0)", true);
    lab_math_laplace_time_contains(lab_math_laplace_time_fields("@L(@S_0^u f(x) dx,u,p)"), "function",
        "laplace(f(u), u, p)/p", true);
    lab_math_laplace_time_same("@L{@S_0^t f(t) dt}", "@L{@S_0^t f(x) dx}", "tex");
    lab_math_laplace_time_contains(lab_math_laplace_time_fields("@L{@S_0^t t*f(x) dx}"), "function", "laplace(integral(", true);
    lab_math_laplace_time_same("a(x+1)", "a*(x+1)", "tex");
    const char *integrals[] = {"@S_0^t sin(x) dx", "∫_0^t sin(x) dx", "integral(sin(x),x,0,t)"};
    for (size_t i = 0; i < 3; ++i)
        lab_math_places(lab_math_number(lab_math_laplace_time_fields(lab_math_format("{@L{%s} | s=2}", integrals[i])),
            "value"), 0.1, 7);
}

static void test_laplace_time_translation_history(void)
{
    const json_t *result = lab_math_laplace_time_fields("@L{u(t-1/4)}");
    lab_math_laplace_time_contains(result, "function", "laplace(u(t), t, s)", true);
    lab_math_laplace_time_contains(result, "function", "integral(u(t).exp(-s.t), t, -1/4, 0)", true);
    lab_math_laplace_time_contains(result, "function", "exp(-(1/4).s)", true);
    lab_math_laplace_time_contains(result, "function", "realpart(s)", false);
    lab_math_laplace_time_contains(result, "transform_identity_TeX", " = ", true);
    lab_math_equal(lab_math_text(result, "value"), "NAN");
    lab_math_equal(lab_math_text(lab_math_laplace_time_fields(lab_math_text(result, "unbound")), "tex"),
        lab_math_text(result, "tex"));
    result = lab_math_laplace_time_fields("@L{16t^2u(t-1/4)}");
    lab_math_laplace_time_contains(result, "function", "16.derivative(", true);
    lab_math_laplace_time_contains(result, "function", ", s, 2)", true);
    lab_math_laplace_time_contains(result, "tex", "\\frac{d^{2}}{d s^{2}}\\left[", true);
    lab_math_laplace_time_contains(result, "function", ", -1/4, 0)", true);
    lab_math_laplace_time_same("@L{16*(t^2*u(t-1/4))}", "@L{16t^2u(t-1/4)}", "tex");
    lab_math_laplace_time_same("@L{u(t-1/4)*(16*t^2)}", "@L{16t^2u(t-1/4)}", "tex");
    const json_t *copy = lab_math_laplace_time_fields(lab_math_text(result, "unbound"));
    lab_math_laplace_time_contains(copy, "function", "derivative(laplace(u(t), t, s), s, 2)", true);
    lab_math_laplace_time_contains(copy, "function", ", -1/4, 0)", true);
    const char *functions[] = {"1", "t", "exp(t)"};
    const char *transforms[] = {"1/s", "1/s^2", "1/(s-1)"};
    const double values[] = {32.0 / 27, 96.0 / 81 - 8.0 / 27, 4 * exp(-0.25)};
    for (size_t i = 0; i < 3; ++i) {
        const char *special = lab_math_replace(lab_math_text(result, "unbound"), "ℒ(u(t), t, s)", transforms[i]);
        special = lab_math_replace(special, "u(t)", functions[i]);
        lab_math_places(lab_math_number(lab_math_laplace_time_fields(lab_math_format("{%s | s=3}", special)), "value"),
            values[i], 11);
    }
    lab_math_laplace_time_same("@L{u(t-0)}", "@L{u(t)}", "tex");
    result = lab_math_laplace_time_fields("@L{g(t+2)}");
    lab_math_laplace_time_contains(result, "function", "exp(2.s)", true);
    lab_math_laplace_time_contains(result, "function", "integral(g(t).exp(-s.t), t, 2, 0)", true);
    result = lab_math_laplace_time_fields("@L{t*u(t)}");
    lab_math_laplace_time_contains(result, "function", "-derivative(laplace(u(t), t, s), s, 1)", true);
    lab_math_laplace_time_contains(result, "function", "integral(", false);
}

static void test_laplace_time_translation_domains(void)
{
    const json_t *result = lab_math_laplace_time_fields("@L(u(x-a),x,p)");
    lab_math_laplace_time_contains(result, "function", "laplace(u(x), x, p)", true);
    lab_math_laplace_time_contains(result, "function", "integral(u(x).exp(-p.x), x, -a, 0)", true);
    lab_math_laplace_time_contains(result, "tex", "\\in\\mathbb{R}", true);
    lab_math_laplace_time_contains(result, "function", "realpart(p) >", false);
    lab_math_laplace_time_contains(lab_math_laplace_time_fields(lab_math_text(result, "unbound")), "function", ", -a, 0)", true);
    lab_math_laplace_time_contains(lab_math_laplace_time_fields("@L(g(x-t),x,p)"), "function",
        "integral(g(x).exp(-p.x), x, -t, 0)", true);
    result = lab_math_laplace_time_fields("@L{t^3*u(t-1)}");
    lab_math_laplace_time_contains(result, "function", "-derivative(", true);
    lab_math_laplace_time_contains(result, "function", ", s, 3)", true);
    lab_math_laplace_time_contains(result, "tex", "\\frac{d^{3}}{d s^{3}}", true);
    lab_math_laplace_time_contains(lab_math_laplace_time_fields("@L{t^64*u(t-1)}"), "tex", "\\frac{d^{64}}{d s^{64}}", true);
    lab_math_laplace_time_contains(lab_math_laplace_time_fields("@L{t^65*u(t-1)}"), "function", "integral(", false);
    result = lab_math_laplace_time_fields("{@L{t^2*u(t-a)} | s=3; a=1/4}");
    lab_math_laplace_time_contains(result, "function", "derivative(", true);
    lab_math_laplace_time_contains(result, "function", ", s, 2)", true);
    lab_math_laplace_time_contains(result, "function", ", -a, 0)", true);
    lab_math_laplace_time_contains(result, "tex", "\\int_{-a}^{0}", true);
    lab_math_laplace_time_contains(result, "tex", "\\frac{1}{4}", false);
    const char *unsupported[] = {"@L{u(2*t-1)}", "@L{u(t^2-1)}", "@L{u(t-1,t)}", "@L{u(t+i)}"};
    for (size_t i = 0; i < 4; ++i) {
        result = lab_math_laplace_time_fields(unsupported[i]);
        lab_math_laplace_time_contains(result, "function", "integral(", false);
        lab_math_laplace_time_contains(result, "transform_identity_TeX", " = ", false);
    }
    int status = -1;
    const char *raw = "";
    lab_math_worker("mars_lab", "@L(t*u(t-s),t,s)", "x", "", 40, &status, &raw);
    lab_math_check(status != 0, "transform requires a distinct target");
    lab_math_contains(raw, "distinct target", true);
}

static void test_laplace_time_readme(void)
{
    /* README examples: docs/expression.md and integral-transform design note. */
    const char *cases[][2] = {
        {"@L{@S_0^t f(x) dx}", "ℒ(f(t), t, s)/s"},
        {"@L{@S^t f(x) dx}", "1/s·(ℒ(f(t), t, s) + ∫^0 f(x)·dx)"},
        {"@L{u(t-1/4)}", "exp(-¼s)·(ℒ(u(t), t, s) + ∫^0_-¼ u(t)·exp(-st)·dt)"},
        {"@L{16t^2u(t-1/4)}", "16·Dss(exp(-¼s)·(ℒ(u(t), t, s) + ∫^0_-¼ u(t)·exp(-st)·dt))"},
    };
    for (size_t i = 0; i < 4; ++i)
        lab_math_equal(lab_math_text(lab_math_laplace_time_fields(cases[i][0]), "unbound"), cases[i][1]);
}

/* Register the ordinary time-integral and translation regressions. */
void test_lab_math_laplace_time_cases(void)
{
    TEST_RUN_IN_GROUP(test_laplace_time_integrals, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_laplace_time_translation_history, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_laplace_time_translation_domains, tests, NULL);
    lab_math_reset();
}

/* Run exact documented output after ordinary mathematical regressions. */
void test_lab_math_laplace_time_readme_cases(void)
{
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(test_laplace_time_readme, readme_examples, "math,readme,output");
    lab_math_reset();
}
