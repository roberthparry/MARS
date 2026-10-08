/**
 * @file test_lab_function.c
 * @brief Native Function-card execution and generated-programme regressions.
 *
 * Exercises the actual Ophelia worker through the C request adapter, including
 * exact constants, late bindings, symbolic guards, matrix returns and rejected
 * programmes. Generated expression, equation and matrix cards are submitted back
 * through /function-run. README examples are registered separately and run last.
 */
#include <string.h>

#include "lab_evaluate.h"
#include "test_harness.h"
#include "test_lab_support.h"

static json_t *test_lab_function_request(const char *path, const char *key, const char *source, unsigned *status)
{
    string_t *route = string_new_with(path), *name = string_new_with(key), *text = string_new_with(source);
    json_t *payload = json_new_object(), *value = text ? json_new_string(text) : NULL;
    bool ready = payload && value && name && json_object_set(payload, name, value);
    json_t *response = ready && route ? lab_eval_request(route, payload, status) : NULL;
    json_free(value);
    json_free(payload);
    string_free(text);
    string_free(name);
    string_free(route);
    return response;
}

static bool test_lab_function_output(const char *source, const char *expected, bool exact)
{
    unsigned status = 0;
    json_t *response = test_lab_function_request("/function-run", "source", source, &status);
    string_t *output = string_new_with(test_lab_text(response, "output"));
    bool ok = status == 200 && test_lab_ok(response, true) && output;
    if (ok) {
        const char *raw = string_c_str(output);
        size_t length = strlen(raw);
        while (length && (raw[length - 1] == '\n' || raw[length - 1] == '\r'))
            --length;
        ok = exact ? length == strlen(expected) && !memcmp(raw, expected, length) : strstr(raw, expected) != NULL;
    }
    if (!ok)
        string_printf("Function source: %s\nExpected: %s\nOutput: %s\nError: %s\n", source, expected,
                      test_lab_text(response, "output"), test_lab_text(response, "error"));
    string_free(output);
    json_free(response);
    return ok;
}

static void test_lab_function_scalar_regressions(void)
{
    static const struct {
        const char *source, *expected;
    } cases[] = {{"output(x-x).", "0"},
                 {"output(i-i).", "0"},
                 {"x=1.25. output(x.2 + 3e-2).", "²⁵³⁄₁₀₀"},
                 {"expression f(const a) { return a+1/3. } const a=2/3. output(f(a)).", "1"},
                 {"expression f(@omega) { return @omega^2. } @omega=3. output(f(@omega)).", "9"},
                 {"[distance]=[speed].[time]. [speed]=3, [time]=4. output([distance]).", "12"},
                 {"expression f(x) { v=1. y=v+x. v=2. return y. } x=3. output(f(x)).", "5"},
                 {"expression f(x) { if (x>0) { return x. } else { return 0. } } x=-1. output(f(x)).", "0"},
                 {"expression f(x) { if (x>0) { return x. } else { return 0. } } x=2. output(f(x)).", "2"},
                 {"` output(999). ` `` output(998).\noutput(1).", "1"},
                 {"output(integral(sin(x), x, 0, @pi)).", "2"},
                 {"expression f(x) { return derivative(x^2,x,1). } x=3. output(f(x)).", "6"}};
    for (size_t i = 0; i < sizeof cases / sizeof *cases; ++i)
        TEST_ASSERT_TRUE(test_lab_function_output(cases[i].source, cases[i].expected, true),
                         "native scalar programme preserves its exact result");
}

static void test_lab_function_matrix_regressions(void)
{
    static const struct {
        const char *body, *expected;
    } cases[] = {
        {"(1,2;3,4)", "(1, 2; 3, 4)"},        {"x.(1,2,3)+(4,5,6)", "(6, 9, 12)"}, {"(x+1).(1,2;3,4)", "(3, 6; 9, 12)"},
        {"(1,2;3,4).(x+1)", "(3, 6; 9, 12)"}, {"((1,2).(3;4))", "(11)"},           {"(x.(1,2))", "(2, 4)"},
        {"-(1,2;3,4)", "(-1, -2; -3, -4)"},   {"+(1,2;3,4)", "(1, 2; 3, 4)"},      {"(x,x^2)-(1,2)", "(1, 2)"}};
    for (size_t i = 0; i < sizeof cases / sizeof *cases; ++i) {
        string_t *source = string_sprintf("matrix mat(x) { return %s. } x=2. output(mat(x)).", cases[i].body);
        bool ok = source && test_lab_function_output(string_c_str(source), cases[i].expected, true);
        string_free(source);
        TEST_ASSERT_TRUE(ok, "matrix return forms retain dimensions, products and signs");
    }
}

