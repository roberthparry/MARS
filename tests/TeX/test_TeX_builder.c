/**
 * @file test_TeX_builder.c
 * @brief Public reverse Polish notation, operand order and transactional failure regressions.
 *
 * Uses only TeX-t.h, exercising complete construction, stack-owned inputs,
 * persistent error handling, direct stack manipulation and optional calculus bounds.
 * The exact runnable Markdown programmes are checked after these ordinary tests.
 */
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>

#include "TeX-t.h"
#include "test_harness.h"
#include "test_TeX_builder.h"

static void finish_matches(TeX_builder_t *builder, const char *expected)
{
    TeX_t *result = TeX_builder_finish(builder);
    TEST_ASSERT_TRUE(result && !strcmp(TeX_source(result), expected), expected);
    TEST_ASSERT_TRUE(TeX_builder_count(builder) == 0, "finish transfers the only operand out of the stack");
    TeX_builder_free(builder);
    TEST_ASSERT_TRUE(result && !strcmp(TeX_source(result), expected), "finished notation outlives its builder");
    TeX_free(result);
}

static void test_TeX_builder_ownership(void)
{
    TeX_builder_t *builder = TeX_builder_new();
    TeX_push_symbol(builder, "x");
    TeX_push_integer(builder, 2);
    TeX_frac(builder);
    TeX_sqrt(builder);
    TeX_t *result = TeX_builder_finish(builder);
    string_t *source = result ? TeX_render(result) : NULL;
    TEST_ASSERT_TRUE(result && source && !strcmp(string_c_str(source), "\\sqrt{\\frac{x}{2}}"),
                     "RPN composition preserves numerator and denominator order");
    TeX_push_notation(builder, result);
    TeX_free(result);
    TEST_ASSERT_TRUE(source && !strcmp(string_c_str(source), "\\sqrt{\\frac{x}{2}}"),
                     "rendered text owns an independent copy");
    string_free(source);
    finish_matches(builder, "\\sqrt{\\frac{x}{2}}");

    builder = TeX_builder_new();
    TeX_sequence(builder, 0);
    finish_matches(builder, "");
    builder = TeX_builder_new();
    TeX_push_integer(builder, LONG_MIN);
    TEST_ASSERT_TRUE(TeX_builder_count(builder) == 1 && !TeX_builder_error(builder),
                     "the complete long range is accepted");
    TeX_builder_free(builder);
}

static void test_TeX_builder_stack(void)
{
    TeX_builder_t *builder = TeX_builder_new();
    TeX_push_symbol(builder, "a");
    const TeX_t *first = TeX_builder_peek(builder, 1);
    for (unsigned i = 0; i < 128; ++i)
        TEST_ASSERT_TRUE(TeX_dup(builder), "DUP survives operand-array growth");
    TEST_ASSERT_TRUE(TeX_builder_count(builder) == 129 && !strcmp(TeX_source(first), "a"),
                     "a borrowed value survives pushes and array relocation");
    TeX_builder_clear(builder);
    TeX_push_symbol(builder, "left");
    TeX_push_symbol(builder, "right");
    TeX_push_integer(builder, 2);
    TeX_roll(builder);
    TeX_frac(builder);
    finish_matches(builder, "\\frac{\\mathit{right}}{\\mathit{left}}");
}

