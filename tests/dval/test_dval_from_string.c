#include "test_dval.h"

static void test_from_string_pure_const(void)
{
    /* Single Unicode letter name */
    check_parse_val("pure const π",
        "{ \xcf\x80 = 3.141592653589793 }",
        3.141592653589793, __LINE__);
    /* Bracketed multi-character name */
    check_parse_val("pure const [e]",
        "{ [e] = 2.718281828459045 }",
        2.718281828459045, __LINE__);
    /* Negative value */
    check_parse_val("pure const negative",
        "{ [neg] = -1.5 }",
        -1.5, __LINE__);
    /* Zero */
    check_parse_val("pure const zero",
        "{ z = 0 }",
        0.0, __LINE__);
    /* Name with spaces (bracketed) */
    check_parse_val("pure const [my const]",
        "{ [my const] = 42 }",
        42.0, __LINE__);
}

/* ---- Arithmetic operators ---- */

static void test_from_string_arithmetic(void)
{
    /* Addition */
    check_parse_val("x\xe2\x82\x80 + x\xe2\x82\x81 = 7",
        "{ x\xe2\x82\x80 + x\xe2\x82\x81 | x\xe2\x82\x80 = 3, x\xe2\x82\x81 = 4 }",
        7.0, __LINE__);
    /* Subtraction */
    check_parse_val("x\xe2\x82\x80 - x\xe2\x82\x81 = 7",
        "{ x\xe2\x82\x80 - x\xe2\x82\x81 | x\xe2\x82\x80 = 10, x\xe2\x82\x81 = 3 }",
        7.0, __LINE__);
    /* Unary negation */
    check_parse_val("-x = -3",
        "{ -x | x = 3 }",
        -3.0, __LINE__);
    /* Implicit multiplication (juxtaposition) */
    check_parse_val("x\xe2\x82\x80x\xe2\x82\x81 implicit = 12",
        "{ x\xe2\x82\x80x\xe2\x82\x81 | x\xe2\x82\x80 = 3, x\xe2\x82\x81 = 4 }",
        12.0, __LINE__);
    /* Explicit middle-dot multiplication */
    check_parse_val("x\xe2\x82\x80\xc2\xb7x\xe2\x82\x81 middle-dot = 12",
        "{ x\xe2\x82\x80\xc2\xb7x\xe2\x82\x81 | x\xe2\x82\x80 = 3, x\xe2\x82\x81 = 4 }",
        12.0, __LINE__);
    /* Superscript ² */
    check_parse_val("x\xc2\xb2 = 9",
        "{ x\xc2\xb2 | x = 3 }",
        9.0, __LINE__);
    /* Superscript ³ */
    check_parse_val("x\xc2\xb3 = 8",
        "{ x\xc2\xb3 | x = 2 }",
        8.0, __LINE__);
    /* Caret exponent */
    check_parse_val("x^2.5 at 4 = 32",
        "{ x^2.5 | x = 4 }",
        32.0, __LINE__);
    /* Parenthesised sub-expression with superscript */
    check_parse_val("(x\xe2\x82\x80 + x\xe2\x82\x81)\xc2\xb2 = 25",
        "{ (x\xe2\x82\x80 + x\xe2\x82\x81)\xc2\xb2 | x\xe2\x82\x80 = 2, x\xe2\x82\x81 = 3 }",
        25.0, __LINE__);
    /* Chained addition */
    check_parse_val("x\xe2\x82\x80 + x\xe2\x82\x81 + x\xe2\x82\x82 = 6",
        "{ x\xe2\x82\x80 + x\xe2\x82\x81 + x\xe2\x82\x82 | x\xe2\x82\x80 = 1, x\xe2\x82\x81 = 2, x\xe2\x82\x82 = 3 }",
        6.0, __LINE__);
    /* Mixed add and implicit mul: 2x + 3y */
    check_parse_val("c\xe2\x82\x80x\xe2\x82\x80 + c\xe2\x82\x81x\xe2\x82\x81 = 13",
        "{ c\xe2\x82\x80x\xe2\x82\x80 + c\xe2\x82\x81x\xe2\x82\x81 | x\xe2\x82\x80 = 2, x\xe2\x82\x81 = 3; c\xe2\x82\x80 = 2, c\xe2\x82\x81 = 3 }",
        13.0, __LINE__);
}

/* ---- Elementary functions ---- */

static void test_from_string_functions(void)
{
    /* All 16 unary functions at values where the result is exact or 0/1 */
    check_parse_val("sin(x) at 0",       "{ sin(x) | x = 0 }",           0.0,          __LINE__);
    check_parse_val("cos(x) at 0",       "{ cos(x) | x = 0 }",           1.0,          __LINE__);
    check_parse_val("tan(x) at 0",       "{ tan(x) | x = 0 }",           0.0,          __LINE__);
    check_parse_val("sinh(x) at 0",      "{ sinh(x) | x = 0 }",          0.0,          __LINE__);
    check_parse_val("cosh(x) at 0",      "{ cosh(x) | x = 0 }",          1.0,          __LINE__);
    check_parse_val("tanh(x) at 0",      "{ tanh(x) | x = 0 }",          0.0,          __LINE__);
    check_parse_val("asin(x) at 0",      "{ asin(x) | x = 0 }",          0.0,          __LINE__);
    check_parse_val("acos(x) at 1",      "{ acos(x) | x = 1 }",          0.0,          __LINE__);
    check_parse_val("atan(x) at 0",      "{ atan(x) | x = 0 }",          0.0,          __LINE__);
    check_parse_val("asinh(x) at 0",     "{ asinh(x) | x = 0 }",         0.0,          __LINE__);
    check_parse_val("acosh(x) at 1",     "{ acosh(x) | x = 1 }",         0.0,          __LINE__);
    check_parse_val("atanh(x) at 0",     "{ atanh(x) | x = 0 }",         0.0,          __LINE__);
    check_parse_val("exp(x) at 0",       "{ exp(x) | x = 0 }",           1.0,          __LINE__);
    check_parse_val("log(x) at 1",       "{ log(x) | x = 1 }",           0.0,          __LINE__);
    check_parse_val("sqrt(x) at 4",      "{ sqrt(x) | x = 4 }",          2.0,          __LINE__);
    check_parse_val("√(x) at 4",         "{ √(x) | x = 4 }",             2.0,          __LINE__);
    /* Binary functions */
    check_parse_val("atan2(1, 1) = π/4",
        "{ atan2(x\xe2\x82\x80, x\xe2\x82\x81) | x\xe2\x82\x80 = 1, x\xe2\x82\x81 = 1 }",
        M_PI / 4.0, __LINE__);
    check_parse_val("pow(2, 3) = 8",
        "{ pow(x\xe2\x82\x80, x\xe2\x82\x81) | x\xe2\x82\x80 = 2, x\xe2\x82\x81 = 3 }",
        8.0, __LINE__);
    /* Superscript on function name: sin²(x) */
    check_parse_val("sin\xc2\xb2(x) at 0",
        "{ sin\xc2\xb2(x) | x = 0 }",
        0.0, __LINE__);
    /* Nested function calls */
    check_parse_val("exp(sin(x)) at 0 = 1",
        "{ exp(sin(x)) | x = 0 }",
        1.0, __LINE__);
    check_parse_val("sqrt(exp(x)) at 0 = 1",
        "{ sqrt(exp(x)) | x = 0 }",
        1.0, __LINE__);
    /* Function applied to parenthesised expression */
    check_parse_val("sin(x\xe2\x82\x80 + x\xe2\x82\x81) = sin(π/2) = 1",
        "{ sin(x\xe2\x82\x80 + x\xe2\x82\x81) | x\xe2\x82\x80 = 0, x\xe2\x82\x81 = 0 }",
        0.0, __LINE__);
}

