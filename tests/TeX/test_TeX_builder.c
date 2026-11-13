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

#include "string-t.h"
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
    TeX_symbol(builder, "x");
    TeX_integer(builder, 2);
    TeX_frac(builder);
    TeX_sqrt(builder);
    TeX_t *result = TeX_builder_finish(builder);
    string_t *source = result ? TeX_render(result) : NULL;
    TEST_ASSERT_TRUE(result && source && !strcmp(string_c_str(source), "\\sqrt{\\frac{x}{2}}"),
                     "RPN composition preserves numerator and denominator order");
    TeX_notation(builder, result);
    TeX_free(result);
    TEST_ASSERT_TRUE(source && !strcmp(string_c_str(source), "\\sqrt{\\frac{x}{2}}"),
                     "rendered text owns an independent copy");
    string_free(source);
    finish_matches(builder, "\\sqrt{\\frac{x}{2}}");

    builder = TeX_builder_new();
    TeX_integer(builder, 0);
    TeX_list(builder);
    TeX_sequence(builder);
    finish_matches(builder, "");
    builder = TeX_builder_new();
    TeX_integer(builder, LONG_MIN);
    TEST_ASSERT_TRUE(TeX_builder_count(builder) == 1 && !TeX_builder_error(builder),
                     "the complete long range is accepted");
    TeX_builder_free(builder);
}

static void test_TeX_builder_stack(void)
{
    TeX_builder_t *builder = TeX_builder_new();
    TeX_symbol(builder, "a");
    const TeX_t *first = TeX_builder_peek(builder, 1);
    for (unsigned i = 0; i < 128; ++i)
        TEST_ASSERT_TRUE(TeX_dup(builder), "DUP survives operand-array growth");
    TEST_ASSERT_TRUE(TeX_builder_count(builder) == 129 && !strcmp(TeX_source(first), "a"),
                     "a borrowed value survives pushes and array relocation");
    TeX_builder_clear(builder);
    TeX_symbol(builder, "left");
    TeX_symbol(builder, "right");
    TeX_integer(builder, 2);
    TeX_roll(builder);
    TeX_frac(builder);
    finish_matches(builder, "\\frac{\\mathit{right}}{\\mathit{left}}");
}

static void test_TeX_builder_drop_swap(void)
{
    TeX_builder_t *builder = TeX_builder_new();
    TeX_integer(builder, 12);
    TeX_integer(builder, 34);
    TeX_integer(builder, 56);
    const TeX_t *a = TeX_builder_peek(builder, 3), *b = TeX_builder_peek(builder, 2);
    const TeX_t *c = TeX_builder_peek(builder, 1);
    TEST_ASSERT_TRUE(TeX_swap(builder) && TeX_builder_count(builder) == 3 &&
                     TeX_builder_peek(builder, 3) == a && TeX_builder_peek(builder, 2) == c &&
                     TeX_builder_peek(builder, 1) == b, "SWAP preserves operand identities and the lower stack");
    TEST_ASSERT_TRUE(TeX_swap(builder) && TeX_builder_peek(builder, 2) == b && TeX_builder_peek(builder, 1) == c,
                     "two swaps restore the exact original operands");
    TEST_ASSERT_TRUE(TeX_drop(builder) && TeX_builder_count(builder) == 2 &&
                     TeX_builder_peek(builder, 2) == a && TeX_builder_peek(builder, 1) == b,
                     "DROP removes only the top operand and preserves surviving borrowed views");
    TEST_ASSERT_TRUE(TeX_drop(builder) && TeX_builder_count(builder) == 1 && TeX_builder_peek(builder, 1) == a &&
                     TeX_drop(builder) && TeX_builder_count(builder) == 0 && !TeX_builder_error(builder),
                     "DROP can empty a stack without leaving an error or changing surviving values");
    TeX_symbol(builder, "left");
    TeX_symbol(builder, "right");
    TeX_swap(builder);
    TeX_frac(builder);
    finish_matches(builder, "\\frac{\\mathit{right}}{\\mathit{left}}");

    builder = TeX_builder_new();
    TeX_symbol(builder, "x");
    TeX_symbol(builder, "y");
    TeX_symbol(builder, "unused");
    TeX_integer(builder, 2);
    TEST_ASSERT_TRUE(TeX_swap(builder) && TeX_drop(builder) && TeX_roll(builder),
                     "SWAP and DROP preserve the literal integer metadata needed by ROLL");
    TeX_frac(builder);
    finish_matches(builder, "\\frac{y}{x}");
}

static void test_TeX_builder_drop_swap_errors(void)
{
    TeX_builder_t *builder = TeX_builder_new();
    TEST_ASSERT_TRUE(!TeX_drop(builder) && errno == ERANGE && TeX_builder_error(builder) == ERANGE &&
                     TeX_builder_count(builder) == 0, "DROP rejects an empty stack without changing it");
    TeX_builder_clear_error(builder);
    TEST_ASSERT_TRUE(!TeX_swap(builder) && errno == ERANGE && TeX_builder_error(builder) == ERANGE &&
                     TeX_builder_count(builder) == 0, "SWAP requires two operands even on an empty stack");
    TeX_builder_clear_error(builder);
    TeX_symbol(builder, "a");
    const TeX_t *a = TeX_builder_peek(builder, 1);
    TEST_ASSERT_TRUE(!TeX_swap(builder) && TeX_builder_error(builder) == ERANGE &&
                     TeX_builder_count(builder) == 1 && TeX_builder_peek(builder, 1) == a,
                     "SWAP underflow preserves a lone operand");
    TEST_ASSERT_TRUE(!TeX_drop(builder) && TeX_builder_count(builder) == 1 &&
                     TeX_builder_peek(builder, 1) == a && TeX_builder_error(builder) == ERANGE,
                     "DROP honours the earlier underflow until explicitly cleared");
    TeX_builder_clear_error(builder);
    TeX_symbol(builder, "b");
    const TeX_t *b = TeX_builder_peek(builder, 1);
    TeX_symbol(builder, "not + a symbol");
    TEST_ASSERT_TRUE(!TeX_swap(builder) && !TeX_drop(builder) && errno == EINVAL &&
                     TeX_builder_error(builder) == EINVAL && TeX_builder_count(builder) == 2 &&
                     TeX_builder_peek(builder, 2) == a && TeX_builder_peek(builder, 1) == b,
                     "both operations preserve operands and the first saved construction error");
    TeX_builder_clear_error(builder);
    TeX_swap(builder);
    TeX_drop(builder);
    finish_matches(builder, "b");
    errno = 0;
    TEST_ASSERT_TRUE(!TeX_drop(NULL) && errno == EINVAL, "DROP rejects NULL without dereferencing it");
    errno = 0;
    TEST_ASSERT_TRUE(!TeX_swap(NULL) && errno == EINVAL, "SWAP rejects NULL without dereferencing it");
}