static void test_TeX_builder_hp48_stack(void)
{
    TeX_builder_t *builder = TeX_builder_new();
    TeX_push_integer(builder, 12);
    TeX_push_integer(builder, 34);
    TeX_push_integer(builder, 56);
    const TeX_t *a = TeX_builder_peek(builder, 3), *b = TeX_builder_peek(builder, 2);
    const TeX_t *c = TeX_builder_peek(builder, 1);
    TEST_ASSERT_TRUE(TeX_rot(builder) && TeX_builder_count(builder) == 3 &&
                     TeX_builder_peek(builder, 3) == b && TeX_builder_peek(builder, 2) == c &&
                     TeX_builder_peek(builder, 1) == a, "HP48 ROT: 12 34 56 becomes 34 56 12 without copying");
    TEST_ASSERT_TRUE(TeX_rot(builder) && TeX_rot(builder) && TeX_builder_peek(builder, 3) == a &&
                     TeX_builder_peek(builder, 2) == b && TeX_builder_peek(builder, 1) == c,
                     "three rotations restore the original operand identities");
    TeX_builder_clear(builder);

    for (long n = 1; n <= 6; ++n) {
        const TeX_t *original[6];
        for (size_t i = 0; i < 6; ++i) {
            TeX_push_integer(builder, (long)(10 * (i + 1)));
            original[i] = TeX_builder_peek(builder, 1);
        }
        TeX_push_integer(builder, n);
        TEST_ASSERT_TRUE(TeX_roll(builder) && TeX_builder_count(builder) == 6,
                         "ROLL consumes its one-based level control");
        for (size_t level = 1; level <= 6; ++level) {
            size_t index = level == 1 ? 6 - (size_t)n : level <= (size_t)n ? 7 - level : 6 - level;
            TEST_ASSERT_TRUE(TeX_builder_peek(builder, level) == original[index],
                             "ROLL shifts only the selected pointer range and preserves lower levels");
        }
        TeX_builder_clear(builder);
    }

    TeX_push_integer(builder, 555);
    TeX_push_integer(builder, 444);
    TeX_push_integer(builder, 333);
    TeX_push_integer(builder, 222);
    TeX_push_integer(builder, 4);
    TEST_ASSERT_TRUE(TeX_roll(builder), "the HP48 manual's four-level ROLL succeeds");
    TeX_matrix(builder, 1, 4, TeX_MATRIX_PLAIN, 0);
    TeX_t *result = TeX_builder_finish(builder);
    TEST_ASSERT_TRUE(result && !strcmp(TeX_source(result), "\\begin{matrix}444 & 333 & 222 & 555\\end{matrix}"),
                     "HP48 ROLL: 555 444 333 222 4 becomes 444 333 222 555");
    TeX_free(result);

    TeX_push_symbol(builder, "a");
    TeX_push_integer(builder, 1);
    const TeX_t *control = TeX_builder_peek(builder, 1);
    TEST_ASSERT_TRUE(TeX_dup(builder) && TeX_builder_peek(builder, 1) != control &&
                     TeX_roll(builder) && TeX_builder_peek(builder, 1) == control,
                     "DUP copies literal controls and ROLL 1 leaves the data untouched");
    TeX_builder_clear(builder);
    TeX_push_integer(builder, 2);
    result = TeX_builder_finish(builder);
    TeX_push_symbol(builder, "a");
    TeX_push_symbol(builder, "b");
    TeX_push_notation(builder, result);
    TeX_free(result);
    TEST_ASSERT_TRUE(TeX_roll(builder), "an independently copied integer remains a valid ROLL control");
    TeX_frac(builder);
    finish_matches(builder, "\\frac{b}{a}");
}

