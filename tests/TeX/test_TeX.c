/**
 * @file test_TeX.c
 * @brief TeX semantic rendering and authorised private fragment regressions.
 *
 * Exercises native semantic callbacks and the private fragment constructors.
 * Public RPN construction checks are registered from test_TeX_builder.c.
 * Standalone programmes in docs/TeX.md are compiled and run separately by the
 * Markdown example runner after ordinary regression checks.
 */
#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "TeX-t.h"
#include "expression-t.h"
#include "matrix-t.h"
#include "equation-t.h"
#include "diffequation-t.h"
#define MARS_SHARED_DIFFEQUATION_INTERNAL_ACCESS
#include "internal/diffequation_internal.h"
#define MARS_EXPR_INTERNAL_ACCESS
#include "expression/expr_notation_internal.h"
#define MARS_MATRIX_INTERNAL_ACCESS
#include "matrix/matrix_notation_internal.h"
#define MARS_EQUATION_INTERNAL_ACCESS
#include "equation/equation_internal.h"
#define MARS_TeX_INTERNAL_ACCESS
#include "TeX/TeX_internal.h"
#include "test_harness.h"
#include "test_TeX_spellings.h"
#include "test_TeX_builder.h"

TEST_SUITE_CONFIG(TEST_CONFIG_GLOBAL);

static void test_TeX_escaping(void)
{
    string_t *input = string_new_with("£ α #$%&\\^_{}~");
    TeX_t *text = TeX_text(input);
    TEST_ASSERT_TRUE(input && strcmp(string_c_str(input), "£ α #$%&\\^_{}~") == 0,
                     "escaping does not modify the borrowed input");
    string_free(input);
    TEST_ASSERT_TRUE(text && strcmp(TeX_source(text),
        "\\text{£ α \\#\\$\\%\\&\\textbackslash{}\\textasciicircum{}\\_\\{\\}\\textasciitilde{}}") == 0,
        "literal text preserves UTF-8 and escapes all TeX metacharacters");
    TeX_free(text);
}

static void test_TeX_text_boundaries(void)
{
    string_t *input = string_new();
    TeX_t *empty = TeX_text(input);
    TEST_ASSERT_TRUE(empty && strcmp(TeX_source(empty), "\\text{}") == 0, "empty strings are valid text");
    TeX_free(empty);
    const char bytes[] = {'a', '\0', '&', 'b'};
    TEST_ASSERT_TRUE(input && string_append_bytes(input, bytes, sizeof bytes) == 0,
                     "construct a length-delimited string containing NUL");
    errno = 0;
    TeX_t *invalid = TeX_text(input);
    TEST_ASSERT_TRUE(!invalid && errno == EINVAL, "embedded NUL is rejected instead of truncating text");
    TeX_free(invalid);
    string_free(input);

    input = string_new_with("ordinary £ α text & final é text");
    TeX_t *spans = TeX_text(input);
    string_free(input);
    TEST_ASSERT_TRUE(spans && strcmp(TeX_source(spans), "\\text{ordinary £ α text \\& final é text}") == 0,
                     "ordinary spans on both sides of an escape survive release of the input");
    TeX_free(spans);
}

static void test_TeX_composition(void)
{
    TeX_t *x = TeX_raw("x"), *two = TeX_raw("2");
    TeX_t *fraction = TeX_compose_fraction(x, two);
    TeX_t *root = TeX_compose_root(fraction, two);
    TeX_t *scripts = TeX_compose_scripts(root, x, two);
    const TeX_t *parts[] = {fraction, scripts};
    TeX_t *sequence = TeX_compose_sequence(parts, 2);
    string_t *rendered = TeX_render(sequence);
    TeX_free(x);
    TeX_free(two);
    TeX_free(fraction);
    TeX_free(root);
    TeX_free(scripts);
    TeX_free(sequence);
    TEST_ASSERT_TRUE(rendered && strcmp(string_c_str(rendered),
        "\\frac{x}{2}{\\sqrt[2]{\\frac{x}{2}}}_{x}^{2}") == 0,
        "compositions and rendered strings own independent copies of children");
    string_free(rendered);
}

static void test_TeX_matrices_and_invalid_inputs(void)
{
    TeX_t *a = TeX_raw("a"), *b = TeX_raw("b");
    const TeX_t *cells[] = {a, NULL, b, a};
    TeX_t *matrix = TeX_compose_matrix(cells, 2, 2, TeX_MATRIX_BRACKETS, 10);
    TEST_ASSERT_TRUE(matrix && strcmp(TeX_source(matrix),
        "\\begin{bmatrix}a &  \\\\[10pt] b & a\\end{bmatrix}") == 0,
        "matrix layout preserves spacing and empty entries");
    TeX_free(matrix);
    matrix = TeX_compose_matrix(cells, 1, 1, TeX_MATRIX_PARENTHESES, 0);
    TEST_ASSERT_TRUE(matrix && strcmp(TeX_source(matrix), "\\begin{pmatrix}a\\end{pmatrix}") == 0,
                     "parenthesised matrices are independent of application policy");
    TeX_free(matrix);
    matrix = TeX_compose_matrix(cells, 2, 1, TeX_MATRIX_PLAIN, 0);
    TEST_ASSERT_TRUE(matrix && strcmp(TeX_source(matrix), "\\begin{matrix}a \\\\ \\end{matrix}") == 0,
                     "plain matrices support default row spacing");
    TeX_free(matrix);
    TEST_ASSERT_TRUE(!TeX_compose_matrix(cells, SIZE_MAX, 2, TeX_MATRIX_PLAIN, 0) && errno == EINVAL,
                     "overflow is rejected before cell access");
    TEST_ASSERT_TRUE(!TeX_compose_matrix(cells, 1, 1, (TeX_matrix_style_t)-1, 0) &&
                     !TeX_compose_matrix(cells, 0, 1, TeX_MATRIX_PLAIN, 0) && !TeX_compose_fraction(a, NULL) &&
                     !TeX_compose_root(NULL, a) && !TeX_compose_scripts(NULL, a, b) && !TeX_text(NULL) &&
                     !TeX_raw(NULL) && !TeX_compose_sequence(NULL, 1) && !TeX_compose_sequence(cells, 2),
                     "invalid arguments never produce partial notation");
    TeX_t *empty = TeX_compose_sequence(NULL, 0);
    TEST_ASSERT_TRUE(empty && !*TeX_source(empty), "empty composition is valid");
    TeX_free(empty);
    TeX_free(a);
    TeX_free(b);
    TeX_free(NULL);
}

static void test_TeX_calculus(void)
{
    TeX_t *body = TeX_raw("f(x)"), *x = TeX_raw("x");
    TeX_t *lower = TeX_raw("0"), *upper = TeX_raw("1");
    TeX_t *sum = TeX_compose_sum(body, lower, upper);
    TeX_t *unbounded = TeX_compose_sum(body, NULL, NULL);
    TeX_t *upper_only = TeX_compose_sum(body, NULL, upper);
    TeX_t *integral = TeX_compose_integral(body, x, lower, upper);
    TeX_t *indefinite = TeX_compose_integral(body, x, NULL, NULL);
    TeX_t *iterated = TeX_compose_integral(integral, x, NULL, NULL);
    TeX_t *ordinary = TeX_compose_derivative(body, x, 1, TeX_DERIVATIVE_ORDINARY);
    TeX_t *partial = TeX_compose_derivative(body, x, 2, TeX_DERIVATIVE_PARTIAL);
    TeX_t *partial_first = TeX_compose_derivative(body, x, 1, TeX_DERIVATIVE_PARTIAL);
    TeX_t *ordinary_third = TeX_compose_derivative(body, x, 3, TeX_DERIVATIVE_ORDINARY);
    TeX_free(body);
    TeX_free(x);
    TeX_free(lower);
    TeX_free(upper);
    TEST_ASSERT_TRUE(sum && strcmp(TeX_source(sum), "\\sum_{0}^{1} {f(x)}") == 0 &&
                     unbounded && strcmp(TeX_source(unbounded), "\\sum {f(x)}") == 0 &&
                     upper_only && strcmp(TeX_source(upper_only), "\\sum^{1} {f(x)}") == 0,
                     "bounds are independently optional and inputs are copied");
    TEST_ASSERT_TRUE(integral && strcmp(TeX_source(integral), "\\int_{0}^{1} {f(x)}\\,\\mathrm{d}{x}") == 0 &&
                     indefinite && strcmp(TeX_source(indefinite), "\\int {f(x)}\\,\\mathrm{d}{x}") == 0 &&
                     iterated && strcmp(TeX_source(iterated),
                     "\\int {\\int_{0}^{1} {f(x)}\\,\\mathrm{d}{x}}\\,\\mathrm{d}{x}") == 0,
                     "definite, indefinite and iterated integrals retain differentials");
    TEST_ASSERT_TRUE(ordinary && strcmp(TeX_source(ordinary), "\\frac{\\mathrm{d} {f(x)}}{\\mathrm{d} {x}}") == 0 &&
                     partial && strcmp(TeX_source(partial), "\\frac{\\partial^{2} {f(x)}}{\\partial {x}^{2}}") == 0 &&
                     partial_first && strcmp(TeX_source(partial_first), "\\frac{\\partial {f(x)}}{\\partial {x}}") == 0 &&
                     ordinary_third && strcmp(TeX_source(ordinary_third),
                     "\\frac{\\mathrm{d}^{3} {f(x)}}{\\mathrm{d} {x}^{3}}") == 0,
                     "first and higher derivatives use matching numerator and denominator orders");
    TeX_free(sum);
    TeX_free(unbounded);
    TeX_free(upper_only);
    TeX_free(integral);
    TeX_free(indefinite);
    TeX_free(iterated);
    TeX_free(ordinary);
    TeX_free(partial);
    TeX_free(partial_first);
    TeX_free(ordinary_third);
}

