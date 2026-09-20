#include "test_expr.h"
#include "matrix.h"
#include "qcomplex.h"

static void test_orthopoly_numeric_and_matrix_layers(void)
{
    NUM_SCOPE(scope);
    qfloat_t two = qf_from_double(2);
    ASSERT_TRUE(qf_to_double(qf_chebyshev_t(3, two)) == 26);
    ASSERT_TRUE(qf_to_double(qf_chebyshev_u(3, two)) == 56);
    ASSERT_TRUE(qf_to_double(qf_hermite_h(3, two)) == 40);
    qcomplex_t imaginary = qc_make(QF_ZERO, QF_ONE);
    ASSERT_TRUE(qf_to_double(qc_imag(qc_chebyshev_t(3, imaginary))) == -7);
    ASSERT_TRUE(qf_to_double(qc_imag(qc_chebyshev_u(3, imaginary))) == -12);
    ASSERT_TRUE(qf_to_double(qc_imag(qc_hermite_h(3, imaginary))) == -20);
    number_t three = num_create_from_long(3);
    ASSERT_TRUE(num_to_double(num_chebyshev_t(three, NUM_TWO)) == 26);
    ASSERT_TRUE(num_to_double(num_chebyshev_u(three, NUM_TWO)) == 56);
    ASSERT_TRUE(num_to_double(num_hermite_h(three, NUM_TWO)) == 40);
    ASSERT_TRUE(num_eq(num_hermite_h(three, NUM_I), num_mul_long(NUM_I, -20)));
    ASSERT_TRUE(num_is_nan(num_hermite_h(NUM_HALF, NUM_TWO)));
    ASSERT_TRUE(num_is_nan(num_chebyshev_t(NUM_NEG_ONE, NUM_TWO)));
    ASSERT_TRUE(num_eq(num_chebyshev_t(NUM_ZERO, NUM_ONE), NUM_ONE));
    number_t elements[] = {NUM_ZERO, NUM_ONE, NUM_ZERO, NUM_ZERO};
    matrix_t *a = mat_create(2, 2, elements);
    matrix_t *t = mat_chebyshev_t(a, 3), *u = mat_chebyshev_u(a, 3), *h = mat_hermite_h(a, 3);
    ASSERT_TRUE(t && u && h);
    if (t && u && h) {
        ASSERT_TRUE(num_to_double(mat_get_num(t, 0, 1)) == -3);
        ASSERT_TRUE(num_to_double(mat_get_num(u, 0, 1)) == -4);
        ASSERT_TRUE(num_to_double(mat_get_num(h, 0, 1)) == -12);
        ASSERT_TRUE(num_is_zero(mat_get_num(h, 1, 0)));
    }
    mat_free(t);
    mat_free(u);
    mat_free(h);
    mat_free(a);
}

void test_orthogonal_polynomials(void)
{
    TEST_RUN_SUBTEST(test_orthopoly_numeric_and_matrix_layers, NULL);
}

/* README examples from the scalar and matrix module guides. */
void example_orthopoly_readme_examples(void)
{
    NUM_SCOPE(scope);
    ASSERT_TRUE(qf_to_double(qf_chebyshev_t(3, qf_from_double(2))) == 26);
    printf("T3(2) = 26\n");
    qcomplex_t h = qc_hermite_h(3, qc_make(QF_ZERO, QF_ONE));
    ASSERT_TRUE(qf_to_double(qc_imag(h)) == -20 && qf_eq(qc_real(h), QF_ZERO));
    printf("Hermite H3(i) = -20i\n");
    ASSERT_TRUE(num_to_double(num_hermite_h(num_create_from_long(3), NUM_TWO)) == 40);
    printf("Hermite H3(2) = 40\n");
    number_t values[] = {NUM_ZERO, NUM_ONE, NUM_ZERO, NUM_ZERO};
    matrix_t *a = mat_create(2, 2, values), *t = mat_chebyshev_t(a, 3);
    ASSERT_TRUE(t && num_to_double(mat_get_num(t, 0, 1)) == -3);
    printf("T3((0, 1; 0, 0)) = (0, -3; 0, 0)\n");
    mat_free(t);
    mat_free(a);
}