static void test_TeX_builder_stack_errors(void)
{
    TeX_builder_t *builder = TeX_builder_new();
    TEST_ASSERT_TRUE(!TeX_dup(builder) && TeX_builder_error(builder) == ERANGE,
                     "DUP rejects an empty stack without creating an operand");
    TeX_builder_clear_error(builder);
    TEST_ASSERT_TRUE(!TeX_roll(builder) && TeX_builder_error(builder) == ERANGE,
                     "ROLL rejects a missing control");
    TeX_builder_clear(builder);
    TeX_push_symbol(builder, "a");
    TeX_push_symbol(builder, "b");
    const TeX_t *a = TeX_builder_peek(builder, 2), *b = TeX_builder_peek(builder, 1);
    TEST_ASSERT_TRUE(!TeX_rot(builder) && TeX_builder_error(builder) == ERANGE &&
                     TeX_builder_count(builder) == 2 && TeX_builder_peek(builder, 2) == a &&
                     TeX_builder_peek(builder, 1) == b, "ROT underflow preserves both original pointers");
    TEST_ASSERT_TRUE(!TeX_dup(builder) && !TeX_roll(builder) && TeX_builder_error(builder) == ERANGE,
                     "stack operations honour the first saved error");
    TeX_builder_clear_error(builder);
    TEST_ASSERT_TRUE(!TeX_builder_peek(builder, 0) && errno == ERANGE && !TeX_builder_error(builder) &&
                     !TeX_builder_peek(builder, SIZE_MAX) && errno == ERANGE && !TeX_builder_error(builder),
                     "invalid inspection levels do not poison the builder");
    TeX_builder_clear(builder);

    const long invalid[] = {0, -1, LONG_MIN, LONG_MAX, 4};
    for (size_t i = 0; i < sizeof invalid / sizeof *invalid; ++i) {
        TeX_push_symbol(builder, "a");
        TeX_push_symbol(builder, "b");
        TeX_push_integer(builder, invalid[i]);
        a = TeX_builder_peek(builder, 3);
        b = TeX_builder_peek(builder, 2);
        const TeX_t *control = TeX_builder_peek(builder, 1);
        TEST_ASSERT_TRUE(!TeX_roll(builder) && TeX_builder_error(builder) == ERANGE &&
                         TeX_builder_count(builder) == 3 && TeX_builder_peek(builder, 3) == a &&
                         TeX_builder_peek(builder, 2) == b && TeX_builder_peek(builder, 1) == control,
                         "invalid ROLL levels preserve the control and every data operand");
        TeX_builder_clear(builder);
    }

    TeX_push_integer(builder, 1);
    const TeX_t *control = TeX_builder_peek(builder, 1);
    TEST_ASSERT_TRUE(!TeX_roll(builder) && TeX_builder_error(builder) == ERANGE &&
                     TeX_builder_count(builder) == 1 && TeX_builder_peek(builder, 1) == control,
                     "a level control alone supplies no data to roll");
    TeX_builder_clear(builder);

    TeX_push_symbol(builder, "a");
    TeX_push_raw(builder, "1");
    TEST_ASSERT_TRUE(!TeX_roll(builder) && TeX_builder_error(builder) == EINVAL &&
                     TeX_builder_count(builder) == 2, "ROLL never parses raw TeX to discover a level");
    TeX_builder_clear(builder);
    TeX_push_symbol(builder, "a");
    TeX_push_integer(builder, 1);
    TeX_push_integer(builder, 1);
    TeX_add(builder);
    TEST_ASSERT_TRUE(!TeX_roll(builder) && TeX_builder_error(builder) == EINVAL &&
                     TeX_builder_count(builder) == 2, "ROLL does not evaluate arithmetic notation as a control");
    TeX_builder_free(builder);
}

static void test_TeX_builder_transactional_errors(void)
{
    TeX_builder_t *builder = TeX_builder_new();
    TeX_push_symbol(builder, "numerator");
    const TeX_t *saved = TeX_builder_peek(builder, 1);
    TEST_ASSERT_TRUE(!TeX_frac(builder) && TeX_builder_error(builder) == ERANGE &&
                     TeX_builder_count(builder) == 1 && TeX_builder_peek(builder, 1) == saved,
                     "underflow preserves the exact original operand");
    TEST_ASSERT_TRUE(!TeX_push_integer(builder, 2) && !TeX_sqrt(builder) && !TeX_builder_finish(builder) &&
                     TeX_builder_error(builder) == ERANGE && TeX_builder_count(builder) == 1,
                     "ignored errors cannot produce an incomplete formula");
    TeX_builder_clear_error(builder);
    TeX_push_integer(builder, 2);
    TEST_ASSERT_TRUE(!TeX_builder_finish(builder) && TeX_builder_count(builder) == 2 &&
                     TeX_builder_error(builder) == EINVAL, "finish rejects leftover operands without discarding them");
    TeX_builder_clear_error(builder);
    TeX_frac(builder);
    finish_matches(builder, "\\frac{\\mathit{numerator}}{2}");

    builder = TeX_builder_new();
    TeX_push_symbol(builder, "x");
    TeX_push_symbol(builder, "y");
    saved = TeX_builder_peek(builder, 1);
    TEST_ASSERT_TRUE(!TeX_equation(builder, (TeX_equation_style_t)-1) &&
                     TeX_builder_peek(builder, 1) == saved && TeX_builder_count(builder) == 2,
                     "invalid presentation parameters preserve the previous stack");
    TeX_builder_clear(builder);
    TEST_ASSERT_TRUE(!TeX_builder_error(builder) && TeX_builder_count(builder) == 0,
                     "clear releases unfinished operands and resets the error");
    TEST_ASSERT_TRUE(!TeX_matrix(builder, SIZE_MAX, 2, TeX_MATRIX_PLAIN, 0) &&
                     TeX_builder_error(builder) == EOVERFLOW, "matrix multiplication overflow precedes cell access");
    TeX_builder_clear(builder);
    TEST_ASSERT_TRUE(!TeX_tensor(builder, NULL, SIZE_MAX) && TeX_builder_error(builder) == EOVERFLOW,
                     "tensor arity overflow precedes slot access");
    TeX_builder_free(builder);
    TEST_ASSERT_TRUE(!TeX_push_raw(NULL, "x") && !TeX_builder_finish(NULL) && !TeX_dup(NULL) &&
                     !TeX_rot(NULL) && !TeX_roll(NULL) && !TeX_builder_peek(NULL, 1) &&
                     TeX_builder_error(NULL) == EINVAL,
                     "NULL builders are rejected safely");
    TeX_builder_free(NULL);
    TeX_builder_clear(NULL);
    TeX_builder_clear_error(NULL);
}

