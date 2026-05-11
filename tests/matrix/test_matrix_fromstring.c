#include <stdint.h>

#include "test_matrix.h"

static void test_mat_from_string_numeric_qf(void)
{
    matrix_t *A = mat_from_string("(1, 2; 3, 4)", NULL);
    qfloat_t x = QF_ZERO;

    check_bool("mat_from_string qfloat matrix non-null", A != NULL);
    check_bool("mat_from_string qfloat matrix type", A && mat_typeof(A) == MAT_TYPE_QFLOAT);
    check_bool("mat_from_string qfloat rows", A && mat_get_row_count(A) == 2);
    check_bool("mat_from_string qfloat cols", A && mat_get_col_count(A) == 2);
    if (A) {
        mat_get(A, 1, 0, &x);
        check_qf_val("mat_from_string qfloat A[1,0]", x, qf_from_double(3.0), 1e-18);
    }

    mat_free(A);
}

static void test_mat_from_string_numeric_qc(void)
{
    matrix_t *A = mat_from_string("((1,2), 3i-1; 4, (5,-6); 3, 2j+4)", NULL);
    qcomplex_t z = QC_ZERO;

    check_bool("mat_from_string qcomplex matrix non-null", A != NULL);
    check_bool("mat_from_string qcomplex matrix type", A && mat_typeof(A) == MAT_TYPE_QCOMPLEX);
    if (A) {
        mat_get(A, 0, 0, &z);
        check_qc_val("mat_from_string qcomplex A[0,0]",
                     z, qc_make(qf_from_double(1.0), qf_from_double(2.0)), 1e-18);
        mat_get(A, 1, 1, &z);
        check_qc_val("mat_from_string qcomplex A[1,1]",
                     z, qc_make(qf_from_double(5.0), qf_from_double(-6.0)), 1e-18);
        mat_get(A, 2, 1, &z);
        check_qc_val("mat_from_string qcomplex A[2,1]",
                     z, qc_make(qf_from_double(4.0), qf_from_double(2.0)), 1e-18);
    }

    mat_free(A);
}

static void test_mat_from_string_symbolic_wrapped(void)
{
    mat_bindings_t *bindings = NULL;
    matrix_t *A = mat_from_string("{ (x, 1; 1, c1) | x = 2; c1 = 3 }",
                                  &bindings);
    dval_t *dv = NULL;
    dval_t *x_binding;
    dval_t *c_binding;

    check_bool("mat_from_string wrapped symbolic matrix non-null", A != NULL);
    check_bool("mat_from_string wrapped symbolic matrix type", A && mat_typeof(A) == MAT_TYPE_DVAL);
    x_binding = mat_bindings_get(bindings, "x");
    c_binding = mat_bindings_get(bindings, "c₁");
    check_bool("wrapped symbolic binding x present", x_binding != NULL);
    check_bool("wrapped symbolic binding c₁ present", c_binding != NULL);

    if (A) {
        mat_get(A, 1, 1, &dv);
        check_qf_val("wrapped symbolic A[1,1] initial",
                     dv_eval_qf(dv), qf_from_double(3.0), 1e-18);
        if (c_binding)
            test_dv_set_val_qf(c_binding, qf_from_double(5.0));
        check_qf_val("wrapped symbolic A[1,1] tracks binding update",
                     dv_eval_qf(dv), qf_from_double(5.0), 1e-18);
    }

    mat_bindings_free(bindings);
    mat_free(A);
}