static void test_TeX_builder_hp48_stack(void)
{
    TeX_builder_t *builder = TeX_builder_new();
    TeX_integer(builder, 12);
    TeX_integer(builder, 34);
    TeX_integer(builder, 56);
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
            TeX_integer(builder, (long)(10 * (i + 1)));
            original[i] = TeX_builder_peek(builder, 1);
        }
        TeX_integer(builder, n);
        TEST_ASSERT_TRUE(TeX_roll(builder) && TeX_builder_count(builder) == 6,
                         "ROLL consumes its one-based level control");
        for (size_t level = 1; level <= 6; ++level) {
            size_t index = level == 1 ? 6 - (size_t)n : level <= (size_t)n ? 7 - level : 6 - level;
            TEST_ASSERT_TRUE(TeX_builder_peek(builder, level) == original[index],
                             "ROLL shifts only the selected pointer range and preserves lower levels");
        }
        TeX_builder_clear(builder);
    }

    TeX_integer(builder, 555);
    TeX_integer(builder, 444);
    TeX_integer(builder, 333);
    TeX_integer(builder, 222);
    TeX_integer(builder, 4);
    TEST_ASSERT_TRUE(TeX_roll(builder), "the HP48 manual's four-level ROLL succeeds");
    TeX_integer(builder, 1);
    TeX_integer(builder, 4);
    TeX_integer(builder, 2);
    TeX_list(builder);
    TeX_matrix(builder);
    TeX_matrix_style(builder, TeX_MATRIX_PLAIN);
    TeX_t *result = TeX_builder_finish(builder);
    TEST_ASSERT_TRUE(result && !strcmp(TeX_source(result), "\\begin{matrix}444 & 333 & 222 & 555\\end{matrix}"),
                     "HP48 ROLL: 555 444 333 222 4 becomes 444 333 222 555");
    TeX_free(result);

    TeX_symbol(builder, "a");
    TeX_integer(builder, 1);
    const TeX_t *control = TeX_builder_peek(builder, 1);
    TEST_ASSERT_TRUE(TeX_dup(builder) && TeX_builder_peek(builder, 1) != control &&
                     TeX_roll(builder) && TeX_builder_peek(builder, 1) == control,
                     "DUP copies literal controls and ROLL 1 leaves the data untouched");
    TeX_builder_clear(builder);
    TeX_integer(builder, 2);
    result = TeX_builder_finish(builder);
    TeX_symbol(builder, "a");
    TeX_symbol(builder, "b");
    TeX_notation(builder, result);
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
    TeX_symbol(builder, "a");
    TeX_symbol(builder, "b");
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
        TeX_symbol(builder, "a");
        TeX_symbol(builder, "b");
        TeX_integer(builder, invalid[i]);
        a = TeX_builder_peek(builder, 3);
        b = TeX_builder_peek(builder, 2);
        const TeX_t *control = TeX_builder_peek(builder, 1);
        TEST_ASSERT_TRUE(!TeX_roll(builder) && TeX_builder_error(builder) == ERANGE &&
                         TeX_builder_count(builder) == 3 && TeX_builder_peek(builder, 3) == a &&
                         TeX_builder_peek(builder, 2) == b && TeX_builder_peek(builder, 1) == control,
                         "invalid ROLL levels preserve the control and every data operand");
        TeX_builder_clear(builder);
    }

    TeX_integer(builder, 1);
    const TeX_t *control = TeX_builder_peek(builder, 1);
    TEST_ASSERT_TRUE(!TeX_roll(builder) && TeX_builder_error(builder) == ERANGE &&
                     TeX_builder_count(builder) == 1 && TeX_builder_peek(builder, 1) == control,
                     "a level control alone supplies no data to roll");
    TeX_builder_clear(builder);

    TeX_symbol(builder, "a");
    TeX_raw(builder, "1");
    TEST_ASSERT_TRUE(!TeX_roll(builder) && TeX_builder_error(builder) == EINVAL &&
                     TeX_builder_count(builder) == 2, "ROLL never parses raw TeX to discover a level");
    TeX_builder_clear(builder);
    TeX_symbol(builder, "a");
    TeX_integer(builder, 1);
    TeX_integer(builder, 1);
    TeX_add(builder);
    TEST_ASSERT_TRUE(!TeX_roll(builder) && TeX_builder_error(builder) == EINVAL &&
                     TeX_builder_count(builder) == 2, "ROLL does not evaluate arithmetic notation as a control");
    TeX_builder_free(builder);
}