static void test_TeX_builder_inputs(void)
{
    TeX_builder_t *builder = TeX_builder_new();
    string_t *text = string_new_with("scaled & normalised");
    TeX_push_text(builder, text);
    string_free(text);
    finish_matches(builder, "\\text{scaled \\& normalised}");
    builder = TeX_builder_new();
    text = string_new_with("sqrt(x/2)");
    TeX_push_expression(builder, text);
    string_free(text);
    finish_matches(builder, "\\sqrt{\\frac{x}{2}}");
    builder = TeX_builder_new();
    TeX_push_math(builder, "sqrt(x/2)");
    finish_matches(builder, "\\sqrt{\\frac{x}{2}}");
    builder = TeX_builder_new();
    TeX_push_symbol(builder, "safe");
    text = string_new_with("a");
    const char tail[] = {'\0', 'b'};
    string_append_bytes(text, tail, sizeof tail);
    TEST_ASSERT_TRUE(!TeX_push_text(builder, text) && TeX_builder_error(builder) == EINVAL &&
                     TeX_builder_count(builder) == 1, "embedded NUL is rejected without truncation or stack loss");
    string_free(text);
    TeX_builder_clear(builder);
    TEST_ASSERT_TRUE(!TeX_push_math(builder, NULL) && TeX_builder_error(builder) == EINVAL,
                     "NULL mathematical strings are rejected");
    TeX_builder_free(builder);
}

static void test_TeX_builder_layout(void)
{
    TeX_builder_t *builder = TeX_builder_new();
    TeX_push_symbol(builder, "r");
    TeX_push_symbol(builder, "n");
    TeX_push_integer(builder, 2);
    TeX_scripts(builder);
    TeX_fence(builder, TeX_FENCE_BRACKETS);
    TeX_push_integer(builder, 3);
    TeX_root(builder);
    finish_matches(builder, "\\sqrt[3]{\\left[ {r}_{n}^{2} \\right]}");
    builder = TeX_builder_new();
    TeX_push_symbol(builder, "x");
    TeX_function(builder, "P", 1);
    TeX_push_integer(builder, 1);
    TeX_push_symbol(builder, "x");
    TeX_dup(builder);
    TeX_dup(builder);
    TeX_push_integer(builder, 2);
    TeX_pow(builder);
    TeX_matrix(builder, 2, 2, TeX_MATRIX_BRACKETS, 6);
    TeX_equation(builder, TeX_EQUATION_INLINE);
    finish_matches(builder, "P\\left(x\\right) = \\begin{bmatrix}1 & x \\\\[6pt] x & {x}^{2}\\end{bmatrix}");
    builder = TeX_builder_new();
    TeX_push_symbol(builder, "a");
    TeX_push_symbol(builder, "b");
    TeX_mul(builder);
    TeX_push_symbol(builder, "c");
    TeX_mul(builder);
    TeX_push_space(builder, TeX_SPACE_QUAD);
    string_t *caption = string_new_with("caption");
    TeX_push_text(builder, caption);
    string_free(caption);
    TeX_sequence(builder, 3);
    finish_matches(builder, "a\\,b\\,c\\qquad \\text{caption}");

    builder = TeX_builder_new();
    TeX_push_symbol(builder, "x");
    TeX_push_symbol(builder, "x");
    TeX_push_integer(builder, 1);
    TeX_equation(builder, TeX_EQUATION_INLINE);
    TeX_bound(builder, true);
    finish_matches(builder, "\\left\\{ x \\;\\middle|\\; x = 1 \\right\\}");
    builder = TeX_builder_new();
    TeX_push_integer(builder, 2);
    TeX_push_symbol(builder, "x");
    TeX_scaled(builder);
    finish_matches(builder, "2\\mkern-5mu x");
    builder = TeX_builder_new();
    TeX_push_symbol(builder, "x");
    TeX_push_symbol(builder, "y");
    TeX_cartesian_sum(builder);
    finish_matches(builder, "\\begin{aligned}&x \\\\[10pt] &{}+ i\\mkern-2mu y\\end{aligned}");
    builder = TeX_builder_new();
    TeX_push_symbol(builder, "x");
    TeX_push_symbol(builder, "y");
    TeX_push_integer(builder, 2);
    TeX_symmetric_pair(builder, true, true);
    finish_matches(builder, "\\frac{x \\pm \\left(y\\right)}{2}");
}