/* ---- Special functions (the 18 new ops) ---- */

static void test_from_string_special_functions(void)
{
    /* Unary — clean exact values */
    check_parse_val("abs(-3) = 3",           "{ abs(x) | x = -3 }",          3.0,                     __LINE__);
    check_parse_val("|-3| = 3",              "{ |x| | x = -3 }",            3.0,                     __LINE__);
    check_parse_val("erf(0) = 0",            "{ erf(x) | x = 0 }",           0.0,                     __LINE__);
    check_parse_val("erfc(0) = 1",           "{ erfc(x) | x = 0 }",          1.0,                     __LINE__);
    check_parse_val("erfinv(0) = 0",         "{ erfinv(x) | x = 0 }",        0.0,                     __LINE__);
    check_parse_val("erfcinv(1) = 0",        "{ erfcinv(x) | x = 1 }",       0.0,                     __LINE__);
    check_parse_val("gamma(3) = 2",          "{ gamma(x) | x = 3 }",         2.0,                     __LINE__);
    check_parse_val("lgamma(1) = 0",         "{ lgamma(x) | x = 1 }",        0.0,                     __LINE__);
    check_parse_val("digamma(1) = -gamma_E", "{ digamma(x) | x = 1 }",      -0.5772156649015329,       __LINE__);
    check_parse_val("lambert_w0(0) = 0",     "{ lambert_w0(x) | x = 0 }",    0.0,                     __LINE__);
    check_parse_val("lambert_wm1(-0.2)",     "{ lambert_wm1(x) | x = -0.2 }",  -2.5426413577735265,     __LINE__);
    check_parse_val("normal_pdf(0)",         "{ normal_pdf(x) | x = 0 }",    1.0/sqrt(2.0*M_PI),      __LINE__);
    check_parse_val("normal_cdf(0) = 0.5",   "{ normal_cdf(x) | x = 0 }",    0.5,                     __LINE__);
    check_parse_val("normal_logpdf(0)",      "{ normal_logpdf(x) | x = 0 }", -0.5*log(2.0*M_PI),      __LINE__);
    check_parse_val("Ei(1)",                 "{ Ei(x) | x = 1 }",            1.8951178163559367,       __LINE__);
    check_parse_val("E1(1)",                 "{ E1(x) | x = 1 }",            0.21938393439552029,      __LINE__);
    /* Binary functions */
    check_parse_val("beta(1,1) = 1",         "{ beta(x, y) | x = 1, y = 1 }", 1.0,                   __LINE__);
    check_parse_val("logbeta(1,1) = 0",      "{ logbeta(x, y) | x = 1, y = 1 }", 0.0,                __LINE__);
    check_parse_val("hypot(3,4) = 5",        "{ hypot(x, y) | x = 3, y = 4 }", 5.0,                  __LINE__);
}

/* ---- Named constants (binding section) ---- */

static void test_from_string_named_consts(void)
{
    /* No ';' means all bindings are variables */
    check_parse_val("x + y (implicit all vars) = 5",
        "{ x + y | x = 2, y = 3 }",
        5.0, __LINE__);
    /* Named constant after ';' combined with a variable */
    check_parse_val("c\xe2\x82\x80x\xe2\x82\x80\xc2\xb2 = 12",
        "{ c\xe2\x82\x80x\xe2\x82\x80\xc2\xb2 | x\xe2\x82\x80 = 2; c\xe2\x82\x80 = 3 }",
        12.0, __LINE__);
    /* Leading ';' means all bindings are named constants */
    check_parse_val("c\xe2\x82\x80 + c\xe2\x82\x81 (implicit all consts) = 3",
        "{ c\xe2\x82\x80 + c\xe2\x82\x81 | ; c\xe2\x82\x80 = 1, c\xe2\x82\x81 = 2 }",
        3.0, __LINE__);
    /* Named constant and variable */
    check_parse_val("c\xe2\x82\x80 + x (const + var) = 15",
        "{ c\xe2\x82\x80 + x | x = 5; c\xe2\x82\x80 = 10 }",
        15.0, __LINE__);
    /* Two named constants in the const section */
    check_parse_val("c\xe2\x82\x80x\xe2\x82\x80 + c\xe2\x82\x81 = 2π+e",
        "{ c\xe2\x82\x80x\xe2\x82\x80 + c\xe2\x82\x81 | x\xe2\x82\x80 = 2; c\xe2\x82\x80 = 3.141592653589793, c\xe2\x82\x81 = 2.718281828459045 }",
        2 * 3.141592653589793 + 2.718281828459045, __LINE__);
    /* Bracketed name in const section */
    check_parse_val("[scale]·x = 6",
        "{ [scale]x | x = 3; [scale] = 2 }",
        6.0, __LINE__);
}

/* ---- Bracketed (multi-character) names ---- */

static void test_from_string_bracketed_names(void)
{
    check_parse_val("[radius]\xc2\xb2 = 25",
        "{ [radius]\xc2\xb2 | [radius] = 5 }",
        25.0, __LINE__);
    check_parse_val("[base]\xc2\xb7[height] = 12",
        "{ [base]\xc2\xb7[height] | [base] = 3, [height] = 4 }",
        12.0, __LINE__);
    check_parse_val("[my var] alone = 7",
        "{ [my var] | [my var] = 7 }",
        7.0, __LINE__);
    check_parse_val("[x']\xc2\xb2 = 36",
        "{ [x']\xc2\xb2 | [x'] = 6 }",
        36.0, __LINE__);
    check_parse_val("[2pi]\xc2\xb7x = 2π",
        "{ [2pi]x | x = 1; [2pi] = 6.283185307179586 }",
        6.283185307179586, __LINE__);
    /* Bracketed name as named const in pure-const format */
    check_parse_val("[my const] pure const = 99",
        "{ [my const] = 99 }",
        99.0, __LINE__);
}