static void test_TeX_tensor_slots_and_fences(void)
{
    TeX_t *base = TeX_raw("T"), *mu = TeX_raw("\\mu"), *nu = TeX_raw("\\nu");
    const TeX_index_t slots[] = {{mu, TeX_INDEX_UPPER}, {nu, TeX_INDEX_LOWER}, {mu, TeX_INDEX_UPPER}};
    TeX_t *tensor = TeX_compose_tensor(base, slots, 3);
    TeX_t *scalar = TeX_compose_tensor(base, NULL, 0);
    TeX_t *round = TeX_compose_fenced(base, TeX_FENCE_PARENTHESES);
    TeX_t *square = TeX_compose_fenced(base, TeX_FENCE_BRACKETS);
    TeX_t *absolute = TeX_compose_fenced(base, TeX_FENCE_ABSOLUTE);
    TeX_free(base);
    TeX_free(mu);
    TeX_free(nu);
    TEST_ASSERT_TRUE(tensor && strcmp(TeX_source(tensor), "{T}^{\\mu}{}_{\\nu}{}^{\\mu}") == 0 &&
                     scalar && strcmp(TeX_source(scalar), "{T}") == 0,
                     "mixed tensor variance keeps distinct horizontal index slots");
    TEST_ASSERT_TRUE(round && strcmp(TeX_source(round), "\\left( T \\right)") == 0 &&
                     square && strcmp(TeX_source(square), "\\left[ T \\right]") == 0 &&
                     absolute && strcmp(TeX_source(absolute), "\\left| T \\right|") == 0,
                     "delimiters remain independent of the source lifetime");
    TeX_free(tensor);
    TeX_free(scalar);
    TeX_free(round);
    TeX_free(square);
    TeX_free(absolute);
}

static void test_TeX_calculus_invalid_inputs(void)
{
    TeX_t *x = TeX_raw("x");
    const TeX_index_t missing[] = {{NULL, TeX_INDEX_LOWER}};
    const TeX_index_t invalid[] = {{x, (TeX_index_position_t)-1}};
    TEST_ASSERT_TRUE(!TeX_compose_tensor(NULL, NULL, 0) && !TeX_compose_tensor(x, NULL, 1) &&
                     !TeX_compose_tensor(x, missing, 1) && !TeX_compose_tensor(x, invalid, 1) &&
                     !TeX_compose_fenced(NULL, TeX_FENCE_BRACKETS) && !TeX_compose_fenced(x, (TeX_fence_t)-1) &&
                     !TeX_compose_fenced(x, (TeX_fence_t)5) && !TeX_compose_sum(NULL, x, x) &&
                     !TeX_compose_integral(NULL, x, NULL, NULL) && !TeX_compose_integral(x, NULL, NULL, NULL) &&
                     !TeX_compose_derivative(NULL, x, 1, TeX_DERIVATIVE_PARTIAL) &&
                     !TeX_compose_derivative(x, NULL, 1, TeX_DERIVATIVE_PARTIAL) &&
                     !TeX_compose_derivative(x, x, 0, TeX_DERIVATIVE_PARTIAL) &&
                     !TeX_compose_derivative(x, x, 1, (TeX_derivative_style_t)-1),
                     "invalid notation arguments and enum values are rejected before lookup");
    TeX_free(x);
}

static void test_TeX_from_expression(void)
{
    string_t *input = string_new_with("1/2(x^2 - 4)/(x-1)*sin(x)/x*log(1/x)");
    TeX_t *notation = TeX_from_expression(input);
    TEST_ASSERT_TRUE(input && strcmp(string_c_str(input),
                     "1/2(x^2 - 4)/(x-1)*sin(x)/x*log(1/x)") == 0, "mathematical input is borrowed unchanged");
    string_free(input);
    const char *expected = "\\frac{\\left(x^{2} - 4\\right)\\mkern-2mu \\sin(x)\\mkern-2mu "
                           "\\lg(\\frac{1}{x})}{2\\mkern-2mu x\\mkern-2mu \\left(x - 1\\right)}";
    TEST_ASSERT_TRUE(notation && strcmp(TeX_source(notation), expected) == 0,
                     "native beautification gathers the product into a readable fraction");
    TeX_free(notation);

    input = string_new_with("(1/2)*(x^2-4)/(x-1)*sin(x)/x*log(1/x)");
    notation = TeX_from_expression(input);
    TEST_ASSERT_TRUE(notation && strcmp(TeX_source(notation), expected) == 0,
                     "implicit multiplication agrees with explicit grouping");
    TeX_free(notation);
    string_free(input);

    input = string_new_with("ln(x)");
    notation = TeX_from_expression(input);
    TEST_ASSERT_TRUE(notation && strcmp(TeX_source(notation), "\\ln(x)") == 0,
                     "natural logarithm remains distinct from base-ten log");
    TeX_free(notation);
    string_free(input);

    input = string_new_with("sin(x)^2 + cos(x)^2");
    notation = TeX_from_expression(input);
    TEST_ASSERT_TRUE(notation && strcmp(TeX_source(notation), "1") == 0,
                     "the adapter deliberately uses the native simplifier");
    TeX_free(notation);
    string_free(input);
}

static void test_TeX_from_expression_invalid(void)
{
    string_t *input = string_new_with("sin(");
    TeX_t *notation = TeX_from_expression(input);
    TEST_ASSERT_TRUE(!notation, "invalid syntax does not produce escaped or partial notation");
    TeX_free(notation);
    string_free(input);
    input = string_new();
    const char bytes[] = {'x', '\0', '+', '1'};
    TEST_ASSERT_TRUE(input && !string_append_bytes(input, bytes, sizeof bytes), "construct embedded-NUL input");
    errno = 0;
    notation = TeX_from_expression(input);
    TEST_ASSERT_TRUE(!notation && errno == EINVAL, "embedded NUL is rejected before parsing");
    TeX_free(notation);
    string_free(input);
    TEST_ASSERT_TRUE(!TeX_from_expression(NULL), "NULL mathematical text is invalid");
}

static void test_TeX_calculus_shorthand(void)
{
    static const struct { const char *input; const char *expected; } cases[] = {
        {"@F{exp(-x^2)}", "\\mathcal{F}_{x\\to k}\\{e^{-x^{2}}\\}"},
        {"Fourier(exp(-x^2),x,k)", "\\mathcal{F}_{x\\to k}\\{e^{-x^{2}}\\}"},
        {"ℱ{exp(-x^2)}", "\\mathcal{F}_{x\\to k}\\{e^{-x^{2}}\\}"},
        {"sin(@F{exp(-x^2)})", "\\sin(\\mathcal{F}_{x\\to k}\\{e^{-x^{2}}\\})"},
        {"@Finv{exp(-k^2)}", "\\mathcal{F}^{-1}_{k\\to x}\\{e^{-k^{2}}\\}"},
        {"@L{sin(t)}", "\\mathcal{L}_{t\\to s}\\{\\sin(t)\\}"},
        {"@Linv{1/(s+1)}", "\\mathcal{L}^{-1}_{s\\to t}\\{\\frac{1}{s + 1}\\}"},
        {"@P_(k=1)^3 k", "\\prod_{k=1}^{3}k"},
        {"lambertw0(x)", "W_{0}(x)"},
        {"BesselJ(2,x)", "J_{2}\\left(x\\right)"},
        {"normalpdf(x)", "\\operatorname{normal\\_pdf}(x)"},
        {"gamma(x)", "\\Gamma(x)"},
        {"conj(x)", "\\overline{x}"},
        {"Re(x)", "\\mathfrak{Re}\\mathopen{}\\left\\{x\\right\\}"},
        {"Im(x)", "\\mathfrak{Im}\\mathopen{}\\left\\{x\\right\\}"},
        {"zeta(x)", "\\zeta(x)"},
        {"erf(x)", "\\operatorname{erf}(x)"},
        {"sinh(x)", "\\sinh(x)"},
        {"@inf", "\\infty"},
        {"@infinity", "\\infty"},
        {"-@inf", "-\\infty"},
        {"∞", "\\infty"},
        {"@eulermascheroni", "\\gamma"},
        {"@gamma", "\\gamma"},
        {"γ", "\\gamma"},
        {"@pi", "\\pi"},
        {"@phi", "\\phi"},
        {"@tau", "\\tau"},
        {"e", "e"},
        {"i", "i"},
        {"@S_0^(@inf) exp(-x) dx", "\\int_{0}^{\\infty} e^{-x}\\, dx"},
        {"@S x^2 dx", "\\int x^{2}\\, dx"},
        {"@S_0^1 x^2 dx", "\\int_{0}^{1} x^{2}\\, dx"},
        {"@Z_(k=1)^3 k^2", "\\sum_{k=1}^{3}k^{2}"},
        {"d/dx(sin(x))", "\\frac{\\partial}{\\partial x}\\left[\\sin(x)\\right]"},
        {"d^2/dx^2(sin(x))", "\\frac{\\partial^{2}}{\\partial x^{2}}\\left[\\sin(x)\\right]"},
        {"d^2/dxy(x^2*y^3)", "\\frac{\\partial^{2}}{\\partial y\\,\\partial x}\\left[x^{2}\\mkern-2mu y^{3}\\right]"},
        {"Dxy(x^2*y^3)", "\\frac{\\partial^{2}}{\\partial y\\,\\partial x}\\left[x^{2}\\mkern-2mu y^{3}\\right]"},
        {"Dxyx(x^3*y^2)", "\\frac{\\partial^{3}}{\\partial x\\,\\partial y\\,\\partial x}\\left[x^{3}\\mkern-2mu y^{2}\\right]"},
        {"D[x^2y](x^3*y^2)", "\\frac{\\partial^{3}}{\\partial y\\,\\partial x^{2}}\\left[x^{3}\\mkern-2mu y^{2}\\right]"}
    };
    for (size_t i = 0; i < sizeof cases / sizeof *cases; ++i) {
        string_t *input = string_new_with(cases[i].input);
        TeX_t *notation = TeX_from_expression(input);
        TEST_ASSERT_TRUE(notation && strcmp(TeX_source(notation), cases[i].expected) == 0, cases[i].input);
        TeX_free(notation);
        string_free(input);
    }
    const char *invalid[] = {"d^2/dx^3(x)", "d^0/dx(x)", "d^2/dxyx(x)", "d^2/dx^s(x)"};
    for (size_t i = 0; i < sizeof invalid / sizeof *invalid; ++i) {
        string_t *input = string_new_with(invalid[i]);
        TeX_t *notation = TeX_from_expression(input);
        TEST_ASSERT_TRUE(!notation, "mismatched or non-integer derivative orders are not silently multiplied");
        TeX_free(notation);
        string_free(input);
    }
}