static void test_lab_function_guards_and_names(void)
{
    const char *conditions[] = {"realpart(s)>0 && 1<0", "realpart(s)>0 || 1>0", "1<0 && realpart(s)>0",
                                "1>0 || realpart(s)>0"};
    for (size_t i = 0; i < sizeof conditions / sizeof *conditions; ++i) {
        string_t *source = string_sprintf("expression f(s) { if (%s) { return 1. } else { return 0. } } "
                                          "s=?. output(f(s)).",
                                          conditions[i]);
        bool ok = source && test_lab_function_output(string_c_str(source), i % 2 ? "1" : "0", true);
        string_free(source);
        TEST_ASSERT_TRUE(ok, "known boolean operands short-circuit unknown guards");
    }
    const char *names[] = {"[time]", "[elapsed time]", "[sqrt(2)]", "[x>y]", "[a,b]", "[a&&b]"};
    for (size_t i = 0; i < sizeof names / sizeof *names; ++i) {
        string_t *source = string_sprintf("expression f(%s,const [offset]) { if (%s>0) { return %s+[offset]. } "
                                          "else { return @nan. } } %s=3. const [offset]=4. output(f(%s,[offset])).",
                                          names[i], names[i], names[i], names[i], names[i]);
        bool ok = source && test_lab_function_output(string_c_str(source), "7", true);
        string_free(source);
        TEST_ASSERT_TRUE(ok, "quoted names are tokens rather than programme syntax");
    }
    const char *kinds[] = {"expression", "equation", "matrix"};
    const char *bodies[] = {"x+root", "equation(x=root)", "(x+root,root)"};
    for (size_t i = 0; i < 3; ++i) {
        string_t *source =
            string_sprintf("%s f(x) { const root=sqrt(2). return %s. } x=?. output(f(x)).", kinds[i], bodies[i]);
        bool ok = source && test_lab_function_output(string_c_str(source), "√", false);
        string_free(source);
        TEST_ASSERT_TRUE(ok, "local constants stay exact in every function type");
    }
}

static void test_lab_function_rejected_programmes(void)
{
    const char *sources[] = {"matrix f() { return [1,2]. } output(f()).",
                             "matrix f() { return 1. } output(f()).",
                             "matrix f() { return (1,2;3). } output(f()).",
                             "matrix f() { return (1,2).(3,4). } output(f()).",
                             "expression f(array x) { return x. }",
                             "expression f(x) { return f(x). } x=1. output(f(x)).",
                             "while (1) { output(1). }",
                             "x=1",
                             "output(1*2).",
                             "[time=1.",
                             "output([time).",
                             "output([time]]).",
                             "[a}b]=1.",
                             "expression f(x) { if (x>0) { return x. } else { return 0. } } x=?. output(f(x))."};
    for (size_t i = 0; i < sizeof sources / sizeof *sources; ++i) {
        unsigned status = 0;
        json_t *response = test_lab_function_request("/function-run", "source", sources[i], &status);
        bool ok = status == 422 && test_lab_ok(response, false) && strstr(test_lab_text(response, "error"), "line ");
        if (!ok)
            string_printf("Rejected programme %zu: status %u, %s\n", i, status, test_lab_text(response, "error"));
        json_free(response);
        TEST_ASSERT_TRUE(ok, "malformed or unsupported source returns a native line diagnostic");
    }
}

