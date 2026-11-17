/**
 * @file test_matrix_appell.c
 * @brief Joint Appell matrix-function checks against independent scalar and nilpotent identities.
 *
 * Database rows supply parameters, matrix pairs, expected entries and rejected domains. Stateful C fixtures
 * verify that symbolic diagonal results retain their variables after the input matrices are released.
 * Ordinary checks run before the suite's runnable README examples; no memory-tool execution is required.
 */
#include <stdlib.h>
#include <string.h>

#include "expression-t.h"
#include "matrix-t.h"
#include "number-t.h"
#include "string-t.h"
#include "test_harness.h"
#include "test_database.h"
#include "test_matrix.h"

static matrix_t *appell_matrix(unsigned family, const number_t *a, const matrix_t *x, const matrix_t *y)
{
    if (family == 1) return mat_appell_F1(a[0], a[1], a[2], a[3], x, y);
    if (family == 2) return mat_appell_F2(a[0], a[1], a[2], a[3], a[4], x, y);
    if (family == 3) return mat_appell_F3(a[0], a[1], a[2], a[3], a[4], x, y);
    return family == 4 ? mat_appell_F4(a[0], a[1], a[2], a[3], x, y) : NULL;
}

static number_t appell_reference(const string_t *text)
{
    expr_t *expression = expr_from_text(text, NULL);
    number_t value = expression ? expr_eval(expression) : num_clone(NUM_NAN);
    expr_free(expression);
    return value;
}

static bool appell_matrix_close(number_t got, number_t expected, size_t bits)
{
    number_t scale = num_abs(expected);
    if (num_is_zero(scale)) scale = NUM_ONE;
    number_t error = num_abs(num_sub(got, expected));
    return num_is_finite(error) && num_lt(error, num_ldexp(scale, -(int)bits + 24));
}

static void appell_matrix_row(const void *data)
{
    const test_database_case_t *row = data;
    size_t saved = num_get_default_prec_bits();
    num_set_default_prec_digits(row->precision);
    NUM_SCOPE(scope);
    size_t field_count = 0, parameter_count = 0;
    string_t **fields = string_split(row->input, "#", &field_count);
    bool ok = fields && field_count == 4;
    const string_t *family_text = ok ? fields[0] : NULL;
    unsigned family = family_text ? (unsigned)strtoul(string_c_str(family_text), NULL, 10) : 0;
    string_t **parameters = ok ? string_split(fields[1], ",", &parameter_count) : NULL;
    size_t count = family == 1 || family == 4 ? 4u : 5u;
    ok = ok && family >= 1 && family <= 4 && parameters && parameter_count == count;
    number_t a[5] = {NUM_NAN, NUM_NAN, NUM_NAN, NUM_NAN, NUM_NAN};
    for (size_t j = 0; ok && j < count; ++j) a[j] = appell_reference(parameters[j]);
    matrix_t *x = ok ? mat_from_string(string_c_str(fields[2])) : NULL;
    matrix_t *y = ok ? mat_from_string(string_c_str(fields[3])) : NULL;
    matrix_t *result = x && y ? appell_matrix(family, a, x, y) : NULL;
    ok = ok && x && y;
    if (!strcmp(string_c_str(row->expected), "NULL")) ok = ok && !result;
    else {
        size_t expected_count = 0;
        string_t **expected = string_split(row->expected, "#", &expected_count);
        ok = ok && result && expected && expected_count == 4;
        for (size_t j = 0; ok && j < 4; ++j) {
            number_t want = appell_reference(expected[j]);
            number_t got = mat_get_num(result, j / 2, j % 2);
            ok = appell_matrix_close(got, want, num_get_default_prec_bits());
        }
        string_split_free(expected, expected_count);
    }
    mat_free(result); mat_free(y); mat_free(x);
    string_split_free(parameters, parameter_count); string_split_free(fields, field_count);
    num_set_default_prec_bits(saved);
    TEST_ASSERT_TRUE(ok, "Appell matrix calculus agrees with independent entries and domain restrictions");
}

static void test_matrix_appell_symbolic(void)
{
    NUM_SCOPE(scope);
    number_t a[] = {NUM_ONE, NUM_ONE, NUM_ONE, NUM_TWO, NUM_TWO};
    for (unsigned family = 1; family <= 4; ++family) {
        a[3] = family == 1 ? NUM_TWO : NUM_ONE;
        a[4] = family == 3 ? NUM_TWO : NUM_ONE;
        expr_t *variable = expr_new_named_var(num_create_from_frac(1, 16), "x");
        expr_t *zero = expr_new_const(NUM_ZERO);
        expr_t *entries[] = {variable, zero, zero, variable};
        matrix_t *x = mat_create_expr(2, 2, entries), *y = mat_new(2, 2);
        matrix_t *result = appell_matrix(family, a, x, y);
        mat_free(y); mat_free(x);
        bool ok = result != NULL;
        for (unsigned point = 0; ok && point < 2; ++point) {
            number_t value = num_create_from_frac(1, point ? 8 : 16);
            expr_set_val(variable, value);
            number_t reference = family == 1 || family == 3 ?
                num_div(num_neg(num_log(num_sub(NUM_ONE, value))), value) : num_div(NUM_ONE, num_sub(NUM_ONE, value));
            expr_t *entry = NULL;
            mat_get(result, 0, 0, &entry);
            ok = entry && appell_matrix_close(expr_eval(entry), reference, num_get_default_prec_bits());
        }
        mat_free(result); expr_free(zero); expr_free(variable);
        TEST_ASSERT_TRUE(ok, "Symbolic Appell diagonals retain live bindings after input matrices are released");
    }
}

/* Execute database-driven cases and the stateful symbolic-lifetime check sequentially. */
void run_matrix_appell_tests(void)
{
    test_run_database_cases(__FILE__, __LINE__, "matrix.appell_family", NULL, appell_matrix_row);
    test_matrix_appell_symbolic();
}