static void test_from_string_name_normalization(void)
{
    dval_t *a1 = dv_new_named_var_d(1.0, "a1");
    dval_t *a12 = dv_new_named_var_d(1.0, "a12");
    dval_t *a123 = dv_new_named_var_d(1.0, "a123");
    dval_t *pi1 = dv_new_named_var_d(1.0, "@pi1");
    dval_t *pi2 = dv_new_named_var_d(1.0, "@pi_2");
    dval_t *parsed_pi1 = dval_from_string("{ @pi1 }");
    char *a1s = a1 ? dv_to_string(a1, style_EXPRESSION) : NULL;
    char *a12s = a12 ? dv_to_string(a12, style_EXPRESSION) : NULL;
    char *a123s = a123 ? dv_to_string(a123, style_EXPRESSION) : NULL;
    char *pi1s = pi1 ? dv_to_string(pi1, style_EXPRESSION) : NULL;
    char *pi2s = pi2 ? dv_to_string(pi2, style_EXPRESSION) : NULL;
    char *parsed_pi1s = parsed_pi1 ? dv_to_string(parsed_pi1, style_EXPRESSION) : NULL;

    if (a1s && str_eq(a1s, "{ a₁ | a₁ = 1 }")) {
        to_string_pass("normalize a1", a1s, "{ a₁ | a₁ = 1 }");
    } else {
        to_string_fail(__FILE__, __LINE__, 1, "normalize a1",
                       a1s ? a1s : "(null)", "{ a₁ | a₁ = 1 }");
    }

    if (a12s && str_eq(a12s, "{ a₁₂ | a₁₂ = 1 }")) {
        to_string_pass("normalize a12", a12s, "{ a₁₂ | a₁₂ = 1 }");
    } else {
        to_string_fail(__FILE__, __LINE__, 1, "normalize a12",
                       a12s ? a12s : "(null)", "{ a₁₂ | a₁₂ = 1 }");
    }

    if (a123s && str_eq(a123s, "{ a₁₂₃ | a₁₂₃ = 1 }")) {
        to_string_pass("normalize a123", a123s, "{ a₁₂₃ | a₁₂₃ = 1 }");
    } else {
        to_string_fail(__FILE__, __LINE__, 1, "normalize a123",
                       a123s ? a123s : "(null)", "{ a₁₂₃ | a₁₂₃ = 1 }");
    }

    if (pi1s && str_eq(pi1s, "{ π₁ | π₁ = 1 }")) {
        to_string_pass("normalize @pi1", pi1s, "{ π₁ | π₁ = 1 }");
    } else {
        to_string_fail(__FILE__, __LINE__, 1, "normalize @pi1",
                       pi1s ? pi1s : "(null)", "{ π₁ | π₁ = 1 }");
    }

    if (pi2s && str_eq(pi2s, "{ π₂ | π₂ = 1 }")) {
        to_string_pass("normalize @pi_2", pi2s, "{ π₂ | π₂ = 1 }");
    } else {
        to_string_fail(__FILE__, __LINE__, 1, "normalize @pi_2",
                       pi2s ? pi2s : "(null)", "{ π₂ | π₂ = 1 }");
    }

    if (parsed_pi1s && str_eq(parsed_pi1s, "{ π₁ | π₁ = NAN }")) {
        to_string_pass("implicit @pi1 stays variable", parsed_pi1s,
                       "{ π₁ | π₁ = NAN }");
    } else {
        to_string_fail(__FILE__, __LINE__, 1, "implicit @pi1 stays variable",
                       parsed_pi1s ? parsed_pi1s : "(null)",
                       "{ π₁ | π₁ = NAN }");
    }

    if (parsed_pi1 && qf_isnan(dv_eval_qf(parsed_pi1))) {
        printf(C_BOLD C_GREEN "PASS" C_RESET " implicit @pi1 evaluates to NaN\n\n");
    } else {
        printf(C_BOLD C_RED "FAIL" C_RESET " implicit @pi1 evaluates to NaN %s:%d:1\n\n",
               __FILE__, __LINE__);
        TEST_FAIL();
    }

    free(parsed_pi1s);
    free(pi2s);
    free(pi1s);
    free(a123s);
    free(a12s);
    free(a1s);
    dv_free(parsed_pi1);
    dv_free(pi2);
    dv_free(pi1);
    dv_free(a123);
    dv_free(a12);
    dv_free(a1);
}

