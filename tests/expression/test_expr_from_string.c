#include "test_expr.h"

#define MARS_SHARED_EXPR_INTERNAL_ACCESS
#include "internal/expr_internal.h"

static void check_parse_num(const char *label, const char *s, const char *expect_text, int line);
static void check_parse_expr(const char *label, const char *s, const char *expect_expr, int line);
static void check_parse_TeX(const char *label, const char *s, const char *expect_TeX, int line);
static void check_parse_simplified_expr(const char *label, const char *s, const char *expect_expr, int line);

static void test_from_string_function_hash(void)
{
    ASSERT_TRUE(expr_stringin_function_hash_is_valid());
}

static void test_from_string_conjugation(void)
{
    check_parse_expr("conj alias parses", "conj(1 + 2i)", "conj(1 + 2i)", __LINE__);
    check_parse_expr("conjugate alias canonicalises", "conjugate(1 + 2i)", "conj(1 + 2i)", __LINE__);
    check_parse_expr("postfix conjugate parses", "(1 + 2i)^*", "conj(1 + 2i)", __LINE__);
    check_parse_num("conj alias evaluates", "conj(1 + 2i)", "1 - 2i", __LINE__);
    check_parse_num("conjugate alias evaluates", "conjugate(1 + 2i)", "1 - 2i", __LINE__);
    check_parse_num("postfix conjugate evaluates", "(1 + 2i)^*", "1 - 2i", __LINE__);
    check_parse_expr("symbolic modulus bars parse", "|z|", "{ |z| | z = NAN }", __LINE__);
    check_parse_val("complex modulus bars evaluate", "|1 + 2i|", sqrt(5.0), __LINE__);
    check_parse_val("complex modulus agrees with sqrt(z z*)",
                    "{ |z| - sqrt(z * z^*) | z = 1 + 2i }", 0.0, __LINE__);
    check_parse_val("abs and modulus bars agree for complex scalars",
                    "{ abs(z) - |z| | z = 1 + 2i }", 0.0, __LINE__);
}

static void test_from_string_pure_const(void)
{
    /* Single Unicode letter name */
    check_parse_val("pure const π", "{ \xcf\x80 = 3.141592653589793 }", 3.141592653589793, __LINE__);
    /* Bracketed multi-character name */
    check_parse_val("pure const [e]", "{ [e] = 2.718281828459045 }", 2.718281828459045, __LINE__);
    /* Negative value */
    check_parse_val("pure const negative", "{ [neg] = -1.5 }", -1.5, __LINE__);
    /* Zero */
    check_parse_val("pure const zero", "{ z = 0 }", 0.0, __LINE__);
    /* Name with spaces (bracketed) */
    check_parse_val("pure const [my const]", "{ [my const] = 42 }", 42.0, __LINE__);
}

/* ---- Arithmetic operators ---- */

static void test_from_string_arithmetic(void)
{
    /* Addition */
    check_parse_val("x\xe2\x82\x80 + x\xe2\x82\x81 = 7",
                    "{ x\xe2\x82\x80 + x\xe2\x82\x81 | x\xe2\x82\x80 = 3, x\xe2\x82\x81 = 4 }", 7.0, __LINE__);
    /* Subtraction */
    check_parse_val("x\xe2\x82\x80 - x\xe2\x82\x81 = 7",
                    "{ x\xe2\x82\x80 - x\xe2\x82\x81 | x\xe2\x82\x80 = 10, x\xe2\x82\x81 = 3 }", 7.0, __LINE__);
    /* Unary negation */
    check_parse_val("-x = -3", "{ -x | x = 3 }", -3.0, __LINE__);
    /* Implicit multiplication (juxtaposition) */
    check_parse_val("x\xe2\x82\x80x\xe2\x82\x81 implicit = 12",
                    "{ x\xe2\x82\x80x\xe2\x82\x81 | x\xe2\x82\x80 = 3, x\xe2\x82\x81 = 4 }", 12.0, __LINE__);
    /* Explicit middle-dot multiplication */
    check_parse_val("x\xe2\x82\x80\xc2\xb7x\xe2\x82\x81 middle-dot = 12",
                    "{ x\xe2\x82\x80\xc2\xb7x\xe2\x82\x81 | x\xe2\x82\x80 = 3, x\xe2\x82\x81 = 4 }", 12.0, __LINE__);
    /* Ordinary full-stop multiplication */
    check_parse_val("x\xe2\x82\x80.x\xe2\x82\x81 full-stop = 12",
                    "{ x\xe2\x82\x80.x\xe2\x82\x81 | x\xe2\x82\x80 = 3, x\xe2\x82\x81 = 4 }", 12.0, __LINE__);
    /* Superscript ² */
    check_parse_val("x\xc2\xb2 = 9", "{ x\xc2\xb2 | x = 3 }", 9.0, __LINE__);
    /* Superscript ³ */
    check_parse_val("x\xc2\xb3 = 8", "{ x\xc2\xb3 | x = 2 }", 8.0, __LINE__);
    /* Caret exponent */
    check_parse_val("x^2.5 at 4 = 32", "{ x^2.5 | x = 4 }", 32.0, __LINE__);
    check_parse_expr("power binds before following division", "{ x^2/6 | x = NAN }", "{ x²/6 | x = NAN }", __LINE__);
    check_parse_expr("parenthesised rational remains an exponent", "{ x^(2/6) | x = NAN }", "{ x^⅓ | x = NAN }",
                     __LINE__);
    check_parse_num("decimal binding stays exact through cancellation-sensitive cube",
                    "{ (5x)^3 | x = 128064.000000000120974 }",
                    "262537412640768744007706072064702817249456224221302405921303/"
                    "1000000000000000000000000000000000000000000",
                    __LINE__);
    /* Parenthesised sub-expression with superscript */
    check_parse_val("(x\xe2\x82\x80 + x\xe2\x82\x81)\xc2\xb2 = 25",
                    "{ (x\xe2\x82\x80 + x\xe2\x82\x81)\xc2\xb2 | x\xe2\x82\x80 = 2, x\xe2\x82\x81 = 3 }", 25.0,
                    __LINE__);
    check_parse_simplified_expr("parenthesised exponent can itself be powered", "{ a^(ix)\xc2\xb2 | x = NAN; a = NAN }",
                                "{ a^(2ix) | x = NAN; a = NAN }", __LINE__);
    check_parse_simplified_expr("ASCII parenthesised exponent can itself be powered", "{ a^(ix)^2 | x = NAN; a = NAN }",
                                "{ a^(2ix) | x = NAN; a = NAN }", __LINE__);
    check_parse_simplified_expr("unparenthesised chained powers are left associative", "{ a^x^2 | x = NAN; a = NAN }",
                                "{ a^(2x) | x = NAN; a = NAN }", __LINE__);
    check_parse_simplified_expr("explicit exponent power stays inside exponent", "{ a^((ix)^2) | x = NAN; a = NAN }",
                                "{ a^(-x\xc2\xb2) | x = NAN; a = NAN }", __LINE__);
    check_parse_simplified_expr("power of symbolic power folds integer exponent",
                                "{ (a^(-x))\xc2\xb2 | x = NAN; a = NAN }", "{ a^(-2x) | x = NAN; a = NAN }", __LINE__);
    /* Chained addition */
    check_parse_val(
        "x\xe2\x82\x80 + x\xe2\x82\x81 + x\xe2\x82\x82 = 6",
        "{ x\xe2\x82\x80 + x\xe2\x82\x81 + x\xe2\x82\x82 | x\xe2\x82\x80 = 1, x\xe2\x82\x81 = 2, x\xe2\x82\x82 = 3 }",
        6.0, __LINE__);
    /* Mixed add and implicit mul: 2x + 3y */
    check_parse_val("c\xe2\x82\x80x\xe2\x82\x80 + c\xe2\x82\x81x\xe2\x82\x81 = 13",
                    "{ c\xe2\x82\x80x\xe2\x82\x80 + c\xe2\x82\x81x\xe2\x82\x81 | x\xe2\x82\x80 = 2, x\xe2\x82\x81 = 3; "
                    "c\xe2\x82\x80 = 2, c\xe2\x82\x81 = 3 }",
                    13.0, __LINE__);
}

/* ---- Elementary functions ---- */

static void test_from_string_functions(void)
{
    /* All 16 unary functions at values where the result is exact or 0/1 */
    check_parse_val("sin(x) at 0", "{ sin(x) | x = 0 }", 0.0, __LINE__);
    check_parse_val("cos(x) at 0", "{ cos(x) | x = 0 }", 1.0, __LINE__);
    check_parse_val("tan(x) at 0", "{ tan(x) | x = 0 }", 0.0, __LINE__);
    check_parse_val("sinh(x) at 0", "{ sinh(x) | x = 0 }", 0.0, __LINE__);
    check_parse_val("cosh(x) at 0", "{ cosh(x) | x = 0 }", 1.0, __LINE__);
    check_parse_val("tanh(x) at 0", "{ tanh(x) | x = 0 }", 0.0, __LINE__);
    check_parse_val("sec(x) at 0", "{ sec(x) | x = 0 }", 1.0, __LINE__);
    check_parse_val("cosec(x) at π/2", "{ cosec(x) | x = pi/2 }", 1.0, __LINE__);
    check_parse_val("csc(x) at π/2", "{ csc(x) | x = pi/2 }", 1.0, __LINE__);
    check_parse_val("cot(x) at π/4", "{ cot(x) | x = pi/4 }", 1.0, __LINE__);
    check_parse_val("sech(x) at 0", "{ sech(x) | x = 0 }", 1.0, __LINE__);
    check_parse_val("cosech(asinh(1))", "{ cosech(asinh(x)) | x = 1 }", 1.0, __LINE__);
    check_parse_val("csch(asinh(1))", "{ csch(asinh(x)) | x = 1 }", 1.0, __LINE__);
    check_parse_val("coth(atanh(0.5))", "{ coth(atanh(x)) | x = 0.5 }", 2.0, __LINE__);
    check_parse_val("asin(x) at 0", "{ asin(x) | x = 0 }", 0.0, __LINE__);
    check_parse_val("acos(x) at 1", "{ acos(x) | x = 1 }", 0.0, __LINE__);
    check_parse_val("atan(x) at 0", "{ atan(x) | x = 0 }", 0.0, __LINE__);
    check_parse_val("asinh(x) at 0", "{ asinh(x) | x = 0 }", 0.0, __LINE__);
    check_parse_val("acosh(x) at 1", "{ acosh(x) | x = 1 }", 0.0, __LINE__);
    check_parse_val("atanh(x) at 0", "{ atanh(x) | x = 0 }", 0.0, __LINE__);
    check_parse_val("asec(x) at 1", "{ asec(x) | x = 1 }", 0.0, __LINE__);
    check_parse_val("arcsec(x) at 1", "{ arcsec(x) | x = 1 }", 0.0, __LINE__);
    check_parse_val("acosec(x) at 1", "{ acosec(x) | x = 1 }", M_PI / 2.0, __LINE__);
    check_parse_val("arccosec(x) at 1", "{ arccosec(x) | x = 1 }", M_PI / 2.0, __LINE__);
    check_parse_val("acsc(x) at 1", "{ acsc(x) | x = 1 }", M_PI / 2.0, __LINE__);
    check_parse_val("arccsc(x) at 1", "{ arccsc(x) | x = 1 }", M_PI / 2.0, __LINE__);
    check_parse_val("acot(x) at 1", "{ acot(x) | x = 1 }", M_PI / 4.0, __LINE__);
    check_parse_val("arccot(x) at 1", "{ arccot(x) | x = 1 }", M_PI / 4.0, __LINE__);
    check_parse_val("asech(x) at 1", "{ asech(x) | x = 1 }", 0.0, __LINE__);
    check_parse_val("arsech(x) at 1", "{ arsech(x) | x = 1 }", 0.0, __LINE__);
    check_parse_val("acosech(x) at 1", "{ acosech(x) | x = 1 }", asinh(1.0), __LINE__);
    check_parse_val("arcosech(x) at 1", "{ arcosech(x) | x = 1 }", asinh(1.0), __LINE__);
    check_parse_val("acsch(x) at 1", "{ acsch(x) | x = 1 }", asinh(1.0), __LINE__);
    check_parse_val("arcsch(x) at 1", "{ arcsch(x) | x = 1 }", asinh(1.0), __LINE__);
    check_parse_val("acoth(x) at 2", "{ acoth(x) | x = 2 }", atanh(0.5), __LINE__);
    check_parse_val("arcoth(x) at 2", "{ arcoth(x) | x = 2 }", atanh(0.5), __LINE__);
    check_parse_val("exp(x) at 0", "{ exp(x) | x = 0 }", 1.0, __LINE__);
    check_parse_val("ln(x) at 1", "{ ln(x) | x = 1 }", 0.0, __LINE__);
    check_parse_val("log(x) at 1000", "{ log(x) | x = 1000 }", 3.0, __LINE__);
    check_parse_val("log10(x) at 1000", "{ log10(x) | x = 1000 }", 3.0, __LINE__);
    check_parse_val("lg(x) at 1000", "{ lg(x) | x = 1000 }", 3.0, __LINE__);
    check_parse_val("sqrt(x) at 4", "{ sqrt(x) | x = 4 }", 2.0, __LINE__);
    check_parse_val("floor(x) at 1.75", "{ floor(x) | x = 1.75 }", 1.0, __LINE__);
    check_parse_val("ceil(x) at 1.25", "{ ceil(x) | x = 1.25 }", 2.0, __LINE__);
    check_parse_num("10! lowers to gamma(11)", "{ 10! }", "3628800", __LINE__);
    check_parse_num("x! lowers to gamma(x+1)", "{ x! | x = 5 }", "120", __LINE__);
    check_parse_num("exp(pi*i/2) = i", "{ exp(@pi*i/2) }", "i", __LINE__);
    check_parse_expr("1/pi stays symbolic", "{ 1/pi }", "1/π", __LINE__);
    check_parse_expr("pi/2 stays symbolic", "{ pi/2 }", "π/2", __LINE__);
    check_parse_expr("i^i stays symbolic", "{ i^i }", "i^i", __LINE__);
    check_parse_expr("i^(1+i) parses", "{ i^(1+i) }", "i^(1 + i)", __LINE__);
    check_parse_simplified_expr("W0 identity keeps unbound argument", "{ W_0(x)*exp(W_0(x)) }", "{ x | x = NAN }",
                                __LINE__);
    check_parse_simplified_expr("W0 identity resolves bound pi", "{ W_0(x)*exp(W_0(x)) | x = pi }", "π", __LINE__);
    check_parse_simplified_expr("W-1 identity resolves bound e", "{ W-1(x)*exp(W-1(x)) | x = e }", "e", __LINE__);
    check_parse_expr("W identity recognises e power", "{ W(2e^2) }", "2", __LINE__);
    check_parse_val("√(x) at 4", "{ √(x) | x = 4 }", 2.0, __LINE__);
    /* Binary functions */
    check_parse_val("atan2(1, 1) = π/4",
                    "{ atan2(x\xe2\x82\x80, x\xe2\x82\x81) | x\xe2\x82\x80 = 1, x\xe2\x82\x81 = 1 }", M_PI / 4.0,
                    __LINE__);
    check_parse_val("pow(2, 3) = 8", "{ pow(x\xe2\x82\x80, x\xe2\x82\x81) | x\xe2\x82\x80 = 2, x\xe2\x82\x81 = 3 }",
                    8.0, __LINE__);
    /* Superscript on function name: sin²(x) */
    check_parse_val("sin\xc2\xb2(x) at 0", "{ sin\xc2\xb2(x) | x = 0 }", 0.0, __LINE__);
    {
        expr_t *expr = expr_from_string("{ x^2 | x = ? }", NULL);
        if (expr && expr_is_differentiable(expr)) {
            printf(C_BOLD C_GREEN "PASS" C_RESET " constant-exponent power keeps expression differentiable\n\n");
        } else {
            printf(C_BOLD C_RED "FAIL" C_RESET " constant-exponent power keeps expression differentiable %s:%d:1\n\n",
                   __FILE__, __LINE__);
            TEST_FAIL();
        }
        expr_free(expr);
    }
    /* Nested function calls */
    check_parse_val("exp(sin(x)) at 0 = 1", "{ exp(sin(x)) | x = 0 }", 1.0, __LINE__);
    check_parse_val("Greek aliases in ordinary expressions parse", "{ sin(@alpha) + @beta | @alpha = 0, @beta = 1 }",
                    1.0, __LINE__);
    check_parse_val("sqrt(exp(x)) at 0 = 1", "{ sqrt(exp(x)) | x = 0 }", 1.0, __LINE__);
    /* Function applied to parenthesised expression */
    check_parse_val("sin(x\xe2\x82\x80 + x\xe2\x82\x81) = sin(π/2) = 1",
                    "{ sin(x\xe2\x82\x80 + x\xe2\x82\x81) | x\xe2\x82\x80 = 0, x\xe2\x82\x81 = 0 }", 0.0, __LINE__);
}

static void test_from_text_api(void)
{
    string_t *text = string_new_with("{ cdf(x) - cdf(-x) | x = 1 }");
    expr_bindings_t *bindings = NULL;
    expr_t *expr;
    number_t value;

    ASSERT_NOT_NULL(text);
    expr = expr_from_text(text, &bindings);
    ASSERT_NOT_NULL(expr);
    ASSERT_NOT_NULL(bindings);

    value = expr_eval(expr);
    ASSERT_FALSE(num_is_nan(value));

    num_destroy(&value);
    expr_bindings_free(bindings);
    expr_free(expr);
    string_free(text);
}

/* ---- string_t-based parser entry point ---- */

/* ---- Special functions (the 18 new ops) ---- */