static void test_TeX_builder_calculus_and_tensors(void)
{
    const char *sums[] = {"\\sum {f}", "\\sum_{0} {f}", "\\sum^{1} {f}", "\\sum_{0}^{1} {f}"};
    const char *integrals[] = {"\\int {f}\\,\\mathrm{d}{x}", "\\int_{0} {f}\\,\\mathrm{d}{x}",
        "\\int^{1} {f}\\,\\mathrm{d}{x}", "\\int_{0}^{1} {f}\\,\\mathrm{d}{x}"};
    for (unsigned limits = 0; limits <= TeX_LIMIT_BOTH; ++limits) {
        TeX_builder_t *builder = TeX_builder_new();
        TeX_push_symbol(builder, "f");
        if (limits & TeX_LIMIT_LOWER) TeX_push_integer(builder, 0);
        if (limits & TeX_LIMIT_UPPER) TeX_push_integer(builder, 1);
        TeX_sum(builder, (TeX_limits_t)limits);
        finish_matches(builder, sums[limits]);
        builder = TeX_builder_new();
        TeX_push_symbol(builder, "f");
        TeX_push_symbol(builder, "x");
        if (limits & TeX_LIMIT_LOWER) TeX_push_integer(builder, 0);
        if (limits & TeX_LIMIT_UPPER) TeX_push_integer(builder, 1);
        TeX_integral(builder, (TeX_limits_t)limits);
        finish_matches(builder, integrals[limits]);
    }
    TeX_builder_t *builder = TeX_builder_new();
    TeX_push_symbol(builder, "f");
    TeX_push_symbol(builder, "x");
    TeX_derivative(builder, 2, TeX_DERIVATIVE_PARTIAL);
    finish_matches(builder, "\\frac{\\partial^{2} {f}}{\\partial {x}^{2}}");
    builder = TeX_builder_new();
    TeX_push_symbol(builder, "T");
    TeX_push_symbol(builder, "μ");
    TeX_push_symbol(builder, "ν");
    const TeX_index_position_t positions[] = {TeX_INDEX_UPPER, TeX_INDEX_LOWER};
    TeX_tensor(builder, positions, 2);
    finish_matches(builder, "{T}^{\\mu}{}_{\\nu}");
    builder = TeX_builder_new();
    TeX_push_symbol(builder, "x");
    TEST_ASSERT_TRUE(!TeX_sum(builder, (TeX_limits_t)4) && TeX_builder_error(builder) == EINVAL &&
                     TeX_builder_count(builder) == 1, "invalid bounds cannot reinterpret operands");
    TeX_builder_free(builder);
}