static void test_mat_from_string_symbolic_bare(void)
{
    mat_bindings_t *bindings = NULL;
    matrix_t *A = mat_from_string("(c1, c2*y, c2*x; x, y, z; a, b, c)",
                                  &bindings);
    dval_t *dv = NULL;
    dval_t *x_binding;
    dval_t *y_binding;
    dval_t *c2_binding;
    qfloat_t x_initial = QF_ZERO;
    qfloat_t c2_initial = QF_ZERO;

    check_bool("mat_from_string bare symbolic matrix non-null", A != NULL);
    check_bool("mat_from_string bare symbolic matrix type", A && mat_typeof(A) == MAT_TYPE_DVAL);
    check_bool("mat_from_string bare bindings returned", bindings != NULL);

    x_binding = mat_bindings_get(bindings, "x");
    y_binding = mat_bindings_get(bindings, "y");
    c2_binding = mat_bindings_get(bindings, "c₂");
    check_bool("bare symbolic x binding present", x_binding != NULL);
    check_bool("bare symbolic y binding present", y_binding != NULL);
    check_bool("bare symbolic c₂ binding present", c2_binding != NULL);

    if (x_binding)
        x_initial = dv_eval_qf(x_binding);
    if (c2_binding)
        c2_initial = dv_eval_qf(c2_binding);
    check_bool("bare symbolic x starts as NaN",
               x_binding && qf_isnan(x_initial));
    check_bool("bare symbolic c₂ starts as NaN",
               c2_binding && qf_isnan(c2_initial));

    check_bool("bare symbolic set x binding",
               test_mat_bindings_set_qf(bindings, "x", qf_from_double(2.0)) == 0);
    check_bool("bare symbolic set y binding",
               test_mat_bindings_set_qf(bindings, "y", qf_from_double(3.0)) == 0);
    check_bool("bare symbolic set c₂ binding",
               test_mat_bindings_set_qf(bindings, "c₂", qf_from_double(5.0)) == 0);

    if (A) {
        mat_get(A, 0, 1, &dv);
        check_qf_val("bare symbolic c₂*y",
                     dv_eval_qf(dv), qf_from_double(15.0), 1e-18);
        mat_get(A, 0, 2, &dv);
        check_qf_val("bare symbolic c₂*x",
                     dv_eval_qf(dv), qf_from_double(10.0), 1e-18);
    }

    mat_bindings_free(bindings);
    mat_free(A);
}

static void test_mat_from_string_symbolic_at_aliases(void)
{
    mat_bindings_t *bindings = NULL;
    matrix_t *A = mat_from_string("(@DELTA, @OMEGA; @OMEGA, -@DELTA)",
                                  &bindings);
    dval_t *dv = NULL;

    check_bool("mat_from_string @alias symbolic matrix non-null", A != NULL);
    check_bool("mat_from_string @alias symbolic matrix type", A && mat_typeof(A) == MAT_TYPE_DVAL);
    check_bool("mat_from_string @alias Δ binding present",
               mat_bindings_get(bindings, "Δ") != NULL);
    check_bool("mat_from_string @alias Ω binding present",
               mat_bindings_get(bindings, "Ω") != NULL);
    check_bool("mat_from_string @alias @DELTA binding present",
               mat_bindings_get(bindings, "@DELTA") != NULL);
    check_bool("mat_from_string @alias @OMEGA binding present",
               mat_bindings_get(bindings, "@OMEGA") != NULL);
    check_bool("mat_from_string @alias set @DELTA",
               test_mat_bindings_set_d(bindings, "@DELTA", 2.0) == 0);
    check_bool("mat_from_string @alias set @OMEGA",
               test_mat_bindings_set_d(bindings, "@OMEGA", 3.0) == 0);

    if (A) {
        mat_get(A, 0, 0, &dv);
        check_qf_val("@alias symbolic Δ entry",
                     dv_eval_qf(dv), qf_from_double(2.0), 1e-18);
        mat_get(A, 0, 1, &dv);
        check_qf_val("@alias symbolic Ω entry",
                     dv_eval_qf(dv), qf_from_double(3.0), 1e-18);
        mat_get(A, 1, 1, &dv);
        check_qf_val("@alias symbolic -Δ entry",
                     dv_eval_qf(dv), qf_from_double(-2.0), 1e-18);
    }

    mat_bindings_free(bindings);
    mat_free(A);
}