static void test_from_string_special_functions(void)
{
    /* Unary — clean exact values */
    check_parse_val("abs(-3) = 3", "{ abs(x) | x = -3 }", 3.0, __LINE__);
    check_parse_val("|-3| = 3", "{ |x| | x = -3 }", 3.0, __LINE__);
    check_parse_val("erf(0) = 0", "{ erf(x) | x = 0 }", 0.0, __LINE__);
    check_parse_val("erfc(0) = 1", "{ erfc(x) | x = 0 }", 1.0, __LINE__);
    check_parse_val("erfinv(0) = 0", "{ erfinv(x) | x = 0 }", 0.0, __LINE__);
    check_parse_val("erfcinv(1) = 0", "{ erfcinv(x) | x = 1 }", 0.0, __LINE__);
    check_parse_val("gamma(3) = 2", "{ gamma(x) | x = 3 }", 2.0, __LINE__);
    check_parse_val("Γ(3) = 2", "{ Γ(x) | x = 3 }", 2.0, __LINE__);
    check_parse_val("gammainv(gamma(2.5)) = 2.5", "{ gammainv(x) | x = 1.329340388179137 }", 2.5, __LINE__);
    check_parse_val("lgamma(1) = 0", "{ lgamma(x) | x = 1 }", 0.0, __LINE__);
    check_parse_val("digamma(1) = -gamma_E", "{ digamma(x) | x = 1 }", -0.5772156649015329, __LINE__);
    check_parse_val("ψ⁽⁰⁾(1) = -gamma_E", "{ ψ⁽⁰⁾(x) | x = 1 }", -0.5772156649015329, __LINE__);
    check_parse_val("ψ⁽¹⁾(1) = pi²/6", "{ ψ⁽¹⁾(x) | x = 1 }", M_PI * M_PI / 6.0, __LINE__);
    check_parse_val("polygamma(3, 2) = pi^4/15 - 6", "{ polygamma(3, x) | x = 2 }", pow(M_PI, 4.0) / 15.0 - 6.0,
                    __LINE__);
    check_parse_val("ψ⁽³⁾(2) = pi^4/15 - 6", "{ ψ⁽³⁾(x) | x = 2 }", pow(M_PI, 4.0) / 15.0 - 6.0, __LINE__);
    check_parse_expr("polygamma expression uses standard symbol", "{ polygamma(2, x) | x = ? }",
                     "{ ψ⁽²⁾(x) | x = NAN }", __LINE__);
    check_parse_val("dilog(1/2) = pi^2/12 - log(2)^2/2", "{ dilog(x) | x = 0.5 }", 0.5822405264650125, __LINE__);
    check_parse_val("Li2(1/2) aliases dilog", "{ Li2(x) | x = 0.5 }", 0.5822405264650125, __LINE__);
    check_parse_val("polylog(2, 1/2) = dilog(1/2)", "{ polylog(2, x) | x = 0.5 }", 0.5822405264650125, __LINE__);
    check_parse_val("pFq general 0F0 identity", "{ pFq(0, 0, x) | x = 0.2 }", exp(0.2), __LINE__);
    check_parse_val("HypergeometricpFq general 1F0 identity", "{ HypergeometricpFq(1, 0, 2, x) | x = 0.2 }",
                    1.0 / (0.8 * 0.8), __LINE__);
    check_parse_val("Lauricella FD general product identity",
                    "{ lauricella_f(3, 1.25, 0.5, 1.5, 2, 1.25, 0.1, 0.2, 0.15) }",
                    pow(0.9, -0.5) * pow(0.8, -1.5) * pow(0.85, -2.0), __LINE__);
    {
        expr_t *expr = expr_from_string("{ Γ(x + 1)·(ψ⁽¹⁾(x + 1) + ψ⁽⁰⁾²(x + 1)) | x = ? }", NULL);
        if (expr && expr_is_differentiable(expr)) {
            printf(C_BOLD C_GREEN "PASS" C_RESET " polygamma order parameter keeps expression differentiable\n\n");
        } else {
            printf(C_BOLD C_RED "FAIL" C_RESET " polygamma order parameter keeps expression differentiable %s:%d:1\n\n",
                   __FILE__, __LINE__);
            TEST_FAIL();
        }
        expr_free(expr);
    }
    check_parse_val("W₀(0) = 0 via lambert_w0", "{ lambert_w0(x) | x = 0 }", 0.0, __LINE__);
    check_parse_val("W₀(0) = 0 via productlog", "{ productlog(x) | x = 0 }", 0.0, __LINE__);
    check_parse_val("W₀(0) = 0", "{ W₀(x) | x = 0 }", 0.0, __LINE__);
    check_parse_val("W₀(0) = 0 via W", "{ W(x) | x = 0 }", 0.0, __LINE__);
    check_parse_val("W₀(0) = 0 via W0", "{ W0(x) | x = 0 }", 0.0, __LINE__);
    check_parse_val("W₀(0) = 0 via W_0", "{ W_0(x) | x = 0 }", 0.0, __LINE__);
    check_parse_expr("W(x) infers x binding", "{ W(x) }", "{ W(x) | x = NAN }", __LINE__);
    check_parse_expr("W_0(x) infers x binding", "{ W_0(x) }", "{ W₀(x) | x = NAN }", __LINE__);
    check_parse_expr("lambert_wn parses branch-aware Lambert W", "{ lambert_wn(2, x) | x = ? }",
                     "{ Wₙ(2, x) | x = NAN }", __LINE__);
    check_parse_expr("Wn parses branch-aware Lambert W", "{ Wn(2, x) | x = ? }", "{ Wₙ(2, x) | x = NAN }", __LINE__);
    check_parse_expr("W_n parses branch-aware Lambert W", "{ W_n(2, x) | x = ? }", "{ Wₙ(2, x) | x = NAN }", __LINE__);
    check_parse_expr("Wₙ parses branch-aware Lambert W", "{ Wₙ(2, x) | x = ? }", "{ Wₙ(2, x) | x = NAN }", __LINE__);
    check_parse_val("W₋₁(-0.2) via lambert_wm1", "{ lambert_wm1(x) | x = -0.2 }", -2.5426413577735265, __LINE__);
    check_parse_val("W₋₁(-0.2)", "{ W₋₁(x) | x = -0.2 }", -2.5426413577735265, __LINE__);
    check_parse_val("W₋₁(-0.2) via W-1", "{ W-1(x) | x = -0.2 }", -2.5426413577735265, __LINE__);
    check_parse_val("W₋₁(-0.2) via W_-1", "{ W_-1(x) | x = -0.2 }", -2.5426413577735265, __LINE__);
    check_parse_val("W₋₁ identity evaluates bound e", "{ W-1(x)*exp(W-1(x)) | x = e }", M_E, __LINE__);
    check_parse_val("normal_pdf(0)", "{ normal_pdf(x) | x = 0 }", 1.0 / sqrt(2.0 * M_PI), __LINE__);
    check_parse_val("normal_cdf(0) = 0.5", "{ normal_cdf(x) | x = 0 }", 0.5, __LINE__);
    check_parse_val("normal_logpdf(0)", "{ normal_logpdf(x) | x = 0 }", -0.5 * log(2.0 * M_PI), __LINE__);
    check_parse_val("pdf(0) aliases normal_pdf", "{ pdf(x) | x = 0 }", 1.0 / sqrt(2.0 * M_PI), __LINE__);
    check_parse_val("cdf(0) aliases normal_cdf", "{ cdf(x) | x = 0 }", 0.5, __LINE__);
    check_parse_val("logpdf(0) aliases normal_logpdf", "{ logpdf(x) | x = 0 }", -0.5 * log(2.0 * M_PI), __LINE__);
    check_parse_expr("cdf alias keeps user spelling", "{ cdf(1.96) - cdf(-1.96) }", "cdf(1.96) - cdf(-1.96)", __LINE__);
    check_parse_expr("pdf and logpdf aliases keep user spelling", "{ pdf(0) + logpdf(0) }", "pdf(0) + logpdf(0)",
                     __LINE__);
    {
        expr_bindings_t *bindings = NULL;
        expr_t *expr = expr_from_string("{ cdf(x) | x = ? }", &bindings);
        expr_t *x = bindings ? expr_bindings_get(bindings, "x") : NULL;
        expr_t *deriv = (expr && x) ? expr_create_deriv(expr, x) : NULL;
        char *text = deriv ? expr_to_string(deriv, style_EXPRESSION) : NULL;

        if (text && strstr(text, "pdf(x)") && !strstr(text, "normal_pdf")) {
            printf(C_BOLD C_GREEN "PASS" C_RESET " cdf alias derivative keeps short pdf spelling\n");
            printf(C_BOLD "  expr   " C_RESET "%s\n\n", text);
        } else {
            printf(C_BOLD C_RED "FAIL" C_RESET " cdf alias derivative keeps short pdf spelling %s:%d:1\n", __FILE__,
                   __LINE__);
            printf(C_BOLD "  got    " C_RESET "%s\n\n", text ? text : "(null)");
            TEST_FAIL();
        }

        free(text);
        expr_free(deriv);
        expr_free(expr);
        expr_bindings_free(bindings);
    }
    check_parse_val("Ei(1)", "{ Ei(x) | x = 1 }", 1.8951178163559367, __LINE__);
    check_parse_val("E1(1)", "{ E1(x) | x = 1 }", 0.21938393439552029, __LINE__);
    /* Binary functions */
    check_parse_val("beta(1,1) = 1", "{ beta(x, y) | x = 1, y = 1 }", 1.0, __LINE__);
    check_parse_val("logbeta(1,1) = 0", "{ logbeta(x, y) | x = 1, y = 1 }", 0.0, __LINE__);
    check_parse_val("gammainc_lower(1,1) = 1-exp(-1)", "{ gammainc_lower(s, x) | s = 1, x = 1 }", 1.0 - exp(-1.0),
                    __LINE__);
    check_parse_val("gammainc_upper(1,1) = exp(-1)", "{ gammainc_upper(s, x) | s = 1, x = 1 }", exp(-1.0), __LINE__);
    check_parse_val("gammainc_P(1,1) = 1-exp(-1)", "{ gammainc_P(s, x) | s = 1, x = 1 }", 1.0 - exp(-1.0), __LINE__);
    check_parse_val("gammainc_Q(1,1) = exp(-1)", "{ gammainc_Q(s, x) | s = 1, x = 1 }", exp(-1.0), __LINE__);
    check_parse_val("beta_pdf(0.5,2,3) = 1.5", "{ beta_pdf(x, a, b) | x = 0.5, a = 2, b = 3 }", 1.5, __LINE__);
    check_parse_val("logbeta_pdf(0.5,1,1) = 0", "{ logbeta_pdf(x, a, b) | x = 0.5, a = 1, b = 1 }", 0.0, __LINE__);
    check_parse_val("binomial(5,2) = 10", "{ binomial(n, k) | n = 5, k = 2 }", 10.0, __LINE__);
    check_parse_val("hypot(3,4) = 5", "{ hypot(x, y) | x = 3, y = 4 }", 5.0, __LINE__);
}

static void test_from_string_exact_value_functions(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *expr = NULL;
    expr_t *deriv = NULL;
    number_t deriv_value;

    check_parse_num("factorial(10) = 10!", "{ factorial(n) | n = 10 }", "3628800", __LINE__);
    check_parse_num("fibonacci(50)", "{ fibonacci(n) | n = 50 }", "12586269025", __LINE__);
    check_parse_num("partition(5)", "{ partition(n) | n = 5 }", "7", __LINE__);
    check_parse_num("isqrt(200)", "{ isqrt(n) | n = 200 }", "14", __LINE__);
    check_parse_num("gcd(84, 30)", "{ gcd(a, b) | a = 84, b = 30 }", "6", __LINE__);
    check_parse_num("lcm(-21, 6)", "{ lcm(a, b) | a = -21, b = 6 }", "42", __LINE__);
    check_parse_num("mod(29, 5)", "{ mod(a, b) | a = 29, b = 5 }", "4", __LINE__);
    check_parse_num("modinv(3, 11)", "{ modinv(a, b) | a = 3, b = 11 }", "4", __LINE__);
    check_parse_num("is_prime(97)", "{ is_prime(n) | n = 97 }", "1", __LINE__);
    check_parse_num("is_prime(221)", "{ is_prime(n) | n = 221 }", "0", __LINE__);
    check_parse_num("next_prime(14)", "{ next_prime(n) | n = 14 }", "17", __LINE__);
    check_parse_num("prev_prime(14)", "{ prev_prime(n) | n = 14 }", "13", __LINE__);
    check_parse_num("AND(13, 11)", "{ AND(a, b) | a = 13, b = 11 }", "9", __LINE__);
    check_parse_num("OR(13, 11)", "{ OR(a, b) | a = 13, b = 11 }", "15", __LINE__);
    check_parse_num("XOR(13, 8)", "{ XOR(a, b) | a = 13, b = 8 }", "5", __LINE__);
    check_parse_num("NOT(8)", "{ NOT(a) | a = 8 }", "7", __LINE__);
    check_parse_num("SHL(7, 3)", "{ SHL(a, n) | a = 7, n = 3 }", "56", __LINE__);
    check_parse_num("SHR(56, 3)", "{ SHR(a, n) | a = 56, n = 3 }", "7", __LINE__);
    check_parse_num("factors(360)", "{ a₀³·a₁²·a₂ | ; a₀ = 2, a₁ = 3, a₂ = 5 }", "360", __LINE__);

    expr = expr_from_string("{ gcd(x, y) | x = 84, y = 30 }", &bindings);
    if (!expr || expr_is_differentiable(expr)) {
        printf(C_BOLD C_RED "FAIL" C_RESET " gcd expression is marked non-differentiable %s:%d:1\n\n", __FILE__,
               __LINE__);
        TEST_FAIL();
    } else {
        printf(C_BOLD C_GREEN "PASS" C_RESET " gcd expression is marked non-differentiable\n\n");
    }

    if (expr && bindings) {
        expr_t *x = expr_bindings_get(bindings, "x");

        deriv = x ? expr_create_deriv(expr, x) : NULL;
        deriv_value = deriv ? expr_eval(deriv) : NUM_ZERO;
        if (deriv && num_is_nan(deriv_value)) {
            printf(C_BOLD C_GREEN "PASS" C_RESET " non-differentiable derivative evaluates to NaN\n\n");
        } else {
            printf(C_BOLD C_RED "FAIL" C_RESET " non-differentiable derivative evaluates to NaN %s:%d:1\n\n", __FILE__,
                   __LINE__);
            TEST_FAIL();
        }
        num_destroy(&deriv_value);
    }

    expr_free(deriv);
    expr_bindings_free(bindings);
    expr_free(expr);

    bindings = NULL;
    expr = expr_from_string("{ 1/2*(ln(x^2 + 3*x + 5) "
                            "- 2*atan((2*x + 3)/sqrt(11))/sqrt(11)) + C_0 "
                            "| x = NAN; C_0 = NAN }",
                            &bindings);
    TEST_ASSERT_TRUE(expr != NULL, "ln/atan antiderivative binding parse succeeds");
    TEST_ASSERT_TRUE(expr_bindings_count(bindings) == 2u, "function names add no implicit bindings");
    TEST_ASSERT_TRUE(expr_bindings_get(bindings, "x") != NULL, "antiderivative retains x binding");
    TEST_ASSERT_TRUE(expr_bindings_get(bindings, "C_0") != NULL, "antiderivative retains C_0 binding");
    TEST_ASSERT_TRUE(expr_bindings_get(bindings, "l") == NULL && expr_bindings_get(bindings, "n") == NULL &&
                         expr_bindings_get(bindings, "a") == NULL,
                     "ln and atan letters are not bindings");
    expr_bindings_free(bindings);
    expr_free(expr);
}

/* ---- Named constants (binding section) ---- */

static void test_from_string_named_consts(void)
{
    /* No ';' means all bindings are variables */
    check_parse_val("x + y (implicit all vars) = 5", "{ x + y | x = 2, y = 3 }", 5.0, __LINE__);
    /* Named constant after ';' combined with a variable */
    check_parse_val("c\xe2\x82\x80x\xe2\x82\x80\xc2\xb2 = 12",
                    "{ c\xe2\x82\x80x\xe2\x82\x80\xc2\xb2 | x\xe2\x82\x80 = 2; c\xe2\x82\x80 = 3 }", 12.0, __LINE__);
    /* Leading ';' means all bindings are named constants */
    check_parse_val("c\xe2\x82\x80 + c\xe2\x82\x81 (implicit all consts) = 3",
                    "{ c\xe2\x82\x80 + c\xe2\x82\x81 | ; c\xe2\x82\x80 = 1, c\xe2\x82\x81 = 2 }", 3.0, __LINE__);
    /* Named constant and variable */
    check_parse_val("c\xe2\x82\x80 + x (const + var) = 15", "{ c\xe2\x82\x80 + x | x = 5; c\xe2\x82\x80 = 10 }", 15.0,
                    __LINE__);
    /* Two named constants in the const section */
    check_parse_val("c\xe2\x82\x80x\xe2\x82\x80 + c\xe2\x82\x81 = 2π+e",
                    "{ c\xe2\x82\x80x\xe2\x82\x80 + c\xe2\x82\x81 | x\xe2\x82\x80 = 2; c\xe2\x82\x80 = "
                    "3.141592653589793, c\xe2\x82\x81 = 2.718281828459045 }",
                    2 * 3.141592653589793 + 2.718281828459045, __LINE__);
    /* Bracketed name in const section */
    check_parse_val("[scale]·x = 6", "{ [scale]x | x = 3; [scale] = 2 }", 6.0, __LINE__);
}

/* ---- Bracketed (multi-character) names ---- */

static void test_from_string_bracketed_names(void)
{
    check_parse_val("[radius]\xc2\xb2 = 25", "{ [radius]\xc2\xb2 | [radius] = 5 }", 25.0, __LINE__);
    check_parse_val("[base]\xc2\xb7[height] = 12", "{ [base]\xc2\xb7[height] | [base] = 3, [height] = 4 }", 12.0,
                    __LINE__);
    check_parse_val("[my var] alone = 7", "{ [my var] | [my var] = 7 }", 7.0, __LINE__);
    check_parse_val("[x']\xc2\xb2 = 36", "{ [x']\xc2\xb2 | [x'] = 6 }", 36.0, __LINE__);
    check_parse_val("[2pi]\xc2\xb7x = 2π", "{ [2pi]x | x = 1; [2pi] = 6.283185307179586 }", 6.283185307179586,
                    __LINE__);
    /* Bracketed name as named const in pure-const format */
    check_parse_val("[my const] pure const = 99", "{ [my const] = 99 }", 99.0, __LINE__);
}

static void test_from_string_name_normalisation(void)
{
    expr_t *a1 = test_expr_new_named_var_d(1.0, "a1");
    expr_t *a12 = test_expr_new_named_var_d(1.0, "a12");
    expr_t *a123 = test_expr_new_named_var_d(1.0, "a123");
    expr_t *pi1 = test_expr_new_named_var_d(1.0, "@pi1");
    expr_t *pi2 = test_expr_new_named_var_d(1.0, "@pi_2");
    expr_t *parsed_pi1 = expr_from_string("{ @pi1 }", NULL);
    char *a1s = a1 ? expr_to_string(a1, style_EXPRESSION) : NULL;
    char *a12s = a12 ? expr_to_string(a12, style_EXPRESSION) : NULL;
    char *a123s = a123 ? expr_to_string(a123, style_EXPRESSION) : NULL;
    char *pi1s = pi1 ? expr_to_string(pi1, style_EXPRESSION) : NULL;
    char *pi2s = pi2 ? expr_to_string(pi2, style_EXPRESSION) : NULL;
    char *parsed_pi1s = parsed_pi1 ? expr_to_string(parsed_pi1, style_EXPRESSION) : NULL;

    if (a1s && str_eq(a1s, "{ a₁ | a₁ = 1 }")) {
        to_string_pass("normalise a1", a1s, "{ a₁ | a₁ = 1 }");
    } else {
        to_string_fail(__FILE__, __LINE__, 1, "normalise a1", a1s ? a1s : "(null)", "{ a₁ | a₁ = 1 }");
    }

    if (a12s && str_eq(a12s, "{ a₁₂ | a₁₂ = 1 }")) {
        to_string_pass("normalise a12", a12s, "{ a₁₂ | a₁₂ = 1 }");
    } else {
        to_string_fail(__FILE__, __LINE__, 1, "normalise a12", a12s ? a12s : "(null)", "{ a₁₂ | a₁₂ = 1 }");
    }

    if (a123s && str_eq(a123s, "{ a₁₂₃ | a₁₂₃ = 1 }")) {
        to_string_pass("normalise a123", a123s, "{ a₁₂₃ | a₁₂₃ = 1 }");
    } else {
        to_string_fail(__FILE__, __LINE__, 1, "normalise a123", a123s ? a123s : "(null)", "{ a₁₂₃ | a₁₂₃ = 1 }");
    }

    if (pi1s && str_eq(pi1s, "{ π₁ | π₁ = 1 }")) {
        to_string_pass("normalise @pi1", pi1s, "{ π₁ | π₁ = 1 }");
    } else {
        to_string_fail(__FILE__, __LINE__, 1, "normalise @pi1", pi1s ? pi1s : "(null)", "{ π₁ | π₁ = 1 }");
    }

    if (pi2s && str_eq(pi2s, "{ π₂ | π₂ = 1 }")) {
        to_string_pass("normalise @pi_2", pi2s, "{ π₂ | π₂ = 1 }");
    } else {
        to_string_fail(__FILE__, __LINE__, 1, "normalise @pi_2", pi2s ? pi2s : "(null)", "{ π₂ | π₂ = 1 }");
    }

    if (parsed_pi1s && str_eq(parsed_pi1s, "{ π₁ | π₁ = NAN }")) {
        to_string_pass("implicit @pi1 stays variable", parsed_pi1s, "{ π₁ | π₁ = NAN }");
    } else {
        to_string_fail(__FILE__, __LINE__, 1, "implicit @pi1 stays variable", parsed_pi1s ? parsed_pi1s : "(null)",
                       "{ π₁ | π₁ = NAN }");
    }

    if (parsed_pi1 && qf_isnan(expr_eval_qf(parsed_pi1))) {
        printf(C_BOLD C_GREEN "PASS" C_RESET " implicit @pi1 evaluates to NaN\n\n");
    } else {
        printf(C_BOLD C_RED "FAIL" C_RESET " implicit @pi1 evaluates to NaN %s:%d:1\n\n", __FILE__, __LINE__);
        TEST_FAIL();
    }

    free(parsed_pi1s);
    free(pi2s);
    free(pi1s);
    free(a123s);
    free(a12s);
    free(a1s);
    expr_free(parsed_pi1);
    expr_free(pi2);
    expr_free(pi1);
    expr_free(a123);
    expr_free(a12);
    expr_free(a1);
}