static void test_TeX_expression_style(void)
{
    static const struct { const char *input; const char *expected; } cases[] = {
        {"{sqrt(x^2+c)/(x+1) | x=2; c=4}", "\\frac{\\sqrt{x^{2} + c}}{x + 1}"},
        {"{x²+c | x=[1,2,3]; c=4}", "x^{2} + c"},
        {"{[radius]^2 | [radius]=5}", "\\mathit{radius}^{2}"},
        {"{sin²(x)+cos²(x) | x=?}", "1"},
        {"{x_0+@alpha | x_0=?; @alpha=2}", "x_{0} + \\alpha"},
        {"{120/s^6 where (Re(s)>0) | s=2}", "\\frac{120}{s^{6}}\\quad \\left( \\mathfrak{Re}(s) > 0 \\right)"},
        /* style_EXPRESSION has already evaluated this transform; convert the emitted result faithfully. */
        {"@F{exp(-x^2)}", "\\sqrt{\\pi}\\mkern-2mu e^{-\\frac{1}{4}\\mkern-2mu k^{2}}\\quad \\left( k\\in \\mathbb{R} \\right)"},
        {"@S_0^1 x^2 dx", "\\int_{0}^{1} x^{2}\\, dx"}
    };
    for (size_t i = 0; i < sizeof cases / sizeof *cases; ++i) {
        string_t *input = string_new_with(cases[i].input);
        expr_t *expression = expr_from_text_preserving_calculus(input, NULL);
        string_t *serialised = expression ? expr_to_text(expression, style_EXPRESSION) : NULL;
        TeX_t *notation = serialised ? TeX_from_expression(serialised) : NULL;
        TEST_ASSERT_TRUE(notation && strcmp(TeX_source(notation), cases[i].expected) == 0, cases[i].input);
        TeX_free(notation);
        string_free(serialised);
        expr_free(expression);
        string_free(input);
    }
}

static void test_TeX_equations(void)
{
    TeX_t *left = TeX_raw("x"), *right = TeX_raw("y");
    TeX_t *plain = TeX_compose_equation(left, right, TeX_EQUATION_INLINE);
    TeX_t *aligned = TeX_compose_equation(left, right, TeX_EQUATION_ALIGNED);
    TEST_ASSERT_TRUE(plain && strcmp(TeX_source(plain), "x = y") == 0, "inline equation");
    TEST_ASSERT_TRUE(aligned && strcmp(TeX_source(aligned),
        "\\begin{aligned}[t]\nx &= y\n\\end{aligned}") == 0, "aligned equation");
    TeX_t *wrapped = TeX_raw("\\begin{aligned}[t]\n&a + b\\\\\n&{}+ c  \n\\end{aligned}");
    TeX_t *continued = TeX_compose_equation(left, wrapped, TeX_EQUATION_ALIGNED);
    TEST_ASSERT_TRUE(continued && strcmp(TeX_source(continued),
        "\\begin{aligned}[t]\nx ={}& a + b\\\\\n&{}+ c\n\\end{aligned}") == 0,
        "wrapped right side is incorporated without nested alignment");
    TEST_ASSERT_TRUE(!TeX_compose_equation(NULL, right, TeX_EQUATION_INLINE), "missing side rejected");
    TEST_ASSERT_TRUE(!TeX_compose_equation(left, right, (TeX_equation_style_t)-1), "invalid style rejected");
    TeX_free(right);
    TeX_free(left);
    TEST_ASSERT_TRUE(plain && strcmp(TeX_source(plain), "x = y") == 0, "equation owns copied sides");
    TeX_free(continued);
    TeX_free(wrapped);
    TeX_free(aligned);
    TeX_free(plain);
}

static void test_TeX_relation_input(void)
{
    static const struct { const char *input; const char *expected; } cases[] = {
        {"x < y", "x < y"}, {"x<=y", "x \\le y"}, {"x >= y", "x \\ge y"}, {"x>y", "x > y"},
        {"x != y", "x \\ne y"}, {"x == y", "x = y"}, {"x = y", "x = y"},
        {"x ~~= y", "x \\approx y"}, {"x ~== y", "x \\cong y"},
        {"x ≤ y", "x \\le y"}, {"x ≥ y", "x \\ge y"}, {"x ≠ y", "x \\ne y"},
        {"x ≈ y", "x \\approx y"}, {"x ≅ y", "x \\cong y"},
        {"0 < x <= 1", "0 < x \\le 1"}, {"a>=b>c!=d==e~~=f~==g", "a \\ge b > c \\ne d = e \\approx f \\cong g"},
        {"x=y=z", "x = y = z"},
        {"{x <= y | x = 3, y = 4}", "x \\le y"},
        {"sin(x)~~=x", "\\sin(x) \\approx x"},
        {"@Z_(k=1)^3 k == 6", "\\sum_{k=1}^{3}k = 6"},
        {"{x | x != 0}", "x\\quad \\left( x \\ne 0 \\right)"},
        {"{x | x = 1}", "x"},
        {"(1,2;3,4) == (5,6;7,8)",
         "\\begin{bmatrix}1 & 2 \\\\[10pt] 3 & 4\\end{bmatrix} = "
         "\\begin{bmatrix}5 & 6 \\\\[10pt] 7 & 8\\end{bmatrix}"}
    };
    for (size_t i = 0; i < sizeof cases / sizeof *cases; ++i) {
        string_t *input = string_new_with(cases[i].input);
        TeX_t *notation = TeX_from_expression(input);
        TEST_ASSERT_TRUE(notation && !strcmp(TeX_source(notation), cases[i].expected), cases[i].input);
        TeX_builder_t *builder = TeX_builder_new();
        bool pushed = TeX_push_math(builder, cases[i].input);
        TeX_t *stack_result = TeX_builder_finish(builder);
        TEST_ASSERT_TRUE(pushed && stack_result && !strcmp(TeX_source(stack_result), cases[i].expected),
                         "pushed mathematical text uses the same native relation grammar");
        TeX_free(stack_result);
        TeX_builder_free(builder);
        TeX_free(notation);
        string_free(input);
    }
}

static void test_TeX_relation_input_invalid(void)
{
    static const char *const invalid[] = {
        "x <", "<= y", "x < <= y", "x === y", "x ~= y", "x ~=== y", "x ~~== y",
        "x < = y", "x >< y", "x !=", "x ≅", "x <= y >", "x) < y", "x < (y", "sin(x<y)"
    };
    for (size_t i = 0; i < sizeof invalid / sizeof *invalid; ++i) {
        string_t *input = string_new_with(invalid[i]);
        TeX_t *notation = TeX_from_expression(input);
        TEST_ASSERT_TRUE(!notation, invalid[i]);
        TeX_free(notation);
        string_free(input);
    }
}

static void test_TeX_matrix_input(void)
{
    static const struct { const char *input; const char *expected; } cases[] = {
        {"(1, 2; 3, 4)^x", "{\\begin{bmatrix}1 & 2 \\\\[10pt] 3 & 4\\end{bmatrix}}^{x}"},
        {"(1, 2; 3, 4)^2", "{\\begin{bmatrix}1 & 2 \\\\[10pt] 3 & 4\\end{bmatrix}}^{2}"},
        {" (1 2; 3 4) ^ (x+1) ", "{\\begin{bmatrix}1 & 2 \\\\[10pt] 3 & 4\\end{bmatrix}}^{x + 1}"},
        {"(x, 1; 0, y)^(x+1)", "{\\begin{bmatrix}x & 1 \\\\[10pt] 0 & y\\end{bmatrix}}^{x + 1}"},
        {"(1,2)", "\\begin{bmatrix}1 & 2\\end{bmatrix}"},
        {"(sin(x), root(8,3); 3,4)^(@alpha+1)",
         "{\\begin{bmatrix}\\sin(x) & 2 \\\\[10pt] 3 & 4\\end{bmatrix}}^{\\alpha + 1}"},
        {"(x+1)^2", "\\left(x + 1\\right)^{2}"}
    };
    for (size_t i = 0; i < sizeof cases / sizeof *cases; ++i) {
        string_t *input = string_new_with(cases[i].input);
        TeX_t *notation = TeX_from_expression(input);
        TEST_ASSERT_TRUE(notation && strcmp(TeX_source(notation), cases[i].expected) == 0, cases[i].input);
        TeX_free(notation);
        string_free(input);
    }
    const char *invalid[] = {"(1,2;3)", "(1,2;3,4", "(1,2;3,4)^", "(1,2;3,4)^x+1",
                            "(1,2;3,4)^(x)+1", "(1,2;3,4)^x y", "(1,2;3,4)junk"};
    for (size_t i = 0; i < sizeof invalid / sizeof *invalid; ++i) {
        string_t *input = string_new_with(invalid[i]);
        TeX_t *notation = TeX_from_expression(input);
        TEST_ASSERT_TRUE(!notation, invalid[i]);
        TeX_free(notation);
        string_free(input);
    }
}