static void test_TeX_builder_transactional_errors(void)
{
    TeX_builder_t *builder = TeX_builder_new();
    TeX_symbol(builder, "numerator");
    const TeX_t *saved = TeX_builder_peek(builder, 1);
    TEST_ASSERT_TRUE(!TeX_frac(builder) && TeX_builder_error(builder) == ERANGE &&
                     TeX_builder_count(builder) == 1 && TeX_builder_peek(builder, 1) == saved,
                     "underflow preserves the exact original operand");
    TEST_ASSERT_TRUE(!TeX_integer(builder, 2) && !TeX_sqrt(builder) && !TeX_builder_finish(builder) &&
                     TeX_builder_error(builder) == ERANGE && TeX_builder_count(builder) == 1,
                     "ignored errors cannot produce an incomplete formula");
    TeX_builder_clear_error(builder);
    TeX_integer(builder, 2);
    TEST_ASSERT_TRUE(!TeX_builder_finish(builder) && TeX_builder_count(builder) == 2 &&
                     TeX_builder_error(builder) == EINVAL, "finish rejects leftover operands without discarding them");
    TeX_builder_clear_error(builder);
    TeX_frac(builder);
    finish_matches(builder, "\\frac{\\mathit{numerator}}{2}");

    builder = TeX_builder_new();
    TeX_symbol(builder, "x");
    TeX_symbol(builder, "y");
    saved = TeX_builder_peek(builder, 1);
    TEST_ASSERT_TRUE(!TeX_fence(builder, (TeX_fence_t)-1) &&
                     TeX_builder_peek(builder, 1) == saved && TeX_builder_count(builder) == 2,
                     "invalid presentation parameters preserve the previous stack");
    TeX_builder_clear(builder);
    TEST_ASSERT_TRUE(!TeX_builder_error(builder) && TeX_builder_count(builder) == 0,
                     "clear releases unfinished operands and resets the error");
    TeX_integer(builder, LONG_MAX);
    TeX_integer(builder, 3);
    TeX_integer(builder, 2);
    TeX_list(builder);
    TEST_ASSERT_TRUE(!TeX_matrix(builder) &&
                     TeX_builder_error(builder) == EOVERFLOW, "matrix multiplication overflow precedes cell access");
    TeX_builder_clear(builder);
    TeX_symbol(builder, "T");
    TeX_integer(builder, 2);
    const TeX_t *control = TeX_builder_peek(builder, 1);
    TEST_ASSERT_TRUE(!TeX_tensor(builder) && TeX_builder_error(builder) == EINVAL &&
                     TeX_builder_peek(builder, 1) == control, "tensor indices must be a structural list");
    TeX_builder_free(builder);
    TEST_ASSERT_TRUE(!TeX_raw(NULL, "x") && !TeX_builder_finish(NULL) && !TeX_dup(NULL) &&
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
    TeX_text(builder, text);
    string_free(text);
    finish_matches(builder, "\\text{scaled \\& normalised}");
    builder = TeX_builder_new();
    text = string_new_with("sqrt(x/2)");
    TeX_expression(builder, text);
    string_free(text);
    finish_matches(builder, "\\sqrt{\\frac{x}{2}}");
    builder = TeX_builder_new();
    TeX_math(builder, "sqrt(x/2)");
    finish_matches(builder, "\\sqrt{\\frac{x}{2}}");
    builder = TeX_builder_new();
    TeX_symbol(builder, "safe");
    text = string_new_with("a");
    const char tail[] = {'\0', 'b'};
    string_append_bytes(text, tail, sizeof tail);
    TEST_ASSERT_TRUE(!TeX_text(builder, text) && TeX_builder_error(builder) == EINVAL &&
                     TeX_builder_count(builder) == 1, "embedded NUL is rejected without truncation or stack loss");
    string_free(text);
    TeX_builder_clear(builder);
    TEST_ASSERT_TRUE(!TeX_math(builder, NULL) && TeX_builder_error(builder) == EINVAL,
                     "NULL mathematical strings are rejected");
    TeX_builder_free(builder);
}

static void test_TeX_builder_raw_source(void)
{
    char source[] = "\\frac{\\sqrt{x^{2}+c}}{x+1}";
    TeX_builder_t *builder = TeX_builder_new();
    TEST_ASSERT_TRUE(TeX_raw(builder, source), "authored TeX source is accepted directly");
    source[1] = '?';
    finish_matches(builder, "\\frac{\\sqrt{x^{2}+c}}{x+1}");

    builder = TeX_builder_new();
    TeX_raw(builder, "\\sin x + \\cos x");
    TeX_integer(builder, 2);
    TeX_pow(builder);
    finish_matches(builder, "{\\left( \\sin x + \\cos x \\right)}^{2}");

    builder = TeX_builder_new();
    TeX_raw(builder, "");
    finish_matches(builder, "");

    builder = TeX_builder_new();
    TeX_symbol(builder, "x");
    const TeX_t *saved = TeX_builder_peek(builder, 1);
    TEST_ASSERT_TRUE(!TeX_raw(builder, NULL) && TeX_builder_error(builder) == EINVAL &&
                     TeX_builder_count(builder) == 1 && TeX_builder_peek(builder, 1) == saved,
                     "a missing raw source records an error and preserves the existing operand");
    TeX_builder_free(builder);
}

static void test_TeX_builder_layout(void)
{
    TeX_builder_t *builder = TeX_builder_new();
    TeX_symbol(builder, "r");
    TeX_symbol(builder, "n");
    TeX_integer(builder, 2);
    TeX_scripts(builder);
    TeX_fence(builder, TeX_FENCE_BRACKETS);
    TeX_integer(builder, 3);
    TeX_root(builder);
    finish_matches(builder, "\\sqrt[3]{\\left[ {r}_{n}^{2} \\right]}");
    builder = TeX_builder_new();
    TeX_symbol(builder, "x");
    TeX_integer(builder, 1);
    TeX_list(builder);
    TeX_function(builder, "P");
    TeX_integer(builder, 1);
    TeX_symbol(builder, "x");
    TeX_dup(builder);
    TeX_dup(builder);
    TeX_integer(builder, 2);
    TeX_pow(builder);
    TeX_integer(builder, 2);
    TeX_integer(builder, 2);
    TeX_integer(builder, 2);
    TeX_list(builder);
    TeX_matrix(builder);
    TeX_integer(builder, 6);
    TeX_matrix_spacing(builder);
    TeX_eq(builder);
    finish_matches(builder, "P\\left(x\\right) = \\begin{bmatrix}1 & x \\\\[6pt] x & {x}^{2}\\end{bmatrix}");
    builder = TeX_builder_new();
    TeX_symbol(builder, "a");
    TeX_symbol(builder, "b");
    TeX_mul(builder);
    TeX_symbol(builder, "c");
    TeX_mul(builder);
    TeX_space(builder, TeX_SPACE_QUAD);
    string_t *caption = string_new_with("caption");
    TeX_text(builder, caption);
    string_free(caption);
    TeX_integer(builder, 3);
    TeX_list(builder);
    TeX_sequence(builder);
    finish_matches(builder, "a\\,b\\,c\\qquad \\text{caption}");

    builder = TeX_builder_new();
    TeX_symbol(builder, "x");
    TeX_symbol(builder, "x");
    TeX_integer(builder, 1);
    TeX_eq(builder);
    TeX_integer(builder, 1);
    TeX_list(builder);
    TeX_bound(builder);
    finish_matches(builder, "\\left\\{ x \\;\\middle|\\; x = 1 \\right\\}");
    builder = TeX_builder_new();
    TeX_integer(builder, 2);
    TeX_symbol(builder, "x");
    TeX_scaled(builder);
    finish_matches(builder, "2\\mkern-5mu x");
    builder = TeX_builder_new();
    TeX_symbol(builder, "x");
    TeX_symbol(builder, "y");
    TeX_cartesian_sum(builder);
    finish_matches(builder, "\\begin{aligned}&x \\\\[10pt] &{}+ i\\mkern-2mu y\\end{aligned}");
    builder = TeX_builder_new();
    TeX_symbol(builder, "x");
    TeX_symbol(builder, "y");
    TeX_integer(builder, 2);
    TeX_integer(builder, 1);
    TeX_list(builder);
    TeX_symmetric_pair(builder, true);
    finish_matches(builder, "\\frac{x \\pm \\left(y\\right)}{2}");
}

static void test_TeX_builder_calculus_and_tensors(void)
{
    const char *sums[] = {"\\sum {f}", "\\sum_{0} {f}", "\\sum^{1} {f}", "\\sum_{0}^{1} {f}"};
    const char *integrals[] = {"\\int {f}\\,\\mathrm{d}{x}", "\\int_{0} {f}\\,\\mathrm{d}{x}",
        "\\int^{1} {f}\\,\\mathrm{d}{x}", "\\int_{0}^{1} {f}\\,\\mathrm{d}{x}"};
    for (unsigned limits = 0; limits <= 3; ++limits) {
        TeX_builder_t *builder = TeX_builder_new();
        TeX_symbol(builder, "f");
        if (limits & 1) TeX_integer(builder, 0);
        if (limits & 2) TeX_integer(builder, 1);
        if (limits == 2) TeX_upper(builder);
        TeX_integer(builder, !!(limits & 1) + !!(limits & 2));
        TeX_list(builder);
        TeX_sum(builder);
        finish_matches(builder, sums[limits]);
        builder = TeX_builder_new();
        TeX_symbol(builder, "f");
        TeX_symbol(builder, "x");
        if (limits & 1) TeX_integer(builder, 0);
        if (limits & 2) TeX_integer(builder, 1);
        if (limits == 2) TeX_upper(builder);
        TeX_integer(builder, !!(limits & 1) + !!(limits & 2));
        TeX_list(builder);
        TeX_integral(builder);
        finish_matches(builder, integrals[limits]);
    }
    TeX_builder_t *builder = TeX_builder_new();
    TeX_symbol(builder, "f");
    TeX_symbol(builder, "x");
    TeX_integer(builder, 2);
    TeX_derivative(builder, TeX_DERIVATIVE_PARTIAL);
    finish_matches(builder, "\\frac{\\partial^{2} {f}}{\\partial {x}^{2}}");
    builder = TeX_builder_new();
    TeX_symbol(builder, "T");
    TeX_symbol(builder, "μ");
    TeX_upper(builder);
    TeX_symbol(builder, "ν");
    TeX_integer(builder, 2);
    TeX_list(builder);
    TeX_tensor(builder);
    finish_matches(builder, "{T}^{\\mu}{}_{\\nu}");
    builder = TeX_builder_new();
    TeX_symbol(builder, "x");
    TeX_symbol(builder, "not_a_list");
    TEST_ASSERT_TRUE(!TeX_sum(builder) && TeX_builder_error(builder) == EINVAL &&
                     TeX_builder_count(builder) == 2, "invalid bounds cannot reinterpret operands");
    TeX_builder_free(builder);
}

static void test_TeX_builder_general_indices(void)
{
    TeX_builder_t *b = TeX_builder_new();
    /* F^{mu nu}_{;mu}: compact equal-variance indices precede a separate derivative slot. */
    TeX_symbol(b, "F");
    TeX_symbol(b, "mu");
    TeX_upper(b);
    TeX_symbol(b, "nu");
    TeX_upper(b);
    TeX_symbol(b, "mu");
    TeX_semicolon(b);
    TeX_integer(b, 3);
    TeX_list(b);
    TeX_tensor(b);
    finish_matches(b, "{F}^{\\mu \\nu}{}_{;\\mu}");

    b = TeX_builder_new();
    /* F_{mu n;lambda}: a following Latin label must not extend the Greek control sequence. */
    TeX_symbol(b, "F");
    TeX_symbol(b, "mu");
    TeX_symbol(b, "n");
    TeX_symbol(b, "lambda");
    TeX_semicolon(b);
    TeX_integer(b, 3);
    TeX_list(b);
    TeX_tensor(b);
    finish_matches(b, "{F}_{\\mu n;\\lambda}");

    TeX_builder_t *builder = TeX_builder_new();
    TeX_symbol(builder, "Gamma");
    TeX_symbol(builder, "rho");
    TeX_upper(builder);
    TeX_symbol(builder, "mu");
    TeX_symbol(builder, "nu");
    TeX_integer(builder, 3);
    TeX_list(builder);
    TeX_tensor(builder);
    finish_matches(builder, "{\\Gamma}^{\\rho}{}_{\\mu \\nu}");

    builder = TeX_builder_new();
    TeX_symbol(builder, "nabla");
    TeX_symbol(builder, "mu");
    TeX_integer(builder, 1);
    TeX_list(builder);
    TeX_tensor(builder);
    TeX_symbol(builder, "V");
    TeX_symbol(builder, "nu");
    TeX_upper(builder);
    TeX_integer(builder, 1);
    TeX_list(builder);
    TeX_tensor(builder);
    TeX_mul(builder);
    finish_matches(builder, "{\\nabla}_{\\mu}\\,{V}^{\\nu}");

    builder = TeX_builder_new();
    TeX_symbol(builder, "T");
    TeX_symbol(builder, "rho");
    TeX_upper(builder);
    TeX_symbol(builder, "sigma");
    TeX_symbol(builder, "mu");
    TeX_upper(builder);
    TeX_symbol(builder, "nu");
    TeX_symbol(builder, "lambda");
    TeX_integer(builder, 5);
    TeX_list(builder);
    TeX_tensor(builder);
    finish_matches(builder, "{T}^{\\rho}{}_{\\sigma}{}^{\\mu}{}_{\\nu \\lambda}");

    builder = TeX_builder_new();
    TeX_symbol(builder, "G");
    TeX_symbol(builder, "mu");
    TeX_symbol(builder, "nu");
    TeX_integer(builder, 1);
    TeX_prime(builder);
    TeX_integer(builder, 2);
    TeX_list(builder);
    TeX_tensor(builder);
    finish_matches(builder, "{G}_{\\mu \\nu'}");
}

static void test_TeX_builder_primed_indices(void)
{
    TeX_builder_t *b = TeX_builder_new();
    TeX_symbol(b, "T");
    TeX_symbol(b, "mu");
    TeX_integer(b, 1);
    TeX_prime(b);
    TeX_upper(b);
    TeX_symbol(b, "nu");
    TeX_integer(b, 1);
    TeX_prime(b);
    TeX_lower(b);
    TeX_integer(b, 2);
    TeX_list(b);
    TeX_dup(b);
    TeX_drop(b);
    TeX_tensor(b);
    finish_matches(b, "{T}^{\\mu'}{}_{\\nu'}");

    b = TeX_builder_new();
    TeX_symbol(b, "T");
    TeX_symbol(b, "alpha");
    TeX_integer(b, 2);
    TeX_prime(b);
    TeX_upper(b);
    TeX_symbol(b, "beta");
    TeX_integer(b, 2);
    TeX_prime(b);
    TeX_integer(b, 2);
    TeX_list(b);
    TeX_tensor(b);
    finish_matches(b, "{T}^{\\alpha''}{}_{\\beta''}");
}

static void test_TeX_builder_transforms(void)
{
    static const char *const expected[] = {
        "\\mathcal{L}_{u\\to v}\\{f\\}", "\\mathcal{L}^{-1}_{u\\to v}\\{f\\}",
        "\\mathcal{F}_{u\\to v}\\{f\\}", "\\mathcal{F}^{-1}_{u\\to v}\\{f\\}"
    };
    for (unsigned kind = 0; kind < sizeof expected / sizeof *expected; ++kind) {
        TeX_builder_t *builder = TeX_builder_new();
        TeX_symbol(builder, "f");
        TeX_symbol(builder, "u");
        TeX_symbol(builder, "v");
        TeX_transform(builder, (TeX_transform_kind_t)kind);
        finish_matches(builder, expected[kind]);
    }
    TeX_builder_t *builder = TeX_builder_new();
    TeX_symbol(builder, "f");
    TeX_symbol(builder, "u");
    TeX_symbol(builder, "v");
    const TeX_t *top = TeX_builder_peek(builder, 1);
    TEST_ASSERT_TRUE(!TeX_transform(builder, (TeX_transform_kind_t)-1) &&
                     TeX_builder_error(builder) == EINVAL && TeX_builder_count(builder) == 3 &&
                     TeX_builder_peek(builder, 1) == top, "invalid transform kinds retain all three operands");
    TeX_builder_clear(builder);
    TeX_symbol(builder, "f");
    TeX_symbol(builder, "u");
    TEST_ASSERT_TRUE(!TeX_transform(builder, TeX_TRANSFORM_FOURIER) &&
                     TeX_builder_error(builder) == ERANGE && TeX_builder_count(builder) == 2,
                     "transform underflow retains the body and available variable");
    TeX_builder_free(builder);
}

static void test_TeX_builder_products(void)
{
    static const char *const expected[] = {
        "\\prod {k}", "\\prod_{1} {k}", "\\prod^{3} {k}", "\\prod_{1}^{3} {k}"
    };
    for (unsigned limits = 0; limits < sizeof expected / sizeof *expected; ++limits) {
        TeX_builder_t *builder = TeX_builder_new();
        TeX_symbol(builder, "k");
        if (limits & 1) TeX_integer(builder, 1);
        if (limits & 2) TeX_integer(builder, 3);
        if (limits == 2) TeX_upper(builder);
        TeX_integer(builder, !!(limits & 1) + !!(limits & 2));
        TeX_list(builder);
        TeX_prod(builder);
        finish_matches(builder, expected[limits]);
    }
    TeX_builder_t *builder = TeX_builder_new();
    TeX_symbol(builder, "k");
    const TeX_t *body = TeX_builder_peek(builder, 1);
    TeX_integer(builder, 4);
    TEST_ASSERT_TRUE(!TeX_prod(builder) && TeX_builder_error(builder) == EINVAL &&
                     TeX_builder_count(builder) == 2 && TeX_builder_peek(builder, 2) == body,
                     "invalid product bounds preserve the exact original body");
    TeX_builder_clear_error(builder);
    TeX_drop(builder);
    TEST_ASSERT_TRUE(!TeX_prod(builder) && TeX_builder_error(builder) == ERANGE &&
                     TeX_builder_count(builder) == 1, "product underflow does not consume its body");
    TeX_builder_free(builder);
}

static void test_TeX_builder_partials(void)
{
    static const struct {
        const char *variables[3];
        size_t count;
        const char *expected;
    } cases[] = {
        {{"x"}, 1, "\\frac{\\partial}{\\partial x}\\left[f\\right]"},
        {{"x", "x"}, 2, "\\frac{\\partial^{2}}{\\partial x^{2}}\\left[f\\right]"},
        {{"x", "y"}, 2, "\\frac{\\partial^{2}}{\\partial y\\,\\partial x}\\left[f\\right]"},
        {{"x", "y", "x"}, 3, "\\frac{\\partial^{3}}{\\partial x\\,\\partial y\\,\\partial x}\\left[f\\right]"},
        {{"x", "x", "y"}, 3, "\\frac{\\partial^{3}}{\\partial y\\,\\partial x^{2}}\\left[f\\right]"}
    };
    for (size_t i = 0; i < sizeof cases / sizeof *cases; ++i) {
        TeX_builder_t *builder = TeX_builder_new();
        TeX_symbol(builder, "f");
        for (size_t j = 0; j < cases[i].count; ++j)
            TeX_symbol(builder, cases[i].variables[j]);
        TeX_integer(builder, cases[i].count);
        TeX_list(builder);
        TeX_partial(builder);
        finish_matches(builder, cases[i].expected);
    }
    TeX_builder_t *builder = TeX_builder_new();
    TeX_symbol(builder, "f");
    const TeX_t *body = TeX_builder_peek(builder, 1);
    TeX_integer(builder, 0);
    TeX_list(builder);
    TEST_ASSERT_TRUE(!TeX_partial(builder) && TeX_builder_error(builder) == EINVAL &&
                     TeX_builder_count(builder) == 2 && TeX_builder_peek(builder, 2) == body,
                     "a zero-order partial cannot consume the body");
    TeX_builder_clear_error(builder);
    TeX_drop(builder);
    TeX_symbol(builder, "x");
    TEST_ASSERT_TRUE(!TeX_partial(builder) && TeX_builder_error(builder) == EINVAL &&
                     TeX_builder_count(builder) == 2 && TeX_builder_peek(builder, 2) == body,
                     "a differentiation variable collection must be a list");
    TeX_builder_free(builder);
}

static void test_TeX_builder_primes(void)
{
    static const struct { unsigned order; const char *expected; } cases[] = {
        {1u, "y'"}, {2u, "y''"}, {3u, "y'''"}, {4u, "y^{(4)}"}, {1000u, "y^{(1000)}"}
    };
    for (size_t i = 0; i < sizeof cases / sizeof *cases; ++i) {
        TeX_builder_t *builder = TeX_builder_new();
        TeX_symbol(builder, "y");
        TeX_integer(builder, cases[i].order);
        TeX_prime(builder);
        finish_matches(builder, cases[i].expected);
    }
    TeX_builder_t *builder = TeX_builder_new();
    TEST_ASSERT_TRUE(!TeX_prime(builder) && TeX_builder_error(builder) == ERANGE,
                     "a prime operation requires an operand");
    TeX_builder_clear_error(builder);
    TeX_symbol(builder, "y");
    const TeX_t *saved = TeX_builder_peek(builder, 1);
    TeX_integer(builder, 0);
    TEST_ASSERT_TRUE(!TeX_prime(builder) && TeX_builder_error(builder) == EINVAL &&
                     TeX_builder_count(builder) == 2u && TeX_builder_peek(builder, 2u) == saved,
                     "zero derivative order preserves the exact operand");
    TeX_builder_clear_error(builder);
    TeX_drop(builder);
    TeX_integer(builder, 1);
    TeX_prime(builder);
    TeX_integer(builder, 0);
    TeX_integer(builder, 1);
    TeX_list(builder);
    TeX_apply(builder);
    finish_matches(builder, "y'\\left(0\\right)");

    builder = TeX_builder_new();
    TeX_symbol(builder, "a");
    TeX_symbol(builder, "b");
    TeX_add(builder);
    TeX_integer(builder, 2);
    TeX_prime(builder);
    finish_matches(builder, "\\left( a + b \\right)''");

    builder = TeX_builder_new();
    TeX_symbol(builder, "y");
    TeX_integer(builder, 4);
    TeX_prime(builder);
    TeX_integer(builder, 1);
    TeX_prime(builder);
    finish_matches(builder, "\\left( y^{(4)} \\right)'");
}

static void test_TeX_builder_bars(void)
{
    TeX_builder_t *b = TeX_builder_new();
    TeX_symbol(b, "x");
    TeX_bar(b);
    TeX_symbol(b, "mu");
    TeX_upper(b);
    TeX_integer(b, 1);
    TeX_list(b);
    TeX_tensor(b);
    finish_matches(b, "{\\overline{x}}^{\\mu}");

    b = TeX_builder_new();
    TeX_symbol(b, "x");
    TeX_symbol(b, "y");
    TeX_add(b);
    TeX_bar(b);
    TeX_dup(b);
    TeX_drop(b);
    TeX_integer(b, 2);
    TeX_pow(b);
    finish_matches(b, "{\\overline{x + y}}^{2}");

    b = TeX_builder_new();
    TeX_symbol(b, "T");
    TeX_symbol(b, "mu");
    TeX_upper(b);
    TeX_integer(b, 1);
    TeX_list(b);
    TeX_tensor(b);
    TeX_bar(b);
    finish_matches(b, "\\overline{{T}^{\\mu}}");

    b = TeX_builder_new();
    TeX_symbol(b, "x");
    TeX_bar(b);
    TeX_bar(b);
    finish_matches(b, "\\overline{\\overline{x}}");

    b = TeX_builder_new();
    TEST_ASSERT_TRUE(!TeX_bar(b) && TeX_builder_error(b) == ERANGE && !TeX_builder_count(b),
                     "an overbar needs an operand and preserves an empty stack");
    TEST_ASSERT_TRUE(!TeX_symbol(b, "x") && !TeX_builder_count(b), "a saved error also blocks later operand entry");
    TeX_builder_clear_error(b);
    TeX_symbol(b, "x");
    TeX_bar(b);
    finish_matches(b, "\\overline{x}");
    TEST_ASSERT_TRUE(!TeX_bar(NULL) && errno == EINVAL, "overbar construction rejects a NULL builder safely");
}

static void test_TeX_builder_relations(void)
{
    static const struct {
        bool (*operation)(TeX_builder_t *);
        const char *expected;
    } cases[] = {
        {TeX_eq, "x = y"}, {TeX_ne, "x \\ne y"}, {TeX_lt, "x < y"}, {TeX_le, "x \\le y"},
        {TeX_gt, "x > y"}, {TeX_ge, "x \\ge y"}, {TeX_approx, "x \\approx y"}, {TeX_cong, "x \\cong y"}
    };
    for (size_t i = 0; i < sizeof cases / sizeof *cases; ++i) {
        TeX_builder_t *builder = TeX_builder_new();
        TeX_symbol(builder, "x");
        const TeX_t *saved = TeX_builder_peek(builder, 1);
        TEST_ASSERT_TRUE(!cases[i].operation(builder) && TeX_builder_error(builder) == ERANGE &&
                         TeX_builder_count(builder) == 1 && TeX_builder_peek(builder, 1) == saved,
                         "relation underflow preserves the exact available operand");
        TEST_ASSERT_TRUE(!TeX_symbol(builder, "y") && TeX_builder_error(builder) == ERANGE,
                         "a saved relation error blocks subsequent pushes until cleared");
        TeX_builder_clear_error(builder);
        TeX_symbol(builder, "y");
        cases[i].operation(builder);
        finish_matches(builder, cases[i].expected);
        TEST_ASSERT_TRUE(!cases[i].operation(NULL) && errno == EINVAL, "a NULL relation builder is rejected");
    }
    TeX_builder_t *builder = TeX_builder_new();
    TeX_integer(builder, 0);
    TeX_symbol(builder, "x");
    TeX_lt(builder);
    TeX_integer(builder, 1);
    TeX_le(builder);
    finish_matches(builder, "0 < x \\le 1");
}

static void test_TeX_builder_relation_scope(void)
{
    TeX_builder_t *builder = TeX_builder_new();
    TeX_symbol(builder, "x");
    TeX_symbol(builder, "y");
    TeX_lt(builder);
    TeX_integer(builder, 2);
    TeX_mul(builder);
    finish_matches(builder, "\\left( x < y \\right)\\,2");

    builder = TeX_builder_new();
    TeX_symbol(builder, "x");
    TeX_symbol(builder, "y");
    TeX_approx(builder);
    TeX_integer(builder, 1);
    TeX_add(builder);
    finish_matches(builder, "\\left( x \\approx y \\right) + 1");
}

static void test_TeX_builder_structured_arithmetic(void)
{
    TeX_builder_t *b = TeX_builder_new();
    TeX_integer(b, 1);
    TeX_symbol(b, "x");
    TeX_integer(b, 2);
    TeX_pow(b);
    TeX_add(b);
    finish_matches(b, "1 + {x}^{2}");

    b = TeX_builder_new();
    TeX_symbol(b, "a");
    TeX_symbol(b, "b");
    TeX_symbol(b, "c");
    TeX_sub(b);
    TeX_sub(b);
    finish_matches(b, "a - \\left( b - c \\right)");

    b = TeX_builder_new();
    TeX_symbol(b, "x");
    TeX_symbol(b, "y");
    TeX_add(b);
    TeX_dup(b);
    TeX_mul(b);
    finish_matches(b, "\\left( x + y \\right)\\,\\left( x + y \\right)");

    b = TeX_builder_new();
    TeX_symbol(b, "x");
    TeX_symbol(b, "y");
    TeX_add(b);
    TeX_integer(b, 2);
    TeX_pow(b);
    finish_matches(b, "{\\left( x + y \\right)}^{2}");

    b = TeX_builder_new();
    TeX_math(b, "x+y");
    TeX_neg(b);
    finish_matches(b, "-\\left( x + y \\right)");

    b = TeX_builder_new();
    TeX_integer(b, 2);
    TeX_integer(b, 3);
    TeX_mul(b);
    TeX_integer(b, 4);
    TeX_mul(b);
    finish_matches(b, "2 \\times 3 \\times 4");

    b = TeX_builder_new();
    TeX_integer(b, -2);
    TeX_integer(b, 2);
    TeX_pow(b);
    finish_matches(b, "{\\left( -2 \\right)}^{2}");

    b = TeX_builder_new();
    TeX_symbol(b, "x");
    TeX_integer(b, 1);
    TeX_list(b);
    TeX_function(b, "sin");
    finish_matches(b, "\\sin(x)");
    b = TeX_builder_new();
    TeX_symbol(b, "f");
    TeX_symbol(b, "n");
    TeX_subscript(b);
    TeX_symbol(b, "x");
    TeX_integer(b, 1);
    TeX_list(b);
    TeX_apply(b);
    finish_matches(b, "{f}_{n}\\left(x\\right)");

    b = TeX_builder_new();
    TeX_symbol(b, "ℏ");
    TeX_symbol(b, "∞");
    TeX_neg(b);
    TeX_eq(b);
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
        {"nabla"       , 0x2207u},
    };
    TeX_builder_t *builder = TeX_builder_new();
    for (size_t i = 0; i < sizeof aliases / sizeof *aliases; ++i) {
        string_t *unicode = rune_to_string(rune_from_value(aliases[i].codepoint));
        TEST_ASSERT_TRUE(unicode && TeX_symbol(builder, aliases[i].name) &&
                         TeX_symbol(builder, string_c_str(unicode)), aliases[i].name);
        string_free(unicode);
        TEST_ASSERT_TRUE(!strcmp(TeX_source(TeX_builder_peek(builder, 1)),
                                 TeX_source(TeX_builder_peek(builder, 2))),
                         "keyboard-friendly spelling matches its independently supplied Unicode symbol");
        TeX_builder_clear(builder);
    }
    TeX_symbol(builder, "x");
    TeX_integer(builder, 1);
    TeX_list(builder);
    TeX_function(builder, "psi");
    finish_matches(builder, "\\psi\\left(x\\right)");
    builder = TeX_builder_new();
    TeX_symbol(builder, "x");
    TeX_integer(builder, 1);
    TeX_list(builder);
    TeX_function(builder, "gamma");
    finish_matches(builder, "\\Gamma(x)");
    builder = TeX_builder_new();
    TeX_symbol(builder, "psi_extra");
    finish_matches(builder, "\\mathit{psi\\_extra}");
}