static void test_from_string_implicit_symbolic_bindings(void)
{
    expr_t *x = expr_from_string("{ x }", NULL);
    expr_t *x_pow_n = expr_from_string("{ x^n }", NULL);
    expr_t *e = expr_from_string("{ e }", NULL);
    expr_t *i_unit = expr_from_string("{ i }", NULL);
    expr_t *pi_ascii = expr_from_string("{ pi }", NULL);
    expr_t *phi_ascii = expr_from_string("{ phi }", NULL);
    expr_t *gamma_ascii = expr_from_string("{ gamma }", NULL);
    expr_t *pi_alias = expr_from_string("{ @pi }", NULL);
    expr_t *tau = expr_from_string("{ τ }", NULL);
    expr_t *phi_alias = expr_from_string("{ @phi }", NULL);
    expr_t *gamma_alias = expr_from_string("{ @gamma }", NULL);
    expr_t *tau_alias = expr_from_string("{ @tau }", NULL);
    expr_t *f = expr_from_string("{ [radius]^2 + c_1 + π + e }", NULL);
    expr_t *plain_integral_constant = expr_from_string("{ x + C }", NULL);
    expr_t *integral_family = expr_from_string("{ C₁ + C_0*x + cos(x)*exp(sin(x)) }", NULL);
    char *xs = x ? expr_to_string(x, style_EXPRESSION) : NULL;
    char *x_pow_ns = x_pow_n ? expr_to_string(x_pow_n, style_EXPRESSION) : NULL;
    char *es = e ? expr_to_string(e, style_EXPRESSION) : NULL;
    char *is = i_unit ? expr_to_string(i_unit, style_EXPRESSION) : NULL;
    char *pi_as = pi_ascii ? expr_to_string(pi_ascii, style_EXPRESSION) : NULL;
    char *phi_plain = phi_ascii ? expr_to_string(phi_ascii, style_EXPRESSION) : NULL;
    char *gamma_plain = gamma_ascii ? expr_to_string(gamma_ascii, style_EXPRESSION) : NULL;
    char *pi_ats = pi_alias ? expr_to_string(pi_alias, style_EXPRESSION) : NULL;
    char *taus = tau ? expr_to_string(tau, style_EXPRESSION) : NULL;
    char *phi_as = phi_alias ? expr_to_string(phi_alias, style_EXPRESSION) : NULL;
    char *gamma_as = gamma_alias ? expr_to_string(gamma_alias, style_EXPRESSION) : NULL;
    char *tau_as = tau_alias ? expr_to_string(tau_alias, style_EXPRESSION) : NULL;
    char *fs = f ? expr_to_string(f, style_EXPRESSION) : NULL;
    char *plain_integral_constant_s =
        plain_integral_constant ? expr_to_string(plain_integral_constant, style_EXPRESSION) : NULL;
    char *integral_family_s = integral_family ? expr_to_string(integral_family, style_EXPRESSION) : NULL;

    if (x && xs && str_eq(xs, "{ x | x = NAN }")) {
        to_string_pass("implicit symbolic var inference", xs, "{ x | x = NAN }");
    } else {
        to_string_fail(__FILE__, __LINE__, 1, "implicit symbolic var inference", xs ? xs : "(null)", "{ x | x = NAN }");
    }

    if (x_pow_n && x_pow_ns && str_eq(x_pow_ns, "{ x^n | x = NAN; n = NAN }")) {
        to_string_pass("implicit symbolic constant inference", x_pow_ns, "{ x^n | x = NAN; n = NAN }");
    } else {
        to_string_fail(__FILE__, __LINE__, 1, "implicit symbolic constant inference", x_pow_ns ? x_pow_ns : "(null)",
                       "{ x^n | x = NAN; n = NAN }");
    }

    if (e && es && str_eq(es, "e")) {
        to_string_pass("implicit e constant inference", es, "e");
    } else {
        to_string_fail(__FILE__, __LINE__, 1, "implicit e constant inference", es ? es : "(null)", "e");
    }

    if (i_unit && is && str_eq(is, "i")) {
        to_string_pass("implicit i constant inference", is, "i");
    } else {
        to_string_fail(__FILE__, __LINE__, 1, "implicit i constant inference", is ? is : "(null)", "i");
    }

    if (pi_ascii && pi_as && str_eq(pi_as, "π")) {
        to_string_pass("implicit pi constant inference", pi_as, "π");
    } else {
        to_string_fail(__FILE__, __LINE__, 1, "implicit pi constant inference", pi_as ? pi_as : "(null)", "π");
    }

    if (pi_alias && pi_ats && str_eq(pi_ats, "π")) {
        to_string_pass("implicit @pi constant inference", pi_ats, "π");
    } else {
        to_string_fail(__FILE__, __LINE__, 1, "implicit @pi constant inference", pi_ats ? pi_ats : "(null)", "π");
    }

    if (phi_ascii && phi_plain && str_eq(phi_plain, "φ")) {
        to_string_pass("implicit phi constant inference", phi_plain, "φ");
    } else {
        to_string_fail(__FILE__, __LINE__, 1, "implicit phi constant inference", phi_plain ? phi_plain : "(null)", "φ");
    }

    if (gamma_ascii && gamma_plain && str_eq(gamma_plain, "γ")) {
        to_string_pass("implicit gamma constant inference", gamma_plain, "γ");
    } else {
        to_string_fail(__FILE__, __LINE__, 1, "implicit gamma constant inference", gamma_plain ? gamma_plain : "(null)",
                       "γ");
    }

    if (tau && taus && str_eq(taus, "{ τ | τ = NAN }")) {
        to_string_pass("implicit tau variable inference", taus, "{ τ | τ = NAN }");
    } else {
        to_string_fail(__FILE__, __LINE__, 1, "implicit tau variable inference", taus ? taus : "(null)",
                       "{ τ | τ = NAN }");
    }

    if (phi_alias && phi_as && str_eq(phi_as, "φ")) {
        to_string_pass("implicit @phi constant inference", phi_as, "φ");
    } else {
        to_string_fail(__FILE__, __LINE__, 1, "implicit @phi constant inference", phi_as ? phi_as : "(null)", "φ");
    }

    if (gamma_alias && gamma_as && str_eq(gamma_as, "γ")) {
        to_string_pass("implicit @gamma constant inference", gamma_as, "γ");
    } else {
        to_string_fail(__FILE__, __LINE__, 1, "implicit @gamma constant inference", gamma_as ? gamma_as : "(null)",
                       "γ");
    }

    if (tau_alias && tau_as && str_eq(tau_as, "{ τ | τ = NAN }")) {
        to_string_pass("implicit @tau variable inference", tau_as, "{ τ | τ = NAN }");
    } else {
        to_string_fail(__FILE__, __LINE__, 1, "implicit @tau variable inference", tau_as ? tau_as : "(null)",
                       "{ τ | τ = NAN }");
    }

    if (f && fs && str_eq(fs, "{ [radius]² + c₁ + π + e | [radius] = NAN; c₁ = NAN }")) {
        to_string_pass("implicit mixed symbolic inference", fs,
                       "{ [radius]² + c₁ + π + e | [radius] = NAN; c₁ = NAN }");
    } else {
        to_string_fail(__FILE__, __LINE__, 1, "implicit mixed symbolic inference", fs ? fs : "(null)",
                       "{ [radius]² + c₁ + π + e | [radius] = NAN; c₁ = NAN }");
    }

    if (integral_family && integral_family_s &&
        str_eq(integral_family_s, "{ C₁ + C₀x + cos(x)·exp(sin(x)) | x = NAN; C₁ = NAN, C₀ = NAN }")) {
        to_string_pass("implicit integration constants stay constant", integral_family_s,
                       "{ C₁ + C₀x + cos(x)·exp(sin(x)) | x = NAN; C₁ = NAN, C₀ = NAN }");
    } else {
        to_string_fail(__FILE__, __LINE__, 1, "implicit integration constants stay constant",
                       integral_family_s ? integral_family_s : "(null)",
                       "{ C₁ + C₀x + cos(x)·exp(sin(x)) | x = NAN; C₁ = NAN, C₀ = NAN }");
    }

    if (plain_integral_constant && plain_integral_constant_s &&
        str_eq(plain_integral_constant_s, "{ x + C | x = NAN; C = NAN }")) {
        to_string_pass("implicit plain integration constant stays constant", plain_integral_constant_s,
                       "{ x + C | x = NAN; C = NAN }");
    } else {
        to_string_fail(__FILE__, __LINE__, 1, "implicit plain integration constant stays constant",
                       plain_integral_constant_s ? plain_integral_constant_s : "(null)",
                       "{ x + C | x = NAN; C = NAN }");
    }

    if (x && qf_isnan(expr_eval_qf(x))) {
        printf(C_BOLD C_GREEN "PASS" C_RESET " implicit x evaluates to NaN\n\n");
    } else {
        printf(C_BOLD C_RED "FAIL" C_RESET " implicit x evaluates to NaN %s:%d:1\n\n", __FILE__, __LINE__);
        TEST_FAIL();
    }

    if (tau && qf_isnan(expr_eval_qf(tau))) {
        printf(C_BOLD C_GREEN "PASS" C_RESET " implicit tau evaluates to NaN\n\n");
    } else {
        printf(C_BOLD C_RED "FAIL" C_RESET " implicit tau evaluates to NaN %s:%d:1\n\n", __FILE__, __LINE__);
        TEST_FAIL();
    }

    if (e && qf_eq(expr_eval_qf(e), QF_E)) {
        printf(C_BOLD C_GREEN "PASS" C_RESET " implicit e evaluates to built-in constant\n\n");
    } else {
        printf(C_BOLD C_RED "FAIL" C_RESET " implicit e evaluates to built-in constant %s:%d:1\n\n", __FILE__,
               __LINE__);
        TEST_FAIL();
    }

    if (i_unit) {
        number_t i_value = expr_eval(i_unit);
        bool ok = num_eq(i_value, NUM_I);

        num_destroy(&i_value);
        if (ok) {
            printf(C_BOLD C_GREEN "PASS" C_RESET " implicit i evaluates to built-in constant\n\n");
        } else {
            printf(C_BOLD C_RED "FAIL" C_RESET " implicit i evaluates to built-in constant %s:%d:1\n\n", __FILE__,
                   __LINE__);
            TEST_FAIL();
        }
    } else {
        printf(C_BOLD C_RED "FAIL" C_RESET " implicit i evaluates to built-in constant %s:%d:1\n\n", __FILE__,
               __LINE__);
        TEST_FAIL();
    }

    if (pi_ascii && qf_eq(expr_eval_qf(pi_ascii), QF_PI) && pi_alias && qf_eq(expr_eval_qf(pi_alias), QF_PI)) {
        printf(C_BOLD C_GREEN "PASS" C_RESET " implicit pi aliases evaluate to built-in constant\n\n");
    } else {
        printf(C_BOLD C_RED "FAIL" C_RESET " implicit pi aliases evaluate to built-in constant %s:%d:1\n\n", __FILE__,
               __LINE__);
        TEST_FAIL();
    }

    if (phi_alias) {
        number_t phi_value = expr_eval(phi_alias);
        bool ok = num_eq(phi_value, NUM_PHI) && num_get_prec_bits(phi_value) == num_get_default_prec_bits();

        num_destroy(&phi_value);
        if (ok) {
            printf(C_BOLD C_GREEN "PASS" C_RESET " implicit @phi evaluates to built-in constant\n\n");
        } else {
            printf(C_BOLD C_RED "FAIL" C_RESET " implicit @phi evaluates to built-in constant %s:%d:1\n\n", __FILE__,
                   __LINE__);
            TEST_FAIL();
        }
    } else {
        printf(C_BOLD C_RED "FAIL" C_RESET " implicit @phi evaluates to built-in constant %s:%d:1\n\n", __FILE__,
               __LINE__);
        TEST_FAIL();
    }

    if (phi_ascii) {
        number_t phi_value = expr_eval(phi_ascii);
        bool ok = num_eq(phi_value, NUM_PHI) && num_get_prec_bits(phi_value) == num_get_default_prec_bits();

        num_destroy(&phi_value);
        if (ok) {
            printf(C_BOLD C_GREEN "PASS" C_RESET " implicit phi evaluates to built-in constant\n\n");
        } else {
            printf(C_BOLD C_RED "FAIL" C_RESET " implicit phi evaluates to built-in constant %s:%d:1\n\n", __FILE__,
                   __LINE__);
            TEST_FAIL();
        }
    } else {
        printf(C_BOLD C_RED "FAIL" C_RESET " implicit phi evaluates to built-in constant %s:%d:1\n\n", __FILE__,
               __LINE__);
        TEST_FAIL();
    }

    if (gamma_alias && qf_eq(expr_eval_qf(gamma_alias), QF_EULER_MASCHERONI)) {
        printf(C_BOLD C_GREEN "PASS" C_RESET " implicit @gamma evaluates to built-in constant\n\n");
    } else {
        printf(C_BOLD C_RED "FAIL" C_RESET " implicit @gamma evaluates to built-in constant %s:%d:1\n\n", __FILE__,
               __LINE__);
        TEST_FAIL();
    }

    if (gamma_ascii && qf_eq(expr_eval_qf(gamma_ascii), QF_EULER_MASCHERONI)) {
        printf(C_BOLD C_GREEN "PASS" C_RESET " implicit gamma evaluates to built-in constant\n\n");
    } else {
        printf(C_BOLD C_RED "FAIL" C_RESET " implicit gamma evaluates to built-in constant %s:%d:1\n\n", __FILE__,
               __LINE__);
        TEST_FAIL();
    }

    if (tau_alias && qf_isnan(expr_eval_qf(tau_alias))) {
        printf(C_BOLD C_GREEN "PASS" C_RESET " implicit @tau evaluates to NaN\n\n");
    } else {
        printf(C_BOLD C_RED "FAIL" C_RESET " implicit @tau evaluates to NaN %s:%d:1\n\n", __FILE__, __LINE__);
        TEST_FAIL();
    }

    if (f && qf_isnan(expr_eval_qf(f))) {
        printf(C_BOLD C_GREEN "PASS" C_RESET " implicit mixed expression evaluates to NaN\n\n");
    } else {
        printf(C_BOLD C_RED "FAIL" C_RESET " implicit mixed expression evaluates to NaN %s:%d:1\n\n", __FILE__,
               __LINE__);
        TEST_FAIL();
    }

    free(fs);
    free(plain_integral_constant_s);
    free(integral_family_s);
    free(tau_as);
    free(gamma_as);
    free(gamma_plain);
    free(phi_as);
    free(phi_plain);
    free(taus);
    free(pi_ats);
    free(pi_as);
    free(is);
    free(es);
    free(x_pow_ns);
    free(xs);
    expr_free(integral_family);
    expr_free(plain_integral_constant);
    expr_free(f);
    expr_free(tau_alias);
    expr_free(gamma_alias);
    expr_free(gamma_ascii);
    expr_free(phi_alias);
    expr_free(phi_ascii);
    expr_free(tau);
    expr_free(pi_alias);
    expr_free(pi_ascii);
    expr_free(i_unit);
    expr_free(e);
    expr_free(x_pow_n);
    expr_free(x);
}

/* ---- ASCII alternative syntax — subscripts (_N) ---- */

static void test_from_string_subscripts(void)
{
    /* Basic _N in both binding and expression */
    check_parse_val("x_0^2 = 9", "{ x_0^2 | x_0 = 3 }", 9.0, __LINE__);
    check_parse_val("x_0 + x_1 + x_2 = 6", "{ x_0 + x_1 + x_2 | x_0 = 1, x_1 = 2, x_2 = 3 }", 6.0, __LINE__);
    check_parse_val("c_0*x_0^2 + c_1 = 7", "{ c_0*x_0^2 + c_1 | x_0 = 2; c_0 = 1.5, c_1 = 1 }", 7.0, __LINE__);
    check_parse_val("sin(x_0)*cos(x_0) at pi/4 = 0.5", "{ sin(x_0)*cos(x_0) | x_0 = 0.7853981633974483 }", 0.5,
                    __LINE__);
    /* _N and Unicode ₀-₉ must be interchangeable within one string */
    check_parse_val("Unicode expr, ASCII binding", "{ x\xe2\x82\x80^2 | x_0 = 3 }", 9.0, __LINE__);
    check_parse_val("ASCII expr, Unicode binding", "{ x_0^2 | x\xe2\x82\x80 = 3 }", 9.0, __LINE__);
    check_parse_val("Unicode expr + const, ASCII binding", "{ c\xe2\x82\x80*x\xe2\x82\x80^2 | x_0 = 2; c_0 = 3 }", 12.0,
                    __LINE__);
    check_parse_val("c_0*sin(x_0) + c_1*cos(x_1): Unicode consts, ASCII vars",
                    "{ c\xe2\x82\x80*sin(x_0) + c\xe2\x82\x81*cos(x_1)"
                    " | x_0 = 1.5707963267948966, x_1 = 0; c_0 = 2, c_1 = 5 }",
                    7.0, __LINE__); /* 2*sin(pi/2) + 5*cos(0) = 2*1 + 5*1 = 7 */
}

/* ---- ASCII alternative syntax — star (*) multiplication ---- */

static void test_from_string_star_mul(void)
{
    /* No spaces */
    check_parse_val("a*b = 12 (no spaces)", "{ x_0*x_1 | x_0 = 3, x_1 = 4 }", 12.0, __LINE__);
    /* Spaces around '*' */
    check_parse_val("a * b = 12 (spaces)", "{ x_0 * x_1 | x_0 = 3, x_1 = 4 }", 12.0, __LINE__);
    /* Scalar constant times function */
    check_parse_val("c * sin(pi/2) = 5", "{ c * sin(x) | x = 1.5707963267948966; c = 5 }", 5.0, __LINE__);
    check_parse_val("c * exp(0) = 3", "{ c * exp(x) | x = 0; c = 3 }", 3.0, __LINE__);
    check_parse_val("c * ln(e) = 7", "{ c * ln(x) | x = 2.718281828459045; c = 7 }", 7.0, __LINE__);
    /* Chained '*' */
    check_parse_val("a * b * c = 24", "{ x_0 * x_1 * x_2 | x_0 = 2, x_1 = 3, x_2 = 4 }", 24.0, __LINE__);
    /* '*' combined with negation */
    check_parse_val("a * -b = -12", "{ x_0 * -x_1 | x_0 = 3, x_1 = 4 }", -12.0, __LINE__);
    /* '*' with parenthesised sub-expressions: (a+b)(a-b) = a²-b² */
    check_parse_val("(a+b)*(a-b) = a^2-b^2 = 16", "{ (x_0 + x_1) * (x_0 - x_1) | x_0 = 5, x_1 = 3 }", 16.0, __LINE__);
    /* Sine addition formula: sin(a)cos(b) + sin(b)cos(a) = sin(a+b) */
    check_parse_val("sin(a)*cos(b) + sin(b)*cos(a) = sin(pi/2) = 1",
                    "{ sin(x_0)*cos(x_1) + sin(x_1)*cos(x_0)"
                    " | x_0 = 1.0471975511965976, x_1 = 0.5235987755982988 }",
                    1.0, __LINE__);
    /* Gaussian envelope: c*exp(-x^2) */
    check_parse_val("c * exp(-x^2) at x=0 = c", "{ c * exp(-x_0^2) | x_0 = 0; c = 7 }", 7.0, __LINE__);
    /* exp product identity: exp(f)*exp(-f) = 1 */
    check_parse_val("exp(sin(x)) * exp(-sin(x)) = 1", "{ exp(sin(x)) * exp(-sin(x)) | x = 0.7 }", 1.0, __LINE__);
}

/* ---- ASCII alternative syntax — ^N exponent on function names ---- */