static void test_from_string_implicit_symbolic_bindings(void)
{
    dval_t *x = dval_from_string("{ x }");
    dval_t *e = dval_from_string("{ e }");
    dval_t *pi_ascii = dval_from_string("{ pi }");
    dval_t *pi_alias = dval_from_string("{ @pi }");
    dval_t *tau = dval_from_string("{ τ }");
    dval_t *phi_alias = dval_from_string("{ @phi }");
    dval_t *gamma_alias = dval_from_string("{ @gamma }");
    dval_t *tau_alias = dval_from_string("{ @tau }");
    dval_t *f = dval_from_string("{ [radius]^2 + c_1 + π + e }");
    char *xs = x ? dv_to_string(x, style_EXPRESSION) : NULL;
    char *es = e ? dv_to_string(e, style_EXPRESSION) : NULL;
    char *pi_as = pi_ascii ? dv_to_string(pi_ascii, style_EXPRESSION) : NULL;
    char *pi_ats = pi_alias ? dv_to_string(pi_alias, style_EXPRESSION) : NULL;
    char *taus = tau ? dv_to_string(tau, style_EXPRESSION) : NULL;
    char *phi_as = phi_alias ? dv_to_string(phi_alias, style_EXPRESSION) : NULL;
    char *gamma_as = gamma_alias ? dv_to_string(gamma_alias, style_EXPRESSION) : NULL;
    char *tau_as = tau_alias ? dv_to_string(tau_alias, style_EXPRESSION) : NULL;
    char *fs = f ? dv_to_string(f, style_EXPRESSION) : NULL;

    if (x && xs && str_eq(xs, "{ x | x = NAN }")) {
        to_string_pass("implicit symbolic var inference", xs, "{ x | x = NAN }");
    } else {
        to_string_fail(__FILE__, __LINE__, 1, "implicit symbolic var inference",
                       xs ? xs : "(null)", "{ x | x = NAN }");
    }

    if (e && es && str_eq(es, "{ e | e = 2.718281828459045235360287471352664 }")) {
        to_string_pass("implicit e constant inference", es,
                       "{ e | e = 2.718281828459045235360287471352664 }");
    } else {
        to_string_fail(__FILE__, __LINE__, 1, "implicit e constant inference",
                       es ? es : "(null)",
                       "{ e | e = 2.718281828459045235360287471352664 }");
    }

    if (pi_ascii && pi_as &&
        str_eq(pi_as, "{ π | π = 3.141592653589793238462643383279505 }")) {
        to_string_pass("implicit pi constant inference", pi_as,
                       "{ π | π = 3.141592653589793238462643383279505 }");
    } else {
        to_string_fail(__FILE__, __LINE__, 1, "implicit pi constant inference",
                       pi_as ? pi_as : "(null)",
                       "{ π | π = 3.141592653589793238462643383279505 }");
    }

    if (pi_alias && pi_ats &&
        str_eq(pi_ats, "{ π | π = 3.141592653589793238462643383279505 }")) {
        to_string_pass("implicit @pi constant inference", pi_ats,
                       "{ π | π = 3.141592653589793238462643383279505 }");
    } else {
        to_string_fail(__FILE__, __LINE__, 1, "implicit @pi constant inference",
                       pi_ats ? pi_ats : "(null)",
                       "{ π | π = 3.141592653589793238462643383279505 }");
    }

    if (tau && taus && str_eq(taus, "{ τ | τ = NAN }")) {
        to_string_pass("implicit tau variable inference", taus, "{ τ | τ = NAN }");
    } else {
        to_string_fail(__FILE__, __LINE__, 1, "implicit tau variable inference",
                       taus ? taus : "(null)", "{ τ | τ = NAN }");
    }

    if (phi_alias && phi_as &&
        str_eq(phi_as, "{ φ | φ = 1.618033988749894848204586834365641 }")) {
        to_string_pass("implicit @phi constant inference", phi_as,
                       "{ φ | φ = 1.618033988749894848204586834365641 }");
    } else {
        to_string_fail(__FILE__, __LINE__, 1, "implicit @phi constant inference",
                       phi_as ? phi_as : "(null)",
                       "{ φ | φ = 1.618033988749894848204586834365641 }");
    }

    if (gamma_alias && gamma_as &&
        str_eq(gamma_as, "{ γ | γ = 0.5772156649015328606065120900820167 }")) {
        to_string_pass("implicit @gamma constant inference", gamma_as,
                       "{ γ | γ = 0.5772156649015328606065120900820167 }");
    } else {
        to_string_fail(__FILE__, __LINE__, 1, "implicit @gamma constant inference",
                       gamma_as ? gamma_as : "(null)",
                       "{ γ | γ = 0.5772156649015328606065120900820167 }");
    }

    if (tau_alias && tau_as && str_eq(tau_as, "{ τ | τ = NAN }")) {
        to_string_pass("implicit @tau variable inference", tau_as, "{ τ | τ = NAN }");
    } else {
        to_string_fail(__FILE__, __LINE__, 1, "implicit @tau variable inference",
                       tau_as ? tau_as : "(null)", "{ τ | τ = NAN }");
    }

    if (f && fs && str_eq(fs, "{ c₁ + e + π + [radius]² | [radius] = NAN; c₁ = NAN, e = 2.718281828459045235360287471352664, π = 3.141592653589793238462643383279505 }")) {
        to_string_pass("implicit mixed symbolic inference", fs,
                       "{ c₁ + e + π + [radius]² | [radius] = NAN; c₁ = NAN, e = 2.718281828459045235360287471352664, π = 3.141592653589793238462643383279505 }");
    } else {
        to_string_fail(__FILE__, __LINE__, 1, "implicit mixed symbolic inference",
                       fs ? fs : "(null)",
                       "{ c₁ + e + π + [radius]² | [radius] = NAN; c₁ = NAN, e = 2.718281828459045235360287471352664, π = 3.141592653589793238462643383279505 }");
    }

    if (x && qf_isnan(dv_eval_qf(x))) {
        printf(C_BOLD C_GREEN "PASS" C_RESET " implicit x evaluates to NaN\n\n");
    } else {
        printf(C_BOLD C_RED "FAIL" C_RESET " implicit x evaluates to NaN %s:%d:1\n\n",
               __FILE__, __LINE__);
        TEST_FAIL();
    }

    if (tau && qf_isnan(dv_eval_qf(tau))) {
        printf(C_BOLD C_GREEN "PASS" C_RESET " implicit tau evaluates to NaN\n\n");
    } else {
        printf(C_BOLD C_RED "FAIL" C_RESET " implicit tau evaluates to NaN %s:%d:1\n\n",
               __FILE__, __LINE__);
        TEST_FAIL();
    }

    if (e && qf_eq(dv_eval_qf(e), QF_E)) {
        printf(C_BOLD C_GREEN "PASS" C_RESET " implicit e evaluates to built-in constant\n\n");
    } else {
        printf(C_BOLD C_RED "FAIL" C_RESET " implicit e evaluates to built-in constant %s:%d:1\n\n",
               __FILE__, __LINE__);
        TEST_FAIL();
    }

    if (pi_ascii && qf_eq(dv_eval_qf(pi_ascii), QF_PI) &&
        pi_alias && qf_eq(dv_eval_qf(pi_alias), QF_PI)) {
        printf(C_BOLD C_GREEN "PASS" C_RESET " implicit pi aliases evaluate to built-in constant\n\n");
    } else {
        printf(C_BOLD C_RED "FAIL" C_RESET " implicit pi aliases evaluate to built-in constant %s:%d:1\n\n",
               __FILE__, __LINE__);
        TEST_FAIL();
    }

    if (phi_alias && qf_eq(dv_eval_qf(phi_alias),
                           qf_div(qf_add(qf_from_double(1.0), qf_sqrt(qf_from_double(5.0))),
                                  qf_from_double(2.0)))) {
        printf(C_BOLD C_GREEN "PASS" C_RESET " implicit @phi evaluates to built-in constant\n\n");
    } else {
        printf(C_BOLD C_RED "FAIL" C_RESET " implicit @phi evaluates to built-in constant %s:%d:1\n\n",
               __FILE__, __LINE__);
        TEST_FAIL();
    }

    if (gamma_alias && qf_eq(dv_eval_qf(gamma_alias), QF_EULER_MASCHERONI)) {
        printf(C_BOLD C_GREEN "PASS" C_RESET " implicit @gamma evaluates to built-in constant\n\n");
    } else {
        printf(C_BOLD C_RED "FAIL" C_RESET " implicit @gamma evaluates to built-in constant %s:%d:1\n\n",
               __FILE__, __LINE__);
        TEST_FAIL();
    }

    if (tau_alias && qf_isnan(dv_eval_qf(tau_alias))) {
        printf(C_BOLD C_GREEN "PASS" C_RESET " implicit @tau evaluates to NaN\n\n");
    } else {
        printf(C_BOLD C_RED "FAIL" C_RESET " implicit @tau evaluates to NaN %s:%d:1\n\n",
               __FILE__, __LINE__);
        TEST_FAIL();
    }

    if (f && qf_isnan(dv_eval_qf(f))) {
        printf(C_BOLD C_GREEN "PASS" C_RESET " implicit mixed expression evaluates to NaN\n\n");
    } else {
        printf(C_BOLD C_RED "FAIL" C_RESET " implicit mixed expression evaluates to NaN %s:%d:1\n\n",
               __FILE__, __LINE__);
        TEST_FAIL();
    }

    free(fs);
    free(tau_as);
    free(gamma_as);
    free(phi_as);
    free(taus);
    free(pi_ats);
    free(pi_as);
    free(es);
    free(xs);
    dv_free(f);
    dv_free(tau_alias);
    dv_free(gamma_alias);
    dv_free(phi_alias);
    dv_free(tau);
    dv_free(pi_alias);
    dv_free(pi_ascii);
    dv_free(e);
    dv_free(x);
}

/* ---- ASCII alternative syntax — subscripts (_N) ---- */

static void test_from_string_subscripts(void)
{
    /* Basic _N in both binding and expression */
    check_parse_val("x_0^2 = 9",
        "{ x_0^2 | x_0 = 3 }",
        9.0, __LINE__);
    check_parse_val("x_0 + x_1 + x_2 = 6",
        "{ x_0 + x_1 + x_2 | x_0 = 1, x_1 = 2, x_2 = 3 }",
        6.0, __LINE__);
    check_parse_val("c_0*x_0^2 + c_1 = 7",
        "{ c_0*x_0^2 + c_1 | x_0 = 2; c_0 = 1.5, c_1 = 1 }",
        7.0, __LINE__);
    check_parse_val("sin(x_0)*cos(x_0) at pi/4 = 0.5",
        "{ sin(x_0)*cos(x_0) | x_0 = 0.7853981633974483 }",
        0.5, __LINE__);
    /* _N and Unicode ₀-₉ must be interchangeable within one string */
    check_parse_val("Unicode expr, ASCII binding",
        "{ x\xe2\x82\x80^2 | x_0 = 3 }",
        9.0, __LINE__);
    check_parse_val("ASCII expr, Unicode binding",
        "{ x_0^2 | x\xe2\x82\x80 = 3 }",
        9.0, __LINE__);
    check_parse_val("Unicode expr + const, ASCII binding",
        "{ c\xe2\x82\x80*x\xe2\x82\x80^2 | x_0 = 2; c_0 = 3 }",
        12.0, __LINE__);
    check_parse_val("c_0*sin(x_0) + c_1*cos(x_1): Unicode consts, ASCII vars",
        "{ c\xe2\x82\x80*sin(x_0) + c\xe2\x82\x81*cos(x_1)"
        " | x_0 = 1.5707963267948966, x_1 = 0; c_0 = 2, c_1 = 5 }",
        7.0, __LINE__);   /* 2*sin(pi/2) + 5*cos(0) = 2*1 + 5*1 = 7 */
}