static void test_TeX_builder_structured_arithmetic(void)
{
    TeX_builder_t *b = TeX_builder_new();
    TeX_push_integer(b, 1);
    TeX_push_symbol(b, "x");
    TeX_push_integer(b, 2);
    TeX_pow(b);
    TeX_add(b);
    finish_matches(b, "1 + {x}^{2}");

    b = TeX_builder_new();
    TeX_push_symbol(b, "a");
    TeX_push_symbol(b, "b");
    TeX_push_symbol(b, "c");
    TeX_sub(b);
    TeX_sub(b);
    finish_matches(b, "a - \\left( b - c \\right)");

    b = TeX_builder_new();
    TeX_push_symbol(b, "x");
    TeX_push_symbol(b, "y");
    TeX_add(b);
    TeX_dup(b);
    TeX_mul(b);
    finish_matches(b, "\\left( x + y \\right)\\,\\left( x + y \\right)");

    b = TeX_builder_new();
    TeX_push_symbol(b, "x");
    TeX_push_symbol(b, "y");
    TeX_add(b);
    TeX_push_integer(b, 2);
    TeX_pow(b);
    finish_matches(b, "{\\left( x + y \\right)}^{2}");

    b = TeX_builder_new();
    TeX_push_math(b, "x+y");
    TeX_neg(b);
    finish_matches(b, "-\\left( x + y \\right)");

    b = TeX_builder_new();
    TeX_push_integer(b, 2);
    TeX_push_integer(b, 3);
    TeX_mul(b);
    TeX_push_integer(b, 4);
    TeX_mul(b);
    finish_matches(b, "2 \\times 3 \\times 4");

    b = TeX_builder_new();
    TeX_push_integer(b, -2);
    TeX_push_integer(b, 2);
    TeX_pow(b);
    finish_matches(b, "{\\left( -2 \\right)}^{2}");

    b = TeX_builder_new();
    TeX_push_symbol(b, "x");
    TeX_function(b, "sin", 1);
    finish_matches(b, "\\sin(x)");
    b = TeX_builder_new();
    TeX_push_symbol(b, "f");
    TeX_push_symbol(b, "n");
    TeX_subscript(b);
    TeX_push_symbol(b, "x");
    TeX_call(b, 1);
    finish_matches(b, "{f}_{n}\\left(x\\right)");

    b = TeX_builder_new();
    TeX_push_symbol(b, "ℏ");
    TeX_push_symbol(b, "∞");
    TeX_neg(b);
    TeX_equation(b, TeX_EQUATION_INLINE);
    finish_matches(b, "\\hbar = -\\infty");
}

static void test_TeX_builder_ascii_symbols(void)
{
    static const struct { const char *name; unsigned codepoint; } aliases[] = {
        {"alpha"       , 0x3b1u},
        {"Alpha"       , 0x391u},
        {"beta"        , 0x3b2u},
        {"Beta"        , 0x392u},
        {"gamma"       , 0x3b3u},
        {"Gamma"       , 0x393u},
        {"delta"       , 0x3b4u},
        {"Delta"       , 0x394u},
        {"epsilon"     , 0x3b5u},
        {"Epsilon"     , 0x395u},
        {"zeta"        , 0x3b6u},
        {"Zeta"        , 0x396u},
        {"eta"         , 0x3b7u},
        {"Eta"         , 0x397u},
        {"theta"       , 0x3b8u},
        {"Theta"       , 0x398u},
        {"iota"        , 0x3b9u},
        {"Iota"        , 0x399u},
        {"kappa"       , 0x3bau},
        {"Kappa"       , 0x39au},
        {"lambda"      , 0x3bbu},
        {"Lambda"      , 0x39bu},
        {"mu"          , 0x3bcu},
        {"Mu"          , 0x39cu},
        {"nu"          , 0x3bdu},
        {"Nu"          , 0x39du},
        {"xi"          , 0x3beu},
        {"Xi"          , 0x39eu},
        {"omicron"     , 0x3bfu},
        {"Omicron"     , 0x39fu},
        {"pi"          , 0x3c0u},
        {"Pi"          , 0x3a0u},
        {"rho"         , 0x3c1u},
        {"Rho"         , 0x3a1u},
        {"sigma"       , 0x3c3u},
        {"Sigma"       , 0x3a3u},
        {"tau"         , 0x3c4u},
        {"Tau"         , 0x3a4u},
        {"upsilon"     , 0x3c5u},
        {"Upsilon"     , 0x3a5u},
        {"phi"         , 0x3c6u},
        {"Phi"         , 0x3a6u},
        {"chi"         , 0x3c7u},
        {"Chi"         , 0x3a7u},
        {"psi"         , 0x3c8u},
        {"Psi"         , 0x3a8u},
        {"omega"       , 0x3c9u},
        {"Omega"       , 0x3a9u},
        {"vartheta"    , 0x3d1u},
        {"varphi"      , 0x3d5u},
        {"varkappa"    , 0x3f0u},
        {"varrho"      , 0x3f1u},
        {"varpi"       , 0x3d6u},
        {"varepsilon"  , 0x3f5u},
        {"varsigma"    , 0x3c2u},
        {"hbar"        , 0x210fu},
        {"infinity"    , 0x221eu},
        {"infty"       , 0x221eu},
        {"partial"     , 0x2202u},
    };
    TeX_builder_t *builder = TeX_builder_new();
    for (size_t i = 0; i < sizeof aliases / sizeof *aliases; ++i) {
        string_t *unicode = rune_to_string(rune_from_value(aliases[i].codepoint));
        TEST_ASSERT_TRUE(unicode && TeX_push_symbol(builder, aliases[i].name) &&
                         TeX_push_symbol(builder, string_c_str(unicode)), aliases[i].name);
        string_free(unicode);
        TEST_ASSERT_TRUE(!strcmp(TeX_source(TeX_builder_peek(builder, 1)),
                                 TeX_source(TeX_builder_peek(builder, 2))),
                         "keyboard-friendly spelling matches its independently supplied Unicode symbol");
        TeX_builder_clear(builder);
    }
    TeX_push_symbol(builder, "x");
    TeX_function(builder, "psi", 1);
    finish_matches(builder, "\\psi\\left(x\\right)");
    builder = TeX_builder_new();
    TeX_push_symbol(builder, "x");
    TeX_function(builder, "gamma", 1);
    finish_matches(builder, "\\Gamma(x)");
    builder = TeX_builder_new();
    TeX_push_symbol(builder, "psi_extra");
    finish_matches(builder, "\\mathit{psi\\_extra}");
}