static void test_mat_from_string_symbolic_math_conventions(void)
{
    mat_bindings_t *bindings = NULL;
    matrix_t *A = mat_from_string("(x, e; pi, τ; @phi, @gamma; [radius], c1; a, d_2)",
                                  &bindings);
    dval_t *x_binding;
    dval_t *e_binding;
    dval_t *pi_binding;
    dval_t *phi_binding;
    dval_t *gamma_binding;
    dval_t *tau_binding;
    dval_t *radius_binding;
    dval_t *c1_binding;
    dval_t *a_binding;
    dval_t *d2_binding;
    dval_t *dv = NULL;
    qfloat_t e_initial = QF_ZERO;
    qfloat_t pi_initial = QF_ZERO;
    qfloat_t phi_initial = QF_ZERO;
    qfloat_t gamma_initial = QF_ZERO;
    qfloat_t tau_initial = QF_ZERO;

    check_bool("mat_from_string math-convention symbolic matrix non-null", A != NULL);
    check_bool("mat_from_string math-convention symbolic matrix type",
               A && mat_typeof(A) == MAT_TYPE_DVAL);

    x_binding = mat_bindings_get(bindings, "x");
    e_binding = mat_bindings_get(bindings, "e");
    pi_binding = mat_bindings_get(bindings, "π");
    phi_binding = mat_bindings_get(bindings, "φ");
    gamma_binding = mat_bindings_get(bindings, "γ");
    tau_binding = mat_bindings_get(bindings, "τ");
    radius_binding = mat_bindings_get(bindings, "radius");
    c1_binding = mat_bindings_get(bindings, "c₁");
    a_binding = mat_bindings_get(bindings, "a");
    d2_binding = mat_bindings_get(bindings, "d₂");

    check_bool("math-convention x binding present", x_binding != NULL);
    check_bool("math-convention e binding present", e_binding != NULL);
    check_bool("math-convention π binding present", pi_binding != NULL);
    check_bool("math-convention φ binding present", phi_binding != NULL);
    check_bool("math-convention γ binding present", gamma_binding != NULL);
    check_bool("math-convention τ binding present", tau_binding != NULL);
    check_bool("math-convention radius binding present", radius_binding != NULL);
    check_bool("math-convention c₁ binding present", c1_binding != NULL);
    check_bool("math-convention a binding present", a_binding != NULL);
    check_bool("math-convention d₂ binding present", d2_binding != NULL);

    if (e_binding)
        e_initial = dv_eval_qf(e_binding);
    if (pi_binding)
        pi_initial = dv_eval_qf(pi_binding);
    if (phi_binding)
        phi_initial = dv_eval_qf(phi_binding);
    if (gamma_binding)
        gamma_initial = dv_eval_qf(gamma_binding);
    if (tau_binding)
        tau_initial = dv_eval_qf(tau_binding);

    check_qf_val("math-convention e built-in value",
                 e_initial, QF_E, 1e-30);
    check_qf_val("math-convention π built-in value",
                 pi_initial, QF_PI, 1e-30);
    check_qf_val("math-convention φ built-in value",
                 phi_initial,
                 qf_div(qf_add(qf_from_double(1.0), qf_sqrt(qf_from_double(5.0))),
                        qf_from_double(2.0)),
                 1e-30);
    check_qf_val("math-convention γ built-in value",
                 gamma_initial, QF_EULER_MASCHERONI, 1e-30);
    check_bool("math-convention τ still starts as variable NaN",
               tau_binding && qf_isnan(tau_initial));

    if (A) {
        mat_get(A, 0, 1, &dv);
        check_qf_val("math-convention matrix e entry",
                     dv_eval_qf(dv), QF_E, 1e-30);
        mat_get(A, 1, 0, &dv);
        check_qf_val("math-convention matrix π entry",
                     dv_eval_qf(dv), QF_PI, 1e-30);
        mat_get(A, 2, 0, &dv);
        check_qf_val("math-convention matrix φ entry",
                     dv_eval_qf(dv),
                     qf_div(qf_add(qf_from_double(1.0), qf_sqrt(qf_from_double(5.0))),
                            qf_from_double(2.0)),
                     1e-30);
        mat_get(A, 2, 1, &dv);
        check_qf_val("math-convention matrix γ entry",
                     dv_eval_qf(dv), QF_EULER_MASCHERONI, 1e-30);
    }

    mat_bindings_free(bindings);
    mat_free(A);
}