static void test_from_string_func_power(void)
{
    /* All 15 unary functions with ^2: exact-value cases */
    check_parse_val("sin^2(0) = 0", "{ sin^2(x) | x = 0 }", 0.0, __LINE__);
    check_parse_val("cos^2(0) = 1", "{ cos^2(x) | x = 0 }", 1.0, __LINE__);
    check_parse_val("tan^2(0) = 0", "{ tan^2(x) | x = 0 }", 0.0, __LINE__);
    check_parse_val("sinh^2(0) = 0", "{ sinh^2(x) | x = 0 }", 0.0, __LINE__);
    check_parse_val("cosh^2(0) = 1", "{ cosh^2(x) | x = 0 }", 1.0, __LINE__);
    check_parse_val("tanh^2(0) = 0", "{ tanh^2(x) | x = 0 }", 0.0, __LINE__);
    check_parse_val("asin^2(0) = 0", "{ asin^2(x) | x = 0 }", 0.0, __LINE__);
    check_parse_val("acos^2(1) = 0", "{ acos^2(x) | x = 1 }", 0.0, __LINE__);
    check_parse_val("atan^2(0) = 0", "{ atan^2(x) | x = 0 }", 0.0, __LINE__);
    check_parse_val("asinh^2(0) = 0", "{ asinh^2(x) | x = 0 }", 0.0, __LINE__);
    check_parse_val("acosh^2(1) = 0", "{ acosh^2(x) | x = 1 }", 0.0, __LINE__);
    check_parse_val("atanh^2(0) = 0", "{ atanh^2(x) | x = 0 }", 0.0, __LINE__);
    check_parse_val("exp^2(0) = 1", "{ exp^2(x) | x = 0 }", 1.0, __LINE__);
    check_parse_val("ln^2(e) = 1", "{ ln^2(x) | x = 2.718281828459045 }", 1.0, __LINE__);
    check_parse_val("sqrt^2(9) = 9", "{ sqrt^2(x) | x = 9 }", 9.0, __LINE__);
    /* Non-trivial exponent values */
    check_parse_val("tan^2(pi/4) = 1", "{ tan^2(x) | x = 0.7853981633974483 }", 1.0, __LINE__);
    check_parse_val("sqrt^3(4) = 8", "{ sqrt^3(x) | x = 4 }", 8.0, __LINE__);
    check_parse_val("exp^3(0) = 1", "{ exp^3(x) | x = 0 }", 1.0, __LINE__);
    /* Multi-digit exponent */
    check_parse_val("sin^10(0) = 0", "{ sin^10(x) | x = 0 }", 0.0, __LINE__);
    check_parse_val("cos^10(0) = 1", "{ cos^10(x) | x = 0 }", 1.0, __LINE__);
    /* Pythagorean identities expressed with ^N */
    check_parse_val("sin^2(x) + cos^2(x) = 1", "{ sin^2(x) + cos^2(x) | x = 1.234 }", 1.0, __LINE__);
    check_parse_val("cosh^2(x) - sinh^2(x) = 1", "{ cosh^2(x) - sinh^2(x) | x = 2.5 }", 1.0, __LINE__);
    check_parse_val("sqrt(sin^2(x) + cos^2(x)) = 1", "{ sqrt(sin^2(x) + cos^2(x)) | x = 0.9 }", 1.0, __LINE__);
    /* ^N combined with subscripted variable names */
    check_parse_val("sin^2(x_0) + cos^2(x_0) = 1 (subscript + ^N)", "{ sin^2(x_0) + cos^2(x_0) | x_0 = 0.7 }", 1.0,
                    __LINE__);
    check_parse_val("exp^2(x_0) * exp(-2*x_0^2) at x_0=1", "{ exp^2(x_0) * exp(-2*x_0^2) | x_0 = 1 }", 1.0, __LINE__);
}

/* ---- Complex composed expressions using all ASCII alternatives ---- */

static void test_from_string_composed(void)
{
    /* Euclidean distance in 2D: sqrt(x^2 + y^2) */
    check_parse_val("sqrt(x_0^2 + x_1^2) = 5 (3-4-5 triangle)", "{ sqrt(x_0^2 + x_1^2) | x_0 = 3, x_1 = 4 }", 5.0,
                    __LINE__);

    /* exp / log mutual inverses */
    check_parse_val("exp(ln(x_0)) = x_0 = 3", "{ exp(ln(x_0)) | x_0 = 3 }", 3.0, __LINE__);
    check_parse_val("ln(exp(x_0)) = x_0 = 2.5", "{ ln(exp(x_0)) | x_0 = 2.5 }", 2.5, __LINE__);

    /* Trig inverse pairs */
    check_parse_val("asin(sin(x_0)) = 0.5", "{ asin(sin(x_0)) | x_0 = 0.5 }", 0.5, __LINE__);
    check_parse_val("acos(cos(x_0)) = 0.6", "{ acos(cos(x_0)) | x_0 = 0.6 }", 0.6, __LINE__);
    check_parse_val("atan(tan(x_0)) = pi/6", "{ atan(tan(x_0)) | x_0 = 0.5235987755982988 }", 0.5235987755982988,
                    __LINE__);

    /* Hyperbolic inverse pairs */
    check_parse_val("asinh(sinh(x_0)) = 1.5", "{ asinh(sinh(x_0)) | x_0 = 1.5 }", 1.5, __LINE__);
    check_parse_val("acosh(cosh(x_0)) = 1.2", "{ acosh(cosh(x_0)) | x_0 = 1.2 }", 1.2, __LINE__);
    check_parse_val("atanh(tanh(x_0)) = 0.8", "{ atanh(tanh(x_0)) | x_0 = 0.8 }", 0.8, __LINE__);

    /* atan2 recovers angle from unit-circle coordinates */
    check_parse_val("atan2(sin(x_0), cos(x_0)) = x_0", "{ atan2(sin(x_0), cos(x_0)) | x_0 = 0.5 }", 0.5, __LINE__);

    /* tan(x)*cos(x) = sin(x): at x=pi/6, result = 0.5 */
    check_parse_val("tan(x_0) * cos(x_0) = sin(x_0) = 0.5", "{ tan(x_0) * cos(x_0) | x_0 = 0.5235987755982988 }", 0.5,
                    __LINE__);

    /* exp product identity: exp(f(x)) * exp(-f(x)) = 1 */
    check_parse_val("exp(sin(x)) * exp(-sin(x)) = 1", "{ exp(sin(x)) * exp(-sin(x)) | x = 0.7 }", 1.0, __LINE__);
    check_parse_val("exp(cos(x)) * exp(-cos(x)) = 1", "{ exp(cos(x)) * exp(-cos(x)) | x = 1.2 }", 1.0, __LINE__);
    check_parse_val("exp(ln(x_0)) * exp(-ln(x_0)) = 1", "{ exp(ln(x_0)) * exp(-ln(x_0)) | x_0 = 4 }", 1.0, __LINE__);

    /* c_0*sin^2(x_0) + c_1*cos^2(x_1): three ASCII features together */
    check_parse_val("c_0*sin^2(pi/2) + c_1*cos^2(0) = 3+5 = 8",
                    "{ c_0*sin^2(x_0) + c_1*cos^2(x_1)"
                    " | x_0 = 1.5707963267948966, x_1 = 0; c_0 = 3, c_1 = 5 }",
                    8.0, __LINE__);

    /* Gaussian bell: c*exp(-x^2) at peak */
    check_parse_val("c_0 * exp(-x_0^2) at x_0=0 = c_0", "{ c_0 * exp(-x_0^2) | x_0 = 0; c_0 = 4 }", 4.0, __LINE__);

    /* Chain: exp(c_0*x_0^2) * sin^2(x_1) + ln(x_2) = 1 */
    check_parse_val("exp(c*x^2)*sin^2(y) + ln(z) = 1",
                    "{ exp(c_0*x_0^2)*sin^2(x_1) + ln(x_2)"
                    " | x_0 = 0, x_1 = 1.5707963267948966, x_2 = 1; c_0 = 1 }",
                    1.0, __LINE__);

    /* sqrt(exp(2*ln(3))) = sqrt(9) = 3 */
    check_parse_val("sqrt(exp(c_0 * x_0)) = 3", "{ sqrt(exp(c_0 * x_0)) | x_0 = 1.0986122886681098; c_0 = 2 }", 3.0,
                    __LINE__);

    /* ln(x_0^3) = 3*ln(x_0): at x_0=e this is 3 */
    check_parse_val("ln(x_0^3) = 3*ln(e) = 3", "{ ln(x_0^3) | x_0 = 2.718281828459045 }", 3.0, __LINE__);

    /* cosh^2(x_0) - sinh^2(x_0) = 1 with subscripted name */
    check_parse_val("cosh^2(x_0) - sinh^2(x_0) = 1", "{ cosh^2(x_0) - sinh^2(x_0) | x_0 = 3.1 }", 1.0, __LINE__);

    /* Four-variable expression: a*sin(x) + b*cos(y) + c*exp(-z) + d */
    check_parse_val("a*sin(x)+b*cos(y)+c*exp(-z)+d at zeros",
                    "{ c_0*sin(x_0) + c_1*cos(x_1) + c_2*exp(-x_2) + c_3"
                    " | x_0 = 0, x_1 = 0, x_2 = 0; c_0 = 1, c_1 = 2, c_2 = 3, c_3 = 4 }",
                    0.0 + 2.0 + 3.0 + 4.0, __LINE__);
}

static void test_from_string_simplified_identity_text(void)
{
    expr_t *expr = expr_from_string("{ sin^2(x) + cos^2(x) | x = 1.234 }", NULL);
    expr_t *simp = expr ? expr_simplify(expr) : NULL;
    char *text = simp ? expr_to_string(simp, style_EXPRESSION) : NULL;

    if (!(text && strcmp(text, "1") == 0)) {
        printf(C_BOLD C_RED "FAIL" C_RESET " parsed sin^2(x) + cos^2(x) simplifies explicitly to 1 %s:%d:1\n", __FILE__,
               __LINE__);
        printf("  got:      %s\n", text ? text : "<null>");
        printf("  expected: 1\n\n");
        TEST_FAIL();
    } else {
        printf(C_BOLD C_GREEN "PASS" C_RESET " parsed sin^2(x) + cos^2(x) simplifies explicitly to 1\n\n");
    }

    free(text);
    expr_free(simp);
    expr_free(expr);
}

/* ---- Group runner for all ASCII-alternative tests ---- */

static void test_from_string_ascii_alternatives(void)
{
    TEST_RUN_SUBTEST(test_from_string_subscripts, NULL);
    TEST_RUN_SUBTEST(test_from_string_star_mul, NULL);
    TEST_RUN_SUBTEST(test_from_string_composed, NULL);
    TEST_RUN_SUBTEST(test_from_string_func_power, NULL);
    TEST_RUN_SUBTEST(test_from_string_simplified_identity_text, NULL);
}

/* ---- f, f', f'' of exp(sin(x)) + 3*x^2 - 7: parse, evaluate, differentiate ----
 *
 * The three explicit strings are the value, first derivative, and second
 * derivative of f(x) = exp(sin(x)) + 3*x² - 7 at x = 1.25.
 *
 * f'  = cos(x)·exp(sin(x)) + 6x
 * f'' = exp(sin(x))·(cos²(x) − sin(x)) + 6
 *
 * Tests cover: explicit star (*), implicit mul (6x), function power (cos^2),
 * parenthesised grouping ((cos^2(x) − sin(x))·exp(sin(x)) + 6),
 * and programmatic differentiation of a parsed expr_t.
 */

/* Inline comparison helper for the derivative checks below. */
static void check_expr_d(const char *label, const expr_t *node, double expect, int line)
{
    qfloat_t qval = expr_eval_qf(node);
    double got = qf_to_double(qval);
    double err = fabs(got - expect);
    double rel = (fabs(expect) > 0.0) ? err / fabs(expect) : err;
    const double TOL = 2e-14;
    char *expr = expr_to_string(node, style_EXPRESSION);
    if (err < TOL || rel < TOL) {
        printf(C_BOLD C_GREEN "PASS" C_RESET " %s\n", label);
        printf(C_BOLD "  expr   " C_RESET "%s\n", expr ? expr : "(null)");
        qf_printf(C_BOLD "  value  " C_RESET "%.34q\n\n", qval);
    } else {
        printf(C_BOLD C_RED "FAIL" C_RESET " %s %s:%d:1\n", label, __FILE__, line);
        printf(C_BOLD "  expr   " C_RESET "%s\n", expr ? expr : "(null)");
        qf_printf(C_BOLD "  got    " C_RESET "%.34q\n", qval);
        printf(C_BOLD "  expect " C_RESET "%.17g\n\n", expect);
        TEST_FAIL();
    }
    free(expr);
}

static void check_parse_num(const char *label, const char *s, const char *expect_text, int line)
{
    expr_t *expr = expr_from_string(s, NULL);
    number_t got;
    number_t expect;
    char *expr_text;

    if (!expr) {
        printf(C_BOLD C_RED "FAIL" C_RESET " %s %s:%d:1\n", label, __FILE__, line);
        printf(C_BOLD "  input  " C_RESET "%s\n", s);
        printf(C_BOLD "  error  " C_RESET "parser returned NULL\n\n");
        TEST_FAIL();
        return;
    }

    got = expr_eval(expr);
    expect = num_create_from_string(expect_text);
    expr_text = expr_to_string(expr, style_EXPRESSION);

    if (num_eq(got, expect)) {
        printf(C_BOLD C_GREEN "PASS" C_RESET " %s\n", label);
        printf(C_BOLD "  input  " C_RESET "%s\n", s);
        printf(C_BOLD "  expr   " C_RESET "%s\n", expr_text ? expr_text : "(null)");
        printf(C_BOLD "  value  " C_RESET "%s\n\n", expect_text);
    } else {
        string_t *got_text = num_to_string(got);

        printf(C_BOLD C_RED "FAIL" C_RESET " %s %s:%d:1\n", label, __FILE__, line);
        printf(C_BOLD "  input  " C_RESET "%s\n", s);
        printf(C_BOLD "  expr   " C_RESET "%s\n", expr_text ? expr_text : "(null)");
        printf(C_BOLD "  got    " C_RESET "%s\n", got_text ? string_c_str(got_text) : "(null)");
        printf(C_BOLD "  expect " C_RESET "%s\n\n", expect_text);
        string_free(got_text);
        TEST_FAIL();
    }

    free(expr_text);
    num_destroy(&expect);
    num_destroy(&got);
    expr_free(expr);
}

static void check_parse_expr(const char *label, const char *s, const char *expect_expr, int line)
{
    expr_t *expr = expr_from_string(s, NULL);
    char *expr_text;

    if (!expr) {
        printf(C_BOLD C_RED "FAIL" C_RESET " %s %s:%d:1\n", label, __FILE__, line);
        printf(C_BOLD "  input  " C_RESET "%s\n", s);
        printf(C_BOLD "  error  " C_RESET "parser returned NULL\n\n");
        TEST_FAIL();
        return;
    }

    expr_text = expr_to_string(expr, style_EXPRESSION);
    if (expr_text && strcmp(expr_text, expect_expr) == 0) {
        printf(C_BOLD C_GREEN "PASS" C_RESET " %s\n", label);
        printf(C_BOLD "  input  " C_RESET "%s\n", s);
        printf(C_BOLD "  expr   " C_RESET "%s\n\n", expr_text);
    } else {
        printf(C_BOLD C_RED "FAIL" C_RESET " %s %s:%d:1\n", label, __FILE__, line);
        printf(C_BOLD "  input  " C_RESET "%s\n", s);
        printf(C_BOLD "  got    " C_RESET "%s\n", expr_text ? expr_text : "(null)");
        printf(C_BOLD "  expect " C_RESET "%s\n\n", expect_expr);
        TEST_FAIL();
    }

    free(expr_text);
    expr_free(expr);
}

static void check_parse_TeX(const char *label, const char *s, const char *expect_TeX, int line)
{
    expr_t *expr = expr_from_string(s, NULL);
    char *TeX_text;

    if (!expr) {
        printf(C_BOLD C_RED "FAIL" C_RESET " %s %s:%d:1\n", label, __FILE__, line);
        printf(C_BOLD "  input  " C_RESET "%s\n", s);
        printf(C_BOLD "  error  " C_RESET "parser returned NULL\n\n");
        TEST_FAIL();
        return;
    }

    TeX_text = expr_to_string(expr, style_LATEX);
    if (TeX_text && strcmp(TeX_text, expect_TeX) == 0) {
        printf(C_BOLD C_GREEN "PASS" C_RESET " %s\n", label);
        printf(C_BOLD "  input  " C_RESET "%s\n", s);
        printf(C_BOLD "  tex    " C_RESET "%s\n\n", TeX_text);
    } else {
        printf(C_BOLD C_RED "FAIL" C_RESET " %s %s:%d:1\n", label, __FILE__, line);
        printf(C_BOLD "  input  " C_RESET "%s\n", s);
        printf(C_BOLD "  got    " C_RESET "%s\n", TeX_text ? TeX_text : "(null)");
        printf(C_BOLD "  expect " C_RESET "%s\n\n", expect_TeX);
        TEST_FAIL();
    }

    free(TeX_text);
    expr_free(expr);
}

static void check_parse_simplified_expr(const char *label, const char *s, const char *expect_expr, int line)
{
    expr_t *expr = expr_from_string(s, NULL);
    expr_t *simp = expr ? expr_simplify(expr) : NULL;
    char *expr_text;

    if (!expr || !simp) {
        printf(C_BOLD C_RED "FAIL" C_RESET " %s %s:%d:1\n", label, __FILE__, line);
        printf(C_BOLD "  input  " C_RESET "%s\n", s);
        printf(C_BOLD "  error  " C_RESET "parser or simplifier returned NULL\n\n");
        TEST_FAIL();
        expr_free(simp);
        expr_free(expr);
        return;
    }

    expr_text = expr_to_string(simp, style_EXPRESSION);
    if (expr_text && strcmp(expr_text, expect_expr) == 0) {
        printf(C_BOLD C_GREEN "PASS" C_RESET " %s\n", label);
        printf(C_BOLD "  input  " C_RESET "%s\n", s);
        printf(C_BOLD "  expr   " C_RESET "%s\n\n", expr_text);
    } else {
        printf(C_BOLD C_RED "FAIL" C_RESET " %s %s:%d:1\n", label, __FILE__, line);
        printf(C_BOLD "  input  " C_RESET "%s\n", s);
        printf(C_BOLD "  got    " C_RESET "%s\n", expr_text ? expr_text : "(null)");
        printf(C_BOLD "  expect " C_RESET "%s\n\n", expect_expr);
        TEST_FAIL();
    }

    free(expr_text);
    expr_free(simp);
    expr_free(expr);
}

static void test_from_string_number_literals(void)
{
    check_parse_num("pure const rational", "{ [half] = 1/2 }", "1/2", __LINE__);
    check_parse_num("rational atom expression", "{ 1/2 + 1/4 }", "3/4", __LINE__);
    check_parse_num("unicode rational atom expression", "{ ½ + ¼ }", "3/4", __LINE__);
    check_parse_num("stacked unicode rational atom", "{ ³⁵⁵⁄₁₁₃ }", "355/113", __LINE__);
    check_parse_expr("rational atom expression simplifies display", "{ 231/2310 }", "⅒", __LINE__);
    check_parse_expr("stacked unicode rational atom simplifies display", "{ ²³¹⁄₂₃₁₀ }", "⅒", __LINE__);
    check_parse_expr("unparenthesised radical constant round-trips", "{ √3/2 }", "√(3)/2", __LINE__);
    check_parse_expr("unparenthesised radical supports implicit multiplication", "{ 4√3/2 }", "2·√(3)", __LINE__);
    check_parse_TeX("stacked unicode rational atom renders TeX", "{ ²³¹⁄₂₃₁₀ }", "\\frac{1}{10}", __LINE__);
    check_parse_expr("NAN is a numeric placeholder, not a variable name", "{ NAN }", "NAN", __LINE__);
    check_parse_expr("lowercase nan is a numeric placeholder", "{ nan }", "NAN", __LINE__);
    check_parse_expr("repeated unary signs simplify", "{ --2x | x = ? }", "{ 2x | x = NAN }", __LINE__);
    check_parse_num("pure imaginary coefficient atom", "{ 3/2i }", "3/2i", __LINE__);
    check_parse_num("pure const rational complex", "{ [z] = 1/2 - 3/2i }", "1/2 - 3/2i", __LINE__);
    check_parse_num("binding rational complex", "{ z | z = 1/2 - 3/2i }", "1/2 - 3/2i", __LINE__);
    check_parse_num("binding parenthesized imag coeff", "{ z | z = 1/2 + (2)i }", "1/2 + 2i", __LINE__);
}