static void test_TeX_bound_problems(void)
{
    static const struct { const char *input; const char *expected; } cases[] = {
        {"{ (1, 2; 3, 4)^x | x = 3 }", "{\\begin{bmatrix}1 & 2 \\\\[10pt] 3 & 4\\end{bmatrix}}^{x}"},
        {"{(x,1;0,x)^x | x=3}", "{\\begin{bmatrix}x & 1 \\\\[10pt] 0 & x\\end{bmatrix}}^{x}"},
        {"{(1,2;3,4)^(x+1) | x=3}", "{\\begin{bmatrix}1 & 2 \\\\[10pt] 3 & 4\\end{bmatrix}}^{x + 1}"},
        {"{x | x=3}", "x"},
        {"{ y'' + 3y' + 40y | y'(0) = 1, y(0) = 0 }",
         "y'' + 3\\mkern-2mu y' + 40\\mkern-2mu y = 0,\\qquad "
         "y'\\left(0\\right) = 1,\\qquad y\\left(0\\right) = 0"},
        {"{ Dx(y) = x*y | x = ?;; y(0) = 1 }",
         "\\frac{d y}{d x} = x\\mkern-2mu y,\\qquad \\left. y\\right|_{x=0} = 1"},
        {"{ Dx(u)+Dy(u)=0 | x=?, y=?;; u(x,0)=sin(x) }",
         "\\frac{\\partial u}{\\partial x} + \\frac{\\partial u}{\\partial y} = 0,\\qquad "
         "\\left. u\\right|_{x=x,\\,y=0} = \\sin(x)"}
    };
    for (size_t i = 0; i < sizeof cases / sizeof *cases; ++i) {
        string_t *input = string_new_with(cases[i].input);
        TeX_t *notation = TeX_from_expression(input);
        TEST_ASSERT_TRUE(notation && strcmp(TeX_source(notation), cases[i].expected) == 0, cases[i].input);
        TeX_free(notation);
        string_free(input);
    }
    const char *invalid[] = {"{(1,2;3,4)^x | x=}", "{y''+y|y'(0)=1,y(0)=}",
                            "{(1,2;3,4)^x | x=3} trailing", "{(1,2;3,4)^x | x=3"};
    for (size_t i = 0; i < sizeof invalid / sizeof *invalid; ++i) {
        string_t *input = string_new_with(invalid[i]);
        TeX_t *notation = TeX_from_expression(input);
        TEST_ASSERT_TRUE(!notation, invalid[i]);
        TeX_free(notation);
        string_free(input);
    }
}

static void test_TeX_prime_input(void)
{
    static const struct { const char *input; const char *expected; } cases[] = {
        {"y''+y=0", "y'' + y = 0"},
        {"{y''''+y | y'''(0)=1, y(0)=0}",
         "y^{(4)} + y = 0,\\qquad y'''\\left(0\\right) = 1,\\qquad y\\left(0\\right) = 0"},
        {"{z''+z | z'(1)=2, z(1)=0}",
         "z'' + z = 0,\\qquad z'\\left(1\\right) = 2,\\qquad z\\left(1\\right) = 0"},
        {"{ y''+y=0 | t=?;; y'(0)=1 }", "y'' + y = 0,\\qquad y'\\left(0\\right) = 1"},
        {"{Dx(y)+y=0 | x=?;; y'(0)=1}",
         "\\frac{d y}{d x} + y = 0,\\qquad \\left. \\frac{d y}{d x}\\right|_{x=0} = 1"}
    };
    for (size_t i = 0; i < sizeof cases / sizeof *cases; ++i) {
        string_t *input = string_new_with(cases[i].input);
        TeX_t *notation = input ? TeX_from_expression(input) : NULL;
        TEST_ASSERT_TRUE(notation && !strcmp(TeX_source(notation), cases[i].expected), cases[i].input);
        TeX_free(notation);
        string_free(input);
    }
}

static void test_TeX_prime_snapshot_modes(void)
{
    string_t *input = string_new_with("Dxx(y)");
    expr_t *expression = expr_from_text_preserving_calculus(input, NULL);
    notation_build_t *build = notation_build_new();
    notation_build_derivatives(build, false);
    notation_build_dependent_variables(build, true);
    const TeX_model_node_t *fraction = notation_build_expr(build, expression);
    expr_notation_prime_derivatives(build, true);
    const TeX_model_node_t *prime = notation_build_expr(build, expression);
    expr_notation_prime_derivatives(build, false);
    const TeX_model_node_t *again = notation_build_expr(build, expression);
    TEST_ASSERT_TRUE(fraction && prime && fraction != prime && fraction == again,
                     "snapshot memoisation distinguishes prime and differential-fraction preferences");
    const TeX_model_node_t *rows[] = {fraction, prime};
    const TeX_model_node_t *root = notation_build_node(build,
        (TeX_node_view_t){.kind = TeX_NODE_LINES, .child_count = 2u}, rows);
    string_t *rendered = notation_build_render(build, root);
    TEST_ASSERT_TRUE(rendered && strstr(string_c_str(rendered), "\\frac") && strstr(string_c_str(rendered), "y''"),
                     "the same native derivative supports either presentation in one immutable snapshot");
    string_free(rendered);
    notation_build_free(build);
    expr_free(expression);
    string_free(input);
}

static void test_TeX_native_layout(void)
{
    TeX_t *x = TeX_raw("x"), *y = TeX_raw("y"), *two = TeX_raw("2");
    TeX_t *bindings = TeX_raw("x = 1");
    TeX_t *bound = TeX_compose_bound(x, bindings), *unbound = TeX_compose_bound(x, NULL);
    TeX_t *scaled = TeX_compose_scaled(two, x), *cartesian = TeX_compose_cartesian_sum(x, y);
    TeX_t *pair = TeX_compose_symmetric_pair(x, y, two, true);
    TeX_t *plain_pair = TeX_compose_symmetric_pair(x, y, NULL, false);
    TeX_free(bindings);
    TeX_free(two);
    TeX_free(y);
    TeX_free(x);
    TEST_ASSERT_TRUE(bound && !strcmp(TeX_source(bound), "\\left\\{ x \\;\\middle|\\; x = 1 \\right\\}"),
                     "bound notation owns copies of both parts");
    TEST_ASSERT_TRUE(unbound && !strcmp(TeX_source(unbound), "\\left\\{ x \\right\\}"), "optional binding bar");
    TEST_ASSERT_TRUE(scaled && !strcmp(TeX_source(scaled), "2\\mkern-5mu x"), "matrix-factor spacing");
    TEST_ASSERT_TRUE(cartesian && !strcmp(TeX_source(cartesian),
        "\\begin{aligned}&x \\\\[10pt] &{}+ i\\mkern-2mu y\\end{aligned}"), "complex-component alignment");
    TEST_ASSERT_TRUE(pair && !strcmp(TeX_source(pair), "\\frac{x \\pm \\left(y\\right)}{2}"), "symmetric quotient");
    TEST_ASSERT_TRUE(plain_pair && !strcmp(TeX_source(plain_pair), "x \\pm y"), "optional denominator and grouping");
    TEST_ASSERT_TRUE(!TeX_compose_bound(NULL, NULL) && !TeX_compose_scaled(NULL, scaled) && !TeX_compose_scaled(scaled, NULL) &&
                     !TeX_compose_cartesian_sum(NULL, cartesian) && !TeX_compose_cartesian_sum(cartesian, NULL) &&
                     !TeX_compose_symmetric_pair(NULL, pair, NULL, true) &&
                     !TeX_compose_symmetric_pair(pair, NULL, NULL, true), "layout constructors reject missing operands");
    TeX_free(plain_pair);
    TeX_free(pair);
    TeX_free(cartesian);
    TeX_free(scaled);
    TeX_free(unbound);
    TeX_free(bound);
}


static void test_TeX_native_text_array(void)
{
    static const struct { const char *input; const char *expected; } cases[] = {
        {"r_(n) = root(1+x_(n)^2,3)/sqrt(1+@lambda^2)",
         "r_{n} = \\frac{\\sqrt[3]{x_{n}^{2} + 1}}{\\sqrt{\\lambda^{2} + 1}}"},
        {"P(x) = 1/(1+x^2)*(1,x;x,x^2)",
         "P\\left(x\\right) = \\frac{1}{x^{2} + 1}\\mkern-5mu "
         "\\begin{bmatrix}1 & x \\\\[10pt] x & x^{2}\\end{bmatrix}"},
        {"i*h*Dt(@psi) = -h^2/(2*m)*Dxx(@psi)+V*@psi",
         "i\\mkern-2mu h\\mkern-2mu \\frac{\\partial}{\\partial t}\\left[\\psi\\right] = "
         "-\\frac{h^{2}}{2\\mkern-2mu m}\\mkern-2mu "
         "\\frac{\\partial^{2}}{\\partial x^{2}}\\left[\\psi\\right] + V\\mkern-2mu \\psi"},
        {"{x=y|x=3;y=4}", "x = y"},
        {"2*(1,2;3,4)", "2\\mkern-5mu \\begin{bmatrix}1 & 2 \\\\[10pt] 3 & 4\\end{bmatrix}"},
        {"(1,2;3,4)*2", "2\\mkern-5mu \\begin{bmatrix}1 & 2 \\\\[10pt] 3 & 4\\end{bmatrix}"},
        {"-2*(1,2)", "-2\\mkern-5mu \\begin{bmatrix}1 & 2\\end{bmatrix}"},
        {"(1,2)=(3,4)", "\\begin{bmatrix}1 & 2\\end{bmatrix} = \\begin{bmatrix}3 & 4\\end{bmatrix}"},
        {"r_n", "r_{n}"},
        {"x_(n+1)", "x_{n + 1}"}
    };
    for (size_t i = 0; i < sizeof cases / sizeof *cases; ++i) {
        string_t *input = string_new_with(cases[i].input);
        TeX_t *notation = TeX_from_expression(input);
        string_free(input);
        TEST_ASSERT_TRUE(notation && strcmp(TeX_source(notation), cases[i].expected) == 0, cases[i].input);
        TeX_free(notation);
    }
    const char *invalid[] = {"x=", "=x", "2*(1,2;3)", "(1,2)*", "x_()", "x_(n+1",
                            "1/(1+x^2)+2*(1,2)", "(1,2)*(3,4)"};
    for (size_t i = 0; i < sizeof invalid / sizeof *invalid; ++i) {
        string_t *input = string_new_with(invalid[i]);
        TeX_t *notation = TeX_from_expression(input);
        TEST_ASSERT_TRUE(!notation, invalid[i]);
        TeX_free(notation);
        string_free(input);
    }
}

