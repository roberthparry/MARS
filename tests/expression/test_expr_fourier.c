#include "test_expr.h"
#include "matrix.h"
#include "qcomplex.h"

static void test_signal_numeric_layers(void)
{
    NUM_SCOPE(scope);
    const double points[] = {-2, -1, -0.5, 0, 0.5, 1, 2};
    const double steps[] = {0, 0, 0, 0.5, 1, 1, 1};
    const double rectangles[] = {0, 0, 0.5, 1, 0.5, 0, 0};
    const double triangles[] = {0, 0, 0.5, 1, 0.5, 0, 0};
    const double circles[] = {0, 0.5, 1, 1, 1, 0.5, 0};
    for (size_t n = 0u; n < sizeof(points) / sizeof(points[0]); ++n) {
        qfloat_t x = qf_from_double(points[n]);
        qcomplex_t z = qc_make(x, QF_ZERO);
        number_t value = num_create_from_double(points[n]);
        ASSERT_TRUE(qf_to_double(qf_step(x)) == steps[n]);
        ASSERT_TRUE(qf_to_double(qf_rect(x)) == rectangles[n]);
        ASSERT_TRUE(qf_to_double(qf_tri(x)) == triangles[n]);
        ASSERT_TRUE(qf_to_double(qf_circ(x)) == circles[n]);
        ASSERT_TRUE(qf_to_double(qc_real(qc_step(z))) == steps[n]);
        ASSERT_TRUE(qf_to_double(qc_real(qc_rect(z))) == rectangles[n]);
        ASSERT_TRUE(qf_to_double(qc_real(qc_tri(z))) == triangles[n]);
        ASSERT_TRUE(qf_to_double(qc_real(qc_circ(z))) == circles[n]);
        ASSERT_TRUE(num_to_double(num_step(value)) == steps[n]);
        ASSERT_TRUE(num_to_double(num_rect(value)) == rectangles[n]);
        ASSERT_TRUE(num_to_double(num_tri(value)) == triangles[n]);
        ASSERT_TRUE(num_to_double(num_circ(value)) == circles[n]);
    }
    ASSERT_TRUE(qf_eq(qf_sinc(QF_ZERO), QF_ONE));
    ASSERT_TRUE(qc_eq(qc_sinc(qc_make(QF_ZERO, QF_ZERO)), qc_make(QF_ONE, QF_ZERO)));
    ASSERT_TRUE(num_eq(num_sinc(NUM_ZERO), NUM_ONE));
    ASSERT_TRUE(num_is_nan(num_step(NUM_I)));
    ASSERT_TRUE(qc_isnan(qc_rect(qc_make(QF_ONE, QF_ONE))));
    ASSERT_TRUE(qf_isnan(qf_tri(QF_NAN)));
    ASSERT_TRUE(num_is_zero(num_tri(NUM_INF)));
    ASSERT_TRUE(num_is_zero(num_rect(NUM_INF)));
}

static void test_signal_spectral_matrices(void)
{
    NUM_SCOPE(scope);
    number_t diagonal[] = {NUM_NEG_ONE, NUM_ZERO, NUM_ONE};
    matrix_t *matrix = mat_create_diagonal(3u, diagonal);
    matrix_t *step = mat_step(matrix);
    matrix_t *rect = mat_rect(matrix);
    matrix_t *tri = mat_tri(matrix);
    matrix_t *circ = mat_circ(matrix);
    matrix_t *sinc = mat_sinc(matrix);
    ASSERT_TRUE(step && rect && tri && circ && sinc);
    if (step && rect && tri && circ && sinc) {
        ASSERT_TRUE(num_eq(mat_get_num(step, 0, 0), NUM_ZERO));
        ASSERT_TRUE(num_eq(mat_get_num(step, 1, 1), NUM_HALF));
        ASSERT_TRUE(num_eq(mat_get_num(step, 2, 2), NUM_ONE));
        ASSERT_TRUE(num_eq(mat_get_num(rect, 1, 1), NUM_ONE));
        ASSERT_TRUE(num_eq(mat_get_num(tri, 1, 1), NUM_ONE));
        ASSERT_TRUE(num_eq(mat_get_num(circ, 0, 0), NUM_HALF));
        ASSERT_TRUE(num_eq(mat_get_num(sinc, 1, 1), NUM_ONE));
        ASSERT_TRUE(num_eq(mat_get_num(step, 0, 1), NUM_ZERO));
        ASSERT_TRUE(num_eq(mat_get_num(sinc, 0, 1), NUM_ZERO));
    }
    mat_free(sinc);
    mat_free(circ);
    mat_free(tri);
    mat_free(rect);
    mat_free(step);
    mat_free(matrix);
    number_t nilpotent[] = {NUM_ZERO, NUM_ONE, NUM_ZERO, NUM_ZERO, NUM_ZERO, NUM_ONE,
                           NUM_ZERO, NUM_ZERO, NUM_ZERO};
    matrix_t *jordan = mat_create(3, 3, nilpotent);
    matrix_t *entire = mat_sinc(jordan);
    ASSERT_TRUE(entire != NULL);
    if (entire) {
        ASSERT_TRUE(num_eq(mat_get_num(entire, 0, 0), NUM_ONE));
        ASSERT_TRUE(num_is_zero(mat_get_num(entire, 0, 1)));
        ASSERT_TRUE(fabs(num_to_double(mat_get_num(entire, 0, 2)) + M_PI*M_PI/6.0) < 1e-12);
    }
    mat_free(entire);
    mat_free(jordan);
}