static void test_lab_function_generated_cards(void)
{
    static const struct {
        const char *route, *key, *source, *expected;
    } cases[] = {{"/eval", "expression", "{sqrt(x^2+y^2) | x=3; y=4}", "5"},
                 {"/eval", "expression", "{sin(x)+cos(x) | x=0}", "1"},
                 {"/eval", "expression", "{[radius]^2 | [radius]=3}", "9"},
                 {"/eval", "expression", "{[elapsed time]^2 | [elapsed time]=3}", "9"},
                 {"/eval", "expression", "{[return]^2 | [return]=3}", "9"},
                 {"/eval", "expression", "{[sqrt(2)]^2 | [sqrt(2)]=3}", "9"},
                 {"/eval", "expression", "sin(x)^2+cos(x)^2", "1"},
                 {"/matrix-eval", "matrix", "{(x,1;0,x) | x=2}", "(2, 1; 0, 2)"},
                 {"/equation-eval", "equation", "26Y=320/9", "Y = ¹⁶⁰⁄₁₁₇"}};
    for (size_t i = 0; i < sizeof cases / sizeof *cases; ++i) {
        unsigned status = 0;
        json_t *response = test_lab_function_request(cases[i].route, cases[i].key, cases[i].source, &status);
        const char *programme = test_lab_text(response, "display_function");
        if (!*programme)
            programme = test_lab_text(response, "function");
        bool ok = status == 200 && test_lab_ok(response, true) && *programme &&
                  test_lab_function_output(programme, cases[i].expected, true);
        if (!ok)
            string_printf("Generated card %zu (%s): status %u, %s\n", i, cases[i].source, status,
                          test_lab_text(response, "error"));
        json_free(response);
        TEST_ASSERT_TRUE(ok, "the displayed Function card executes without Python rewriting");
    }
}

static void test_lab_function_readme_examples(void)
{
    /* README examples: docs/mars-lab.md, Running Function cards. Keep these last. */
    static const struct {
        const char *source, *expected;
    } cases[] = {
        {"matrix mat(x) {\n    const scale = 2.\n    v1 = x^2.\n    return scale.(v1, x; x + 1, v1).\n}\nx = "
         "3.\noutput(mat(x)).\n",
         "(18, 6; 8, 18)"},
        {"expression expr(x, y) {\n    return sqrt(x^2 + y^2).\n}\nx = 3.\ny = 4.\noutput(expr(x, y)).\n", "5"},
        {"equation equ(Y) {\n    return equation(26.Y = 320/9).\n}\n`` Y = ?\noutputa(solve(equ(Y))).\n",
         "Y = ¹⁶⁰⁄₁₁₇"},
        {"expression expr(s) {\n    if (realpart(s) > 0) {\n        return 1/s.\n    } else {\n        return @nan.\n  "
         "  }\n}\ns = ?.\noutput(expr(s)).\n",
         "{ 1/s | s = ?; Re(s) > 0 }"},
        {"expression expr(z) {\n    return integral(sin(x), x, z).\n}\nz = ?.\noutput(expr(z)).\n",
         "{ -cos(z) | z = ? }"},
        {"expression expr(x, const C) {\n    return integral(sin(x), x).\n}\nx = @pi.\nconst C = 0.\noutput(expr(x, "
         "C)).\n",
         "1"},
        {"output(integral(sin(x), x, 0, @pi)).", "2"},
        {"expression expr(x, const C) {\n    return integral(sin(x), x).\n}\nx = ?.\nconst C = ?.\noutput(expr(x, "
         "C)).\n",
         "{ C - cos(x) | x = ?; C = ? }"},
        {"expression expr(x) {\n    return derivative(sin(x), x, 1).\n}\nx = ?.\noutput(expr(x)).\n",
         "{ cos(x) | x = ? }"},
        {"expression expr(s) {\n    if (realpart(s) > 0) {\n        return laplace(integral(sin(x), x, t), t, s).\n    "
         "} else {\n        return @nan.\n    }\n}\ns = ?.\noutput(expr(s)).\n",
         "{ -s/(s² + 1) | s = ?; Re(s) > 0 }"},
        {"expression expr(t) {\n    return inverselaplace(5.(s + 3 + 10/s^2)/(s^2 + 4.s + 5), s, t).\n}\nt = "
         "?.\noutput(expr(t)).\n",
         "{ 10t + exp(-2t)·(13·cos(t) + 11·sin(t)) - 8 | t = ? }"}};
    for (size_t i = 0; i < sizeof cases / sizeof *cases; ++i) {
        TEST_ASSERT_TRUE(test_lab_function_output(cases[i].source, cases[i].expected, true),
                         "README Function example produces its documented output");
        string_printf("README Function example %zu: %s\n", i + 1, cases[i].expected);
    }
}

/* Register runtime contracts before state, HTTP and README cases. */
void test_lab_function_cases(void)
{
    TEST_RUN_IN_GROUP(test_lab_function_scalar_regressions, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_function_matrix_regressions, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_function_guards_and_names, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_function_rejected_programmes, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_function_generated_cards, tests, NULL);
}

/* Run documentation examples only after every ordinary Lab test. */
void test_lab_function_readme_cases(void)
{
    TEST_SECTION("README examples: native Function cards");
    TEST_RUN_IN_GROUP(test_lab_function_readme_examples, tests, NULL);
}