static void test_from_string_deriv(void)
{
    const double xv = 1.25;
    const double sx = sin(xv);
    const double cx = cos(xv);
    const double esx = exp(sx);

    /* ---- Explicit parse-and-evaluate: f, f', f'' as strings ---- */

    /* f(x) = exp(sin(x)) + 3*x^2 - 7 */
    check_parse_val("f = exp(sin(x)) + 3*x^2 - 7 at x=1.25", "{ exp(sin(x)) + 3*x^2 - 7 | x = 1.25 }",
                    esx + 3 * xv * xv - 7, __LINE__);

    /* f'(x) = cos(x)*exp(sin(x)) + 6*x  (explicit star) */
    check_parse_val("f' = cos(x)*exp(sin(x)) + 6*x at x=1.25", "{ cos(x)*exp(sin(x)) + 6*x | x = 1.25 }",
                    cx * esx + 6 * xv, __LINE__);

    /* f'(x) same expression with implicit mul for 6x */
    check_parse_val("f' = cos(x)*exp(sin(x)) + 6x (implicit 6x) at x=1.25", "{ cos(x)*exp(sin(x)) + 6x | x = 1.25 }",
                    cx * esx + 6 * xv, __LINE__);

    /* f''(x) = cos^2(x)*exp(sin(x)) - sin(x)*exp(sin(x)) + 6  (expanded) */
    check_parse_val("f'' expanded: cos^2(x)*exp(sin(x)) - sin(x)*exp(sin(x)) + 6",
                    "{ cos^2(x)*exp(sin(x)) - sin(x)*exp(sin(x)) + 6 | x = 1.25 }", cx * cx * esx - sx * esx + 6,
                    __LINE__);

    /* f''(x) same value, factored with brackets: (cos^2(x) - sin(x))*exp(sin(x)) + 6 */
    check_parse_val("f'' factored: (cos^2(x) - sin(x))*exp(sin(x)) + 6",
                    "{ (cos^2(x) - sin(x))*exp(sin(x)) + 6 | x = 1.25 }", (cx * cx - sx) * esx + 6, __LINE__);

    /* Additional bracket-heavy forms */
    check_parse_val("f'' double-bracket: (cos^2(x) - sin(x)) * (exp(sin(x))) + 6",
                    "{ (cos^2(x) - sin(x)) * (exp(sin(x))) + 6 | x = 1.25 }", (cx * cx - sx) * esx + 6, __LINE__);
    check_parse_val("(sin(x) + cos(x))^2 at x=0 = 1", "{ (sin(x) + cos(x))^2 | x = 0 }", 1.0, __LINE__);
    check_parse_val("exp((x_0 + x_1)^2) at (0,0) = 1", "{ exp((x_0 + x_1)^2) | x_0 = 0, x_1 = 0 }", 1.0, __LINE__);
    check_parse_val("(x^2 + 1)^2 at x=2 = 25", "{ (x^2 + 1)^2 | x = 2 }", 25.0, __LINE__);

    check_parse_simplified_expr("Dx(...) parses as expression derivative", "{ Dx(exp(x^2)) | x = ? }",
                                "{ 2x·exp(x²) | x = NAN }", __LINE__);
    check_parse_simplified_expr("Dx(...) infers only its derivative variable", "{ Dx(exp(x^2)) }",
                                "{ 2x·exp(x²) | x = NAN }", __LINE__);
    check_parse_simplified_expr("Dxx(...) parses as repeated expression derivative", "{ Dxx(x^3) | x = ? }",
                                "{ 6x | x = NAN }", __LINE__);
    check_parse_simplified_expr("Dxy(...) parses as mixed expression derivative", "{ Dxy(x^2*y^3) | x = ?, y = ? }",
                                "{ 6xy² | x = NAN, y = NAN }", __LINE__);
    check_parse_simplified_expr("D[x^5](...) parses as compact high-order derivative", "{ D[x^5](x^7) }",
                                "{ 2520x² | x = NAN }", __LINE__);
    check_parse_simplified_expr("D[x^2y^3](...) preserves mixed derivative order", "{ D[x^2y^3](x^3*y^4) }",
                                "{ 144xy | x = NAN, y = NAN }", __LINE__);
    check_parse_simplified_expr("canonical superscript derivative input round trips", "{ Dx²(x^3) }",
                                "{ 6x | x = NAN }", __LINE__);
    {
        expr_bindings_t *bindings = NULL;
        expr_t *parsed = expr_from_string("{ Dxxxxx((x + 1)/(x^2 + 3*x + 5)) | x = ? }", &bindings);

        TEST_ASSERT_TRUE(parsed != NULL, "high-order derivative metadata input parses");
        TEST_ASSERT_TRUE(expr_bindings_has_symbolic_derivative(bindings), "bindings retain explicit derivative syntax");
        expr_free(parsed);
        expr_bindings_free(bindings);
    }
    {
        expr_bindings_t *bindings = NULL;
        expr_t *parsed = expr_from_string("{ (x + 1)/(x^2 + 3*x + 5) | x = ? }", &bindings);

        TEST_ASSERT_TRUE(parsed != NULL, "ordinary rational metadata input parses");
        TEST_ASSERT_TRUE(!expr_bindings_has_symbolic_derivative(bindings),
                         "ordinary expression has no derivative metadata");
        expr_free(parsed);
        expr_bindings_free(bindings);
    }
    check_parse_val("Dx(...) derivative evaluates numerically", "{ Dx(exp(x^2)) | x = 1.25 }", 2.0 * xv * exp(xv * xv),
                    __LINE__);

    /* ---- Programmatic differentiation ---- */
    /* Build f(x) = exp(sin(x)) + 3*x^2 - 7 explicitly so we hold the wrt pointer. */
    {
        expr_t *xvar = test_expr_new_named_var_d(xv, "x");
        expr_t *sinx = expr_sin(xvar);
        expr_t *esinx = expr_exp(sinx);
        expr_t *x2 = expr_pow_d(xvar, 2.0);
        expr_t *t = expr_mul_d(x2, 3.0);
        expr_t *t2 = expr_sub_d(t, 7.0);
        expr_t *f = expr_add(esinx, t2);

        check_expr_d("f(1.25) via expr_eval_d", f, esx + 3 * xv * xv - 7, __LINE__);

        expr_t *df = expr_create_deriv(f, xvar);
        check_expr_d("f'(1.25) via expr_create_deriv", df, cx * esx + 6 * xv, __LINE__);

        expr_t *d2f = expr_create_deriv(df, xvar);
        check_expr_d("f''(1.25) via expr_create_deriv", d2f, cx * cx * esx - sx * esx + 6, __LINE__);

        expr_free(d2f);
        expr_free(df);
        expr_free(f);
        expr_free(t2);
        expr_free(t);
        expr_free(x2);
        expr_free(esinx);
        expr_free(sinx);
        expr_free(xvar);
    }

    /* Long decimal coefficients can exceed the exact-rational work budget.
     * Their precision-bounded conversion must remain alive while the parsed
     * antiderivative is simplified and differentiated. */
    {
        const char *input =
            "{ 1/2*ln(x^2+3*x+5)"
            "-0.30151134457776362264681206697006242581155350414448669064169837691968042205536762242807"
            "*atan(0.60302268915552724529362413394012485162310700828897338128339675383936084411073524485615"
            "*x+0.90453403373329086794043620091018727743466051243346007192509513075904126616610286728422)"
            "+C₀ | x=1 }";
        expr_bindings_t *bindings = NULL;
        expr_t *parsed = expr_from_string(input, &bindings);
        expr_t *xvar = bindings ? expr_bindings_get(bindings, "x") : NULL;
        expr_t *deriv = (parsed && xvar) ? expr_create_deriv(parsed, xvar) : NULL;

        ASSERT_NOT_NULL(parsed);
        ASSERT_NOT_NULL(xvar);
        ASSERT_NOT_NULL(deriv);
        check_expr_d("long-decimal integral differentiates at x=1", deriv, 2.0 / 9.0, __LINE__);

        expr_free(deriv);
        expr_free(parsed);
        expr_bindings_free(bindings);
    }
}

/* ---- Error paths — all must return NULL ----
 * Note: expr_from_string writes diagnostics to stderr for these cases. */

static void test_from_string_errors(void)
{
    /* NULL input (silent — no stderr output) */
    check_parse_null("NULL input", NULL, __LINE__);
    /* Missing closing '}' */
    check_parse_null_stderr_contains("no closing brace", "{ x | x = 1", "}", __LINE__);
    /* Unexpected token in expression */
    check_parse_null_stderr_contains("unexpected token", "{ ? | x = 1 }", "expected expression", __LINE__);
    /* Duplicate variable name */
    check_parse_null("duplicate var name", "{ x | x = 1, x = 2 }", __LINE__);
    /* Same name used as both variable and named constant */
    check_parse_null("var-const name clash", "{ x | x = 1; x = 2 }", __LINE__);
    /* Missing '=' in binding */
    check_parse_null_stderr_contains("missing '=' in binding", "{ x | x 1 }", "trailing input", __LINE__);
    /* Missing numeric value after '=' in binding */
    check_parse_null_stderr_contains("missing value in binding", "{ x | x = }", "incorrect syntax for x:", __LINE__);
    /* Malformed binding value should identify the binding that failed. */
    check_parse_null_stderr_contains("malformed variable binding identifies name", "{ x | x = 1+ }",
                                     "incorrect syntax for x: 1+", __LINE__);
    check_parse_null_stderr_contains("malformed constant binding identifies name", "{ x + c | x = 1; c = sin() }",
                                     "incorrect syntax for c: sin()", __LINE__);
    check_parse_null_stderr_contains("unclosed paren binding identifies name", "{ a^(3x) | x = 8*pi; a = 2^(3*pi }",
                                     "incorrect syntax for a: 2^(3*pi", __LINE__);
    /* Missing exponent after '^' */
    check_parse_null("missing exponent after '^'", "{ x^ | x = 2 }", __LINE__);
    /* Empty exponent after function-name '^' */
    check_parse_null("missing function exponent digits", "{ sin^() | x = 0 }", __LINE__);
    /* Malformed numeric exponent */
    check_parse_null("malformed decimal exponent", "{ x^2e+ | x = 2 }", __LINE__);
    /* Binary function with too few arguments */
    check_parse_null("binary function missing arg", "{ atan2(x) | x = 1 }", __LINE__);
    /* Unary function with too many arguments */
    check_parse_null("unary function extra arg", "{ sin(x, y) | x = 1, y = 2 }", __LINE__);
    /* Missing closing ')' in grouped expression */
    check_parse_null("missing closing paren", "{ (x + 1 | x = 2 }", __LINE__);
    /* Missing closing ']' in bracketed name */
    check_parse_null("missing closing bracket", "{ [radius | [radius] = 5 }", __LINE__);
    /* Trailing expression input */
    check_parse_null("trailing input after expression", "{ x y z | x = 1, y = 2, z = 3 }", __LINE__);
    /* Extra comma in binary function */
    check_parse_null("binary function extra comma", "{ atan2(x, y, z) | x = 1, y = 2, z = 3 }", __LINE__);
}

static void test_from_expression_string_api(void)
{
    expr_t *x = test_expr_new_named_var_d(3.0, "x");
    expr_t *y = test_expr_new_named_var_d(4.0, "y");
    expr_t *c = test_expr_new_named_const_d(2.0, "c");

    const char *names[] = {"x", "y", "c"};
    expr_t *symbols[] = {x, y, c};

    expr_t *ok = expr_from_expression_string("c*(x + y)", names, symbols, 3);
    if (!ok) {
        printf(C_BOLD C_RED "FAIL" C_RESET " bare expression parse returned NULL %s:%d:1\n\n", __FILE__, __LINE__);
        TEST_FAIL();
    } else {
        double got = expr_eval_d(ok);
        double expect = 14.0;
        double err = fabs(got - expect);
        if (err < 2e-14) {
            printf(C_BOLD C_GREEN "PASS" C_RESET " bare expression parse API\n\n");
        } else {
            printf(C_BOLD C_RED "FAIL" C_RESET " bare expression parse API %s:%d:1\n", __FILE__, __LINE__);
            printf(C_BOLD "  got     " C_RESET "%.17g\n", got);
            printf(C_BOLD "  expect  " C_RESET "%.17g\n\n", expect);
            TEST_FAIL();
        }
        expr_free(ok);
    }

    {
        expr_bindings_t *bare_bindings = NULL;
        expr_t *bare = expr_from_string("x + y", &bare_bindings);
        string_t *bare_text = string_new_with("x + y");
        expr_bindings_t *text_bindings = NULL;
        expr_t *from_text = expr_from_text(bare_text, &text_bindings);

        TEST_ASSERT_NOT_NULL(bare);
        TEST_ASSERT_NOT_NULL(bare_bindings);
        TEST_ASSERT_TRUE(expr_bindings_count(bare_bindings) == 2u, "bare shorthand returns both inferred bindings");
        TEST_ASSERT_NOT_NULL(expr_bindings_get(bare_bindings, "x"));
        TEST_ASSERT_NOT_NULL(expr_bindings_get(bare_bindings, "y"));
        TEST_ASSERT_NOT_NULL(from_text);
        TEST_ASSERT_TRUE(expr_bindings_count(text_bindings) == 2u, "expr_from_text infers bare shorthand bindings");

        expr_free(from_text);
        expr_bindings_free(text_bindings);
        string_free(bare_text);
        expr_free(bare);
        expr_bindings_free(bare_bindings);
    }

    if (expr_from_expression_string("x + y", names, NULL, 2) != NULL) {
        printf(C_BOLD C_RED "FAIL" C_RESET " incomplete symbol table should fail %s:%d:1\n\n", __FILE__, __LINE__);
        TEST_FAIL();
    } else {
        printf(C_BOLD C_GREEN "PASS" C_RESET " incomplete symbol table rejected\n\n");
    }

    {
        const char *dup_names[] = {"x", "x"};
        expr_t *dup_symbols[] = {x, y};
        if (expr_from_expression_string("x + 1", dup_names, dup_symbols, 2) != NULL) {
            printf(C_BOLD C_RED "FAIL" C_RESET " duplicate external symbols should fail %s:%d:1\n\n", __FILE__,
                   __LINE__);
            TEST_FAIL();
        } else {
            printf(C_BOLD C_GREEN "PASS" C_RESET " duplicate external symbols rejected\n\n");
        }
    }

    if (expr_from_expression_string("x + z", names, symbols, 3) != NULL) {
        printf(C_BOLD C_RED "FAIL" C_RESET " unknown external symbol should fail %s:%d:1\n\n", __FILE__, __LINE__);
        TEST_FAIL();
    } else {
        printf(C_BOLD C_GREEN "PASS" C_RESET " unknown external symbol rejected\n\n");
    }

    expr_free(c);
    expr_free(y);
    expr_free(x);
}

static void test_from_string_bindings_api(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *expr = expr_from_string("{ x^2 + c_1 }", &bindings);
    expr_t *x_binding;
    expr_t *c_binding;
    expr_t *deriv;

    if (!expr) {
        printf(C_BOLD C_RED "FAIL" C_RESET " parse with bindings returned NULL %s:%d:1\n\n", __FILE__, __LINE__);
        TEST_FAIL();
        return;
    }

    x_binding = expr_bindings_get(bindings, "x");
    c_binding = expr_bindings_get(bindings, "c₁");

    if (!x_binding) {
        printf(C_BOLD C_RED "FAIL" C_RESET " inferred x binding missing %s:%d:1\n\n", __FILE__, __LINE__);
        TEST_FAIL();
    } else {
        printf(C_BOLD C_GREEN "PASS" C_RESET " inferred x binding returned for differentiation\n\n");
    }

    if (!c_binding) {
        printf(C_BOLD C_RED "FAIL" C_RESET " inferred c₁ binding missing %s:%d:1\n\n", __FILE__, __LINE__);
        TEST_FAIL();
    } else {
        printf(C_BOLD C_GREEN "PASS" C_RESET " inferred c₁ constant binding returned\n\n");
    }

    {
        number_t xval = num_create_from_double(3.0);
        number_t cval = num_create_from_double(5.0);

        if (!x_binding || !c_binding) {
            num_destroy(&cval);
            num_destroy(&xval);
            printf(C_BOLD C_RED "FAIL" C_RESET " binding lookup failed before assignment %s:%d:1\n\n", __FILE__,
                   __LINE__);
            TEST_FAIL();
            expr_bindings_free(bindings);
            expr_free(expr);
            return;
        }

        expr_set_val(x_binding, xval);
        expr_set_val(c_binding, cval);

        num_destroy(&cval);
        num_destroy(&xval);
    }

    check_expr_d("parsed expr after binding update", expr, 14.0, __LINE__);

    deriv = expr_create_deriv(expr, x_binding);
    if (!deriv) {
        printf(C_BOLD C_RED "FAIL" C_RESET " derivative from inferred binding returned NULL %s:%d:1\n\n", __FILE__,
               __LINE__);
        TEST_FAIL();
    } else {
        check_expr_d("derivative from inferred x binding", deriv, 6.0, __LINE__);
        expr_free(deriv);
    }

    expr_bindings_free(bindings);
    expr_free(expr);
}

static void test_from_string_bindings_with_implicit_builtin_constant(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *expr = expr_from_string("{ exp(pi*sqrt(x)) | x = 163 }", &bindings);
    char *got = expr ? expr_to_string(expr, style_EXPRESSION) : NULL;
    const char *expect = "{ exp(π·√(x)) | x = 163 }";

    if (expr && got && str_eq(got, expect) && expr_bindings_count(bindings) == 1u &&
        expr_bindings_get(bindings, "x") != NULL && expr_bindings_get(bindings, "@pi") == NULL)
        to_string_pass("bindings keep implicit pi constant inference", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "bindings keep implicit pi constant inference", got ? got : "(null)",
                       expect);

    free(got);
    expr_bindings_free(bindings);
    expr_free(expr);

    bindings = NULL;
    expr = expr_from_string("{ exp(pi*sqrt(x)) | x = 163/1 }", &bindings);
    got = expr ? expr_to_string(expr, style_EXPRESSION) : NULL;
    if (expr && got && str_eq(got, expect))
        to_string_pass("bindings suppress denominator-one fractions", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "bindings suppress denominator-one fractions", got ? got : "(null)",
                       expect);

    free(got);
    expr_bindings_free(bindings);
    expr_free(expr);
}