/* ---- ASCII alternative syntax — star (*) multiplication ---- */

static void test_from_string_star_mul(void)
{
    /* No spaces */
    check_parse_val("a*b = 12 (no spaces)",
        "{ x_0*x_1 | x_0 = 3, x_1 = 4 }",
        12.0, __LINE__);
    /* Spaces around '*' */
    check_parse_val("a * b = 12 (spaces)",
        "{ x_0 * x_1 | x_0 = 3, x_1 = 4 }",
        12.0, __LINE__);
    /* Scalar constant times function */
    check_parse_val("c * sin(pi/2) = 5",
        "{ c * sin(x) | x = 1.5707963267948966; c = 5 }",
        5.0, __LINE__);
    check_parse_val("c * exp(0) = 3",
        "{ c * exp(x) | x = 0; c = 3 }",
        3.0, __LINE__);
    check_parse_val("c * log(e) = 7",
        "{ c * log(x) | x = 2.718281828459045; c = 7 }",
        7.0, __LINE__);
    /* Chained '*' */
    check_parse_val("a * b * c = 24",
        "{ x_0 * x_1 * x_2 | x_0 = 2, x_1 = 3, x_2 = 4 }",
        24.0, __LINE__);
    /* '*' combined with negation */
    check_parse_val("a * -b = -12",
        "{ x_0 * -x_1 | x_0 = 3, x_1 = 4 }",
        -12.0, __LINE__);
    /* '*' with parenthesised sub-expressions: (a+b)(a-b) = a²-b² */
    check_parse_val("(a+b)*(a-b) = a^2-b^2 = 16",
        "{ (x_0 + x_1) * (x_0 - x_1) | x_0 = 5, x_1 = 3 }",
        16.0, __LINE__);
    /* Sine addition formula: sin(a)cos(b) + sin(b)cos(a) = sin(a+b) */
    check_parse_val("sin(a)*cos(b) + sin(b)*cos(a) = sin(pi/2) = 1",
        "{ sin(x_0)*cos(x_1) + sin(x_1)*cos(x_0)"
        " | x_0 = 1.0471975511965976, x_1 = 0.5235987755982988 }",
        1.0, __LINE__);
    /* Gaussian envelope: c*exp(-x^2) */
    check_parse_val("c * exp(-x^2) at x=0 = c",
        "{ c * exp(-x_0^2) | x_0 = 0; c = 7 }",
        7.0, __LINE__);
    /* exp product identity: exp(f)*exp(-f) = 1 */
    check_parse_val("exp(sin(x)) * exp(-sin(x)) = 1",
        "{ exp(sin(x)) * exp(-sin(x)) | x = 0.7 }",
        1.0, __LINE__);
}

/* ---- ASCII alternative syntax — ^N exponent on function names ---- */

static void test_from_string_func_power(void)
{
    /* All 15 unary functions with ^2: exact-value cases */
    check_parse_val("sin^2(0) = 0",       "{ sin^2(x) | x = 0 }",   0.0, __LINE__);
    check_parse_val("cos^2(0) = 1",       "{ cos^2(x) | x = 0 }",   1.0, __LINE__);
    check_parse_val("tan^2(0) = 0",       "{ tan^2(x) | x = 0 }",   0.0, __LINE__);
    check_parse_val("sinh^2(0) = 0",      "{ sinh^2(x) | x = 0 }",  0.0, __LINE__);
    check_parse_val("cosh^2(0) = 1",      "{ cosh^2(x) | x = 0 }",  1.0, __LINE__);
    check_parse_val("tanh^2(0) = 0",      "{ tanh^2(x) | x = 0 }",  0.0, __LINE__);
    check_parse_val("asin^2(0) = 0",      "{ asin^2(x) | x = 0 }",  0.0, __LINE__);
    check_parse_val("acos^2(1) = 0",      "{ acos^2(x) | x = 1 }",  0.0, __LINE__);
    check_parse_val("atan^2(0) = 0",      "{ atan^2(x) | x = 0 }",  0.0, __LINE__);
    check_parse_val("asinh^2(0) = 0",     "{ asinh^2(x) | x = 0 }", 0.0, __LINE__);
    check_parse_val("acosh^2(1) = 0",     "{ acosh^2(x) | x = 1 }", 0.0, __LINE__);
    check_parse_val("atanh^2(0) = 0",     "{ atanh^2(x) | x = 0 }", 0.0, __LINE__);
    check_parse_val("exp^2(0) = 1",       "{ exp^2(x) | x = 0 }",   1.0, __LINE__);
    check_parse_val("log^2(e) = 1",
        "{ log^2(x) | x = 2.718281828459045 }",                      1.0, __LINE__);
    check_parse_val("sqrt^2(9) = 9",      "{ sqrt^2(x) | x = 9 }",  9.0, __LINE__);
    /* Non-trivial exponent values */
    check_parse_val("tan^2(pi/4) = 1",
        "{ tan^2(x) | x = 0.7853981633974483 }",                     1.0, __LINE__);
    check_parse_val("sqrt^3(4) = 8",      "{ sqrt^3(x) | x = 4 }",  8.0, __LINE__);
    check_parse_val("exp^3(0) = 1",       "{ exp^3(x) | x = 0 }",   1.0, __LINE__);
    /* Multi-digit exponent */
    check_parse_val("sin^10(0) = 0",      "{ sin^10(x) | x = 0 }",  0.0, __LINE__);
    check_parse_val("cos^10(0) = 1",      "{ cos^10(x) | x = 0 }",  1.0, __LINE__);
    /* Pythagorean identities expressed with ^N */
    check_parse_val("sin^2(x) + cos^2(x) = 1",
        "{ sin^2(x) + cos^2(x) | x = 1.234 }",                      1.0, __LINE__);
    check_parse_val("cosh^2(x) - sinh^2(x) = 1",
        "{ cosh^2(x) - sinh^2(x) | x = 2.5 }",                      1.0, __LINE__);
    check_parse_val("sqrt(sin^2(x) + cos^2(x)) = 1",
        "{ sqrt(sin^2(x) + cos^2(x)) | x = 0.9 }",                  1.0, __LINE__);
    /* ^N combined with subscripted variable names */
    check_parse_val("sin^2(x_0) + cos^2(x_0) = 1 (subscript + ^N)",
        "{ sin^2(x_0) + cos^2(x_0) | x_0 = 0.7 }",                  1.0, __LINE__);
    check_parse_val("exp^2(x_0) * exp(-2*x_0^2) at x_0=1",
        "{ exp^2(x_0) * exp(-2*x_0^2) | x_0 = 1 }",                 1.0, __LINE__);
}

/* ---- Complex composed expressions using all ASCII alternatives ---- */