static void test_signal_calculus_and_finite_sums(void)
{
    NUM_SCOPE(scope);
    expr_t *x = expr_new_named_var(NUM_ZERO, "x");
    expr_t *f = expr_sinc(x);
    expr_t *derivative = expr_create_deriv(f, x);
    expr_t *second = expr_create_2nd_deriv(f, x, x);
    ASSERT_TRUE(derivative && second);
    if (derivative && second) {
        ASSERT_TRUE(num_is_zero(expr_eval(derivative)));
        ASSERT_TRUE(fabs(num_to_double(expr_eval(second)) + M_PI * M_PI / 3.0) < 1e-12);
    }
    expr_t *primitive = expr_integrate(f, x);
    ASSERT_TRUE(primitive != NULL);
    expr_t *back = primitive ? expr_create_deriv(primitive, x) : NULL;
    ASSERT_TRUE(back != NULL);
    if (back)
        ASSERT_TRUE(fabs(num_to_double(expr_eval(back)) - 1.0) < 1e-12);
    expr_t *window = expr_rect(x);
    expr_t *lower = expr_new_const(NUM_NEG_ONE);
    expr_t *upper = expr_new_const(NUM_ONE);
    expr_t *sum = expr_new_finite_summation_range(window, x, lower, upper);
    ASSERT_TRUE(sum && num_eq(expr_eval(sum), NUM_ONE));
    expr_free(sum);
    expr_free(upper);
    expr_free(lower);
    expr_free(window);
    expr_free(back);
    expr_free(primitive);
    expr_free(second);
    expr_free(derivative);
    expr_free(f);
    expr_free(x);
}

void test_fourier_and_signal_functions(void)
{
    TEST_RUN_SUBTEST(test_signal_numeric_layers, NULL);
    TEST_RUN_SUBTEST(test_signal_spectral_matrices, NULL);
    TEST_RUN_SUBTEST(test_signal_calculus_and_finite_sums, NULL);
}

/* README examples from the qfloat, qcomplex, number and matrix module guides. */
void example_signal_readme_examples(void)
{
    NUM_SCOPE(scope);
    double half = qf_to_double(qf_step(QF_ZERO));
    ASSERT_TRUE(half == 0.5);
    printf("step(0) = %.1f\n", half);

    qcomplex_t value = qc_sinc(qc_make(QF_ZERO, QF_ZERO));
    ASSERT_TRUE(qc_eq(value, qc_make(QF_ONE, QF_ZERO)));
    printf("sinc(0) = %.0f + %.0fi\n", qf_to_double(qc_real(value)), qf_to_double(qc_imag(value)));

    number_t edge = num_rect(NUM_HALF);
    ASSERT_TRUE(num_eq(edge, NUM_HALF));
    printf("rect(1/2) = %.1f\n", num_to_double(edge));

    number_t diagonal[] = {NUM_NEG_ONE, NUM_ZERO, NUM_ONE};
    matrix_t *a = mat_create_diagonal(3u, diagonal);
    matrix_t *b = mat_step(a);
    ASSERT_TRUE(b != NULL);
    if (b) {
        ASSERT_TRUE(num_eq(mat_get_num(b, 0, 0), NUM_ZERO));
        ASSERT_TRUE(num_eq(mat_get_num(b, 1, 1), NUM_HALF));
        ASSERT_TRUE(num_eq(mat_get_num(b, 2, 2), NUM_ONE));
        printf("step(diag(-1, 0, 1)) = diag(%.1f, %.1f, %.1f)\n",
               num_to_double(mat_get_num(b, 0, 0)), num_to_double(mat_get_num(b, 1, 1)),
               num_to_double(mat_get_num(b, 2, 2)));
    }
    mat_free(b);
    mat_free(a);
}