static void test_from_string_bindings_with_constant_expression_value(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *expr;
    expr_t *x;
    expr_t *deriv;
    number_t expr_value;
    char *deriv_text;
    size_t old_precision_bits;
    size_t low_bits;
    size_t high_bits;
    number_t x_value;
    number_t two;

    check_parse_val("binding value may be builtin constant expression", "{ e^(sin(x)) | x = pi/2 }", M_E, __LINE__);
    check_parse_expr("binding value preserves symbolic pi/2", "{ e^(sin(x)) | x = pi/2 }", "{ e^sin(x) | x = π/2 }",
                     __LINE__);
    check_parse_expr("binding value preserves symbolic 3/2*pi", "{ e^(sin(x)) | x = 3/2*pi }",
                     "{ e^sin(x) | x = ³⁄₂π }", __LINE__);
    check_parse_expr("binding value preserves symbolic pi^2/2", "{ x | x = (pi^2)/2 }", "π²/2", __LINE__);
    check_parse_expr("pure numeric expression preserves full mathematical tree", "{ phi - 1/2(1+sqrt(5)) }",
                     "φ - ½·(1 + √(5))", __LINE__);
    check_parse_expr("binding value preserves full mathematical tree", "{ x | x = 1/2(1+sqrt(5)) }", "¹⁄₂·(1 + √(5))",
                     __LINE__);
    check_parse_expr("binding value round-trips pretty multiply", "{ -x + phi | x = ½·(1 + √(5)) }",
                     "{ -x + φ | x = ½·(1 + √(5)) }", __LINE__);
    check_parse_expr("binding value preserves mathematical notation functions",
                     "{ x | x = abs(-3)+floor(pi)+ceil(phi) }", "|-3| + ⌊π⌋ + ⌈φ⌉", __LINE__);
    check_parse_expr("binding value accepts unknown marker", "{ sinh(x) | x = ? }", "{ sinh(x) | x = NAN }", __LINE__);
    check_parse_expr("user-bound e remains symbolic", "{ E - M - e·sin(E) | ; M = pi/1.234, e=0.0167 }",
                     "{ E - M - e·sin(E) | E = NAN; M = π/1.234, e = 0.0167 }", __LINE__);
    check_parse_expr("constant binding simplifies Lambert e power",
                     "{ polygamma(9, x*y*c) | x = 7, y = 1; c = W(2e^2) }", "{ ψ⁽⁹⁾(cxy) | x = 7, y = 1; c = 2 }",
                     __LINE__);
    check_parse_expr("simplified constants keep bindings",
                     "{ ax + yb + zc - 8 | a = NAN, b = NAN, c = NAN; x=1, y = 2, z = 3 }",
                     "{ ax + by + cz - 8 | a = NAN, b = NAN, c = NAN; x = 1, y = 2, z = 3 }", __LINE__);
    check_parse_expr("constant bindings accept semicolon separators",
                     "{ ax + by + cz - 8 | a = NAN, b = NAN, c = NAN; x = 1; y = 2, z = 3 }",
                     "{ ax + by + cz - 8 | a = NAN, b = NAN, c = NAN; x = 1, y = 2, z = 3 }", __LINE__);
    check_parse_expr("const-only binding preserves leading semicolon", "{ exp(π·√(H)) | ;H = 163 }",
                     "{ exp(π·√(H)) | ; H = 163 }", __LINE__);
    check_parse_expr("const-only binding accepts trailing digit subscript", "{ exp(π·√(H8)) | ; H8 = 163 }",
                     "{ exp(π·√(H₈)) | ; H₈ = 163 }", __LINE__);
    check_parse_expr("minus after factor remains subtraction", "{ exp(π·√(H8))-(5x)^3 | H8 = 163, x = NAN }",
                     "{ exp(π·√(H₈)) - (5x)³ | H₈ = 163, x = NAN }", __LINE__);
    check_parse_expr("absolute-value bars omit inner spaces", "{ exp(pi*sqrt(x)) - | exp(pi*sqrt(x)) | | x = NAN }",
                     "{ exp(π·√(x)) - |exp(π·√(x))| | x = NAN }", __LINE__);
    check_parse_expr("symbolic pi quotient cancels powers", "{ pi/pi^2 }", "π/π²", __LINE__);
    check_parse_simplified_expr(
        "generated derivative with NaN bindings simplifies explicitly",
        "{ -y²z²·sin(xyz)·exp(sin(xyz)) + y²z²·cos²(xyz)·exp(sin(xyz)) | y = NAN, z = NAN, x = NAN }",
        "{ y²z²·exp(sin(xyz))·(cos²(xyz) - sin(xyz)) | y = NAN, z = NAN, x = NAN }", __LINE__);
    check_parse_simplified_expr("reparsed symbolic pi derivative simplifies explicitly",
                                "{ (-2π·exp(π·√(x)) + 2·π²·√(x)·exp(π·√(x)))/(2·√(x))/(2·√(x))² | x = 163 }",
                                "{ ¼·exp(π·√(x))/x^³⁄₂·(π^2·√(x) - π) | x = 163 }", __LINE__);
    check_parse_simplified_expr(
        "nested rational derivative numerator canonicalizes",
        "{ (2*(x^2 + 3*x + 5)*(2*(x + 1)*(2*x + 3) - 9*x - (-x - 1)*(2*x + 3) - 3*x^2 - 15) - 6*(2*x + 3)*((-x - "
        "1)*(x^2 + 3*x + 5) - (2*x + 3)*(3*x - (x + 1)*(2*x + 3) + x^2 + 5)))/(x^2 + 3*x + 5)^4 | x = 1 }",
        "{ 6/(x² + 3x + 5)⁴·(12x² + 44x - x⁴ - 4x³ + 23) | x = 1 }", __LINE__);
    check_parse_simplified_expr("higher rational derivative numerator canonicalizes",
                                "{ (24*(x^2 + 3*x + 5)*(6*x - x^3 - 3*x^2 + 11) - 24*(2*x + 3)*(12*x^2 + 44*x - x^4 - "
                                "4*x^3 + 23))/(x^2 + 3*x + 5)^5 | x = ? }",
                                "{ 24/(x² + 3x + 5)⁵·(x⁵ + 5x⁴ - 20x³ - 110x² - 115x - 14) | x = NAN }", __LINE__);
    bindings = NULL;
    expr = expr_from_string(
        "{ 720*(x^7 + 7*x^6 - 42*x^5 - 385*x^4 - 805*x^3 - 294*x^2 + 511*x + 289)/(x^2 + 3*x + 5)^7 | x = ? }",
        &bindings);
    x = bindings ? expr_bindings_get(bindings, "x") : NULL;
    deriv = (expr && x) ? expr_create_deriv(expr, x) : NULL;
    deriv_text = deriv ? expr_to_string(deriv, style_EXPRESSION) : NULL;
    if (deriv_text && strcmp(deriv_text, "{ 1/(x² + 3x + 5)⁸·(-5040x⁸ - 40320x⁷ + 282240x⁶ + 3104640x⁵ + "
                                         "8114400x⁴ + 3951360x³ - 10301760x² - 11652480x - 2530080) | x = NAN }") == 0) {
        printf(C_BOLD C_GREEN "PASS" C_RESET " rational quadratic-power derivative uses polynomial fast path\n");
        printf(C_BOLD "  expr   " C_RESET "%s\n\n", deriv_text);
    } else {
        printf(C_BOLD C_RED "FAIL" C_RESET " rational quadratic-power derivative uses polynomial fast path %s:%d:1\n",
               __FILE__, __LINE__);
        printf(C_BOLD "  got    " C_RESET "%s\n\n", deriv_text ? deriv_text : "(null)");
        TEST_FAIL();
    }
    free(deriv_text);
    expr_free(deriv);
    expr_bindings_free(bindings);
    expr_free(expr);
    bindings = NULL;
    expr = NULL;
    x = NULL;
    deriv = NULL;

    bindings = NULL;
    expr = expr_from_string("{ 5040*(56*x^6 - 2312*x - x^8 - 8*x^7 + 616*x^5 + 1610*x^4 + 784*x^3 - 2044*x^2 - "
                            "502)/(x^2 + 3*x + 5)^8 | x = ? }",
                            &bindings);
    x = bindings ? expr_bindings_get(bindings, "x") : NULL;
    deriv = (expr && x) ? expr_create_deriv(expr, x) : NULL;
    deriv_text = deriv ? expr_to_string(deriv, style_EXPRESSION) : NULL;
    if (deriv_text &&
        strcmp(deriv_text, "{ 1/(x² + 3x + 5)⁹·(40320x⁹ + 362880x⁸ - 2903040x⁷ - 37255680x⁶ - 116847360x⁵ - "
                           "71124480x⁴ + 247242240x³ + 419489280x² + 182165760x + 2459520) | x = NAN }") == 0) {
        printf(C_BOLD C_GREEN "PASS" C_RESET " rational derivative polynomial storage follows degree\n");
        printf(C_BOLD "  expr   " C_RESET "%s\n\n", deriv_text);
    } else {
        printf(C_BOLD C_RED "FAIL" C_RESET " rational derivative polynomial storage follows degree %s:%d:1\n", __FILE__,
               __LINE__);
        printf(C_BOLD "  got    " C_RESET "%s\n\n", deriv_text ? deriv_text : "(null)");
        TEST_FAIL();
    }
    free(deriv_text);
    expr_free(deriv);
    expr_bindings_free(bindings);
    expr_free(expr);
    bindings = NULL;
    expr = NULL;
    x = NULL;
    deriv = NULL;

    old_precision_bits = num_get_default_prec_bits();
    ASSERT_EQ_INT(num_set_default_prec_bits(80u), 0);
    expr = expr_from_string("{ x - phi | x = 1/2(1+sqrt(5)) }", &bindings);
    x = bindings ? expr_bindings_get(bindings, "x") : NULL;
    ASSERT_NOT_NULL(expr);
    ASSERT_NOT_NULL(x);
    x_value = expr_eval(x);
    low_bits = num_get_effective_prec_bits(x_value);
    num_destroy(&x_value);
    ASSERT_EQ_INT(num_set_default_prec_bits(256u), 0);
    x_value = expr_eval(x);
    high_bits = num_get_effective_prec_bits(x_value);
    ASSERT_TRUE(high_bits > low_bits);
    ASSERT_TRUE(high_bits >= 256u);
    num_destroy(&x_value);
    ASSERT_EQ_INT(num_set_default_prec_bits(old_precision_bits), 0);
    expr_bindings_free(bindings);
    expr_free(expr);

    bindings = NULL;
    expr = expr_from_string("{ x | x = 1/2(1+sqrt(5)) }", &bindings);
    x = bindings ? expr_bindings_get(bindings, "x") : NULL;
    ASSERT_NOT_NULL(expr);
    ASSERT_NOT_NULL(x);
    two = num_create_from_long(2L);
    expr_set_val(x, two);
    num_destroy(&two);
    old_precision_bits = num_get_default_prec_bits();
    ASSERT_EQ_INT(num_set_default_prec_bits(old_precision_bits + 64u), 0);
    x_value = expr_eval(x);
    two = num_create_from_long(2L);
    ASSERT_TRUE(num_eq(x_value, two));
    num_destroy(&two);
    num_destroy(&x_value);
    ASSERT_EQ_INT(num_set_default_prec_bits(old_precision_bits), 0);
    expr_bindings_free(bindings);
    expr_free(expr);
    bindings = NULL;

    expr = expr_from_string("{ e^(sin(x)) | x = pi/2 }", &bindings);
    x = bindings ? expr_bindings_get(bindings, "x") : NULL;
    deriv = (expr && x) ? expr_create_deriv(expr, x) : NULL;
    deriv_text = deriv ? expr_to_string(deriv, style_EXPRESSION) : NULL;

    if (deriv_text && strcmp(deriv_text, "{ cos(x)·exp(sin(x)) | x = π/2 }") == 0) {
        to_string_pass("e^sin(x) derivative uses exp simplification", deriv_text, "{ cos(x)·exp(sin(x)) | x = π/2 }");
    } else {
        to_string_fail(__FILE__, __LINE__, 1, "e^sin(x) derivative uses exp simplification",
                       deriv_text ? deriv_text : "(null)", "{ cos(x)·exp(sin(x)) | x = π/2 }");
    }

    if (deriv) {
        expr_value = expr_eval(deriv);
        if (num_eq(expr_value, NUM_ZERO)) {
            printf(C_BOLD C_GREEN "PASS" C_RESET " derivative at symbolic pi/2 evaluates exactly to zero\n\n");
        } else {
            string_t *got = num_to_string(expr_value);

            printf(C_BOLD C_RED "FAIL" C_RESET " derivative at symbolic pi/2 evaluates exactly to zero %s:%d:1\n",
                   __FILE__, __LINE__);
            printf(C_BOLD "  got    " C_RESET "%s\n", got ? string_c_str(got) : "(null)");
            printf(C_BOLD "  expect " C_RESET "0\n\n");
            string_free(got);
            TEST_FAIL();
        }
        num_destroy(&expr_value);
    } else {
        printf(C_BOLD C_RED "FAIL" C_RESET " derivative at symbolic pi/2 evaluates exactly to zero %s:%d:1\n", __FILE__,
               __LINE__);
        printf(C_BOLD "  error  " C_RESET "parser or derivative returned NULL\n\n");
        TEST_FAIL();
    }

    free(deriv_text);
    if (deriv)
        expr_free(deriv);
    expr_bindings_free(bindings);
    if (expr)
        expr_free(expr);

    bindings = NULL;
    expr = expr_from_string("{ x/pi }", &bindings);
    x = bindings ? expr_bindings_get(bindings, "x") : NULL;
    deriv = (expr && x) ? expr_create_deriv(expr, x) : NULL;
    deriv_text = deriv ? expr_to_string(deriv, style_EXPRESSION) : NULL;

    if (deriv_text && strcmp(deriv_text, "1/π") == 0) {
        to_string_pass("x/pi derivative simplifies symbolic quotient", deriv_text, "1/π");
    } else {
        to_string_fail(__FILE__, __LINE__, 1, "x/pi derivative simplifies symbolic quotient",
                       deriv_text ? deriv_text : "(null)", "1/π");
    }

    free(deriv_text);
    if (deriv)
        expr_free(deriv);
    expr_bindings_free(bindings);
    if (expr)
        expr_free(expr);

    bindings = NULL;
    expr = expr_from_string("{ pi*exp(pi*sqrt(x))/(2*sqrt(x)) | x = 163 }", &bindings);
    x = bindings ? expr_bindings_get(bindings, "x") : NULL;
    deriv = (expr && x) ? expr_create_deriv(expr, x) : NULL;
    deriv_text = deriv ? expr_to_string(deriv, style_EXPRESSION) : NULL;

    if (deriv_text && strcmp(deriv_text, "{ ¼π·exp(π·√(x))/x^³⁄₂·(π·√(x) - 1) | x = 163 }") == 0) {
        to_string_pass("symbolic pi derivative keeps exact coefficient", deriv_text,
                       "{ ¼π·exp(π·√(x))/x^³⁄₂·(π·√(x) - 1) | x = 163 }");
    } else {
        to_string_fail(__FILE__, __LINE__, 1, "symbolic pi derivative keeps exact coefficient",
                       deriv_text ? deriv_text : "(null)", "{ ¼π·exp(π·√(x))/x^³⁄₂·(π·√(x) - 1) | x = 163 }");
    }

    free(deriv_text);
    if (deriv)
        expr_free(deriv);
    expr_bindings_free(bindings);
    if (expr)
        expr_free(expr);

    bindings = NULL;
    expr = expr_from_string("{ ¼π*exp(π*sqrt(x))*(π*sqrt(x)-1)/x^(3/2) | x = 30π/180 }", &bindings);
    x = bindings ? expr_bindings_get(bindings, "x") : NULL;
    deriv = (expr && x) ? expr_create_deriv(expr, x) : NULL;
    deriv_text = deriv ? expr_to_string(deriv, style_EXPRESSION) : NULL;

    if (deriv_text && strcmp(deriv_text, "{ ⅛π·exp(π·√(x))/x^⁵⁄₂·(3 - 3π·√(x) + π²·x) | x = ⅙π }") == 0) {
        to_string_pass("symbolic coefficient folding happens in derivative DAG", deriv_text,
                       "{ ⅛π·exp(π·√(x))/x^⁵⁄₂·(3 - 3π·√(x) + π²·x) | x = ⅙π }");
    } else {
        to_string_fail(__FILE__, __LINE__, 1, "symbolic coefficient folding happens in derivative DAG",
                       deriv_text ? deriv_text : "(null)", "{ ⅛π·exp(π·√(x))/x^⁵⁄₂·(3 - 3π·√(x) + π²·x) | x = ⅙π }");
    }

    free(deriv_text);
    if (deriv)
        expr_free(deriv);
    expr_bindings_free(bindings);
    if (expr)
        expr_free(expr);

    bindings = NULL;
    expr = expr_from_string("{ π·exp(π·√(x))·(π·√(x) - 1)/(4x^³⁄₂) | x = 163 }", &bindings);
    x = bindings ? expr_bindings_get(bindings, "x") : NULL;
    deriv = (expr && x) ? expr_create_deriv(expr, x) : NULL;
    deriv_text = deriv ? expr_to_string(deriv, style_EXPRESSION) : NULL;

    if (deriv_text && strcmp(deriv_text, "{ ⅛π·exp(π·√(x))/x^⁵⁄₂·(3 - 3π·√(x) + π²·x) | x = 163 }") == 0) {
        to_string_pass("nested symbolic pi derivative factors common terms", deriv_text,
                       "{ ⅛π·exp(π·√(x))/x^⁵⁄₂·(3 - 3π·√(x) + π²·x) | x = 163 }");
    } else {
        to_string_fail(__FILE__, __LINE__, 1, "nested symbolic pi derivative factors common terms",
                       deriv_text ? deriv_text : "(null)", "{ ⅛π·exp(π·√(x))/x^⁵⁄₂·(3 - 3π·√(x) + π²·x) | x = 163 }");
    }

    free(deriv_text);
    if (deriv)
        expr_free(deriv);
    expr_bindings_free(bindings);
    if (expr)
        expr_free(expr);
}

static void test_from_string_bindings_skip_function_name_letters(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *expr = expr_from_string("{ exp(@pi*i) }", &bindings);
    expr_t *x_binding;
    expr_t *i_binding;
    qcomplex_t value;
    qfloat_t err;

    if (!expr) {
        printf(C_BOLD C_RED "FAIL" C_RESET " function-name binding parse returned NULL %s:%d:1\n\n", __FILE__,
               __LINE__);
        TEST_FAIL();
        return;
    }

    x_binding = expr_bindings_get(bindings, "x");
    i_binding = expr_bindings_get(bindings, "i");

    if (!x_binding) {
        printf(C_BOLD C_GREEN "PASS" C_RESET " function names do not create bogus x binding\n\n");
    } else {
        printf(C_BOLD C_RED "FAIL" C_RESET " function names do not create bogus x binding %s:%d:1\n\n", __FILE__,
               __LINE__);
        TEST_FAIL();
    }

    if (!i_binding) {
        printf(C_BOLD C_GREEN "PASS" C_RESET " implicit i is not returned as an editable binding\n\n");
    } else {
        printf(C_BOLD C_RED "FAIL" C_RESET " implicit i is not returned as an editable binding %s:%d:1\n\n", __FILE__,
               __LINE__);
        TEST_FAIL();
    }

    value = test_expr_eval_qc(expr);
    err = qc_abs(qc_sub(value, qc_make(qf_from_double(-1.0), QF_ZERO)));
    if (qf_lt(err, qf_from_double(1e-30))) {
        printf(C_BOLD C_GREEN "PASS" C_RESET " exp(@pi*i) close to -1\n\n");
    } else {
        printf(C_BOLD C_RED "FAIL" C_RESET " exp(@pi*i) close to -1 %s:%d:1\n\n", __FILE__, __LINE__);
        TEST_FAIL();
    }

    expr_bindings_free(bindings);
    expr_free(expr);
}

static void test_binding_edit_is_owned_by_expression_library(void)
{
    expr_bindings_t *bindings = NULL;
    expr_bindings_t *edited_bindings = NULL;
    expr_t *expr = expr_from_string("{ a*x + C_0 + C_1 | x = NAN; a = 2, C_0 = NAN, C_1 = NAN }", &bindings);
    expr_t *edited = expr_edit_binding(expr, bindings, "C_0", "", &edited_bindings);

    TEST_ASSERT_TRUE(edited != NULL, "binding edit returns a canonical expression");
    TEST_ASSERT_TRUE(expr_bindings_get(edited_bindings, "x") != NULL, "binding edit retains x");
    TEST_ASSERT_TRUE(expr_bindings_get(edited_bindings, "a") != NULL, "binding edit retains ordinary constants");
    TEST_ASSERT_TRUE(expr_bindings_get(edited_bindings, "C_0") == NULL,
                     "binding edit removes only the selected constant");
    TEST_ASSERT_TRUE(expr_bindings_get(edited_bindings, "C_1") != NULL,
                     "binding edit retains other integration constants");

    expr_bindings_free(edited_bindings);
    expr_free(edited);
    expr_bindings_free(bindings);
    expr_free(expr);
}

static void test_binding_edit_preserves_symbolic_value(void)
{
    expr_bindings_t *bindings = NULL;
    expr_bindings_t *edited_bindings = NULL;
    expr_bindings_t *fraction_bindings = NULL;
    expr_t *expr = expr_from_string("{ x^2 | x = NAN }", &bindings);
    expr_t *edited = expr_edit_binding(expr, bindings, "x", "pi", &edited_bindings);
    expr_t *fraction = expr_edit_binding(expr, bindings, "x", "½", &fraction_bindings);
    char *expr_text = edited ? expr_to_string(edited, style_EXPRESSION) : NULL;
    char *func_text = edited ? expr_to_string(edited, style_FUNCTION) : NULL;
    char *fraction_func_text = fraction ? expr_to_string(fraction, style_FUNCTION) : NULL;
    bool expression_preserved = expr_text && strstr(expr_text, "x = π");
    bool function_preserved = func_text && strstr(func_text, "x = @pi\noutput(expr(x));");
    bool fraction_preserved = fraction_func_text && strstr(fraction_func_text, "x = ½\noutput(expr(x));");

    free(fraction_func_text);
    free(func_text);
    free(expr_text);
    expr_bindings_free(fraction_bindings);
    expr_free(fraction);
    expr_bindings_free(edited_bindings);
    expr_free(edited);
    expr_bindings_free(bindings);
    expr_free(expr);

    TEST_ASSERT_TRUE(expression_preserved, "binding edit preserves pi in expression output");
    TEST_ASSERT_TRUE(function_preserved, "binding edit preserves pi in function output");
    TEST_ASSERT_TRUE(fraction_preserved, "function output preserves a Unicode fraction binding");
}