static void test_TeX_native_index_roundtrip(void)
{
    const char *inputs[] = {"x_(n+1)^2", "r_n+x_(n)", "{x_(n)|n=3}", "@S_0^1 x^2 dx"};
    for (size_t i = 0; i < sizeof inputs / sizeof *inputs; ++i) {
        string_t *input = string_new_with(inputs[i]);
        expr_t *parsed = expr_from_text_preserving_calculus(input, NULL);
        string_t *native = parsed ? expr_to_text(parsed, style_EXPRESSION) : NULL;
        TeX_t *original = TeX_from_expression(input), *roundtrip = native ? TeX_from_expression(native) : NULL;
        TEST_ASSERT_TRUE(original && roundtrip && !strcmp(TeX_source(original), TeX_source(roundtrip)), inputs[i]);
        TeX_free(original);
        TeX_free(roundtrip);
        string_free(native);
        expr_free(parsed);
        string_free(input);
    }
}


typedef struct context_test_node context_test_node_t;
struct context_test_node {
    TeX_node_kind_t kind;
    const char *text;
    context_test_node_t *children[4];
    size_t count;
    size_t rows;
    size_t columns;
    unsigned visits;
    unsigned order;
    bool partial;
};

typedef struct {
    string_t *scratch;
    bool fail_child;
} context_test_state_t;

static bool context_test_describe(const void *context, const void *object, TeX_node_view_t *view)
{
    context_test_state_t *state = (context_test_state_t *)context;
    context_test_node_t *node = (context_test_node_t *)object;
    ++node->visits;
    string_free(state->scratch);
    state->scratch = node->text ? string_new_with(node->text) : NULL;
    *view = (TeX_node_view_t){.kind = node->kind, .child_count = node->count, .text = state->scratch,
                             .rows = node->rows, .columns = node->columns,
                             .order = node->order, .partial = node->partial};
    return !node->text || state->scratch != NULL;
}

static bool context_test_child(const void *context, const void *object, size_t index, TeX_node_ref_t *child);

static const TeX_provider_t context_test_provider = {
    .describe = context_test_describe,
    .child = context_test_child
};

static bool context_test_child(const void *context, const void *object, size_t index, TeX_node_ref_t *child)
{
    const context_test_state_t *state = context;
    const context_test_node_t *node = object;
    if (state->fail_child || index >= node->count || index >= 4u)
        return false;
    *child = (TeX_node_ref_t){.provider = &context_test_provider, .context = context, .node = node->children[index]};
    return true;
}

typedef struct {
    context_test_state_t state;
    context_test_node_t *terms[2];
    size_t count_calls;
    size_t term_calls;
    bool fail;
} sum_test_state_t;

static size_t sum_test_count(void *context)
{
    sum_test_state_t *state = context;
    ++state->count_calls;
    return 2u;
}

static TeX_context_t *sum_test_term(void *context, size_t index, int *sign)
{
    sum_test_state_t *state = context;
    ++state->term_calls;
    if (state->fail || index >= 2u)
        return NULL;
    *sign = index ? -1 : 1;
    return TeX_context_new((TeX_node_ref_t){&context_test_provider, &state->state, state->terms[index]});
}

static void test_TeX_sum_wrapping(void)
{
    context_test_state_t state = {0};
    context_test_node_t x = {.kind = TeX_NODE_IDENTIFIER, .text = "x"};
    context_test_node_t two = {.kind = TeX_NODE_NUMBER, .text = "2"};
    context_test_node_t difference = {.kind = TeX_NODE_DIFFERENCE, .children = {&x, &two}, .count = 2u};
    TeX_context_t *context = TeX_context_new((TeX_node_ref_t){&context_test_provider, &state, &difference});
    sum_test_state_t selection = {.terms = {&x, &two}};
    static const TeX_sum_provider_t provider = {.count = sum_test_count, .term = sum_test_term};
    TeX_t *notation = TeX_wrap_sum(context, 100u, &provider, &selection);
    TEST_ASSERT_TRUE(notation && !strcmp(TeX_source(notation), "x - 2") &&
        !selection.count_calls && !selection.term_calls, "short sums do not ask native modules to prepare terms");
    TeX_free(notation);
    notation = TeX_wrap_sum(context, 1u, &provider, &selection);
    TEST_ASSERT_TRUE(notation && !strcmp(TeX_source(notation),
        "\\begin{aligned}[t]\n&x \\\\\n&{} - 2\n\\end{aligned}") &&
        selection.count_calls == 1u && selection.term_calls == 2u,
        "native callbacks select signed terms while TeX owns their alignment");
    TeX_free(notation);
    selection.fail = true;
    notation = TeX_wrap_sum(context, 1u, &provider, &selection);
    TEST_ASSERT_TRUE(notation && !strcmp(TeX_source(notation), "x - 2"),
                     "an unavailable multiline alternative retains the complete ordinary body");
    TeX_free(notation);
    TeX_context_free(context);
    string_free(selection.state.scratch);
    string_free(state.scratch);
}

static void test_TeX_context_semantics(void)
{
    test_TeX_sum_wrapping();
    context_test_state_t state = {0};
    context_test_node_t x = {.kind = TeX_NODE_IDENTIFIER, .text = "x"};
    context_test_node_t two = {.kind = TeX_NODE_NUMBER, .text = "2"};
    context_test_node_t sum = {.kind = TeX_NODE_SUM, .children = {&x, &two}, .count = 2};
    context_test_node_t product = {.kind = TeX_NODE_PRODUCT, .children = {&sum, &sum}, .count = 2};
    TeX_node_ref_t root = {.provider = &context_test_provider, .context = &state, .node = &product};
    TeX_context_t *context = TeX_context_new(root);
    TeX_t *notation = TeX_from_context(context);
    TEST_ASSERT_TRUE(notation && !strcmp(TeX_source(notation),
        "\\left(x + 2\\right)\\mkern-2mu \\left(x + 2\\right)"), "semantic precedence supplies grouping");
    TEST_ASSERT_TRUE(x.visits == 1u && two.visits == 1u && sum.visits == 1u,
                     "shared graph children are queried once, using dictionary lookup");
    TeX_context_free(context);
    string_free(state.scratch);
    TEST_ASSERT_TRUE(notation && strstr(TeX_source(notation), "x + 2"), "notation outlives context and callback text");
    TeX_free(notation);

    context_test_state_t prime_state = {0};
    context_test_node_t prime = {.kind = TeX_NODE_PRIME, .children = {&x}, .count = 1u, .order = 1u};
    context = TeX_context_new((TeX_node_ref_t){&context_test_provider, &prime_state, &prime});
    notation = TeX_from_context(context);
    TEST_ASSERT_TRUE(notation && !strcmp(TeX_source(notation), "x'"), "first-order prime notation");
    TeX_free(notation);
    prime.order = 4u;
    notation = TeX_from_context(context);
    TEST_ASSERT_TRUE(notation && !strcmp(TeX_source(notation), "x^{(4)}"), "higher-order prime notation");
    TeX_free(notation);
    prime.order = 0u;
    notation = TeX_from_context(context);
    TEST_ASSERT_TRUE(!notation, "prime notation requires a positive order");
    TeX_free(notation);
    TeX_context_free(context);
    string_free(prime_state.scratch);

    context_test_state_t sign_state = {0};
    context_test_node_t negative = {.kind = TeX_NODE_NEGATIVE, .children = {&two}, .count = 1u};
    context_test_node_t difference = {.kind = TeX_NODE_SUM, .children = {&x, &negative}, .count = 2u};
    context = TeX_context_new((TeX_node_ref_t){&context_test_provider, &sign_state, &difference});
    notation = TeX_from_context(context);
    TEST_ASSERT_TRUE(notation && !strcmp(TeX_source(notation), "x - 2"), "negative summands render as subtraction");
    TeX_free(notation);
    TeX_context_free(context);
    string_free(sign_state.scratch);

    context_test_state_t integral_state = {0};
    context_test_node_t bold = {.kind = TeX_NODE_BOLD, .children = {&x}, .count = 1u};
    context_test_node_t calligraphic = {.kind = TeX_NODE_CALLIGRAPHIC, .children = {&x}, .count = 1u};
    context_test_node_t integral = {.kind = TeX_NODE_INTEGRAL_CONDITION,
                                    .children = {&calligraphic, &bold, &x}, .count = 3u};
    context = TeX_context_new((TeX_node_ref_t){&context_test_provider, &integral_state, &integral});
    notation = TeX_from_context(context);
    TEST_ASSERT_TRUE(notation && !strcmp(TeX_source(notation),
        "\\int_{\\boldsymbol{x}}\\mathcal{x}\\,\\mathrm{d}{x}"),
        "conditional integrals and mathematical emphasis render from semantic nodes");
    TeX_free(notation);
    TeX_context_free(context);
    string_free(integral_state.scratch);

    context_test_state_t factorial_state = {0};
    context_test_node_t factorial = {.kind = TeX_NODE_FACTORIAL, .children = {&sum}, .count = 1u};
    context = TeX_context_new((TeX_node_ref_t){&context_test_provider, &factorial_state, &factorial});
    notation = TeX_from_context(context);
    TEST_ASSERT_TRUE(notation && !strcmp(TeX_source(notation), "\\left(x + 2\\right)!"),
                     "factorial groups an additive operand");
    TeX_free(notation);
    TeX_context_free(context);
    string_free(factorial_state.scratch);
}