static void test_TeX_builder_rejects_operator_symbols(void)
{
    TeX_builder_t *b = TeX_builder_new();
    TeX_symbol(b, "x");
    const TeX_t *saved = TeX_builder_peek(b, 1);
    TEST_ASSERT_TRUE(!TeX_symbol(b, "1 + ") && TeX_builder_error(b) == EINVAL &&
                     TeX_builder_count(b) == 1 && TeX_builder_peek(b, 1) == saved,
                     "symbols cannot smuggle in an arithmetic operator fragment");
    TeX_builder_clear_error(b);
    TEST_ASSERT_TRUE(!TeX_symbol(b, "\\lambda") && TeX_builder_error(b) == EINVAL &&
                     TeX_builder_peek(b, 1) == saved, "symbols reject TeX commands without consuming operands");
    TeX_builder_clear_error(b);
    TeX_integer(b, 1);
    TeX_list(b);
    saved = TeX_builder_peek(b, 1);
    TEST_ASSERT_TRUE(!TeX_function(b, "a+b") && TeX_builder_error(b) == EINVAL &&
                     TeX_builder_peek(b, 1) == saved, "function names reject operator fragments transactionally");
    TeX_builder_clear_error(b);
    TEST_ASSERT_TRUE(!TeX_apply(b) && TeX_builder_error(b) == ERANGE &&
                     TeX_builder_count(b) == 1, "function application requires both a function and its argument list");
    TeX_builder_free(b);
}