static void test_from_string_unevaluated_integral(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *expr = expr_from_string("{ ∫^x exp(cosh(t)) dt }", &bindings);
    expr_t *x = bindings ? expr_bindings_get(bindings, "x") : NULL;
    expr_t *t = bindings ? expr_bindings_get(bindings, "t") : NULL;
    expr_t *deriv = (expr && x) ? expr_create_deriv(expr, x) : NULL;
    char *text = expr ? expr_to_string(expr, style_EXPRESSION) : NULL;
    char *func_text = expr ? expr_to_string(expr, style_FUNCTION) : NULL;
    char *deriv_text = deriv ? expr_to_string(deriv, style_EXPRESSION) : NULL;

    if (text && strcmp(text, "{ ∫^x exp(cosh(t))·dt | x = NAN }") == 0) {
        printf(C_BOLD C_GREEN "PASS" C_RESET " unevaluated integral pretty form parses\n");
        printf(C_BOLD "  expr   " C_RESET "%s\n\n", text);
    } else {
        printf(C_BOLD C_RED "FAIL" C_RESET " unevaluated integral pretty form parses %s:%d:1\n", __FILE__, __LINE__);
        printf(C_BOLD "  got    " C_RESET "%s\n\n", text ? text : "(null)");
        TEST_FAIL();
    }

    if (expr && expr_is_differentiable(expr)) {
        printf(C_BOLD C_GREEN "PASS" C_RESET
                              " unevaluated integral stays differentiable for symbolic derivative controls\n\n");
    } else {
        printf(C_BOLD C_RED "FAIL" C_RESET
                            " unevaluated integral stays differentiable for symbolic derivative controls %s:%d:1\n\n",
               __FILE__, __LINE__);
        TEST_FAIL();
    }

    if (expr_contains_integral_operation(expr)) {
        printf(C_BOLD C_GREEN "PASS" C_RESET " unevaluated integral is reported as an evaluation operation\n\n");
    } else {
        printf(C_BOLD C_RED "FAIL" C_RESET " unevaluated integral is reported as an evaluation operation %s:%d:1\n\n",
               __FILE__, __LINE__);
        TEST_FAIL();
    }

    if (!t) {
        printf(C_BOLD C_GREEN "PASS" C_RESET " unevaluated integral dummy does not leak into bindings\n\n");
    } else {
        printf(C_BOLD C_RED "FAIL" C_RESET " unevaluated integral dummy does not leak into bindings %s:%d:1\n\n",
               __FILE__, __LINE__);
        TEST_FAIL();
    }

    if (deriv_text && strcmp(deriv_text, "{ exp(cosh(x)) | x = NAN }") == 0) {
        printf(C_BOLD C_GREEN "PASS" C_RESET " unevaluated integral derivative restores upper variable\n");
        printf(C_BOLD "  expr   " C_RESET "%s\n\n", deriv_text);
    } else {
        printf(C_BOLD C_RED "FAIL" C_RESET " unevaluated integral derivative restores upper variable %s:%d:1\n",
               __FILE__, __LINE__);
        printf(C_BOLD "  got    " C_RESET "%s\n\n", deriv_text ? deriv_text : "(null)");
        TEST_FAIL();
    }

    if (func_text && strstr(func_text, "@S^x exp(cosh(t)) dt") != NULL) {
        printf(C_BOLD C_GREEN "PASS" C_RESET " unevaluated integral function form uses @S notation\n");
        printf(C_BOLD "  expr   " C_RESET "%s\n\n", func_text);
    } else {
        printf(C_BOLD C_RED "FAIL" C_RESET " unevaluated integral function form uses @S notation %s:%d:1\n", __FILE__,
               __LINE__);
        printf(C_BOLD "  got    " C_RESET "%s\n\n", func_text ? func_text : "(null)");
        TEST_FAIL();
    }

    free(func_text);
    free(deriv_text);
    free(text);
    expr_free(deriv);
    expr_bindings_free(bindings);
    expr_free(expr);

    expr = expr_from_string("{ integral(x, exp(cosh(t)), t) }", &bindings);
    text = expr ? expr_to_string(expr, style_EXPRESSION) : NULL;
    if (text && strcmp(text, "{ ∫^x exp(cosh(t))·dt | x = NAN }") == 0) {
        printf(C_BOLD C_GREEN "PASS" C_RESET " unevaluated integral ASCII function form parses\n");
        printf(C_BOLD "  expr   " C_RESET "%s\n\n", text);
    } else {
        printf(C_BOLD C_RED "FAIL" C_RESET " unevaluated integral ASCII function form parses %s:%d:1\n", __FILE__,
               __LINE__);
        printf(C_BOLD "  got    " C_RESET "%s\n\n", text ? text : "(null)");
        TEST_FAIL();
    }
    free(text);
    expr_bindings_free(bindings);
    expr_free(expr);
    bindings = NULL;
    expr = NULL;

    expr = expr_from_string("{ @S^x exp(cosh(t)) dt }", &bindings);
    text = expr ? expr_to_string(expr, style_EXPRESSION) : NULL;
    if (text && strcmp(text, "{ ∫^x exp(cosh(t))·dt | x = NAN }") == 0) {
        printf(C_BOLD C_GREEN "PASS" C_RESET " unevaluated integral @S form parses\n");
        printf(C_BOLD "  expr   " C_RESET "%s\n\n", text);
    } else {
        printf(C_BOLD C_RED "FAIL" C_RESET " unevaluated integral @S form parses %s:%d:1\n", __FILE__, __LINE__);
        printf(C_BOLD "  got    " C_RESET "%s\n\n", text ? text : "(null)");
        TEST_FAIL();
    }
    free(text);
    expr_bindings_free(bindings);
    expr_free(expr);
    bindings = NULL;
    expr = NULL;

    expr = expr_from_string("{ @S^3 sin(t) dt }", &bindings);
    text = expr ? expr_to_string(expr, style_EXPRESSION) : NULL;
    func_text = expr ? expr_to_string(expr, style_FUNCTION) : NULL;
    if (text && strcmp(text, "-cos(3)") == 0) {
        printf(C_BOLD C_GREEN "PASS" C_RESET " @S literal upper bound returns a completed antiderivative\n");
        printf(C_BOLD "  expr   " C_RESET "%s\n\n", text);
    } else {
        printf(C_BOLD C_RED "FAIL" C_RESET " @S literal upper bound returns a completed antiderivative %s:%d:1\n",
               __FILE__, __LINE__);
        printf(C_BOLD "  got    " C_RESET "%s\n\n", text ? text : "(null)");
        TEST_FAIL();
    }
    if (func_text && strstr(func_text, "return -(cos(3));") != NULL) {
        printf(C_BOLD C_GREEN "PASS" C_RESET " @S literal upper bound function is completed by MARS\n");
        printf(C_BOLD "  expr   " C_RESET "%s\n\n", func_text);
    } else {
        printf(C_BOLD C_RED "FAIL" C_RESET " @S literal upper bound function is completed by MARS %s:%d:1\n", __FILE__,
               __LINE__);
        printf(C_BOLD "  got    " C_RESET "%s\n\n", func_text ? func_text : "(null)");
        TEST_FAIL();
    }
    free(func_text);
    free(text);
    expr_bindings_free(bindings);
    expr_free(expr);
    bindings = NULL;
    expr = NULL;

    expr = expr_from_string("{ @S^3_1 1/t dt }", &bindings);
    text = expr ? expr_to_string(expr, style_EXPRESSION) : NULL;
    func_text = expr ? expr_to_string(expr, style_FUNCTION) : NULL;
    if (text && strcmp(text, "ln(3)") == 0) {
        printf(C_BOLD C_GREEN "PASS" C_RESET " @S with explicit bounds returns a completed value expression\n");
        printf(C_BOLD "  expr   " C_RESET "%s\n\n", text);
    } else {
        printf(C_BOLD C_RED "FAIL" C_RESET " @S with explicit bounds returns a completed value expression %s:%d:1\n",
               __FILE__, __LINE__);
        printf(C_BOLD "  got    " C_RESET "%s\n\n", text ? text : "(null)");
        TEST_FAIL();
    }
    if (func_text && strstr(func_text, "return ln(3);") != NULL) {
        printf(C_BOLD C_GREEN "PASS" C_RESET " @S bounded function is completed by MARS\n");
        printf(C_BOLD "  expr   " C_RESET "%s\n\n", func_text);
    } else {
        printf(C_BOLD C_RED "FAIL" C_RESET " @S bounded function is completed by MARS %s:%d:1\n", __FILE__, __LINE__);
        printf(C_BOLD "  got    " C_RESET "%s\n\n", func_text ? func_text : "(null)");
        TEST_FAIL();
    }
    free(func_text);
    free(text);
    expr_bindings_free(bindings);
    expr_free(expr);
    bindings = NULL;
    expr = NULL;

    expr = expr_from_string("{ ∫_1^3 1/t·dt }", &bindings);
    text = expr ? expr_to_string(expr, style_EXPRESSION) : NULL;
    if (text && strcmp(text, "∫^3_1 1/t·dt") == 0) {
        printf(C_BOLD C_GREEN "PASS" C_RESET " unevaluated integral standard lower-then-upper form parses\n");
        printf(C_BOLD "  expr   " C_RESET "%s\n\n", text);
    } else {
        printf(C_BOLD C_RED "FAIL" C_RESET " unevaluated integral standard lower-then-upper form parses %s:%d:1\n",
               __FILE__, __LINE__);
        printf(C_BOLD "  got    " C_RESET "%s\n\n", text ? text : "(null)");
        TEST_FAIL();
    }
    free(text);
    expr_bindings_free(bindings);
    expr_free(expr);
    bindings = NULL;
    expr = NULL;

    expr = expr_from_string("{ @S_1^3 1/t.dt }", &bindings);
    text = expr ? expr_to_string(expr, style_EXPRESSION) : NULL;
    if (text && strcmp(text, "ln(3)") == 0) {
        printf(C_BOLD C_GREEN "PASS" C_RESET " lower-then-upper @S form completes with full-stop differential\n");
        printf(C_BOLD "  expr   " C_RESET "%s\n\n", text);
    } else {
        printf(C_BOLD C_RED "FAIL" C_RESET " lower-then-upper @S form completes with full-stop differential %s:%d:1\n",
               __FILE__, __LINE__);
        printf(C_BOLD "  got    " C_RESET "%s\n\n", text ? text : "(null)");
        TEST_FAIL();
    }
    free(text);
    expr_bindings_free(bindings);
    expr_free(expr);
    bindings = NULL;
    expr = NULL;

    check_parse_simplified_expr("unevaluated integral explicit bounds simplify structurally", "{ ∫_1^3 1/t·dt }",
                                "∫^3_1 1/t·dt", __LINE__);

    check_parse_val("unevaluated integral explicit bounds evaluate numerically", "{ @S^3_1 1/t dt }", log(3.0),
                    __LINE__);

    expr = expr_from_string("{ integral(1, 3, 1/t, t) }", &bindings);
    text = expr ? expr_to_string(expr, style_EXPRESSION) : NULL;
    if (text && strcmp(text, "∫^3_1 1/t·dt") == 0) {
        printf(C_BOLD C_GREEN "PASS" C_RESET " unevaluated integral four-argument ASCII function form parses\n");
        printf(C_BOLD "  expr   " C_RESET "%s\n\n", text);
    } else {
        printf(C_BOLD C_RED "FAIL" C_RESET " unevaluated integral four-argument ASCII function form parses %s:%d:1\n",
               __FILE__, __LINE__);
        printf(C_BOLD "  got    " C_RESET "%s\n\n", text ? text : "(null)");
        TEST_FAIL();
    }
    free(text);
    expr_bindings_free(bindings);
    expr_free(expr);
    bindings = NULL;
    expr = NULL;

    expr = expr_from_string("{ @S^x exp(cosh(t))dt }", &bindings);
    text = expr ? expr_to_string(expr, style_EXPRESSION) : NULL;
    if (text && strcmp(text, "{ ∫^x exp(cosh(t))·dt | x = NAN }") == 0) {
        printf(C_BOLD C_GREEN "PASS" C_RESET " unevaluated integral @S form parses without space before dt\n");
        printf(C_BOLD "  expr   " C_RESET "%s\n\n", text);
    } else {
        printf(C_BOLD C_RED "FAIL" C_RESET " unevaluated integral @S form parses without space before dt %s:%d:1\n",
               __FILE__, __LINE__);
        printf(C_BOLD "  got    " C_RESET "%s\n\n", text ? text : "(null)");
        TEST_FAIL();
    }
    free(text);
    expr_bindings_free(bindings);
    expr_free(expr);
    bindings = NULL;
    expr = NULL;

    expr = expr_from_string("{ (½c·(ax + b)²/a + ∫^x a·exp(cosh(b + at)) dt)/a + dx"
                            " | x = NAN, a = NAN, b = NAN; c = NAN, d = NAN }",
                            NULL);
    text = expr ? expr_to_string(expr, style_EXPRESSION) : NULL;
    if (text && strstr(text, "∫^x a·exp(cosh(at + b))·dt")) {
        printf(C_BOLD C_GREEN "PASS" C_RESET " partial antiderivative with unevaluated integral parses\n");
        printf(C_BOLD "  expr   " C_RESET "%s\n\n", text);
    } else {
        printf(C_BOLD C_RED "FAIL" C_RESET " partial antiderivative with unevaluated integral parses %s:%d:1\n",
               __FILE__, __LINE__);
        printf(C_BOLD "  got    " C_RESET "%s\n\n", text ? text : "(null)");
        TEST_FAIL();
    }
    free(text);
    expr_free(expr);

    expr = expr_from_string("{ (½c·(ax + b)²/a + @S^x a·exp(cosh(b + at))dt)/a + dx"
                            " | x = NAN, a = NAN, b = NAN; c = NAN, d = NAN }",
                            NULL);
    text = expr ? expr_to_string(expr, style_EXPRESSION) : NULL;
    if (text && strstr(text, "∫^x a·exp(cosh(at + b))·dt")) {
        printf(C_BOLD C_GREEN "PASS" C_RESET " partial antiderivative with compact differential parses\n");
        printf(C_BOLD "  expr   " C_RESET "%s\n\n", text);
    } else {
        printf(C_BOLD C_RED "FAIL" C_RESET " partial antiderivative with compact differential parses %s:%d:1\n",
               __FILE__, __LINE__);
        printf(C_BOLD "  got    " C_RESET "%s\n\n", text ? text : "(null)");
        TEST_FAIL();
    }
    free(text);
    expr_free(expr);

    expr = expr_from_string("{ @S^x exp(cosh(t))·dt }", &bindings);
    text = expr ? expr_to_string(expr, style_EXPRESSION) : NULL;
    if (text && strcmp(text, "{ ∫^x exp(cosh(t))·dt | x = NAN }") == 0) {
        printf(C_BOLD C_GREEN "PASS" C_RESET " unevaluated integral @S form parses with dotted differential\n");
        printf(C_BOLD "  expr   " C_RESET "%s\n\n", text);
    } else {
        printf(C_BOLD C_RED "FAIL" C_RESET " unevaluated integral @S form parses with dotted differential %s:%d:1\n",
               __FILE__, __LINE__);
        printf(C_BOLD "  got    " C_RESET "%s\n\n", text ? text : "(null)");
        TEST_FAIL();
    }
    free(text);
    expr_bindings_free(bindings);
    expr_free(expr);

    bindings = NULL;
    expr = expr_from_string("{ @S^x exp(u)du + x }", &bindings);
    text = expr ? expr_to_string(expr, style_EXPRESSION) : NULL;
    if (text && strcmp(text, "{ exp(x) + x | x = NAN }") == 0 && bindings && expr_bindings_count(bindings) == 1u &&
        expr_bindings_get(bindings, "x") && !expr_bindings_get(bindings, "d") && !expr_bindings_get(bindings, "u")) {
        printf(C_BOLD C_GREEN "PASS" C_RESET " completed @S does not expose its local dummy\n");
        printf(C_BOLD "  expr   " C_RESET "%s\n\n", text);
    } else {
        printf(C_BOLD C_RED "FAIL" C_RESET " completed @S does not expose its local dummy %s:%d:1\n", __FILE__,
               __LINE__);
        printf(C_BOLD "  got    " C_RESET "%s\n\n", text ? text : "(null)");
        TEST_FAIL();
    }
    free(text);
    expr_bindings_free(bindings);
    expr_free(expr);

    bindings = NULL;
    expr = expr_from_string("{ @S^x u du + @S^x c dc }", &bindings);
    text = expr ? expr_to_string(expr, style_EXPRESSION) : NULL;
    if (text && strstr(text, "½x² + ½x²") && bindings && expr_bindings_count(bindings) == 1u &&
        expr_bindings_get(bindings, "x") && !expr_bindings_get(bindings, "d") && !expr_bindings_get(bindings, "u") &&
        !expr_bindings_get(bindings, "c")) {
        printf(C_BOLD C_GREEN "PASS" C_RESET " completed @S operations keep their dummy variables local\n");
        printf(C_BOLD "  expr   " C_RESET "%s\n\n", text);
    } else {
        printf(C_BOLD C_RED "FAIL" C_RESET " completed @S operations keep their dummy variables local %s:%d:1\n",
               __FILE__, __LINE__);
        printf(C_BOLD "  got    " C_RESET "%s\n\n", text ? text : "(null)");
        TEST_FAIL();
    }
    free(text);
    expr_bindings_free(bindings);
    expr_free(expr);

    bindings = NULL;
    expr = expr_from_string("{ @S^z exp(sin(x))*(cos(x)^2 - sin(x)) + C_0 dx }", &bindings);
    text = expr ? expr_to_string(expr, style_EXPRESSION) : NULL;
    if (text && strstr(text, "z = NAN") && strstr(text, "C₀ = NAN") && !expr_bindings_get(bindings, "x")) {
        printf(C_BOLD C_GREEN "PASS" C_RESET " completed @S hides a dummy used only in its integrand\n");
        printf(C_BOLD "  expr   " C_RESET "%s\n\n", text);
    } else {
        printf(C_BOLD C_RED "FAIL" C_RESET " completed @S hides a dummy used only in its integrand %s:%d:1\n", __FILE__,
               __LINE__);
        printf(C_BOLD "  got    " C_RESET "%s\n\n", text ? text : "(null)");
        TEST_FAIL();
    }
    free(text);
    expr_bindings_free(bindings);
    expr_free(expr);

    bindings = NULL;
    expr = expr_from_string("{ exp(x)*@S^z exp(sin(x))*(cos(x)^2 - sin(x)) + C_0 dx }", &bindings);
    text = expr ? expr_to_string(expr, style_EXPRESSION) : NULL;
    if (text && strstr(text, "x = NAN") && expr_bindings_get(bindings, "x")) {
        printf(C_BOLD C_GREEN "PASS" C_RESET
                              " completed @S retains a free dummy-name occurrence outside the integral\n");
        printf(C_BOLD "  expr   " C_RESET "%s\n\n", text);
    } else {
        printf(C_BOLD C_RED "FAIL" C_RESET
                            " completed @S retains a free dummy-name occurrence outside the integral %s:%d:1\n",
               __FILE__, __LINE__);
        printf(C_BOLD "  got    " C_RESET "%s\n\n", text ? text : "(null)");
        TEST_FAIL();
    }
    free(text);
    expr_bindings_free(bindings);
    expr_free(expr);

    bindings = NULL;
    expr = expr_from_string("{ @S^x exp(c)dc + c }", &bindings);
    text = expr ? expr_to_string(expr, style_EXPRESSION) : NULL;
    if (text && strcmp(text, "{ exp(x) + c | x = NAN; c = NAN }") == 0 && bindings &&
        expr_bindings_count(bindings) == 2u && expr_bindings_get(bindings, "x") && expr_bindings_get(bindings, "c") &&
        expr_bindings_is_constant_at(bindings, 1u) && !expr_bindings_get(bindings, "d")) {
        printf(C_BOLD C_GREEN "PASS" C_RESET " integral dummy scope ends at dc before a free c\n");
        printf(C_BOLD "  expr   " C_RESET "%s\n\n", text);
    } else {
        printf(C_BOLD C_RED "FAIL" C_RESET " integral dummy scope ends at dc before a free c %s:%d:1\n", __FILE__,
               __LINE__);
        printf(C_BOLD "  got    " C_RESET "%s\n\n", text ? text : "(null)");
        TEST_FAIL();
    }
    free(text);
    expr_bindings_free(bindings);
    expr_free(expr);

    bindings = NULL;
    expr = expr_from_string("{ @S^x exp(i)di }", &bindings);
    text = expr ? expr_to_string(expr, style_EXPRESSION) : NULL;
    if (!expr && !text && !bindings) {
        printf(C_BOLD C_GREEN "PASS" C_RESET " di remains d times imaginary i, not an @S differential\n\n");
    } else {
        printf(C_BOLD C_RED "FAIL" C_RESET " di remains d times imaginary i, not an @S differential %s:%d:1\n",
               __FILE__, __LINE__);
        printf(C_BOLD "  got    " C_RESET "%s\n\n", text ? text : "(null)");
        TEST_FAIL();
    }
    free(text);
    expr_bindings_free(bindings);
    expr_free(expr);

    bindings = NULL;
    expr = expr_from_string("{ @S^x exp(i)Di }", &bindings);
    text = expr ? expr_to_string(expr, style_EXPRESSION) : NULL;
    if (!expr && !text && !bindings) {
        printf(C_BOLD C_GREEN "PASS" C_RESET " Di remains D times imaginary i, not an @S differential\n\n");
    } else {
        printf(C_BOLD C_RED "FAIL" C_RESET " Di remains D times imaginary i, not an @S differential %s:%d:1\n",
               __FILE__, __LINE__);
        printf(C_BOLD "  got    " C_RESET "%s\n\n", text ? text : "(null)");
        TEST_FAIL();
    }
    free(text);
    expr_bindings_free(bindings);
    expr_free(expr);

    bindings = NULL;
    expr = expr_from_string("{ @S^x exp(i)di + i }", &bindings);
    text = expr ? expr_to_string(expr, style_EXPRESSION) : NULL;
    if (!expr && !text && !bindings) {
        printf(C_BOLD C_GREEN "PASS" C_RESET " di cannot terminate @S before a trailing imaginary i\n\n");
    } else {
        printf(C_BOLD C_RED "FAIL" C_RESET " di cannot terminate @S before a trailing imaginary i %s:%d:1\n", __FILE__,
               __LINE__);
        printf(C_BOLD "  got    " C_RESET "%s\n\n", text ? text : "(null)");
        TEST_FAIL();
    }
    free(text);
    expr_bindings_free(bindings);
    expr_free(expr);

    bindings = NULL;
    expr = expr_from_string("{ @S x dx }", &bindings);
    {
        char *display_text = expr ? expr_to_string(expr, style_EXPRESSION) : NULL;

        if (display_text && strstr(display_text, "x²") && strstr(display_text, "+ C") && !strstr(display_text, "C₀") &&
            strstr(display_text, "x = NAN") && strstr(display_text, "C = NAN") && expr_bindings_get(bindings, "C") &&
            expr_bindings_has_symbolic_integral(bindings)) {
            printf(C_BOLD C_GREEN "PASS" C_RESET " @S without an upper substitution returns an indefinite family\n");
            printf(C_BOLD "  expr   " C_RESET "%s\n\n", display_text);
        } else {
            printf(C_BOLD C_RED "FAIL" C_RESET
                                " @S without an upper substitution returns an indefinite family %s:%d:1\n",
                   __FILE__, __LINE__);
            printf(C_BOLD "  got    " C_RESET "%s\n\n", display_text ? display_text : "(null)");
            TEST_FAIL();
        }
        free(display_text);
    }
    expr_bindings_free(bindings);
    expr_free(expr);

    bindings = NULL;
    expr = expr_from_string("{ @S(x^2)dx }", &bindings);
    text = expr ? expr_to_string(expr, style_EXPRESSION) : NULL;
    if (text && strstr(text, "⅓x³") && strstr(text, "+ C") && !strstr(text, "C₀") && strstr(text, "x = NAN") &&
        strstr(text, "C = NAN") && expr_bindings_get(bindings, "C") && expr_bindings_has_symbolic_integral(bindings)) {
        printf(C_BOLD C_GREEN "PASS" C_RESET " parenthesized @S without whitespace returns an indefinite family\n");
        printf(C_BOLD "  expr   " C_RESET "%s\n\n", text);
    } else {
        printf(C_BOLD C_RED "FAIL" C_RESET
                            " parenthesized @S without whitespace returns an indefinite family %s:%d:1\n",
               __FILE__, __LINE__);
        printf(C_BOLD "  got    " C_RESET "%s\n\n", text ? text : "(null)");
        TEST_FAIL();
    }
    free(text);
    expr_bindings_free(bindings);
    expr_free(expr);

    bindings = NULL;
    expr = expr_from_string("{ @S x^2 dx }", &bindings);
    text = expr ? expr_to_string(expr, style_EXPRESSION) : NULL;
    if (text && expr_bindings_get(bindings, "x") && strstr(text, "x = NAN")) {
        printf(C_BOLD C_GREEN "PASS" C_RESET " indefinite @S exposes its result variable as a binding\n");
        printf(C_BOLD "  expr   " C_RESET "%s\n\n", text);
    } else {
        printf(C_BOLD C_RED "FAIL" C_RESET " indefinite @S exposes its result variable as a binding %s:%d:1\n",
               __FILE__, __LINE__);
        printf(C_BOLD "  got    " C_RESET "%s\n\n", text ? text : "(null)");
        TEST_FAIL();
    }
    free(text);
    expr_bindings_free(bindings);
    expr_free(expr);

    bindings = NULL;
    expr = expr_from_string("{ @S_0^1 x^2 dx }", &bindings);
    text = expr ? expr_to_string(expr, style_EXPRESSION) : NULL;
    if (text && (!bindings || !expr_bindings_get(bindings, "x")) && !strstr(text, "x = NAN")) {
        printf(C_BOLD C_GREEN "PASS" C_RESET " definite @S does not expose its bound variable\n");
        printf(C_BOLD "  expr   " C_RESET "%s\n\n", text);
    } else {
        printf(C_BOLD C_RED "FAIL" C_RESET " definite @S does not expose its bound variable %s:%d:1\n", __FILE__,
               __LINE__);
        printf(C_BOLD "  got    " C_RESET "%s\n\n", text ? text : "(null)");
        TEST_FAIL();
    }
    free(text);
    expr_bindings_free(bindings);
    expr_free(expr);

    bindings = NULL;
    expr = expr_from_string("{ @S^x x dx }", &bindings);
    {
        char *display_text = expr ? expr_to_string(expr, style_EXPRESSION) : NULL;

        if (display_text && strstr(display_text, "½x²") && !strstr(display_text, "C₀") &&
            expr_bindings_has_symbolic_integral(bindings)) {
            printf(C_BOLD C_GREEN "PASS" C_RESET
                                  " explicit @S^x returns one antiderivative without a family constant\n");
            printf(C_BOLD "  expr   " C_RESET "%s\n\n", display_text);
        } else {
            printf(C_BOLD C_RED "FAIL" C_RESET
                                " explicit @S^x returns one antiderivative without a family constant %s:%d:1\n",
                   __FILE__, __LINE__);
            printf(C_BOLD "  got    " C_RESET "%s\n\n", display_text ? display_text : "(null)");
            TEST_FAIL();
        }
        free(display_text);
    }
    expr_bindings_free(bindings);
    expr_free(expr);

    bindings = NULL;
    expr = expr_from_string("{ @S^u x dx }", &bindings);
    {
        char *display_text = expr ? expr_to_string(expr, style_EXPRESSION) : NULL;

        if (display_text && strstr(display_text, "½u²") && !strstr(display_text, "C₀") &&
            expr_bindings_has_symbolic_integral(bindings)) {
            printf(C_BOLD C_GREEN "PASS" C_RESET " @S^u returns an antiderivative substituted with u\n");
            printf(C_BOLD "  expr   " C_RESET "%s\n\n", display_text);
        } else {
            printf(C_BOLD C_RED "FAIL" C_RESET " @S^u returns an antiderivative substituted with u %s:%d:1\n", __FILE__,
                   __LINE__);
            printf(C_BOLD "  got    " C_RESET "%s\n\n", display_text ? display_text : "(null)");
            TEST_FAIL();
        }
        free(display_text);
    }
    expr_bindings_free(bindings);
    expr_free(expr);

    bindings = NULL;
    expr = expr_from_string("{ @S (x + C_0) dx }", &bindings);
    {
        char *display_text = expr ? expr_to_string(expr, style_EXPRESSION) : NULL;

        if (display_text && strstr(display_text, "C₀x") && strstr(display_text, "C₁") &&
            strstr(display_text, "C₁ = NAN") && expr_bindings_get(bindings, "C_0") &&
            expr_bindings_get(bindings, "C_1")) {
            printf(C_BOLD C_GREEN "PASS" C_RESET " indefinite integral chooses the next unused family constant\n");
            printf(C_BOLD "  expr   " C_RESET "%s\n\n", display_text);
        } else {
            printf(C_BOLD C_RED "FAIL" C_RESET " indefinite integral chooses the next unused family constant %s:%d:1\n",
                   __FILE__, __LINE__);
            printf(C_BOLD "  got    " C_RESET "%s\n\n", display_text ? display_text : "(null)");
            TEST_FAIL();
        }
        free(display_text);
    }
    expr_bindings_free(bindings);
    expr_free(expr);

    bindings = NULL;
    expr = expr_from_string("{ @S^z exp(-x^2)dx }", &bindings);
    text = expr ? expr_to_string(expr, style_EXPRESSION) : NULL;
    func_text = expr ? expr_to_string(expr, style_FUNCTION) : NULL;
    if (text && strstr(text, "√(π)·erf(z)") && strstr(text, "z = NAN")) {
        printf(C_BOLD C_GREEN "PASS" C_RESET " @S substitutes its explicit upper variable in the antiderivative\n");
        printf(C_BOLD "  expr   " C_RESET "%s\n\n", text);
    } else {
        printf(C_BOLD C_RED "FAIL" C_RESET
                            " @S substitutes its explicit upper variable in the antiderivative %s:%d:1\n",
               __FILE__, __LINE__);
        printf(C_BOLD "  got    " C_RESET "%s\n\n", text ? text : "(null)");
        TEST_FAIL();
    }
    if (func_text && strstr(func_text, "erf(z)") != NULL && strstr(func_text, "integral(") == NULL) {
        printf(C_BOLD C_GREEN "PASS" C_RESET " completed @S function contains no integral node\n");
        printf(C_BOLD "  expr   " C_RESET "%s\n\n", func_text);
    } else {
        printf(C_BOLD C_RED "FAIL" C_RESET " completed @S function contains no integral node %s:%d:1\n", __FILE__,
               __LINE__);
        printf(C_BOLD "  got    " C_RESET "%s\n\n", func_text ? func_text : "(null)");
        TEST_FAIL();
    }
    free(func_text);
    free(text);
    expr_bindings_free(bindings);
    expr_free(expr);

    bindings = NULL;
    expr = expr_from_string("{ @S_0^1 exp(-x^2)dx }", &bindings);
    text = expr ? expr_to_string(expr, style_EXPRESSION) : NULL;
    if (text && strstr(text, "√(π)") && strstr(text, "erf(1) - erf(0)") && strstr(text, "∫") == NULL) {
        printf(C_BOLD C_GREEN "PASS" C_RESET " bounded @S returns a completed expression\n");
        printf(C_BOLD "  expr   " C_RESET "%s\n\n", text);
    } else {
        printf(C_BOLD C_RED "FAIL" C_RESET " bounded @S returns a completed expression %s:%d:1\n", __FILE__, __LINE__);
        printf(C_BOLD "  got    " C_RESET "%s\n\n", text ? text : "(null)");
        TEST_FAIL();
    }
    free(text);
    expr_bindings_free(bindings);
    expr_free(expr);

    bindings = NULL;
    expr = expr_from_string("{ @S^xexp(cosh(t))*dt }", &bindings);
    text = expr ? expr_to_string(expr, style_EXPRESSION) : NULL;
    if (text && strcmp(text, "{ ∫^x exp(cosh(t))·dt | x = NAN }") == 0) {
        printf(C_BOLD C_GREEN "PASS" C_RESET " unevaluated integral @S form parses with starred differential\n");
        printf(C_BOLD "  expr   " C_RESET "%s\n\n", text);
    } else {
        printf(C_BOLD C_RED "FAIL" C_RESET " unevaluated integral @S form parses with starred differential %s:%d:1\n",
               __FILE__, __LINE__);
        printf(C_BOLD "  got    " C_RESET "%s\n\n", text ? text : "(null)");
        TEST_FAIL();
    }
    free(text);
    expr_bindings_free(bindings);
    expr_free(expr);

    bindings = NULL;
    expr = expr_from_string("{ @S^xexp(cosh(t)).dt }", &bindings);
    text = expr ? expr_to_string(expr, style_EXPRESSION) : NULL;
    if (text && strcmp(text, "{ ∫^x exp(cosh(t))·dt | x = NAN }") == 0) {
        printf(C_BOLD C_GREEN "PASS" C_RESET " unevaluated integral @S form parses with full-stop differential\n");
        printf(C_BOLD "  expr   " C_RESET "%s\n\n", text);
    } else {
        printf(C_BOLD C_RED "FAIL" C_RESET " unevaluated integral @S form parses with full-stop differential %s:%d:1\n",
               __FILE__, __LINE__);
        printf(C_BOLD "  got    " C_RESET "%s\n\n", text ? text : "(null)");
        TEST_FAIL();
    }
    free(text);
    expr_bindings_free(bindings);
    expr_free(expr);

    bindings = NULL;
    expr = expr_from_string("{ ∫^xexp(cosh(t))*dt }", &bindings);
    text = expr ? expr_to_string(expr, style_EXPRESSION) : NULL;
    if (text && strcmp(text, "{ ∫^x exp(cosh(t))·dt | x = NAN }") == 0) {
        printf(C_BOLD C_GREEN "PASS" C_RESET " unevaluated integral Unicode form parses with starred differential\n");
        printf(C_BOLD "  expr   " C_RESET "%s\n\n", text);
    } else {
        printf(C_BOLD C_RED "FAIL" C_RESET
                            " unevaluated integral Unicode form parses with starred differential %s:%d:1\n",
               __FILE__, __LINE__);
        printf(C_BOLD "  got    " C_RESET "%s\n\n", text ? text : "(null)");
        TEST_FAIL();
    }
    free(text);
    expr_bindings_free(bindings);
    expr_free(expr);

    bindings = NULL;
    expr = expr_from_string("{ @S^@lambdasin(@alpha)d@alpha }", &bindings);
    text = expr ? expr_to_string(expr, style_EXPRESSION) : NULL;
    if (text && strcmp(text, "{ -cos(λ) | λ = NAN }") == 0) {
        printf(C_BOLD C_GREEN "PASS" C_RESET " @S completes with Greek aliases\n");
        printf(C_BOLD "  expr   " C_RESET "%s\n\n", text);
    } else {
        printf(C_BOLD C_RED "FAIL" C_RESET " @S completes with Greek aliases %s:%d:1\n", __FILE__, __LINE__);
        printf(C_BOLD "  got    " C_RESET "%s\n\n", text ? text : "(null)");
        TEST_FAIL();
    }
    free(text);
    expr_bindings_free(bindings);
    expr_free(expr);
}