static void test_TeX_context_callback_lifetime(void)
{
    context_test_state_t state = {0};
    context_test_node_t x = {.kind = TeX_NODE_IDENTIFIER, .text = "x"};
    context_test_node_t call = {.kind = TeX_NODE_CALL, .text = "f", .children = {&x}, .count = 1};
    TeX_context_t *context = TeX_context_new((TeX_node_ref_t){&context_test_provider, &state, &call});
    TeX_t *notation = TeX_from_context(context);
    TEST_ASSERT_TRUE(notation && !strcmp(TeX_source(notation), "f\\left(x\\right)"),
                     "parent name survives a child callback replacing its scratch buffer");
    TeX_free(notation);
    call.kind = TeX_NODE_MATRIX;
    call.text = NULL;
    call.rows = 1u;
    call.columns = 2u;
    call.children[1] = &x;
    call.count = 2u;
    notation = TeX_from_context(context);
    TEST_ASSERT_TRUE(notation && !strcmp(TeX_source(notation), "\\begin{bmatrix}x & x\\end{bmatrix}"),
                     "contexts read the current graph and do not retain stale memoised values between calls");
    TeX_free(notation);
    TeX_context_free(context);
    string_free(state.scratch);
}

static void test_TeX_context_validation(void)
{
    context_test_state_t state = {0};
    context_test_node_t node = {.kind = TeX_NODE_NEGATIVE, .count = 1};
    node.children[0] = &node;
    TeX_context_t *context = TeX_context_new((TeX_node_ref_t){&context_test_provider, &state, &node});
    errno = 0;
    TeX_t *notation = TeX_from_context(context);
    TEST_ASSERT_TRUE(!notation && errno == ELOOP, "cycles fail without unbounded recursion");
    TeX_free(notation);
    node.kind = TeX_NODE_NUMBER;
    node.count = 0u;
    node.text = "1}\\input{bad}";
    notation = TeX_from_context(context);
    TEST_ASSERT_TRUE(!notation, "numeric text cannot inject typesetting commands");
    TeX_free(notation);
    node.kind = TeX_NODE_TEXT;
    node.text = "a & b";
    notation = TeX_from_context(context);
    TEST_ASSERT_TRUE(notation && !strcmp(TeX_source(notation), "\\text{a \\& b}"), "literal text is escaped");
    TeX_free(notation);
    node.kind = TeX_NODE_COUNT;
    notation = TeX_from_context(context);
    TEST_ASSERT_TRUE(!notation, "unknown node kinds are rejected before dispatch");
    TeX_free(notation);
    node.kind = TeX_NODE_NEGATIVE;
    node.count = 1u;
    state.fail_child = true;
    notation = TeX_from_context(context);
    TEST_ASSERT_TRUE(!notation, "child callback failure yields no partial result");
    TeX_free(notation);
    TeX_context_free(context);
    string_free(state.scratch);
    TEST_ASSERT_TRUE(!TeX_context_new((TeX_node_ref_t){0}) && !TeX_from_context(NULL), "invalid context arguments");
    TeX_context_free(NULL);
}