static void test_TeX_builder_rejects_operator_symbols(void)
{
    TeX_builder_t *b = TeX_builder_new();
    TeX_push_symbol(b, "x");
    const TeX_t *saved = TeX_builder_peek(b, 1);
    TEST_ASSERT_TRUE(!TeX_push_symbol(b, "1 + ") && TeX_builder_error(b) == EINVAL &&
                     TeX_builder_count(b) == 1 && TeX_builder_peek(b, 1) == saved,
                     "symbols cannot smuggle in an arithmetic operator fragment");
    TeX_builder_clear_error(b);
    TEST_ASSERT_TRUE(!TeX_push_symbol(b, "\\lambda") && TeX_builder_error(b) == EINVAL &&
                     TeX_builder_peek(b, 1) == saved, "symbols reject TeX commands without consuming operands");
    TeX_builder_clear_error(b);
    TEST_ASSERT_TRUE(!TeX_function(b, "a+b", 1) && TeX_builder_error(b) == EINVAL &&
                     TeX_builder_peek(b, 1) == saved, "function names reject operator fragments transactionally");
    TeX_builder_clear_error(b);
    TEST_ASSERT_TRUE(!TeX_call(b, SIZE_MAX) && TeX_builder_error(b) == EOVERFLOW &&
                     TeX_builder_count(b) == 1, "call arity overflow is rejected before operand access");
    TeX_builder_free(b);
}

/* Register these public-API cases through the TeX suite's shared harness. */
void run_TeX_builder_tests(void)
{
    TEST_RUN_IN_GROUP(test_TeX_builder_ownership, tests, NULL);
    TEST_RUN_IN_GROUP(test_TeX_builder_stack, tests, NULL);
    TEST_RUN_IN_GROUP(test_TeX_builder_hp48_stack, tests, NULL);
    TEST_RUN_IN_GROUP(test_TeX_builder_stack_errors, tests, NULL);
    TEST_RUN_IN_GROUP(test_TeX_builder_transactional_errors, tests, NULL);
    TEST_RUN_IN_GROUP(test_TeX_builder_inputs, tests, NULL);
    TEST_RUN_IN_GROUP(test_TeX_builder_layout, tests, NULL);
    TEST_RUN_IN_GROUP(test_TeX_builder_calculus_and_tensors, tests, NULL);
    TEST_RUN_IN_GROUP(test_TeX_builder_structured_arithmetic, tests, NULL);
    TEST_RUN_IN_GROUP(test_TeX_builder_rejects_operator_symbols, tests, NULL);
    TEST_RUN_IN_GROUP(test_TeX_builder_ascii_symbols, tests, NULL);
}