static void test_TeX_builder_lists(void)
{
    TeX_builder_t *b = TeX_builder_new();
    TeX_symbol(b, "kept");
    const TeX_t *kept = TeX_builder_peek(b, 1);
    TeX_symbol(b, "mu");
    TeX_upper(b);
    TeX_symbol(b, "nu");
    TeX_integer(b, 2);
    TeX_list(b);
    TeX_integer(b, 2);
    TeX_roll(b);
    TeX_swap(b);
    TeX_dup(b);
    TeX_swap(b);
    TeX_drop(b);
    TeX_unlist(b);
    TEST_ASSERT_TRUE(TeX_builder_count(b) == 4 && TeX_builder_peek(b, 4) == kept,
                     "LIST-> preserves lower operand identities and pushes members then their count");
    TeX_list(b);
    TeX_symbol(b, "Gamma");
    TeX_swap(b);
    TeX_tensor(b);
    TeX_swap(b);
    TeX_drop(b);
    finish_matches(b, "{\\Gamma}^{\\mu}{}_{\\nu}");

    b = TeX_builder_new();
    TeX_integer(b, 0);
    TeX_list(b);
    TeX_dup(b);
    TeX_unlist(b);
    TeX_list(b);
    TeX_integer(b, 2);
    TeX_list(b);
    finish_matches(b, "\\left\\{\\left\\{\\right\\}, \\left\\{\\right\\}\\right\\}");

    b = TeX_builder_new();
    TeX_symbol(b, "x");
    TeX_symbol(b, "y");
    TeX_depth(b);
    TeX_list(b);
    TeX_function(b, "f");
    finish_matches(b, "f\\left(x, y\\right)");

    b = TeX_builder_new();
    TeX_symbol(b, "f");
    TeX_integer(b, 0);
    TeX_list(b);
    TeX_apply(b);
    finish_matches(b, "f\\left(\\right)");
}