static void test_mat_from_string_invalid_syntax(void)
{
    mat_bindings_t *bindings = (mat_bindings_t *)(uintptr_t)1;
    matrix_t *A;

    A = mat_from_string("(1, 2; 3)", &bindings);
    check_bool("mat_from_string rejects ragged matrix", A == NULL);
    check_bool("mat_from_string ragged clears bindings", bindings == NULL);

    bindings = (mat_bindings_t *)(uintptr_t)1;
    A = mat_from_string("(1, 2; 3, 4", &bindings);
    check_bool("mat_from_string rejects missing closing paren", A == NULL);
    check_bool("mat_from_string missing paren clears bindings", bindings == NULL);

    bindings = (mat_bindings_t *)(uintptr_t)1;
    A = mat_from_string("{ (x, 1; 1, y) | x = }", &bindings);
    check_bool("mat_from_string rejects invalid binding syntax", A == NULL);
    check_bool("mat_from_string invalid binding clears bindings", bindings == NULL);

    bindings = (mat_bindings_t *)(uintptr_t)1;
    A = mat_from_string("(Δ, Ω; Ω, -)", &bindings);
    check_bool("mat_from_string rejects invalid symbolic expression", A == NULL);
    check_bool("mat_from_string invalid symbolic clears bindings", bindings == NULL);
}

static void test_mat_from_string_bracketed_names(void)
{
    mat_bindings_t *bindings = NULL;
    matrix_t *A = mat_from_string("{ ([radius], [scale]*x; y, [offset]) | x = 2, y = 5; [radius] = 3, [scale] = 4, [offset] = 7 }",
                                  &bindings);
    dval_t *dv = NULL;

    check_bool("mat_from_string bracketed symbolic matrix non-null", A != NULL);
    check_bool("mat_from_string bracketed symbolic matrix type", A && mat_typeof(A) == MAT_TYPE_DVAL);
    check_bool("mat_from_string bracketed binding radius present",
               mat_bindings_get(bindings, "[radius]") != NULL);
    check_bool("mat_from_string bracketed binding scale present",
               mat_bindings_get(bindings, "scale") != NULL);
    check_bool("mat_from_string bracketed binding offset present",
               mat_bindings_get(bindings, "[offset]") != NULL);

    if (A) {
        mat_get(A, 0, 0, &dv);
        check_qf_val("bracketed symbolic [radius]",
                     dv_eval_qf(dv), qf_from_double(3.0), 1e-18);
        mat_get(A, 0, 1, &dv);
        check_qf_val("bracketed symbolic [scale]*x",
                     dv_eval_qf(dv), qf_from_double(8.0), 1e-18);
        mat_get(A, 1, 1, &dv);
        check_qf_val("bracketed symbolic [offset]",
                     dv_eval_qf(dv), qf_from_double(7.0), 1e-18);
    }

    mat_bindings_free(bindings);
    mat_free(A);
}

static void test_mat_symbolic_derivative_helpers_by_name(void)
{
    mat_bindings_t *bindings = NULL;
    matrix_t *A = mat_from_string("([radius], x*y; y, c1)", &bindings);
    matrix_t *Dr = NULL;
    dval_t *dtr = NULL;
    dval_t *ddet = NULL;
    dval_t *dv = NULL;

    check_bool("mat symbolic helpers source non-null", A != NULL);
    check_bool("mat symbolic helpers set x",
               test_mat_bindings_set_d(bindings, "x", 2.0) == 0);
    check_bool("mat symbolic helpers set y",
               test_mat_bindings_set_d(bindings, "y", 3.0) == 0);
    check_bool("mat symbolic helpers set [radius]",
               test_mat_bindings_set_d(bindings, "[radius]", 5.0) == 0);
    check_bool("mat symbolic helpers set c₁",
               test_mat_bindings_set_d(bindings, "c₁", 7.0) == 0);

    Dr = mat_deriv_by_name(A, bindings, "[radius]");
    dtr = mat_deriv_trace_by_name(A, bindings, "[radius]");
    ddet = mat_deriv_det_by_name(A, bindings, "[radius]");

    check_bool("mat_deriv_by_name([radius]) not NULL", Dr != NULL);
    check_bool("mat_deriv_trace_by_name([radius]) not NULL", dtr != NULL);
    check_bool("mat_deriv_det_by_name([radius]) not NULL", ddet != NULL);
    check_bool("mat_deriv_by_name missing symbol returns NULL",
               mat_deriv_by_name(A, bindings, "missing") == NULL);

    if (Dr) {
        mat_get(Dr, 0, 0, &dv);
        check_qf_val("mat_deriv_by_name [0,0] = 1",
                     dv_eval_qf(dv), qf_from_double(1.0), 1e-18);
        mat_get(Dr, 0, 1, &dv);
        check_qf_val("mat_deriv_by_name [0,1] = 0",
                     dv_eval_qf(dv), qf_from_double(0.0), 1e-18);
        mat_get(Dr, 1, 0, &dv);
        check_qf_val("mat_deriv_by_name [1,0] = 0",
                     dv_eval_qf(dv), qf_from_double(0.0), 1e-18);
        mat_get(Dr, 1, 1, &dv);
        check_qf_val("mat_deriv_by_name [1,1] = 0",
                     dv_eval_qf(dv), qf_from_double(0.0), 1e-18);
    }

    if (dtr)
        check_qf_val("mat_deriv_trace_by_name([radius]) = 1",
                     dv_eval_qf(dtr), qf_from_double(1.0), 1e-18);
    if (ddet)
        check_qf_val("mat_deriv_det_by_name([radius]) = c₁",
                     dv_eval_qf(ddet), qf_from_double(7.0), 1e-18);

    dv_free(ddet);
    dv_free(dtr);
    mat_free(Dr);
    mat_bindings_free(bindings);
    mat_free(A);
}

