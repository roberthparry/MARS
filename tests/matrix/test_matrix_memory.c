/**
 * @file test_matrix_memory.c
 * @brief Matrix numeric-scope lifetime regression cases.
 *
 * Checks that matrix entries and special-function results survive the temporary numeric scopes used to construct
 * them. These are ordinary ownership assertions; running this file does not itself enable a memory-analysis tool.
 *
 * Used by the project test harness for regression verification. Select cases through tests/test_config.json and
 * run suites sequentially; this source is not part of the installed library.
 */

#include <errno.h>

#include "test_matrix.h"

/* Linker wrapping is confined to the matrix test binary; production allocators are unchanged. */
void *__real_calloc(size_t count, size_t size);
void *__real_realloc(void *pointer, size_t size);
void __real_string_free(string_t *text);
string_t *__real_expr_to_text_symbolic(const expr_t *expr);

static size_t calloc_countdown;
static size_t injected_failures;
static bool fail_constant;
static bool track_parser;
static bool parser_sequence_valid;
static size_t parser_reallocations;
static size_t parser_token_frees;

void *__wrap_calloc(size_t count, size_t size)
{
    if (calloc_countdown && --calloc_countdown == 0u) {
        ++injected_failures;
        errno = ENOMEM;
        return NULL;
    }
    return __real_calloc(count, size);
}

void *__wrap_realloc(void *pointer, size_t size)
{
    if (track_parser) {
        /* Three string buffers, then the row vector and its destination entries vector. */
        const size_t sizes[] = {32u, 32u, 32u, 8u * sizeof(string_t *), 8u * sizeof(string_t *)};
        size_t index = parser_reallocations++;

        if (index >= sizeof(sizes) / sizeof(sizes[0]) || pointer || size != sizes[index])
            parser_sequence_valid = false;
        if (parser_sequence_valid && index == 4u) {
            ++injected_failures;
            errno = ENOMEM;
            return NULL;
        }
    }
    return __real_realloc(pointer, size);
}

void __wrap_string_free(string_t *text)
{
    if (track_parser && injected_failures && text && strcmp(string_c_str(text), "1") == 0)
        ++parser_token_frees;
    __real_string_free(text);
}

string_t *__wrap_expr_to_text_symbolic(const expr_t *expr)
{
    const char *name = fail_constant ? expr_symbol_name(expr) : NULL;

    if (name && name[0] == 'C' && name[1]) {
        fail_constant = false;
        ++injected_failures;
        return NULL;
    }
    return __real_expr_to_text_symbolic(expr);
}

static void check_formatter_failure(size_t allocation, bool constant, mat_string_style_t style)
{
    matrix_t *matrix = mat_from_string_expr("{ (x+C1) | x=?; C1=? }", NULL);
    check_bool("allocation-failure fixture parses", matrix != NULL);
    if (!matrix)
        return;

    injected_failures = 0u;
    calloc_countdown = allocation;
    fail_constant = constant;
    string_t *text = mat_to_text(matrix, style);
    calloc_countdown = 0u;
    fail_constant = false;

    check_bool("formatter failure was injected once", injected_failures == 1u);
    if (style == MAT_STRING_FUNCTION)
        check_bool("failed Function formatting returns NULL", text == NULL);
    else
        check_bool("formatter recovers from allocation failure", text && strcmp(string_c_str(text), "<expr matrix>") == 0);
    string_free(text);
    text = mat_to_text(matrix, style);
    check_bool("matrix remains usable after formatting failure", text && strstr(string_c_str(text), "x"));
    string_free(text);
    mat_free(matrix);
}

static void test_mat_format_entry_array_allocation_failure(void)
{
    check_formatter_failure(1u, false, MAT_STRING_EXPRESSION);
}

static void test_mat_format_constant_array_allocation_failure(void)
{
    check_formatter_failure(2u, false, MAT_STRING_EXPRESSION);
}

static void test_mat_format_additive_constant_failure(void)
{
    check_formatter_failure(0u, true, MAT_STRING_EXPRESSION);
}

static void test_mat_format_function_constant_failure(void)
{
    check_formatter_failure(0u, true, MAT_STRING_FUNCTION);
}

static void test_mat_parse_row_transfer_allocation_failure(void)
{
    injected_failures = 0u;
    parser_reallocations = 0u;
    parser_token_frees = 0u;
    parser_sequence_valid = true;
    track_parser = true;
    matrix_t *matrix = mat_from_function_body_with_symbols("[[1]]", NULL, NULL, 0u);
    track_parser = false;

    check_bool("parser allocation sequence targets the entries vector", parser_sequence_valid && parser_reallocations == 5u);
    check_bool("parser failure was injected once", injected_failures == 1u);
    check_bool("failed row transfer rejects the matrix", matrix == NULL);
    check_bool("failed row transfer releases its untransferred token", parser_token_frees == 1u);
    mat_free(matrix);
}