static void test_from_string_composed(void)
{
    /* Euclidean distance in 2D: sqrt(x^2 + y^2) */
    check_parse_val("sqrt(x_0^2 + x_1^2) = 5 (3-4-5 triangle)",
        "{ sqrt(x_0^2 + x_1^2) | x_0 = 3, x_1 = 4 }",
        5.0, __LINE__);

    /* exp / log mutual inverses */
    check_parse_val("exp(log(x_0)) = x_0 = 3",
        "{ exp(log(x_0)) | x_0 = 3 }",
        3.0, __LINE__);
    check_parse_val("log(exp(x_0)) = x_0 = 2.5",
        "{ log(exp(x_0)) | x_0 = 2.5 }",
        2.5, __LINE__);

    /* Trig inverse pairs */
    check_parse_val("asin(sin(x_0)) = 0.5",
        "{ asin(sin(x_0)) | x_0 = 0.5 }",
        0.5, __LINE__);
    check_parse_val("acos(cos(x_0)) = 0.6",
        "{ acos(cos(x_0)) | x_0 = 0.6 }",
        0.6, __LINE__);
    check_parse_val("atan(tan(x_0)) = pi/6",
        "{ atan(tan(x_0)) | x_0 = 0.5235987755982988 }",
        0.5235987755982988, __LINE__);

    /* Hyperbolic inverse pairs */
    check_parse_val("asinh(sinh(x_0)) = 1.5",
        "{ asinh(sinh(x_0)) | x_0 = 1.5 }",
        1.5, __LINE__);
    check_parse_val("acosh(cosh(x_0)) = 1.2",
        "{ acosh(cosh(x_0)) | x_0 = 1.2 }",
        1.2, __LINE__);
    check_parse_val("atanh(tanh(x_0)) = 0.8",
        "{ atanh(tanh(x_0)) | x_0 = 0.8 }",
        0.8, __LINE__);

    /* atan2 recovers angle from unit-circle coordinates */
    check_parse_val("atan2(sin(x_0), cos(x_0)) = x_0",
        "{ atan2(sin(x_0), cos(x_0)) | x_0 = 0.5 }",
        0.5, __LINE__);

    /* tan(x)*cos(x) = sin(x): at x=pi/6, result = 0.5 */
    check_parse_val("tan(x_0) * cos(x_0) = sin(x_0) = 0.5",
        "{ tan(x_0) * cos(x_0) | x_0 = 0.5235987755982988 }",
        0.5, __LINE__);

    /* exp product identity: exp(f(x)) * exp(-f(x)) = 1 */
    check_parse_val("exp(sin(x)) * exp(-sin(x)) = 1",
        "{ exp(sin(x)) * exp(-sin(x)) | x = 0.7 }",
        1.0, __LINE__);
    check_parse_val("exp(cos(x)) * exp(-cos(x)) = 1",
        "{ exp(cos(x)) * exp(-cos(x)) | x = 1.2 }",
        1.0, __LINE__);
    check_parse_val("exp(log(x_0)) * exp(-log(x_0)) = 1",
        "{ exp(log(x_0)) * exp(-log(x_0)) | x_0 = 4 }",
        1.0, __LINE__);

    /* c_0*sin^2(x_0) + c_1*cos^2(x_1): three ASCII features together */
    check_parse_val("c_0*sin^2(pi/2) + c_1*cos^2(0) = 3+5 = 8",
        "{ c_0*sin^2(x_0) + c_1*cos^2(x_1)"
        " | x_0 = 1.5707963267948966, x_1 = 0; c_0 = 3, c_1 = 5 }",
        8.0, __LINE__);

    /* Gaussian bell: c*exp(-x^2) at peak */
    check_parse_val("c_0 * exp(-x_0^2) at x_0=0 = c_0",
        "{ c_0 * exp(-x_0^2) | x_0 = 0; c_0 = 4 }",
        4.0, __LINE__);

    /* Chain: exp(c_0*x_0^2) * sin^2(x_1) + log(x_2) = 1 */
    check_parse_val("exp(c*x^2)*sin^2(y) + log(z) = 1",
        "{ exp(c_0*x_0^2)*sin^2(x_1) + log(x_2)"
        " | x_0 = 0, x_1 = 1.5707963267948966, x_2 = 1; c_0 = 1 }",
        1.0, __LINE__);

    /* sqrt(exp(2*ln(3))) = sqrt(9) = 3 */
    check_parse_val("sqrt(exp(c_0 * x_0)) = 3",
        "{ sqrt(exp(c_0 * x_0)) | x_0 = 1.0986122886681098; c_0 = 2 }",
        3.0, __LINE__);

    /* log(x_0^3) = 3*log(x_0): at x_0=e this is 3 */
    check_parse_val("log(x_0^3) = 3*log(e) = 3",
        "{ log(x_0^3) | x_0 = 2.718281828459045 }",
        3.0, __LINE__);

    /* cosh^2(x_0) - sinh^2(x_0) = 1 with subscripted name */
    check_parse_val("cosh^2(x_0) - sinh^2(x_0) = 1",
        "{ cosh^2(x_0) - sinh^2(x_0) | x_0 = 3.1 }",
        1.0, __LINE__);

    /* Four-variable expression: a*sin(x) + b*cos(y) + c*exp(-z) + d */
    check_parse_val("a*sin(x)+b*cos(y)+c*exp(-z)+d at zeros",
        "{ c_0*sin(x_0) + c_1*cos(x_1) + c_2*exp(-x_2) + c_3"
        " | x_0 = 0, x_1 = 0, x_2 = 0; c_0 = 1, c_1 = 2, c_2 = 3, c_3 = 4 }",
        0.0 + 2.0 + 3.0 + 4.0, __LINE__);
}

static void test_from_string_simplified_identity_text(void)
{
    dval_t *expr = dval_from_string("{ sin^2(x) + cos^2(x) | x = 1.234 }");
    char *text = expr ? dv_to_string(expr, style_EXPRESSION) : NULL;

    if (!(text && strcmp(text, "{ 1 }") == 0)) {
        printf(C_BOLD C_RED "FAIL" C_RESET " sin^2(x) + cos^2(x) exact text simplifies to 1 %s:%d:1\n",
               __FILE__, __LINE__);
        printf("  got:      %s\n", text ? text : "<null>");
        printf("  expected: { 1 }\n\n");
        TEST_FAIL();
    } else {
        printf(C_BOLD C_GREEN "PASS" C_RESET " sin^2(x) + cos^2(x) exact text simplifies to 1\n\n");
    }

    free(text);
    dv_free(expr);
}

/* ---- Group runner for all ASCII-alternative tests ---- */