static void test_mat_symbolic_jacobian_helper_by_names(void)
{
    mat_bindings_t *bindings = NULL;
    matrix_t *A = mat_from_string("(x, x*y)", &bindings);
    const char *names[2] = {"x", "y"};
    matrix_t *J = NULL;
    dval_t *dv = NULL;

    check_bool("mat symbolic Jacobian helper source non-null", A != NULL);
    check_bool("mat symbolic Jacobian helper set x",
               test_mat_bindings_set_d(bindings, "x", 2.0) == 0);
    check_bool("mat symbolic Jacobian helper set y",
               test_mat_bindings_set_d(bindings, "y", 3.0) == 0);

    J = mat_jacobian_by_names(A, bindings, names, 2);
    check_bool("mat_jacobian_by_names not NULL", J != NULL);
    check_bool("mat_jacobian_by_names rows", J && mat_get_row_count(J) == 2);
    check_bool("mat_jacobian_by_names cols", J && mat_get_col_count(J) == 2);
    check_bool("mat_jacobian_by_names missing symbol returns NULL",
               mat_jacobian_by_names(A, bindings,
                                     (const char *const[]){"x", "missing"}, 2) == NULL);

    if (J) {
        mat_get(J, 0, 0, &dv);
        check_qf_val("mat_jacobian_by_names [0,0] = 1",
                     dv_eval_qf(dv), qf_from_double(1.0), 1e-18);
        mat_get(J, 0, 1, &dv);
        check_qf_val("mat_jacobian_by_names [0,1] = 0",
                     dv_eval_qf(dv), qf_from_double(0.0), 1e-18);
        mat_get(J, 1, 0, &dv);
        check_qf_val("mat_jacobian_by_names [1,0] = y",
                     dv_eval_qf(dv), qf_from_double(3.0), 1e-18);
        mat_get(J, 1, 1, &dv);
        check_qf_val("mat_jacobian_by_names [1,1] = x",
                     dv_eval_qf(dv), qf_from_double(2.0), 1e-18);
    }

    mat_free(J);
    mat_bindings_free(bindings);
    mat_free(A);
}