static void test_TeX_native_contexts(void)
{
    expr_t *argument = expr_new_named_var(NUM_ONE, "θ");
    expr_t *sine = expr_sin(argument);
    expr_t *signal = expr_delta(argument);
    TeX_context_t *sine_context = expr_TeX_context(sine);
    TeX_context_t *signal_context = expr_TeX_context(signal);
    TeX_node_ref_t call = TeX_context_root(sine_context), child = {0};
    TeX_node_view_t call_view = {0}, child_view = {0};
    TEST_ASSERT_TRUE(call.provider && call.provider->describe(call.context, call.node, &call_view) &&
        call_view.kind == TeX_NODE_CALL && call_view.text && !strcmp(string_c_str(call_view.text), "sin"),
        "semantic callbacks supply mathematical operation names without typesetting syntax");
    TEST_ASSERT_TRUE(call.provider && call.provider->child(call.context, call.node, 0u, &child) &&
        child.provider->describe(child.context, child.node, &child_view) && child_view.kind == TeX_NODE_IDENTIFIER &&
        child_view.text && !strcmp(string_c_str(child_view.text), "θ"),
        "semantic callbacks identify the Greek argument without exposing an expression graph");
    expr_free(signal);
    expr_free(sine);
    expr_free(argument);
    TeX_t *sine_notation = TeX_from_context(sine_context);
    TeX_t *signal_notation = TeX_from_context(signal_context);
    TEST_ASSERT_TRUE(sine_notation && strstr(TeX_source(sine_notation), "\\sin(\\theta)"),
                     "unary notation survives destruction of its native expression");
    TEST_ASSERT_TRUE(signal_notation && strstr(TeX_source(signal_notation), "\\delta"),
                     "the signal operation is supplied through the same semantic callbacks");
    TeX_free(signal_notation);
    TeX_free(sine_notation);
    TeX_context_free(signal_context);
    TeX_context_free(sine_context);

    expr_t *expression = expr_from_string("sqrt(x/2)", NULL);
    TeX_context_t *context = expr_TeX_context(expression);
    expr_free(expression);
    TeX_t *notation = TeX_from_context(context);
    TEST_ASSERT_TRUE(notation && !strcmp(TeX_source(notation), "\\sqrt{\\frac{x}{2}}"),
                     "expression snapshot owns its meaning after source destruction");
    TeX_free(notation);
    TeX_context_free(context);

    expr_t *eighth = expr_new_const(NUM_ONE_EIGHTH);
    expression = expr_exp(eighth);
    context = expr_TeX_context(expression);
    expr_free(expression);
    expr_free(eighth);
    notation = TeX_from_context(context);
    TEST_ASSERT_TRUE(notation && !strcmp(TeX_source(notation), "\\sqrt[8]{e}"),
                     "exact unit-fraction exponential uses its root form");
    TeX_free(notation);
    TeX_context_free(context);

    expression = expr_from_string("cl(n,x)", NULL);
    context = expr_TeX_context(expression);
    expr_free(expression);
    notation = TeX_from_context(context);
    TEST_ASSERT_TRUE(notation && !strcmp(TeX_source(notation), "\\operatorname{Cl}_{n}(x)"),
                     "Clausen order and argument retain their conventional notation");
    TeX_free(notation);
    TeX_context_free(context);

    expression = expr_new_named_var(NUM_ZERO, "time₁");
    context = expr_TeX_context(expression);
    expr_free(expression);
    notation = TeX_from_context(context);
    TEST_ASSERT_TRUE(notation && !strcmp(TeX_source(notation), "\\mathit{time_{1}}"),
                     "compound names preserve their Unicode subscript within the name");
    TeX_free(notation);
    TeX_context_free(context);

    expr_t *a = expr_new_named_const(NUM_ONE, "a");
    expr_t *b = expr_new_named_const(NUM_ONE, "b");
    expr_t *x = expr_new_named_var(NUM_ONE, "x");
    const expr_t *upper[] = {a}, *lower[] = {b};
    expression = expr_hypergeometric_pFq(1u, upper, 1u, lower, x);
    context = expr_TeX_context(expression);
    expr_free(expression);
    expr_free(x);
    expr_free(b);
    expr_free(a);
    notation = TeX_from_context(context);
    TEST_ASSERT_TRUE(notation && !strcmp(TeX_source(notation), "{}_{1}F_{1}\\left(a; b; x\\right)"),
                     "general hypergeometric parameter groups retain their conventional spacing");
    TeX_free(notation);
    TeX_context_free(context);

    expression = expr_from_string("@L(t)", NULL);
    context = expr_TeX_result_context(expression);
    expr_free(expression);
    notation = TeX_from_context(context);
    TEST_ASSERT_TRUE(notation && !strcmp(TeX_source(notation), "\\frac{1}{s^{2}}\\quad (\\mathfrak{Re}(s)>0)"),
                     "resolved transforms retain their convergence conditions");
    TeX_free(notation);
    TeX_context_free(context);

    number_t complex = num_create_from_string("1 + 2i");
    number_t exponent = num_create_from_long(6);
    expr_t *base = expr_new_const(complex);
    expression = expr_pow(base, &exponent);
    context = expr_TeX_context(expression);
    expr_free(expression);
    expr_free(base);
    num_destroy(&complex);
    num_destroy(&exponent);
    notation = TeX_from_context(context);
    TEST_ASSERT_TRUE(notation && !strcmp(TeX_source(notation), "\\left(1 + 2i\\right)^{6}"),
                     "complex literal power bases are grouped without changing their values");
    TeX_free(notation);
    TeX_context_free(context);

    x = expr_new_named_var(NUM_NAN, "x");
    expr_t *c = expr_new_named_const(NUM_ONE, "c");
    expression = expr_add(x, c);
    context = NULL;
    bool bindings_ok = expr_notation_bindings_context(expression, &context);
    expr_free(expression);
    expr_free(c);
    expr_free(x);
    notation = context ? TeX_from_context(context) : NULL;
    TEST_ASSERT_TRUE(bindings_ok && notation && !strcmp(TeX_source(notation), "x = ?; c = 1"),
                     "binding snapshots preserve unset variables and named constants after source destruction");
    TeX_free(notation);
    TeX_context_free(context);

    x = expr_new_named_var(NUM_ONE, "x");
    expr_t *integrand = expr_sin(x);
    expression = expr_integral(integrand, x);
    context = expr_notation_primitive_context(expression);
    expr_free(expression);
    expr_free(integrand);
    expr_free(x);
    notation = context ? TeX_from_context(context) : NULL;
    TEST_ASSERT_TRUE(notation && !strcmp(TeX_source(notation),
        "\\qquad F'(t)=\\sin(t)\\quad\\text{(F is the chosen antiderivative)}"),
        "antiderivative definition snapshots retain the integration variable and integrand");
    TeX_free(notation);
    TeX_context_free(context);

    x = expr_new_named_var(NUM_ONE, "x");
    expr_t *one = expr_new_const(NUM_ONE);
    expr_t *absolute = expr_abs(x);
    expr_t *reciprocal = expr_div(one, absolute);
    expression = expr_finite_part(reciprocal);
    context = expr_TeX_result_context(expression);
    expr_free(expression);
    expr_free(reciprocal);
    expr_free(absolute);
    expr_free(one);
    expr_free(x);
    notation = context ? TeX_from_context(context) : NULL;
    TEST_ASSERT_TRUE(notation && strstr(TeX_source(notation), "\\text{finite part}"),
                     "finite-part snapshot retains its qualification after source destruction");
    TeX_free(notation);
    TeX_context_free(context);

    x = expr_new_named_var(NUM_ONE, "x");
    expr_t *delta = expr_delta(x);
    expression = expr_add(x, delta);
    context = expr_TeX_result_context(expression);
    expr_free(expression);
    expr_free(delta);
    expr_free(x);
    notation = context ? TeX_from_context(context) : NULL;
    TEST_ASSERT_TRUE(notation && strstr(TeX_source(notation), "\\delta(x)"),
                     "nested delta keeps its semantic spelling after source destruction");
    TeX_free(notation);
    TeX_context_free(context);

    x = expr_new_named_var(NUM_ONE, "x");
    one = expr_new_const(NUM_ONE);
    absolute = expr_abs(x);
    reciprocal = expr_div(one, absolute);
    expr_t *finite = expr_finite_part(reciprocal);
    expression = expr_add(x, finite);
    context = expr_TeX_result_context(expression);
    expr_free(expression);
    expr_free(finite);
    expr_free(reciprocal);
    expr_free(absolute);
    expr_free(one);
    expr_free(x);
    notation = context ? TeX_from_context(context) : NULL;
    TEST_ASSERT_TRUE(notation && strstr(TeX_source(notation), "\\text{finite part}") &&
        strstr(TeX_source(notation), "\\quad ("),
        "a unique nested finite part retains its scoped qualification after source destruction");
    TeX_free(notation);
    TeX_context_free(context);

    x = expr_new_named_var(NUM_ONE, "x");
    one = expr_new_const(NUM_ONE);
    absolute = expr_abs(x);
    reciprocal = expr_div(one, absolute);
    finite = expr_finite_part(reciprocal);
    expr_t *principal = expr_principal_value(x);
    expression = expr_add(finite, principal);
    expr_t *domain = expr_from_string("{1/x | x=?; Re(x)>0}", NULL);
    expr_t *single_conditioned = domain ? expr_with_domain_of(finite, domain) : NULL;
    TeX_context_t *single_context = single_conditioned ? expr_TeX_result_context(single_conditioned) : NULL;
    expr_free(single_conditioned);
    expr_t *conditioned = domain ? expr_with_domain_of(expression, domain) : NULL;
    TeX_context_t *conditioned_context = conditioned ? expr_TeX_result_context(conditioned) : NULL;
    expr_free(conditioned);
    expr_free(domain);
    context = expr_TeX_result_context(expression);
    expr_free(expression);
    expr_free(principal);
    expr_free(finite);
    expr_free(reciprocal);
    expr_free(absolute);
    expr_free(one);
    expr_free(x);
    notation = context ? TeX_from_context(context) : NULL;
    TEST_ASSERT_TRUE(notation && strstr(TeX_source(notation), "\\text{finite part}") &&
        strstr(TeX_source(notation), "\\text{principal value}") && strstr(TeX_source(notation), "\\underbrace{"),
        "multiple distribution qualifications remain locally scoped in an independent snapshot");
    TeX_free(notation);
    TeX_context_free(context);
    notation = conditioned_context ? TeX_from_context(conditioned_context) : NULL;
    TEST_ASSERT_TRUE(notation && strstr(TeX_source(notation), "\\text{finite part}") &&
        strstr(TeX_source(notation), "\\text{principal value}") && strstr(TeX_source(notation), "\\mathfrak{Re}"),
        "multiple local qualifications retain their surrounding domain condition");
    TeX_free(notation);
    TeX_context_free(conditioned_context);
    notation = single_context ? TeX_from_context(single_context) : NULL;
    TEST_ASSERT_TRUE(notation && strstr(TeX_source(notation), "\\mathfrak{Re}(x)>0;\\;") &&
        strstr(TeX_source(notation), ":\\;\\text{finite part}"),
        "a detached finite-part qualification follows its domain condition");
    TeX_free(notation);
    TeX_context_free(single_context);

    matrix_t *matrix = mat_from_string("(1, 2; 3, 4)");
    context = mat_TeX_context(matrix);
    mat_free(matrix);
    notation = TeX_from_context(context);
    TEST_ASSERT_TRUE(notation && !strcmp(TeX_source(notation),
        "\\begin{bmatrix}1 & 2 \\\\[10pt] 3 & 4\\end{bmatrix}"),
        "matrix provider preserves dimensions and owns numeric entries");
    TeX_free(notation);
    TeX_context_free(context);

    equation_t *equation = equ_from_string("x = 2");
    context = equ_TeX_context(equation);
    equ_free(equation);
    notation = TeX_from_context(context);
    TEST_ASSERT_TRUE(notation && !strcmp(TeX_source(notation), "x = 2"), "equation snapshot retains both sides");
    TeX_free(notation);
    TeX_context_free(context);

    diffequ_t *problem = de_from_string("{ y' + y = 0 | y(0) = 1 }");
    context = de_TeX_context(problem);
    de_free(problem);
    notation = TeX_from_context(context);
    TEST_ASSERT_TRUE(notation && strstr(TeX_source(notation), "\\right|_{x=0} = 1"),
                     "differential-equation conditions retain their evaluation coordinates");
    TeX_free(notation);
    TeX_context_free(context);
    TEST_ASSERT_TRUE(!expr_TeX_context(NULL) && !mat_TeX_context(NULL) && !equ_TeX_context(NULL) &&
                     !de_TeX_context(NULL), "native factories reject absent input");
}

static void test_TeX_context_calculus(void)
{
    context_test_state_t state = {0};
    context_test_node_t body = {.kind = TeX_NODE_IDENTIFIER, .text = "ψ"};
    context_test_node_t x = {.kind = TeX_NODE_IDENTIFIER, .text = "x"};
    context_test_node_t y = {.kind = TeX_NODE_IDENTIFIER, .text = "y"};
    context_test_node_t two = {.kind = TeX_NODE_NUMBER, .text = "2"};
    context_test_node_t derivative = {.kind = TeX_NODE_DERIVATIVE, .children = {&body, &x, &y},
                                      .count = 3u, .order = 2u, .partial = true};
    context_test_node_t quotient = {.kind = TeX_NODE_FRACTION, .children = {&derivative, &two}, .count = 2u};
    TeX_context_t *context = TeX_context_new((TeX_node_ref_t){&context_test_provider, &state, &quotient});
    TeX_t *notation = TeX_from_context(context);
    TEST_ASSERT_TRUE(notation && strstr(TeX_source(notation),
        "\\frac{\\partial^{2}}{\\partial y\\,\\partial x}\\left[\\psi\\right]"),
        "ordered mixed derivatives and Greek symbols remain presentation data");
    TEST_ASSERT_TRUE(notation && !strstr(TeX_source(notation), "\\frac{\\frac") &&
        strstr(TeX_source(notation), "^{-1}"), "calculus remains outside enclosing algebraic fractions");
    TeX_free(notation);
    TeX_context_free(context);
    string_free(state.scratch);

}

static TeX_t *numeric_notation(number_t value)
{
    notation_build_t *build = notation_build_new();
    const TeX_model_node_t *root = notation_build_number(build, value);
    TeX_context_t *context = notation_build_context(build, root);
    notation_build_free(build);
    TeX_t *notation = TeX_from_context(context);
    TeX_context_free(context);
    return notation;
}