static void test_TeX_builder_collection_errors(void)
{
    TeX_builder_t *b = TeX_builder_new();
    TeX_symbol(b, "x");
    const TeX_t *x = TeX_builder_peek(b, 1);
    TeX_integer(b, 2);
    const TeX_t *control = TeX_builder_peek(b, 1);
    TEST_ASSERT_TRUE(!TeX_list(b) && TeX_builder_error(b) == ERANGE &&
                     TeX_builder_count(b) == 2 && TeX_builder_peek(b, 1) == control &&
                     TeX_builder_peek(b, 2) == x, "a missing list member preserves both count and operands");
    TeX_builder_clear_error(b);
    TeX_drop(b);
    TeX_raw(b, "1");
    TEST_ASSERT_TRUE(!TeX_list(b) && TeX_builder_error(b) == EINVAL && TeX_builder_count(b) == 2,
                     "list controls are literal objects, never parsed from rendered source");
    TeX_builder_clear(b);
    TeX_integer(b, -1);
    TEST_ASSERT_TRUE(!TeX_list(b) && TeX_builder_error(b) == EINVAL, "negative list counts are rejected");
    TeX_builder_clear(b);
    TeX_symbol(b, "x");
    TEST_ASSERT_TRUE(!TeX_unlist(b) && TeX_builder_error(b) == EINVAL,
                     "LIST-> rejects an ordinary formula instead of parsing it as a collection");
    TeX_builder_clear(b);
    TeX_symbol(b, "f");
    TeX_integer(b, 1);
    TeX_upper(b);
    TeX_integer(b, 2);
    TeX_upper(b);
    TeX_integer(b, 2);
    TeX_list(b);
    const TeX_t *bounds = TeX_builder_peek(b, 1);
    TEST_ASSERT_TRUE(!TeX_sum(b) && TeX_builder_error(b) == EINVAL &&
                     TeX_builder_count(b) == 2 && TeX_builder_peek(b, 1) == bounds,
                     "duplicate upper bounds retain the original bound list and body");
    TeX_builder_clear(b);
    TeX_integer(b, 0);
    TeX_list(b);
    for (unsigned i = 1; i < 128; ++i) {
        TeX_integer(b, 1);
        TEST_ASSERT_TRUE(TeX_list(b), "bounded nesting accepts each supported list level");
    }
    const TeX_t *nested = TeX_builder_peek(b, 1);
    TeX_integer(b, 1);
    TEST_ASSERT_TRUE(!TeX_list(b) && TeX_builder_error(b) == EOVERFLOW &&
                     TeX_builder_count(b) == 2 && TeX_builder_peek(b, 2) == nested,
                     "excessive nesting fails transactionally before recursive copying");
    TeX_builder_free(b);
}

static void test_TeX_builder_row_lists(void)
{
    TeX_builder_t *b = TeX_builder_new();
    TeX_integer(b, 1);
    TeX_integer(b, 2);
    TeX_integer(b, 2);
    TeX_list(b);
    TeX_integer(b, 3);
    TeX_integer(b, 4);
    TeX_integer(b, 2);
    TeX_list(b);
    TeX_integer(b, 2);
    TeX_list(b);
    TeX_dup(b);
    TeX_matrix(b);
    TeX_dup(b);
    TeX_swap(b);
    TeX_drop(b);
    const TeX_t *matrix = TeX_builder_peek(b, 1);
    TEST_ASSERT_TRUE(!TeX_matrix_style(b, (TeX_matrix_style_t)-1) && TeX_builder_error(b) == EINVAL &&
                     TeX_builder_peek(b, 1) == matrix, "invalid matrix styles preserve retained cell objects");
    TeX_builder_clear_error(b);
    TeX_integer(b, -1);
    TEST_ASSERT_TRUE(!TeX_matrix_spacing(b) && TeX_builder_error(b) == EINVAL &&
                     TeX_builder_peek(b, 2) == matrix, "negative spacing cannot consume a matrix or its control");
    TeX_builder_clear_error(b);
    TeX_drop(b);
    TeX_matrix_style(b, TeX_MATRIX_PARENTHESES);
    TeX_integer(b, 6);
    TeX_matrix_spacing(b);
    TeX_swap(b);
    TeX_drop(b);
    finish_matches(b, "\\begin{pmatrix}1 & 2 \\\\[6pt] 3 & 4\\end{pmatrix}");

    b = TeX_builder_new();
    TeX_integer(b, 1);
    TeX_integer(b, 1);
    TeX_list(b);
    TeX_integer(b, 2);
    TeX_integer(b, 3);
    TeX_integer(b, 2);
    TeX_list(b);
    TeX_integer(b, 2);
    TeX_list(b);
    const TeX_t *ragged = TeX_builder_peek(b, 1);
    TEST_ASSERT_TRUE(!TeX_matrix(b) && TeX_builder_error(b) == EINVAL &&
                     TeX_builder_count(b) == 1 && TeX_builder_peek(b, 1) == ragged,
                     "ragged row lists retain their complete structure on failure");
    TeX_builder_free(b);
}