static void test_mat_symbolic_matrix_calculus_helpers_by_name(void)
{
    mat_bindings_t *bindings = NULL;
    matrix_t *A = mat_from_string("(x, 1; y, 2)", &bindings);
    dval_t *x_binding = NULL;
    dval_t *y_binding = NULL;
    dval_t *b_entries[2] = {NULL, NULL};
    matrix_t *B = NULL;
    matrix_t *dAi = NULL;
    matrix_t *dAbi = NULL;
    matrix_t *dX = NULL;
    matrix_t *dXb = NULL;
    dval_t *dv = NULL;

    check_bool("mat symbolic calculus by-name source A non-null", A != NULL);
    x_binding = mat_bindings_get(bindings, "x");
    y_binding = mat_bindings_get(bindings, "y");
    check_bool("mat symbolic calculus by-name shared x binding present", x_binding != NULL);
    check_bool("mat symbolic calculus by-name shared y binding present", y_binding != NULL);
    if (x_binding && y_binding) {
        b_entries[0] = x_binding;
        b_entries[1] = y_binding;
        B = mat_create_dv(2, 1, b_entries);
    }
    check_bool("mat symbolic calculus by-name source B non-null", B != NULL);

    check_bool("mat symbolic calculus by-name set A x",
               test_mat_bindings_set_d(bindings, "x", 2.0) == 0);
    check_bool("mat symbolic calculus by-name set A y",
               test_mat_bindings_set_d(bindings, "y", 3.0) == 0);

    dAi = mat_deriv_inverse_by_name(A, bindings, "x");
    dAbi = mat_deriv_block_inverse_by_name(A, 1, bindings, "x");
    dX = mat_deriv_solve_by_name(A, B, bindings, "x");
    dXb = mat_deriv_block_solve_by_name(A, B, 1, bindings, "x");

    check_bool("mat_deriv_inverse_by_name(x) not NULL", dAi != NULL);
    check_bool("mat_deriv_block_inverse_by_name(x) not NULL", dAbi != NULL);
    check_bool("mat_deriv_solve_by_name(x) not NULL", dX != NULL);
    check_bool("mat_deriv_block_solve_by_name(x) not NULL", dXb != NULL);
    check_bool("mat_deriv_inverse_by_name missing symbol returns NULL",
               mat_deriv_inverse_by_name(A, bindings, "missing") == NULL);

    if (dAi) {
        mat_get(dAi, 0, 0, &dv);
        check_qf_val("mat_deriv_inverse_by_name [0,0]",
                     dv_eval_qf(dv), qf_from_double(-4.0), 1e-18);
        mat_get(dAi, 1, 0, &dv);
        check_qf_val("mat_deriv_inverse_by_name [1,0]",
                     dv_eval_qf(dv), qf_from_double(6.0), 1e-18);
    }

    if (dAbi) {
        mat_get(dAbi, 0, 1, &dv);
        check_qf_val("mat_deriv_block_inverse_by_name [0,1]",
                     dv_eval_qf(dv), qf_from_double(2.0), 1e-18);
        mat_get(dAbi, 1, 1, &dv);
        check_qf_val("mat_deriv_block_inverse_by_name [1,1]",
                     dv_eval_qf(dv), qf_from_double(-3.0), 1e-18);
    }

    if (dX) {
        mat_get(dX, 0, 0, &dv);
        check_qf_val("mat_deriv_solve_by_name [0,0]",
                     dv_eval_qf(dv), qf_from_double(0.0), 1e-18);
        mat_get(dX, 1, 0, &dv);
        check_qf_val("mat_deriv_solve_by_name [1,0]",
                     dv_eval_qf(dv), qf_from_double(0.0), 1e-18);
    }

    if (dXb) {
        mat_get(dXb, 0, 0, &dv);
        check_qf_val("mat_deriv_block_solve_by_name [0,0]",
                     dv_eval_qf(dv), qf_from_double(0.0), 1e-18);
        mat_get(dXb, 1, 0, &dv);
        check_qf_val("mat_deriv_block_solve_by_name [1,0]",
                     dv_eval_qf(dv), qf_from_double(0.0), 1e-18);
    }

    mat_free(dXb);
    mat_free(dX);
    mat_free(dAbi);
    mat_free(dAi);
    mat_free(B);
    mat_bindings_free(bindings);
    mat_free(A);
}

void run_matrix_fromstring_tests(void)
{
    test_mat_from_string_numeric_qf();
    test_mat_from_string_numeric_qc();
    test_mat_from_string_symbolic_wrapped();
    test_mat_from_string_symbolic_bare();
    test_mat_from_string_symbolic_at_aliases();
    test_mat_from_string_symbolic_math_conventions();
    test_mat_from_string_bracketed_names();
    test_mat_symbolic_derivative_helpers_by_name();
    test_mat_symbolic_jacobian_helper_by_names();
    test_mat_symbolic_matrix_calculus_helpers_by_name();
    test_mat_from_string_invalid_syntax();
}