static void test_from_string_ascii_alternatives(void)
{
    RUN_SUBTEST(test_from_string_subscripts);
    RUN_SUBTEST(test_from_string_star_mul);
    RUN_SUBTEST(test_from_string_composed);
    RUN_SUBTEST(test_from_string_func_power);
    RUN_SUBTEST(test_from_string_simplified_identity_text);
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
 * and programmatic differentiation of a parsed dval_t.
 */

/* Inline comparison helper for the derivative checks below. */
static void check_dval_d(const char *label, const dval_t *node,
                          double expect, int line)
{
    qfloat_t qval = dv_eval_qf(node);
    double got   = qf_to_double(qval);
    double err   = fabs(got - expect);
    double rel   = (fabs(expect) > 0.0) ? err / fabs(expect) : err;
    const double TOL = 2e-14;
    char *expr = dv_to_string(node, style_EXPRESSION);
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

static void test_from_string_deriv(void)
{
    const double xv  = 1.25;
    const double sx  = sin(xv);
    const double cx  = cos(xv);
    const double esx = exp(sx);

    /* ---- Explicit parse-and-evaluate: f, f', f'' as strings ---- */

    /* f(x) = exp(sin(x)) + 3*x^2 - 7 */
    check_parse_val("f = exp(sin(x)) + 3*x^2 - 7 at x=1.25",
        "{ exp(sin(x)) + 3*x^2 - 7 | x = 1.25 }",
        esx + 3*xv*xv - 7, __LINE__);

    /* f'(x) = cos(x)*exp(sin(x)) + 6*x  (explicit star) */
    check_parse_val("f' = cos(x)*exp(sin(x)) + 6*x at x=1.25",
        "{ cos(x)*exp(sin(x)) + 6*x | x = 1.25 }",
        cx*esx + 6*xv, __LINE__);

    /* f'(x) same expression with implicit mul for 6x */
    check_parse_val("f' = cos(x)*exp(sin(x)) + 6x (implicit 6x) at x=1.25",
        "{ cos(x)*exp(sin(x)) + 6x | x = 1.25 }",
        cx*esx + 6*xv, __LINE__);

    /* f''(x) = cos^2(x)*exp(sin(x)) - sin(x)*exp(sin(x)) + 6  (expanded) */
    check_parse_val("f'' expanded: cos^2(x)*exp(sin(x)) - sin(x)*exp(sin(x)) + 6",
        "{ cos^2(x)*exp(sin(x)) - sin(x)*exp(sin(x)) + 6 | x = 1.25 }",
        cx*cx*esx - sx*esx + 6, __LINE__);

    /* f''(x) same value, factored with brackets: (cos^2(x) - sin(x))*exp(sin(x)) + 6 */
    check_parse_val("f'' factored: (cos^2(x) - sin(x))*exp(sin(x)) + 6",
        "{ (cos^2(x) - sin(x))*exp(sin(x)) + 6 | x = 1.25 }",
        (cx*cx - sx)*esx + 6, __LINE__);

    /* Additional bracket-heavy forms */
    check_parse_val("f'' double-bracket: (cos^2(x) - sin(x)) * (exp(sin(x))) + 6",
        "{ (cos^2(x) - sin(x)) * (exp(sin(x))) + 6 | x = 1.25 }",
        (cx*cx - sx)*esx + 6, __LINE__);
    check_parse_val("(sin(x) + cos(x))^2 at x=0 = 1",
        "{ (sin(x) + cos(x))^2 | x = 0 }",
        1.0, __LINE__);
    check_parse_val("exp((x_0 + x_1)^2) at (0,0) = 1",
        "{ exp((x_0 + x_1)^2) | x_0 = 0, x_1 = 0 }",
        1.0, __LINE__);
    check_parse_val("(x^2 + 1)^2 at x=2 = 25",
        "{ (x^2 + 1)^2 | x = 2 }",
        25.0, __LINE__);

    /* ---- Programmatic differentiation ---- */
    /* Build f(x) = exp(sin(x)) + 3*x^2 - 7 explicitly so we hold the wrt pointer. */
    {
        dval_t *xvar  = dv_new_named_var_d(xv, "x");
        dval_t *sinx  = dv_sin(xvar);
        dval_t *esinx = dv_exp(sinx);
        dval_t *x2    = dv_pow_d(xvar, 2.0);
        dval_t *t     = dv_mul_d(x2, 3.0);
        dval_t *t2    = dv_sub_d(t, 7.0);
        dval_t *f     = dv_add(esinx, t2);

        check_dval_d("f(1.25) via dv_eval_d",   f,   esx + 3*xv*xv - 7, __LINE__);

        dval_t *df  = dv_create_deriv(f,  xvar);
        check_dval_d("f'(1.25) via dv_create_deriv",  df,  cx*esx + 6*xv, __LINE__);

        dval_t *d2f = dv_create_deriv(df, xvar);
        check_dval_d("f''(1.25) via dv_create_deriv", d2f, cx*cx*esx - sx*esx + 6, __LINE__);

        dv_free(d2f); dv_free(df); dv_free(f);
        dv_free(t2); dv_free(t); dv_free(x2); dv_free(esinx); dv_free(sinx);
        dv_free(xvar);
    }
}

/* ---- Error paths — all must return NULL ----
 * Note: dval_from_string writes diagnostics to stderr for these cases. */

static void test_from_string_errors(void)
{
    /* NULL input (silent — no stderr output) */
    check_parse_null("NULL input",
        NULL, __LINE__);
    /* Missing opening '{' */
    check_parse_null("no opening brace",
        "x + 1", __LINE__);
    /* Missing closing '}' */
    check_parse_null("no closing brace",
        "{ x | x = 1", __LINE__);
    /* Unknown symbol in expression */
    check_parse_null("unknown symbol",
        "{ z | x = 1 }", __LINE__);
    /* Duplicate variable name */
    check_parse_null("duplicate var name",
        "{ x | x = 1, x = 2 }", __LINE__);
    /* Same name used as both variable and named constant */
    check_parse_null("var-const name clash",
        "{ x | x = 1; x = 2 }", __LINE__);
    /* Missing '=' in binding */
    check_parse_null("missing '=' in binding",
        "{ x | x 1 }", __LINE__);
    /* Missing numeric value after '=' in binding */
    check_parse_null("missing value in binding",
        "{ x | x = }", __LINE__);
    /* Missing exponent after '^' */
    check_parse_null("missing exponent after '^'",
        "{ x^ | x = 2 }", __LINE__);
    /* Missing exponent digits after function-name '^' */
    check_parse_null("missing function exponent digits",
        "{ sin^(x) | x = 0 }", __LINE__);
    /* Malformed decimal exponent */
    check_parse_null("malformed decimal exponent",
        "{ x^2e | x = 2 }", __LINE__);
    /* Binary function with too few arguments */
    check_parse_null("binary function missing arg",
        "{ atan2(x) | x = 1 }", __LINE__);
    /* Unary function with too many arguments */
    check_parse_null("unary function extra arg",
        "{ sin(x, y) | x = 1, y = 2 }", __LINE__);
    /* Missing closing ')' in grouped expression */
    check_parse_null("missing closing paren",
        "{ (x + 1 | x = 2 }", __LINE__);
    /* Missing closing ']' in bracketed name */
    check_parse_null("missing closing bracket",
        "{ [radius | [radius] = 5 }", __LINE__);
    /* Trailing expression input */
    check_parse_null("trailing input after expression",
        "{ x y z | x = 1, y = 2, z = 3 }", __LINE__);
    /* Extra comma in binary function */
    check_parse_null("binary function extra comma",
        "{ atan2(x, y, z) | x = 1, y = 2, z = 3 }", __LINE__);
}

static void test_from_expression_string_api(void)
{
    dval_t *x = dv_new_named_var_d(3.0, "x");
    dval_t *y = dv_new_named_var_d(4.0, "y");
    dval_t *c = dv_new_named_const_d(2.0, "c");

    const char *names[] = { "x", "y", "c" };
    dval_t *symbols[] = { x, y, c };

    dval_t *ok = dval_from_expression_string("c*(x + y)", names, symbols, 3);
    if (!ok) {
        printf(C_BOLD C_RED "FAIL" C_RESET " bare expression parse returned NULL %s:%d:1\n\n",
               __FILE__, __LINE__);
        TEST_FAIL();
    } else {
        double got = dv_eval_d(ok);
        double expect = 14.0;
        double err = fabs(got - expect);
        if (err < 2e-14) {
            printf(C_BOLD C_GREEN "PASS" C_RESET " bare expression parse API\n\n");
        } else {
            printf(C_BOLD C_RED "FAIL" C_RESET " bare expression parse API %s:%d:1\n",
                   __FILE__, __LINE__);
            printf(C_BOLD "  got     " C_RESET "%.17g\n", got);
            printf(C_BOLD "  expect  " C_RESET "%.17g\n\n", expect);
            TEST_FAIL();
        }
        dv_free(ok);
    }

    if (dval_from_expression_string("x + y", names, NULL, 2) != NULL) {
        printf(C_BOLD C_RED "FAIL" C_RESET " incomplete symbol table should fail %s:%d:1\n\n",
               __FILE__, __LINE__);
        TEST_FAIL();
    } else {
        printf(C_BOLD C_GREEN "PASS" C_RESET " incomplete symbol table rejected\n\n");
    }

    {
        const char *dup_names[] = { "x", "x" };
        dval_t *dup_symbols[] = { x, y };
        if (dval_from_expression_string("x + 1", dup_names, dup_symbols, 2) != NULL) {
            printf(C_BOLD C_RED "FAIL" C_RESET " duplicate external symbols should fail %s:%d:1\n\n",
                   __FILE__, __LINE__);
            TEST_FAIL();
        } else {
            printf(C_BOLD C_GREEN "PASS" C_RESET " duplicate external symbols rejected\n\n");
        }
    }

    if (dval_from_expression_string("x + z", names, symbols, 3) != NULL) {
        printf(C_BOLD C_RED "FAIL" C_RESET " unknown external symbol should fail %s:%d:1\n\n",
               __FILE__, __LINE__);
        TEST_FAIL();
    } else {
        printf(C_BOLD C_GREEN "PASS" C_RESET " unknown external symbol rejected\n\n");
    }

    dv_free(c);
    dv_free(y);
    dv_free(x);
}

static void test_from_string_bindings_api(void)
{
    dval_binding_t *bindings = NULL;
    size_t nbindings = 0;
    dval_t *expr = dval_from_string_with_bindings("{ x^2 + c_1 }", &bindings, &nbindings);
    dval_binding_t *x_binding;
    dval_binding_t *c_binding;
    dval_t *deriv;

    if (!expr) {
        printf(C_BOLD C_RED "FAIL" C_RESET " parse with bindings returned NULL %s:%d:1\n\n",
               __FILE__, __LINE__);
        TEST_FAIL();
        return;
    }

    x_binding = dval_binding_find(bindings, nbindings, "x");
    c_binding = dval_binding_find(bindings, nbindings, "c₁");

    if (!x_binding || x_binding->is_constant) {
        printf(C_BOLD C_RED "FAIL" C_RESET " inferred x binding missing or wrong kind %s:%d:1\n\n",
               __FILE__, __LINE__);
        TEST_FAIL();
    } else {
        printf(C_BOLD C_GREEN "PASS" C_RESET " inferred x binding returned for differentiation\n\n");
    }

    if (!c_binding || !c_binding->is_constant) {
        printf(C_BOLD C_RED "FAIL" C_RESET " inferred c₁ binding missing or wrong kind %s:%d:1\n\n",
               __FILE__, __LINE__);
        TEST_FAIL();
    } else {
        printf(C_BOLD C_GREEN "PASS" C_RESET " inferred c₁ constant binding returned\n\n");
    }

    if (dval_binding_set_d(bindings, nbindings, "x", 3.0) != 0 ||
        dval_binding_set_d(bindings, nbindings, "c_1", 5.0) != 0) {
        printf(C_BOLD C_RED "FAIL" C_RESET " binding setters failed %s:%d:1\n\n",
               __FILE__, __LINE__);
        TEST_FAIL();
        free(bindings);
        dv_free(expr);
        return;
    }

    check_dval_d("parsed expr after binding update", expr, 14.0, __LINE__);

    deriv = dv_create_deriv(expr, x_binding->symbol);
    if (!deriv) {
        printf(C_BOLD C_RED "FAIL" C_RESET " derivative from inferred binding returned NULL %s:%d:1\n\n",
               __FILE__, __LINE__);
        TEST_FAIL();
    } else {
        check_dval_d("derivative from inferred x binding", deriv, 6.0, __LINE__);
        dv_free(deriv);
    }

    free(bindings);
    dv_free(expr);
}

/* ---- Round-trips: build → string → parse → compare value ---- */

static void test_from_string_round_trips(void)
{
    /* Unnamed-variable expressions (auto-subscripted names x₀, x₁, …) */
    check_roundtrip("u01: x0^2",                    make_expr_u01(), __LINE__);
    check_roundtrip("u02: x0^3",                    make_expr_u02(), __LINE__);
    check_roundtrip("u03: x0^2 * y0^3 * x0",       make_expr_u03(), __LINE__);
    check_roundtrip("u04: x0^2 + x0^2",             make_expr_u04(), __LINE__);
    check_roundtrip("u05: sin(x0) * cos(x0)",       make_expr_u05(), __LINE__);
    check_roundtrip("u06: exp(sin(x0)) * exp(cos(x0))", make_expr_u06(), __LINE__);
    /* Named-constant expressions */
    check_roundtrip("c01: c0 * x0^2",               make_expr_c01(), __LINE__);
    check_roundtrip("c02: c0 * sin(x0)",             make_expr_c02(), __LINE__);
    check_roundtrip("c03: x0 + x1 + c0",            make_expr_c03(), __LINE__);
    check_roundtrip("c04: c0*x0 + c1",              make_expr_c04(), __LINE__);
    /* Bracketed (multi-character) name expressions */
    check_roundtrip("l01: [radius]^2",               make_expr_l01(), __LINE__);
    check_roundtrip("l02: [base] * [height]",        make_expr_l02(), __LINE__);
    check_roundtrip("l03: [pi] * [radius]^2",        make_expr_l03(), __LINE__);
    check_roundtrip("l04: pi * [radius]^2",          make_expr_l04(), __LINE__);
    check_roundtrip("l05: sin([theta]) * cos([theta])", make_expr_l05(), __LINE__);
    check_roundtrip("l06: [pi] * [tau] * x",         make_expr_l06(), __LINE__);
    check_roundtrip("l07: [my var]^2",               make_expr_l07(), __LINE__);
    check_roundtrip("l08: [2pi] * x",                make_expr_l08(), __LINE__);
    check_roundtrip("l09: [x']^2",                   make_expr_l09(), __LINE__);
}

void test_dval_t_from_string(void)
{
    RUN_SUBTEST(test_from_string_pure_const);
    RUN_SUBTEST(test_from_string_arithmetic);
    RUN_SUBTEST(test_from_string_functions);
    RUN_SUBTEST(test_from_string_special_functions);
    RUN_SUBTEST(test_from_string_named_consts);
    RUN_SUBTEST(test_from_string_bracketed_names);
    RUN_SUBTEST(test_from_string_name_normalization);
    RUN_SUBTEST(test_from_string_implicit_symbolic_bindings);
    RUN_SUBTEST(test_from_string_ascii_alternatives);
    RUN_SUBTEST(test_from_string_errors);
    RUN_SUBTEST(test_from_expression_string_api);
    RUN_SUBTEST(test_from_string_bindings_api);
    RUN_SUBTEST(test_from_string_round_trips);
    RUN_SUBTEST(test_from_string_deriv);
}