static void test_TeX_builder_multiline(void)
{
    TeX_builder_t *b = TeX_builder_new();
    TeX_symbol(b, "x");
    const TeX_t *body = TeX_builder_peek(b, 1);
    TEST_ASSERT_TRUE(!TeX_newline(b) && TeX_builder_error(b) == ERANGE &&
                     TeX_builder_peek(b, 1) == body, "a newline needs two rows and preserves an available row");
    TeX_builder_clear_error(b);
    TeX_integer(b, -1);
    TEST_ASSERT_TRUE(!TeX_indent(b) && TeX_builder_error(b) == EINVAL &&
                     TeX_builder_count(b) == 2 && TeX_builder_peek(b, 2) == body,
                     "negative indentation preserves body and spacing");
    TeX_builder_clear(b);
    TeX_integer(b, 0);
    TeX_list(b);
    const TeX_t *empty = TeX_builder_peek(b, 1);
    TEST_ASSERT_TRUE(!TeX_align(b) && TeX_builder_error(b) == EINVAL && TeX_builder_peek(b, 1) == empty,
                     "an empty alignment list is rejected without consuming its structure");
    TeX_builder_clear(b);
    TeX_symbol(b, "x");
    TeX_integer(b, 1);
    TeX_eq(b);
    TeX_symbol(b, "y");
    TeX_integer(b, 2);
    TeX_le(b);
    TeX_newline(b);
    TeX_dup(b);
    TeX_swap(b);
    TeX_drop(b);
    TeX_align(b);
    TeX_symbol(b, "z");
    TeX_integer(b, 8);
    TeX_indent(b);
    TeX_newline(b);
    finish_matches(b, "\\begin{aligned}x &= 1 \\\\ y &\\le 2 \\\\ &\\hspace{8pt}z\\end{aligned}");

    b = TeX_builder_new();
    TeX_raw(b, "x = 1");
    TeX_raw(b, "y = 2");
    TeX_newline(b);
    TeX_align(b);
    finish_matches(b, "\\begin{aligned}&x = 1 \\\\ &y = 2\\end{aligned}");
}

static void test_TeX_builder_column_alignment(void)
{
    TeX_builder_t *b = TeX_builder_new();
    TeX_symbol(b, "a");
    TeX_symbol(b, "b");
    TeX_integer(b, 2);
    TeX_list(b);
    TeX_symbol(b, "c");
    TeX_symbol(b, "d");
    TeX_integer(b, 2);
    TeX_list(b);
    TeX_integer(b, 2);
    TeX_list(b);
    TeX_align(b);
    TeX_dup(b);
    TeX_drop(b);
    TeX_symbol(b, "e");
    TeX_symbol(b, "f");
    TeX_integer(b, 2);
    TeX_list(b);
    TeX_newline(b);
    finish_matches(b, "\\begin{array}{ll}a & b \\\\ c & d \\\\ e & f\\end{array}");

    b = TeX_builder_new();
    TeX_symbol(b, "a");
    TeX_symbol(b, "b");
    TeX_symbol(b, "c");
    TeX_integer(b, 3);
    TeX_list(b);
    TeX_symbol(b, "d");
    TeX_integer(b, 1);
    TeX_list(b);
    TeX_integer(b, 8);
    TeX_indent(b);
    TeX_integer(b, 2);
    TeX_list(b);
    TeX_align(b);
    finish_matches(b, "\\begin{array}{lll}a & b & c \\\\ \\hspace{8pt}d\\end{array}");

    b = TeX_builder_new();
    TeX_integer(b, 0);
    TeX_list(b);
    TeX_integer(b, 1);
    TeX_list(b);
    const TeX_t *empty_row = TeX_builder_peek(b, 1);
    TEST_ASSERT_TRUE(!TeX_align(b) && TeX_builder_error(b) == EINVAL &&
                     TeX_builder_count(b) == 1 && TeX_builder_peek(b, 1) == empty_row,
                     "an empty explicit row cannot consume its enclosing row list");
    TeX_builder_free(b);
}

static void test_TeX_builder_index_derivatives(void)
{
    TeX_builder_t *b = TeX_builder_new();
    TeX_symbol(b, "V");
    TeX_symbol(b, "nu");
    TeX_upper(b);
    TeX_symbol(b, "mu");
    TeX_comma(b);
    TeX_integer(b, 2);
    TeX_list(b);
    TeX_tensor(b);
    finish_matches(b, "{V}^{\\nu}{}_{,\\mu}");

    b = TeX_builder_new();
    TeX_symbol(b, "V");
    TeX_symbol(b, "i");
    TeX_symbol(b, "j");
    TeX_comma(b);
    TeX_semicolon(b);
    TeX_dup(b);
    TeX_swap(b);
    TeX_drop(b);
    TeX_integer(b, 2);
    TeX_list(b);
    TeX_tensor(b);
    finish_matches(b, "{V}_{i;j}");

    b = TeX_builder_new();
    TeX_symbol(b, "V");
    TeX_symbol(b, "i");
    TeX_symbol(b, "j");
    TeX_symbol(b, "k");
    TeX_integer(b, 2);
    TeX_list(b);
    TeX_semicolon(b);
    TeX_integer(b, 2);
    TeX_list(b);
    TeX_tensor(b);
    finish_matches(b, "{V}_{i;j\\,k}");

    b = TeX_builder_new();
    TeX_symbol(b, "T");
    TeX_symbol(b, "a");
    TeX_upper(b);
    TeX_symbol(b, "i");
    TeX_symbol(b, "j");
    TeX_semicolon(b);
    TeX_symbol(b, "k");
    TeX_comma(b);
    TeX_integer(b, 4);
    TeX_list(b);
    TeX_tensor(b);
    finish_matches(b, "{T}^{a}{}_{i;j,k}");

    b = TeX_builder_new();
    TeX_symbol(b, "V");
    TeX_symbol(b, "nu");
    TeX_upper(b);
    TeX_symbol(b, "mu");
    TeX_comma(b);
    TeX_upper(b);
    TeX_integer(b, 2);
    TeX_list(b);
    TeX_tensor(b);
    finish_matches(b, "{V}^{\\nu,\\mu}");
}

static void test_TeX_builder_index_derivative_errors(void)
{
    TeX_builder_t *b = TeX_builder_new();
    TEST_ASSERT_TRUE(!TeX_comma(b) && TeX_builder_error(b) == ERANGE && !TeX_builder_count(b),
                     "a comma requires an index operand");
    TeX_builder_clear(b);
    TeX_integer(b, 0);
    TeX_list(b);
    const TeX_t *empty = TeX_builder_peek(b, 1);
    TEST_ASSERT_TRUE(!TeX_semicolon(b) && TeX_builder_error(b) == EINVAL && TeX_builder_peek(b, 1) == empty,
                     "an empty covariant derivative group preserves its original list");
    TeX_builder_clear_error(b);
    TEST_ASSERT_TRUE(!TeX_comma(b) && TeX_builder_error(b) == EINVAL && TeX_builder_peek(b, 1) == empty,
                     "an empty partial derivative group is also rejected");
    TeX_builder_clear(b);
    TeX_symbol(b, "V");
    const TeX_t *base = TeX_builder_peek(b, 1);
    TeX_integer(b, 0);
    TeX_list(b);
    TeX_upper(b);
    TeX_semicolon(b);
    TeX_integer(b, 1);
    TeX_list(b);
    const TeX_t *indices = TeX_builder_peek(b, 1);
    TEST_ASSERT_TRUE(!TeX_tensor(b) && TeX_builder_error(b) == EINVAL && TeX_builder_count(b) == 2 &&
                     TeX_builder_peek(b, 2) == base && TeX_builder_peek(b, 1) == indices,
                     "an invalid nested derivative group cannot consume the tensor or its index list");
    TeX_builder_free(b);
    TEST_ASSERT_TRUE(!TeX_comma(NULL) && !TeX_semicolon(NULL) && errno == EINVAL,
                     "index punctuation rejects NULL builders safely");
}