static void test_TeX_numeric_constant_lookups(void)
{
    static const struct { const number_t *value; const char *expected; } constants[] = {
        {&NUM_SQRT1ONPI,            "\\frac{1}{\\sqrt{\\pi}}"},
        {&NUM_2_SQRTPI,             "\\frac{2}{\\sqrt{\\pi}}"},
        {&NUM_NEG_TWO_OVER_SQRT_PI, "-\\frac{2}{\\sqrt{\\pi}}"},
        {&NUM_SQRT_PI,             "\\sqrt{\\pi}"},
        {&NUM_SQRT_2PI,            "\\sqrt{2\\mkern-2mu \\pi}"},
        {&NUM_INV_SQRT_2PI,         "\\frac{1}{\\sqrt{2\\mkern-2mu \\pi}}"},
        {&NUM_SQRT_PI_OVER_TWO,     "\\sqrt{\\frac{\\pi}{2}}"},
        {&NUM_SQRT2,               "\\sqrt{2}"},
        {&NUM_SQRT3,               "\\sqrt{3}"},
        {&NUM_SQRT_HALF,           "\\sqrt{\\frac{1}{2}}"},
        {&NUM_SQRT2_OVER_TWO,       NULL},
        {&NUM_SQRT3_OVER_TWO,       "\\frac{\\sqrt{3}}{2}"}
    };
    for (size_t i = 0u; i < sizeof constants / sizeof *constants; ++i) {
        const char *expected = constants[i].expected;
        if (!expected)
            expected = num_eq(NUM_SQRT_HALF, NUM_SQRT2_OVER_TWO) ? "\\sqrt{\\frac{1}{2}}" : "\\frac{\\sqrt{2}}{2}";
        number_t copy = num_const_prec(*constants[i].value, num_get_prec_bits(*constants[i].value));
        expr_t *expression = expr_new_const(num_clone(copy));
        TeX_t *literal = numeric_notation(copy);
        char *atom = expr_to_TeX_body(expression);
        TEST_ASSERT_TRUE(literal && !strcmp(TeX_source(literal), expected),
                         "known numeric constant retains its mathematical identity");
        if (!atom || strcmp(atom, expected))
            fprintf(stderr, "Constant %zu: expected %s, got %s\n", i, expected, atom ? atom : "(null)");
        TEST_ASSERT_TRUE(atom && !strcmp(atom, expected), "constant-expression output uses the same semantic lookup");
        free(atom);
        TeX_free(literal);
        expr_free(expression);
        num_destroy(&copy);

        const size_t precisions[] = {64u, 128u, 512u};
        for (size_t j = 0u; j < sizeof precisions / sizeof *precisions; ++j) {
            number_t retained = num_const_prec(*constants[i].value, precisions[j]);
            literal = numeric_notation(retained);
            TEST_ASSERT_TRUE(literal && !strcmp(TeX_source(literal), expected),
                             "constant identity supplies notation independently of numeric precision");
            TeX_free(literal);
            num_destroy(&retained);
        }
    }
    const double nonmembers[] = {-2.0, -1.125, 0.0, 1.25, 3.0, 4.0, NAN, INFINITY, -INFINITY,
                                 nextafter(num_to_double(NUM_SQRT2), INFINITY)};
    for (size_t i = 0u; i < sizeof nonmembers / sizeof *nonmembers; ++i) {
        number_t value = num_new();
        num_set_double(&value, nonmembers[i]);
        TeX_t *output = numeric_notation(value);
        TEST_ASSERT_TRUE(output && !strstr(TeX_source(output), "\\sqrt"),
                         "collisions, nearby values and non-finite numbers do not acquire a constant name");
        TeX_free(output);
        num_destroy(&value);
    }
}

static void test_TeX_compact_symbol_lookups(void)
{
    test_TeX_numeric_constant_lookups();
    for (size_t i = 0u; i < sizeof test_TeX_spellings / sizeof *test_TeX_spellings; ++i) {
        const test_TeX_spelling_t *fixture = &test_TeX_spellings[i];
        string_t *name = string_new_with(fixture->name);
        const char *operation = TeX_operation_name(fixture->name);
        const char *semantic = name ? TeX_semantic_function(name) : NULL;
        TEST_ASSERT_TRUE(name && ((fixture->uses & 1u) ? operation && !strcmp(operation, fixture->notation)
                                                     : operation == NULL), fixture->name);
        TEST_ASSERT_TRUE((fixture->uses & 2u) ? semantic && !strcmp(semantic, fixture->notation)
                                              : semantic == NULL, fixture->name);
        string_free(name);
    }
    TEST_ASSERT_TRUE(!TeX_operation_name(NULL) && !TeX_operation_name("Sin") &&
                     !TeX_semantic_function(NULL), "unknown operation names are rejected");
    string_t *embedded_nul = string_new();
    const char bytes[] = {'s', 'i', 'n', '\0', 'x'};
    TEST_ASSERT_TRUE(embedded_nul && !string_append_bytes(embedded_nul, bytes, sizeof bytes) &&
                     !TeX_semantic_function(embedded_nul), "embedded NUL does not match a known name");
    string_free(embedded_nul);
    for (unsigned codepoint = 0u; codepoint <= 0x0400u; ++codepoint) {
        const char *expected = codepoint >= 0x0391u && codepoint < 0x03F6u
            ? test_TeX_greek[codepoint - 0x0391u] : NULL;
        const char *actual = TeX_semantic_greek(codepoint);
        TEST_ASSERT_TRUE(expected ? actual && !strcmp(expected, actual) : actual == NULL,
                         "Greek spelling and non-member coverage");
    }
    TEST_ASSERT_TRUE(!TeX_semantic_greek(UINT32_MAX), "out-of-range Greek code point is rejected");
}

static void test_TeX_native_aggregate_contexts(void)
{
    static const struct { const char *input; const char *required; } cases[] = {
        {"@Z_(k=1)^n k^2", "\\sum_{k=1}^{n}"},
        {"@P_(k=1)^n k", "\\prod_{k=1}^{n}"},
        {"@L(f(t),t,s)", "\\mathcal{L}_{t\\to s}"},
        {"@F(f(t),t,w)", "\\mathcal{F}_{t\\to w}"},
        {"sin(x)", "\\sin(x)"},
        {"gamma(x)", "\\Gamma(x)"},
        {"f''(x)", "f''\\left(x\\right)"},
        {"{1/s^2 | s = ?; Re(s)>0}", "\\mathfrak{Re}(s) > 0"},
        {"LerchPhi(z,s,a)", "\\Phi\\left(z, s, a\\right)"},
        {"LommelS(mu,nu,x)", "s_{"},
        {"appell_f1(a,b1,b2,c,x,y)", "\\operatorname{F}_{1}\\left("},
        {"lauricella_f(2,a,b1,b2,c,x,y)", "F_D^{"},
        {"hypergeometric_pFq(1,1,a,b,x)", "{}_{1}F_{1}"},
        {"@S x^2 dx", "\\int x^{2}\\, dx"},
        {"x+x^2+1", "x^{2} + x + 1"}
    };
    for (size_t i = 0; i < sizeof cases / sizeof *cases; ++i) {
        string_t *input = string_new_with(cases[i].input);
        expr_t *expression = expr_from_text_preserving_calculus(input, NULL);
        TeX_context_t *context = expression ? expr_TeX_context(expression) : NULL;
        expr_free(expression);
        string_free(input);
        TeX_t *notation = context ? TeX_from_context(context) : NULL;
        if (!notation || !strstr(TeX_source(notation), cases[i].required))
            fprintf(stderr, "%s -> %s\n", cases[i].input, notation ? TeX_source(notation) : "no notation");
        TEST_ASSERT_TRUE(notation && strstr(TeX_source(notation), cases[i].required), cases[i].input);
        TeX_free(notation);
        TeX_context_free(context);
    }
}

int tests_main(void)
{
    run_TeX_builder_tests();
    TEST_RUN_IN_GROUP(test_TeX_escaping, tests, NULL);
    TEST_RUN_IN_GROUP(test_TeX_text_boundaries, tests, NULL);
    TEST_RUN_IN_GROUP(test_TeX_composition, tests, NULL);
    TEST_RUN_IN_GROUP(test_TeX_matrices_and_invalid_inputs, tests, NULL);
    TEST_RUN_IN_GROUP(test_TeX_calculus, tests, NULL);
    TEST_RUN_IN_GROUP(test_TeX_tensor_slots_and_fences, tests, NULL);
    TEST_RUN_IN_GROUP(test_TeX_calculus_invalid_inputs, tests, NULL);
    TEST_RUN_IN_GROUP(test_TeX_from_expression, tests, NULL);
    TEST_RUN_IN_GROUP(test_TeX_from_expression_invalid, tests, NULL);
    TEST_RUN_IN_GROUP(test_TeX_calculus_shorthand, tests, NULL);
    TEST_RUN_IN_GROUP(test_TeX_expression_style, tests, NULL);
    TEST_RUN_IN_GROUP(test_TeX_equations, tests, NULL);
    TEST_RUN_IN_GROUP(test_TeX_relation_input, tests, NULL);
    TEST_RUN_IN_GROUP(test_TeX_relation_input_invalid, tests, NULL);
    TEST_RUN_IN_GROUP(test_TeX_matrix_input, tests, NULL);
    TEST_RUN_IN_GROUP(test_TeX_bound_problems, tests, NULL);
    TEST_RUN_IN_GROUP(test_TeX_prime_input, tests, NULL);
    TEST_RUN_IN_GROUP(test_TeX_prime_snapshot_modes, tests, NULL);
    TEST_RUN_IN_GROUP(test_TeX_native_layout, tests, NULL);
    TEST_RUN_IN_GROUP(test_TeX_native_text_array, tests, NULL);
    TEST_RUN_IN_GROUP(test_TeX_native_index_roundtrip, tests, NULL);
    TEST_RUN_IN_GROUP(test_TeX_context_semantics, tests, NULL);
    TEST_RUN_IN_GROUP(test_TeX_context_callback_lifetime, tests, NULL);
    TEST_RUN_IN_GROUP(test_TeX_context_validation, tests, NULL);
    TEST_RUN_IN_GROUP(test_TeX_native_contexts, tests, NULL);
    TEST_RUN_IN_GROUP(test_TeX_context_calculus, tests, NULL);
    TEST_RUN_IN_GROUP(test_TeX_compact_symbol_lookups, tests, NULL);
    TEST_RUN_IN_GROUP(test_TeX_native_aggregate_contexts, tests, NULL);
    return TESTS_EXIT_CODE();
}