static void test_mat_numeric_storage_survives_scope(void)
{
    matrix_t *matrices[5] = {0};
    number_t expected = num_create_from_string("1.25 + 0.5i");
    check_bool("numeric storage fixture accepts arbitrary precision", num_set_prec_bits(&expected, 256u) == 0);
    {
        NUM_SCOPE(scope);
        number_t value = num_clone(expected);
        number_t dense[] = {value, value, value, value};
        number_t diagonal[] = {value, value};
        matrices[0] = mat_create(2u, 2u, dense);
        matrices[1] = mat_create_diagonal(2u, diagonal);
        matrices[2] = mat_new_sparse(2u, 2u);
        if (matrices[2]) {
            number_t initial = num_neg(value);

            mat_set(matrices[2], 0u, 0u, &initial);
            mat_set(matrices[2], 0u, 0u, &value);
            mat_set(matrices[2], 1u, 1u, &initial);
            mat_set(matrices[2], 1u, 1u, &NUM_ZERO);
        }
        matrices[3] = matrices[0] ? mat_to_sparse(matrices[0]) : NULL;
        matrices[4] = matrices[3] ? mat_to_dense(matrices[3]) : NULL;
        num_destroy(&value);
    }
    for (size_t i = 0u; i < sizeof(matrices) / sizeof(matrices[0]); ++i) {
        check_bool("numeric matrix survives its creation scope", matrices[i] != NULL);
        if (matrices[i]) {
            number_t value;
            {
                NUM_SCOPE(scope);
                number_t temporary = mat_get_num(matrices[i], 0u, 0u);

                check_bool("scope-managed matrix read matches stored value", num_eq(temporary, expected));
                /* Deliberately leave the read scoped: Valgrind catches accidental detachment here. */
            }
            if (i == 1u) {
                {
                    NUM_SCOPE(scope);
                    number_t inserted = num_clone(expected);

                    mat_set(matrices[i], 0u, 1u, &inserted);
                }
                {
                    NUM_SCOPE(scope);
                    number_t inserted = mat_get_num(matrices[i], 0u, 1u);
                    number_t preserved = mat_get_num(matrices[i], 1u, 1u);

                    check_bool("materialised diagonal matrix retains new entry", num_eq(inserted, expected));
                    check_bool("materialised diagonal matrix preserves old entry", num_eq(preserved, expected));
                }
            }
            if (i == 2u) {
                NUM_SCOPE(scope);
                number_t removed = mat_get_num(matrices[i], 1u, 1u);

                check_bool("sparse replacement followed by zero removes the entry", num_is_zero(removed));
            }
            {
                NUM_SCOPE(scope);
                value = num_scope_detach(mat_get_num(matrices[i], 0u, 0u));
            }
            mat_free(matrices[i]);
            check_bool("detached matrix read survives scope and matrix release", num_eq(value, expected));
            check_bool("owning matrix read retains arbitrary precision", num_get_prec_bits(value) >= 256u);
            num_destroy(&value);
        }
    }
    num_destroy(&expected);
}

static void test_mat_bessel_y_survives_scope(void)
{
    size_t precision = num_get_default_prec_bits();
    check_bool("Bessel Y scope fixture accepts arbitrary precision", num_set_default_prec_bits(256u) == 0);
    number_t upper[] = {NUM_TWO, NUM_ONE, NUM_ZERO, NUM_TWO};
    number_t lower[] = {NUM_TWO, NUM_ZERO, NUM_ONE, NUM_TWO};
    const number_t *values[] = {upper, lower};
    number_t y1 = num_bessel_y(NUM_ONE, NUM_TWO);
    number_t expected = num_neg(y1);
    num_destroy(&y1);
    for (size_t i = 0u; i < 2u; ++i) {
        matrix_t *input = mat_create(2u, 2u, values[i]);
        matrix_t *result;
        {
            NUM_SCOPE(scope);
            result = input ? mat_bessel_y(input, &NUM_ZERO) : NULL;
        }
        check_bool("Bessel Y matrix survives its evaluation scope", result != NULL);
        if (result) {
            number_t value = mat_get_num(result, i, 1u - i);
            check_d("Bessel Y off-diagonal survives scope", num_to_double(value), num_to_double(expected), 1e-14);
            check_bool("Bessel Y off-diagonal retains precision", num_get_prec_bits(value) >= 256u);
            num_destroy(&value);

            number_t diagonal = mat_get_num(result, 0u, 0u);
            {
                NUM_SCOPE(scope);
                number_t inserted = num_clone(expected);

                /* Fill the opposite triangle, forcing upper/lower storage to materialise in place. */
                mat_set(result, 1u - i, i, &inserted);
            }
            {
                NUM_SCOPE(scope);
                number_t inserted = mat_get_num(result, 1u - i, i);
                number_t preserved = mat_get_num(result, i, 1u - i);
                number_t preserved_diagonal = mat_get_num(result, 0u, 0u);

                check_bool("materialised triangular matrix retains new entry", num_eq(inserted, expected));
                check_d("materialised triangular matrix preserves off-diagonal", num_to_double(preserved),
                         num_to_double(expected), 1e-14);
                check_bool("materialised triangular matrix preserves diagonal", num_eq(preserved_diagonal, diagonal));
                check_bool("materialised triangular matrix retains precision", num_get_prec_bits(preserved) >= 256u);
                /* All three read temporaries must be reclaimed by this scope. */
            }
            num_destroy(&diagonal);
        }
        mat_free(result);
        mat_free(input);
    }
    num_destroy(&expected);
    check_bool("Bessel Y scope fixture restores default precision", num_set_default_prec_bits(precision) == 0);
}

void run_matrix_memory_tests(void)
{
    TEST_RUN_CASE(test_mat_format_entry_array_allocation_failure, NULL);
    TEST_RUN_CASE(test_mat_format_constant_array_allocation_failure, NULL);
    TEST_RUN_CASE(test_mat_format_additive_constant_failure, NULL);
    TEST_RUN_CASE(test_mat_format_function_constant_failure, NULL);
    TEST_RUN_CASE(test_mat_parse_row_transfer_allocation_failure, NULL);
    TEST_RUN_CASE(test_mat_numeric_storage_survives_scope, NULL);
    TEST_RUN_CASE(test_mat_bessel_y_survives_scope, NULL);
}