static void test_TeX_builder_quantum(void)
{
    TeX_builder_t *b = TeX_builder_new();
    TeX_symbol(b, "p");
    TeX_hat(b);
    TeX_dagger(b);
    finish_matches(b, "{\\widehat{p}}^{\\dagger}");

    b = TeX_builder_new();
    TeX_symbol(b, "phi");
    TeX_bra(b);
    finish_matches(b, "\\left\\langle \\phi \\right|");
    b = TeX_builder_new();
    TeX_symbol(b, "psi");
    TeX_ket(b);
    finish_matches(b, "\\left| \\psi \\right\\rangle");
    b = TeX_builder_new();
    TeX_symbol(b, "phi");
    TeX_symbol(b, "psi");
    TeX_braket(b);
    finish_matches(b, "\\left\\langle \\phi \\middle| \\psi \\right\\rangle");
    b = TeX_builder_new();
    TeX_symbol(b, "psi");
    TeX_symbol(b, "phi");
    TeX_ketbra(b);
    finish_matches(b, "\\left| \\psi \\right\\rangle\\!\\left\\langle \\phi \\right|");
    b = TeX_builder_new();
    TeX_symbol(b, "phi");
    TeX_symbol(b, "A");
    TeX_hat(b);
    TeX_symbol(b, "psi");
    TeX_matrix_element(b);
    finish_matches(b, "\\left\\langle \\phi \\middle| \\widehat{A} \\middle| \\psi \\right\\rangle");
    b = TeX_builder_new();
    TeX_symbol(b, "p");
    TeX_fence(b, TeX_FENCE_ANGLE);
    finish_matches(b, "\\left\\langle p \\right\\rangle");
}

static void test_TeX_builder_quantum_scope(void)
{
    TeX_builder_t *b = TeX_builder_new();
    TeX_symbol(b, "A");
    TeX_symbol(b, "B");
    TeX_add(b);
    TeX_hat(b);
    finish_matches(b, "\\widehat{A + B}");
    b = TeX_builder_new();
    TeX_symbol(b, "A");
    TeX_symbol(b, "B");
    TeX_mul(b);
    TeX_dagger(b);
    finish_matches(b, "{\\left( A\\,B \\right)}^{\\dagger}");
    b = TeX_builder_new();
    TeX_symbol(b, "A");
    TeX_integer(b, 2);
    TeX_pow(b);
    TeX_dagger(b);
    finish_matches(b, "{\\left( {A}^{2} \\right)}^{\\dagger}");
    b = TeX_builder_new();
    TeX_symbol(b, "A");
    TeX_dagger(b);
    TeX_dagger(b);
    finish_matches(b, "{\\left( {A}^{\\dagger} \\right)}^{\\dagger}");
    b = TeX_builder_new();
    TeX_raw(b, "A+B");
    TeX_dagger(b);
    finish_matches(b, "{\\left( A+B \\right)}^{\\dagger}");
}

static void test_TeX_builder_quantum_errors(void)
{
    static const struct {
        bool (*apply)(TeX_builder_t *);
        size_t arity;
    } operations[] = {
        {TeX_hat, 1u}, {TeX_dagger, 1u}, {TeX_bra, 1u}, {TeX_ket, 1u},
        {TeX_braket, 2u}, {TeX_ketbra, 2u}, {TeX_matrix_element, 3u}
    };
    for (size_t i = 0u; i < sizeof operations / sizeof *operations; ++i) {
        TeX_builder_t *b = TeX_builder_new();
        for (size_t j = 1u; j < operations[i].arity; ++j)
            TeX_symbol(b, "psi");
        const TeX_t *top = TeX_builder_peek(b, 1u);
        TEST_ASSERT_TRUE(!operations[i].apply(b) && TeX_builder_error(b) == ERANGE &&
                         TeX_builder_count(b) == operations[i].arity - 1u,
                         "quantum operations preserve all operands on stack underflow");
        if (top)
            TEST_ASSERT_TRUE(TeX_builder_peek(b, 1u) == top, "underflow preserves operand identity");
        TEST_ASSERT_TRUE(!TeX_symbol(b, "x"), "a saved quantum-operation error blocks later pushes");
        TeX_builder_clear_error(b);
        TeX_symbol(b, "psi");
        TEST_ASSERT_TRUE(operations[i].apply(b) && !TeX_builder_error(b) && TeX_builder_count(b) == 1u,
                         "clearing the saved error permits successful reuse with the missing operand supplied");
        TeX_builder_free(b);
        TEST_ASSERT_TRUE(!operations[i].apply(NULL) && errno == EINVAL, "NULL builders are rejected safely");
    }
}

/* Register these public-API cases through the TeX suite's shared harness. */
void run_TeX_builder_tests(void)
{
    TEST_RUN_IN_GROUP(test_TeX_builder_lists, tests, NULL);
    TEST_RUN_IN_GROUP(test_TeX_builder_collection_errors, tests, NULL);
    TEST_RUN_IN_GROUP(test_TeX_builder_row_lists, tests, NULL);
    TEST_RUN_IN_GROUP(test_TeX_builder_multiline, tests, NULL);
    TEST_RUN_IN_GROUP(test_TeX_builder_column_alignment, tests, NULL);
    TEST_RUN_IN_GROUP(test_TeX_builder_ownership, tests, NULL);
    TEST_RUN_IN_GROUP(test_TeX_builder_stack, tests, NULL);
    TEST_RUN_IN_GROUP(test_TeX_builder_drop_swap, tests, NULL);
    TEST_RUN_IN_GROUP(test_TeX_builder_drop_swap_errors, tests, NULL);
    TEST_RUN_IN_GROUP(test_TeX_builder_hp48_stack, tests, NULL);
    TEST_RUN_IN_GROUP(test_TeX_builder_stack_errors, tests, NULL);
    TEST_RUN_IN_GROUP(test_TeX_builder_transactional_errors, tests, NULL);
    TEST_RUN_IN_GROUP(test_TeX_builder_inputs, tests, NULL);
    TEST_RUN_IN_GROUP(test_TeX_builder_raw_source, tests, NULL);
    TEST_RUN_IN_GROUP(test_TeX_builder_layout, tests, NULL);
    TEST_RUN_IN_GROUP(test_TeX_builder_calculus_and_tensors, tests, NULL);
    TEST_RUN_IN_GROUP(test_TeX_builder_general_indices, tests, NULL);
    TEST_RUN_IN_GROUP(test_TeX_builder_primed_indices, tests, NULL);
    TEST_RUN_IN_GROUP(test_TeX_builder_index_derivatives, tests, NULL);
    TEST_RUN_IN_GROUP(test_TeX_builder_index_derivative_errors, tests, NULL);
    TEST_RUN_IN_GROUP(test_TeX_builder_transforms, tests, NULL);
    TEST_RUN_IN_GROUP(test_TeX_builder_products, tests, NULL);
    TEST_RUN_IN_GROUP(test_TeX_builder_partials, tests, NULL);
    TEST_RUN_IN_GROUP(test_TeX_builder_primes, tests, NULL);
    TEST_RUN_IN_GROUP(test_TeX_builder_bars, tests, NULL);
    TEST_RUN_IN_GROUP(test_TeX_builder_quantum, tests, NULL);
    TEST_RUN_IN_GROUP(test_TeX_builder_quantum_scope, tests, NULL);
    TEST_RUN_IN_GROUP(test_TeX_builder_quantum_errors, tests, NULL);
    TEST_RUN_IN_GROUP(test_TeX_builder_relations, tests, NULL);
    TEST_RUN_IN_GROUP(test_TeX_builder_relation_scope, tests, NULL);
    TEST_RUN_IN_GROUP(test_TeX_builder_structured_arithmetic, tests, NULL);
    TEST_RUN_IN_GROUP(test_TeX_builder_rejects_operator_symbols, tests, NULL);
    TEST_RUN_IN_GROUP(test_TeX_builder_ascii_symbols, tests, NULL);
}