/* ---- Round-trips: build → string → parse → compare value ---- */

static void test_from_string_round_trips(void)
{
    /* Unnamed-variable expressions (auto-subscripted names x₀, x₁, …) */
    check_roundtrip("u01: x0^2", make_expr_u01(), __LINE__);
    check_roundtrip("u02: x0^3", make_expr_u02(), __LINE__);
    check_roundtrip("u03: x0^2 * y0^3 * x0", make_expr_u03(), __LINE__);
    check_roundtrip("u04: x0^2 + x0^2", make_expr_u04(), __LINE__);
    check_roundtrip("u05: sin(x0) * cos(x0)", make_expr_u05(), __LINE__);
    check_roundtrip("u06: exp(sin(x0)) * exp(cos(x0))", make_expr_u06(), __LINE__);
    /* Named-constant expressions */
    check_roundtrip("c01: c0 * x0^2", make_expr_c01(), __LINE__);
    check_roundtrip("c02: c0 * sin(x0)", make_expr_c02(), __LINE__);
    check_roundtrip("c03: x0 + x1 + c0", make_expr_c03(), __LINE__);
    check_roundtrip("c04: c0*x0 + c1", make_expr_c04(), __LINE__);
    /* Bracketed (multi-character) name expressions */
    check_roundtrip("l01: [radius]^2", make_expr_l01(), __LINE__);
    check_roundtrip("l02: [base] * [height]", make_expr_l02(), __LINE__);
    check_roundtrip("l03: [pi] * [radius]^2", make_expr_l03(), __LINE__);
    check_roundtrip("l04: pi * [radius]^2", make_expr_l04(), __LINE__);
    check_roundtrip("l05: sin([theta]) * cos([theta])", make_expr_l05(), __LINE__);
    check_roundtrip("l06: [pi] * [tau] * x", make_expr_l06(), __LINE__);
    check_roundtrip("l07: [my var]^2", make_expr_l07(), __LINE__);
    check_roundtrip("l08: [2pi] * x", make_expr_l08(), __LINE__);
    check_roundtrip("l09: [x']^2", make_expr_l09(), __LINE__);
}

void test_expr_t_from_string(void)
{
    TEST_RUN_SUBTEST(test_from_string_function_hash, NULL);
    TEST_RUN_SUBTEST(test_from_string_conjugation, NULL);
    TEST_RUN_SUBTEST(test_from_string_pure_const, NULL);
    TEST_RUN_SUBTEST(test_from_string_arithmetic, NULL);
    TEST_RUN_SUBTEST(test_from_string_functions, NULL);
    TEST_RUN_SUBTEST(test_from_text_api, NULL);
    TEST_RUN_SUBTEST(test_from_string_special_functions, NULL);
    TEST_RUN_SUBTEST(test_from_string_exact_value_functions, NULL);
    TEST_RUN_SUBTEST(test_from_string_named_consts, NULL);
    TEST_RUN_SUBTEST(test_from_string_bracketed_names, NULL);
    TEST_RUN_SUBTEST(test_from_string_number_literals, NULL);
    TEST_RUN_SUBTEST(test_from_string_name_normalisation, NULL);
    TEST_RUN_SUBTEST(test_from_string_implicit_symbolic_bindings, NULL);
    TEST_RUN_SUBTEST(test_from_string_ascii_alternatives, NULL);
    TEST_RUN_SUBTEST(test_from_string_errors, NULL);
    TEST_RUN_SUBTEST(test_from_expression_string_api, NULL);
    TEST_RUN_SUBTEST(test_from_string_bindings_api, NULL);
    TEST_RUN_SUBTEST(test_from_string_bindings_with_implicit_builtin_constant, NULL);
    TEST_RUN_SUBTEST(test_from_string_bindings_with_constant_expression_value, NULL);
    TEST_RUN_SUBTEST(test_from_string_bindings_skip_function_name_letters, NULL);
    TEST_RUN_SUBTEST(test_binding_edit_is_owned_by_expression_library, NULL);
    TEST_RUN_SUBTEST(test_binding_edit_preserves_symbolic_value, NULL);
    TEST_RUN_SUBTEST(test_from_string_unevaluated_integral, NULL);
    TEST_RUN_SUBTEST(test_from_string_round_trips, NULL);
    TEST_RUN_SUBTEST(test_from_string_deriv, NULL);
}
