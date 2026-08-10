#include <stdint.h>

#include "test_matrix.h"

static void check_matrix_fromstring_expr_double(const char *label, const expr_t *dv, double expected, double tol)
{
    number_t got = expr_eval(dv);
    number_t want = num_create_from_double(expected);
    number_t diff = num_sub(got, want);
    number_t mag = num_abs(diff);
    double err = num_to_double(mag);

    check_bool(label, err <= tol);

    num_destroy(&mag);
    num_destroy(&diff);
    num_destroy(&want);
    num_destroy(&got);
}

static void check_matrix_fromstring_expr_num(const char *label, const expr_t *dv, number_t expected, double tol)
{
    number_t got = expr_eval(dv);
    number_t diff = num_sub(got, expected);
    number_t mag = num_abs(diff);
    double err = num_to_double(mag);

    check_bool(label, err <= tol);

    num_destroy(&mag);
    num_destroy(&diff);
    num_destroy(&got);
}

static void check_matrix_fromstring_num(const char *label, number_t got, number_t expected, double tol)
{
    number_t diff = num_sub(got, expected);
    number_t mag = num_abs(diff);
    double err = num_to_double(mag);

    check_bool(label, err <= tol);

    num_destroy(&mag);
    num_destroy(&diff);
}

static void test_mat_from_string_numeric_num_real(void)
{
    matrix_t *A = mat_from_string("(1/2, 2; 3, 4.5)");
    number_t x = NUM_ZERO;
    number_t expected = num_create_from_string("3");
    number_t half = num_create_from_string("1/2");

    check_bool("mat_from_string number matrix non-null", A != NULL);
    check_bool("mat_from_string number matrix type", A && mat_typeof(A) == MAT_TYPE_NUMBER);
    check_bool("mat_from_string number rows", A && mat_get_row_count(A) == 2);
    check_bool("mat_from_string number cols", A && mat_get_col_count(A) == 2);
    if (A) {
        x = mat_get_num(A, 0, 0);
        check_bool("mat_from_string rational A[0,0]", num_eq(x, half));
        num_destroy(&x);
        x = mat_get_num(A, 1, 0);
        check_bool("mat_from_string number A[1,0]", num_eq(x, expected));
    }

    num_destroy(&half);
    num_destroy(&expected);
    num_destroy(&x);
    mat_free(A);
}

static void test_mat_from_string_numeric_num_complex(void)
{
    matrix_t *A = mat_from_string("(1 + 1i, 3i - 1; 1/2 - 3/2i, 5 - 6i; 3, 4 + 2i)");
    number_t z = NUM_ZERO;
    number_t expected = NUM_ZERO;

    check_bool("mat_from_string complex number matrix non-null", A != NULL);
    check_bool("mat_from_string complex number matrix type",
               A && (mat_typeof(A) == MAT_TYPE_NUMBER || mat_typeof(A) == MAT_TYPE_EXPR));
    if (A) {
        expected = num_create_from_string("1 + 1i");
        z = mat_get_num(A, 0, 0);
        check_bool("mat_from_string number A[0,0]", num_eq(z, expected));
        num_destroy(&z);
        num_destroy(&expected);

        expected = num_create_from_string("1/2 - 3/2i");
        z = mat_get_num(A, 1, 0);
        check_bool("mat_from_string rational complex A[1,0]", num_eq(z, expected));
        num_destroy(&z);
        num_destroy(&expected);

        expected = num_create_from_string("5 - 6i");
        z = mat_get_num(A, 1, 1);
        check_bool("mat_from_string number A[1,1]", num_eq(z, expected));
        num_destroy(&z);
        num_destroy(&expected);

        expected = num_create_from_string("4 + 2i");
        z = mat_get_num(A, 2, 1);
        check_bool("mat_from_string number A[2,1]", num_eq(z, expected));
    }

    num_destroy(&z);
    num_destroy(&expected);
    mat_free(A);
}

static void test_mat_from_string_compact_columns(void)
{
    matrix_t *A = mat_from_string("(1 2; 4 5)");
    number_t expected = num_create_from_string("4");
    number_t got = NUM_ZERO;

    check_bool("mat_from_string compact matrix non-null", A != NULL);
    check_bool("mat_from_string compact matrix rows", A && mat_get_row_count(A) == 2u);
    check_bool("mat_from_string compact matrix columns", A && mat_get_col_count(A) == 2u);
    if (A) {
        got = mat_get_num(A, 1u, 0u);
        check_bool("mat_from_string compact matrix A[1,0]", num_eq(got, expected));
    }

    num_destroy(&got);
    num_destroy(&expected);
    mat_free(A);
}

static void test_mat_from_text_constructors(void)
{
    string_t *numeric_text = string_new_with("(1/3, 2; 3, 4)");
    string_t *symbolic_text = string_new_with("{ (x, c1; c1*x, [radius]) | x = 2; c1 = 3; [radius] = 5 }");
    matrix_t *numeric = mat_from_text(numeric_text);
    mat_bindings_t *bindings = NULL;
    matrix_t *symbolic = mat_from_text_expr(symbolic_text, &bindings);
    number_t got = NUM_ZERO;
    number_t expected = num_create_from_string("1/3");

    check_bool("mat_from_text numeric non-null", numeric != NULL);
    check_bool("mat_from_text numeric type", numeric && mat_typeof(numeric) == MAT_TYPE_NUMBER);
    if (numeric) {
        got = mat_get_num(numeric, 0, 0);
        check_bool("mat_from_text numeric preserves rational", num_eq(got, expected));
    }

    check_bool("mat_from_text_expr symbolic non-null", symbolic != NULL);
    check_bool("mat_from_text_expr symbolic type", symbolic && mat_typeof(symbolic) == MAT_TYPE_EXPR);
    check_bool("mat_from_text_expr x binding present", bindings && mat_bindings_get(bindings, "x") != NULL);
    check_bool("mat_from_text_expr bracketed binding present",
               bindings && mat_bindings_get(bindings, "[radius]") != NULL);

    num_destroy(&expected);
    num_destroy(&got);
    mat_bindings_free(bindings);
    mat_free(symbolic);
    mat_free(numeric);
    string_free(symbolic_text);
    string_free(numeric_text);
}

static void test_mat_from_string_symbolic_number_bindings(void)
{
    mat_bindings_t *bindings = NULL;
    matrix_t *A = mat_from_string_expr("{ (x, z; 1, c1) | x = 1/2; z = 1/2 - 3/2i; c1 = 5/2 }", &bindings);
    expr_t *dv = NULL;
    expr_t *x_binding;
    expr_t *z_binding;
    expr_t *c_binding;
    number_t got = NUM_ZERO;
    number_t expect = NUM_ZERO;

    check_bool("mat_from_string symbolic number bindings non-null", A != NULL);
    check_bool("mat_from_string symbolic number bindings type", A && mat_typeof(A) == MAT_TYPE_EXPR);
    x_binding = mat_bindings_get(bindings, "x");
    z_binding = mat_bindings_get(bindings, "z");
    c_binding = mat_bindings_get(bindings, "c₁");
    check_bool("symbolic number binding x present", x_binding != NULL);
    check_bool("symbolic number binding z present", z_binding != NULL);
    check_bool("symbolic number binding c₁ present", c_binding != NULL);

    if (x_binding) {
        got = expr_eval(x_binding);
        expect = num_create_from_string("1/2");
        check_bool("symbolic number binding x exact", num_eq(got, expect));
        num_destroy(&got);
        num_destroy(&expect);
    }

    if (A) {
        mat_get(A, 0, 1, &dv);
        got = expr_eval(dv);
        expect = num_create_from_string("1/2 - 3/2i");
        check_bool("symbolic number binding z matrix entry", num_eq(got, expect));
        num_destroy(&got);
        num_destroy(&expect);

        mat_get(A, 1, 1, &dv);
        got = expr_eval(dv);
        expect = num_create_from_string("5/2");
        check_bool("symbolic number binding c₁ exact", num_eq(got, expect));
        num_destroy(&got);
        num_destroy(&expect);
    }

    num_destroy(&got);
    num_destroy(&expect);
    mat_bindings_free(bindings);
    mat_free(A);
}

static void test_mat_from_string_symbolic_wrapped(void)
{
    mat_bindings_t *bindings = NULL;
    matrix_t *A = mat_from_string_expr("{ (x, 1; 1, c1) | x = 2; c1 = 3 }", &bindings);
    expr_t *dv = NULL;
    expr_t *x_binding;
    expr_t *c_binding;

    check_bool("mat_from_string wrapped symbolic matrix non-null", A != NULL);
    check_bool("mat_from_string wrapped symbolic matrix type", A && mat_typeof(A) == MAT_TYPE_EXPR);
    x_binding = mat_bindings_get(bindings, "x");
    c_binding = mat_bindings_get(bindings, "c₁");
    check_bool("wrapped symbolic binding x present", x_binding != NULL);
    check_bool("wrapped symbolic binding c₁ present", c_binding != NULL);

    if (A) {
        mat_get(A, 1, 1, &dv);
        check_matrix_fromstring_expr_double("wrapped symbolic A[1,1] initial", dv, 3.0, 1e-18);
        if (c_binding)
            test_expr_set_val_d(c_binding, 5.0);
        check_matrix_fromstring_expr_double("wrapped symbolic A[1,1] tracks binding update", dv, 5.0, 1e-18);
    }

    mat_bindings_free(bindings);
    mat_free(A);
}

static void test_mat_from_string_symbolic_bare(void)
{
    mat_bindings_t *bindings = NULL;
    matrix_t *A = mat_from_string_expr("(c1, c2*y, c2*x; x, y, z; a, b, c)", &bindings);
    expr_t *dv = NULL;
    expr_t *x_binding;
    expr_t *y_binding;
    expr_t *c2_binding;
    number_t x_initial = NUM_ZERO;
    number_t c2_initial = NUM_ZERO;

    check_bool("mat_from_string bare symbolic matrix non-null", A != NULL);
    check_bool("mat_from_string bare symbolic matrix type", A && mat_typeof(A) == MAT_TYPE_EXPR);
    check_bool("mat_from_string bare bindings returned", bindings != NULL);

    x_binding = mat_bindings_get(bindings, "x");
    y_binding = mat_bindings_get(bindings, "y");
    c2_binding = mat_bindings_get(bindings, "c₂");
    check_bool("bare symbolic x binding present", x_binding != NULL);
    check_bool("bare symbolic y binding present", y_binding != NULL);
    check_bool("bare symbolic c₂ binding present", c2_binding != NULL);

    if (x_binding)
        x_initial = expr_eval(x_binding);
    if (c2_binding)
        c2_initial = expr_eval(c2_binding);
    check_bool("bare symbolic x starts as NaN", x_binding && num_is_nan(x_initial));
    check_bool("bare symbolic c₂ starts as NaN", c2_binding && num_is_nan(c2_initial));

    check_bool("bare symbolic set x binding", test_mat_bindings_set_d(bindings, "x", 2.0) == 0);
    check_bool("bare symbolic set y binding", test_mat_bindings_set_d(bindings, "y", 3.0) == 0);
    check_bool("bare symbolic set c₂ binding", test_mat_bindings_set_d(bindings, "c₂", 5.0) == 0);

    if (A) {
        mat_get(A, 0, 1, &dv);
        check_matrix_fromstring_expr_double("bare symbolic c₂*y", dv, 15.0, 1e-18);
        mat_get(A, 0, 2, &dv);
        check_matrix_fromstring_expr_double("bare symbolic c₂*x", dv, 10.0, 1e-18);
    }

    num_destroy(&c2_initial);
    num_destroy(&x_initial);
    mat_bindings_free(bindings);
    mat_free(A);
}

static void test_mat_from_string_symbolic_at_aliases(void)
{
    mat_bindings_t *bindings = NULL;
    matrix_t *A = mat_from_string_expr("(@DELTA, @OMEGA; @OMEGA, -@DELTA)", &bindings);
    expr_t *dv = NULL;

    check_bool("mat_from_string @alias symbolic matrix non-null", A != NULL);
    check_bool("mat_from_string @alias symbolic matrix type", A && mat_typeof(A) == MAT_TYPE_EXPR);
    check_bool("mat_from_string @alias Δ binding present", mat_bindings_get(bindings, "Δ") != NULL);
    check_bool("mat_from_string @alias Ω binding present", mat_bindings_get(bindings, "Ω") != NULL);
    check_bool("mat_from_string @alias @DELTA binding present", mat_bindings_get(bindings, "@DELTA") != NULL);
    check_bool("mat_from_string @alias @OMEGA binding present", mat_bindings_get(bindings, "@OMEGA") != NULL);
    check_bool("mat_from_string @alias set @DELTA", test_mat_bindings_set_d(bindings, "@DELTA", 2.0) == 0);
    check_bool("mat_from_string @alias set @OMEGA", test_mat_bindings_set_d(bindings, "@OMEGA", 3.0) == 0);

    if (A) {
        mat_get(A, 0, 0, &dv);
        check_matrix_fromstring_expr_double("@alias symbolic Δ entry", dv, 2.0, 1e-18);
        mat_get(A, 0, 1, &dv);
        check_matrix_fromstring_expr_double("@alias symbolic Ω entry", dv, 3.0, 1e-18);
        mat_get(A, 1, 1, &dv);
        check_matrix_fromstring_expr_double("@alias symbolic -Δ entry", dv, -2.0, 1e-18);
    }

    mat_bindings_free(bindings);
    mat_free(A);
}

static void test_mat_from_string_symbolic_math_conventions(void)
{
    mat_bindings_t *bindings = NULL;
    matrix_t *A = mat_from_string_expr("(x, e; pi, τ; @phi, @gamma; [radius], c1; a, d_2)", &bindings);
    expr_t *x_binding;
    expr_t *e_binding;
    expr_t *pi_binding;
    expr_t *phi_binding;
    expr_t *gamma_binding;
    expr_t *tau_binding;
    expr_t *radius_binding;
    expr_t *c1_binding;
    expr_t *a_binding;
    expr_t *d2_binding;
    expr_t *dv = NULL;
    number_t e_initial = NUM_ZERO;
    number_t pi_initial = NUM_ZERO;
    number_t phi_initial = NUM_ZERO;
    number_t gamma_initial = NUM_ZERO;
    number_t tau_initial = NUM_ZERO;
    number_t five = num_create_from_long(5);
    number_t sqrt_five = num_sqrt(five);
    number_t phi_sum = num_add(NUM_ONE, sqrt_five);
    number_t phi_expected = num_div(phi_sum, NUM_TWO);

    check_bool("mat_from_string mathematical-convention symbolic matrix non-null", A != NULL);
    check_bool("mat_from_string mathematical-convention symbolic matrix type", A && mat_typeof(A) == MAT_TYPE_EXPR);

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

    check_bool("mathematical-convention x binding present", x_binding != NULL);
    check_bool("mathematical-convention e binding present", e_binding != NULL);
    check_bool("mathematical-convention π binding present", pi_binding != NULL);
    check_bool("mathematical-convention φ binding present", phi_binding != NULL);
    check_bool("mathematical-convention γ binding present", gamma_binding != NULL);
    check_bool("mathematical-convention τ binding present", tau_binding != NULL);
    check_bool("mathematical-convention radius binding present", radius_binding != NULL);
    check_bool("mathematical-convention c₁ binding present", c1_binding != NULL);
    check_bool("mathematical-convention a binding present", a_binding != NULL);
    check_bool("mathematical-convention d₂ binding present", d2_binding != NULL);

    if (e_binding)
        e_initial = expr_eval(e_binding);
    if (pi_binding)
        pi_initial = expr_eval(pi_binding);
    if (phi_binding)
        phi_initial = expr_eval(phi_binding);
    if (gamma_binding)
        gamma_initial = expr_eval(gamma_binding);
    if (tau_binding)
        tau_initial = expr_eval(tau_binding);

    check_matrix_fromstring_expr_num("mathematical-convention e built-in value", e_binding, NUM_E, 1e-30);
    check_matrix_fromstring_expr_num("mathematical-convention π built-in value", pi_binding, NUM_PI, 1e-30);
    check_matrix_fromstring_expr_num("mathematical-convention φ built-in value", phi_binding, phi_expected, 1e-30);
    check_matrix_fromstring_expr_num("mathematical-convention γ built-in value", gamma_binding, NUM_EULER_MASCHERONI,
                                     1e-30);
    check_bool("mathematical-convention τ still starts as variable NaN", tau_binding && num_is_nan(tau_initial));

    if (A) {
        mat_get(A, 0, 1, &dv);
        check_matrix_fromstring_expr_num("mathematical-convention matrix e entry", dv, NUM_E, 1e-30);
        mat_get(A, 1, 0, &dv);
        check_matrix_fromstring_expr_num("mathematical-convention matrix π entry", dv, NUM_PI, 1e-30);
        mat_get(A, 2, 0, &dv);
        check_matrix_fromstring_expr_num("mathematical-convention matrix φ entry", dv, phi_expected, 1e-30);
        mat_get(A, 2, 1, &dv);
        check_matrix_fromstring_expr_num("mathematical-convention matrix γ entry", dv, NUM_EULER_MASCHERONI, 1e-30);
    }

    num_destroy(&phi_expected);
    num_destroy(&phi_sum);
    num_destroy(&sqrt_five);
    num_destroy(&five);
    num_destroy(&tau_initial);
    num_destroy(&gamma_initial);
    num_destroy(&phi_initial);
    num_destroy(&pi_initial);
    num_destroy(&e_initial);
    mat_bindings_free(bindings);
    mat_free(A);
}

static void test_mat_from_string_symbolic_numeric_fallback(void)
{
    matrix_t *A = mat_from_string("{ (x, y; y, x) | x = 2; y = 3/2 }");
    number_t a00 = NUM_ZERO;
    number_t a01 = NUM_ZERO;
    number_t expect_2 = num_create_from_long(2);
    number_t expect_3_over_2 = num_create_from_string("3/2");

    check_bool("mat_from_string symbolic fallback returns non-null", A != NULL);
    check_bool("mat_from_string symbolic fallback returns number matrix", A && mat_typeof(A) == MAT_TYPE_NUMBER);
    if (A) {
        a00 = mat_get_num(A, 0, 0);
        a01 = mat_get_num(A, 0, 1);
        check_bool("mat_from_string symbolic fallback A[0,0]=2", num_eq(a00, expect_2));
        check_bool("mat_from_string symbolic fallback A[0,1]=3/2", num_eq(a01, expect_3_over_2));
    }

    num_destroy(&expect_3_over_2);
    num_destroy(&expect_2);
    num_destroy(&a01);
    num_destroy(&a00);
    mat_free(A);
}

static void test_mat_from_string_symbolic_insufficient_bindings(void)
{
    matrix_t *A_missing = mat_from_string("(x, 1; 2, 3)");
    matrix_t *A_partial = mat_from_string("{ (x, y; 2, 3) | x = 1 }");

    check_bool("mat_from_string bare symbolic with missing bindings returns NULL", A_missing == NULL);
    check_bool("mat_from_string partially bound symbolic returns NULL", A_partial == NULL);

    mat_free(A_missing);
    mat_free(A_partial);
}

static void test_mat_from_string_symbolic_builtin_constants(void)
{
    matrix_t *A = mat_from_string("(pi, e; @phi, @gamma)");
    number_t a00 = NUM_ZERO;
    number_t a01 = NUM_ZERO;
    number_t a10 = NUM_ZERO;
    number_t a11 = NUM_ZERO;
    number_t five = num_create_from_long(5);
    number_t sqrt_five = num_sqrt(five);
    number_t phi_sum = num_add(NUM_ONE, sqrt_five);
    number_t phi_expected = num_div(phi_sum, NUM_TWO);

    check_bool("mat_from_string symbolic built-ins return non-null", A != NULL);
    check_bool("mat_from_string symbolic built-ins return number matrix", A && mat_typeof(A) == MAT_TYPE_NUMBER);

    if (A) {
        a00 = mat_get_num(A, 0, 0);
        a01 = mat_get_num(A, 0, 1);
        a10 = mat_get_num(A, 1, 0);
        a11 = mat_get_num(A, 1, 1);

        check_matrix_fromstring_num("mat_from_string built-in pi resolves", a00, NUM_PI, 1e-30);
        check_matrix_fromstring_num("mat_from_string built-in e resolves", a01, NUM_E, 1e-30);
        check_matrix_fromstring_num("mat_from_string built-in phi resolves", a10, phi_expected, 1e-30);
        check_matrix_fromstring_num("mat_from_string built-in gamma resolves", a11, NUM_EULER_MASCHERONI, 1e-30);
    }

    num_destroy(&phi_expected);
    num_destroy(&phi_sum);
    num_destroy(&sqrt_five);
    num_destroy(&five);
    num_destroy(&a11);
    num_destroy(&a10);
    num_destroy(&a01);
    num_destroy(&a00);
    mat_free(A);
}

static void test_mat_from_string_invalid_syntax(void)
{
    mat_bindings_t *bindings = (mat_bindings_t *)(uintptr_t)1;
    matrix_t *A;

    A = mat_from_string_expr("(1, 2; 3)", &bindings);
    check_bool("mat_from_string rejects ragged matrix", A == NULL);
    check_bool("mat_from_string ragged clears bindings", bindings == NULL);

    bindings = (mat_bindings_t *)(uintptr_t)1;
    A = mat_from_string_expr("(1, 2; 3, 4", &bindings);
    check_bool("mat_from_string rejects missing closing paren", A == NULL);
    check_bool("mat_from_string missing paren clears bindings", bindings == NULL);

    bindings = (mat_bindings_t *)(uintptr_t)1;
    A = mat_from_string_expr("{ (x, 1; 1, y) | x = }", &bindings);
    check_bool("mat_from_string rejects invalid binding syntax", A == NULL);
    check_bool("mat_from_string invalid binding clears bindings", bindings == NULL);

    bindings = (mat_bindings_t *)(uintptr_t)1;
    A = mat_from_string_expr("(Δ, Ω; Ω, -)", &bindings);
    check_bool("mat_from_string rejects invalid symbolic expression", A == NULL);
    check_bool("mat_from_string invalid symbolic clears bindings", bindings == NULL);
}

static void test_mat_expression_from_string(void)
{
    const char *exp_operation = NULL;
    const char *product_operation = NULL;
    matrix_t *actual = mat_expression_from_string("exp(-(.1 2; 4 5))", NULL, &exp_operation);
    matrix_t *negative = mat_from_string("(-.1 -2; -4 -5)");
    matrix_t *expected = mat_exp(negative);
    matrix_t *product = mat_expression_from_string("(1 2; 3 4).(5; 6)", NULL, &product_operation);
    number_t actual_value = NUM_ZERO;
    number_t expected_value = NUM_ZERO;
    number_t seventeen = num_create_from_long(17);
    number_t thirty_nine = num_create_from_long(39);

    check_bool("complete matrix expression exp parses in MARSlib", actual != NULL);
    check_bool("complete matrix expression exp reports operation", exp_operation && strcmp(exp_operation, "exp") == 0);
    check_bool("complete matrix expression explicit comparison parses", expected != NULL);

    if (actual && expected) {
        for (size_t row = 0u; row < 2u; ++row) {
            for (size_t col = 0u; col < 2u; ++col) {
                actual_value = mat_get_num(actual, row, col);
                expected_value = mat_get_num(expected, row, col);
                check_matrix_fromstring_num("grouped unary matrix exponential matches explicit negation", actual_value,
                                            expected_value, 1e-18);
                num_destroy(&expected_value);
                num_destroy(&actual_value);
            }
        }
    }

    check_bool("complete matrix expression product parses in MARSlib", product != NULL);
    check_bool("complete matrix expression product reports operation",
               product_operation && strcmp(product_operation, "multiply") == 0);
    if (product) {
        actual_value = mat_get_num(product, 0u, 0u);
        check_matrix_fromstring_num("complete matrix expression product first entry", actual_value, seventeen, 1e-18);
        num_destroy(&actual_value);
        actual_value = mat_get_num(product, 1u, 0u);
        check_matrix_fromstring_num("complete matrix expression product second entry", actual_value, thirty_nine, 1e-18);
        num_destroy(&actual_value);
    }

    num_destroy(&thirty_nine);
    num_destroy(&seventeen);
    mat_free(product);
    mat_free(expected);
    mat_free(negative);
    mat_free(actual);
}

static void test_mat_expression_registered_function_aliases(void)
{
    const char *natural_log_operation = NULL;
    const char *common_log_operation = NULL;
    const char *gamma_operation = NULL;
    const char *floor_operation = NULL;
    const char *symbolic_operation = NULL;
    mat_bindings_t *symbolic_bindings = NULL;
    matrix_t *natural_log = mat_expression_from_string("ln(1 0; 0 2)", NULL, &natural_log_operation);
    matrix_t *common_log = mat_expression_from_string("log(1 0; 0 100)", NULL, &common_log_operation);
    matrix_t *gamma = mat_expression_from_string("Γ(1 0; 0 2)", NULL, &gamma_operation);
    matrix_t *floor_matrix = mat_expression_from_string("floor(1.5 0; 0 2.9)", NULL, &floor_operation);
    matrix_t *symbolic = mat_expression_from_string("sin({ (x 0; 0 y) | x = 0, y = 1; })", &symbolic_bindings,
                                                    &symbolic_operation);
    expr_t *x_binding = mat_bindings_get(symbolic_bindings, "x");
    expr_t *diagonal_entry = NULL;
    number_t value = NUM_ZERO;
    number_t two = num_create_from_long(2);
    number_t one = num_create_from_long(1);

    check_bool("registered ln alias resolves through expression function hash", natural_log != NULL);
    check_bool("registered ln alias reports canonical operation",
               natural_log_operation && strcmp(natural_log_operation, "ln") == 0);
    check_bool("registered log spelling resolves as common matrix logarithm", common_log != NULL);
    check_bool("registered log spelling reports canonical operation",
               common_log_operation && strcmp(common_log_operation, "log") == 0);
    check_bool("registered Greek gamma alias resolves through expression function hash", gamma != NULL);
    check_bool("registered Greek gamma alias reports canonical operation",
               gamma_operation && strcmp(gamma_operation, "gamma") == 0);
    check_bool("registered function outside the former matrix subset resolves", floor_matrix != NULL);
    check_bool("registered floor function reports canonical operation", floor_operation && strcmp(floor_operation, "floor") == 0);
    check_bool("symbolic matrix function preserves argument bindings",
               symbolic != NULL && symbolic_bindings != NULL && x_binding != NULL);
    check_bool("symbolic matrix function reports canonical operation",
               symbolic_operation && strcmp(symbolic_operation, "sin") == 0);

    if (common_log) {
        value = mat_get_num(common_log, 1u, 1u);
        check_matrix_fromstring_num("registered log alias retains scalar registry semantics", value, two, 1e-18);
        num_destroy(&value);
    }
    if (gamma) {
        value = mat_get_num(gamma, 1u, 1u);
        check_matrix_fromstring_num("registered gamma alias evaluates matrix function", value, one, 1e-18);
        num_destroy(&value);
    }
    if (floor_matrix) {
        value = mat_get_num(floor_matrix, 1u, 1u);
        check_matrix_fromstring_num("registered floor function evaluates diagonal entries through expression", value, two, 1e-18);
        num_destroy(&value);
    }
    if (symbolic && x_binding) {
        mat_get(symbolic, 0u, 0u, &diagonal_entry);
        test_expr_set_val_d(x_binding, 1.57079632679489661923);
        check_matrix_fromstring_expr_num("symbolic matrix function retains live external binding", diagonal_entry, one, 1e-18);
    }

    num_destroy(&one);
    num_destroy(&two);
    mat_bindings_free(symbolic_bindings);
    mat_free(symbolic);
    mat_free(floor_matrix);
    mat_free(gamma);
    mat_free(common_log);
    mat_free(natural_log);
}

static void test_mat_from_string_bracketed_names(void)
{
    mat_bindings_t *bindings = NULL;
    matrix_t *A = mat_from_string_expr(
        "{ ([radius], [scale]*x; y, [offset]) | x = 2, y = 5; [radius] = 3, [scale] = 4, [offset] = 7 }", &bindings);
    expr_t *dv = NULL;

    check_bool("mat_from_string bracketed symbolic matrix non-null", A != NULL);
    check_bool("mat_from_string bracketed symbolic matrix type", A && mat_typeof(A) == MAT_TYPE_EXPR);
    check_bool("mat_from_string bracketed binding radius present", mat_bindings_get(bindings, "[radius]") != NULL);
    check_bool("mat_from_string bracketed binding scale present", mat_bindings_get(bindings, "scale") != NULL);
    check_bool("mat_from_string bracketed binding offset present", mat_bindings_get(bindings, "[offset]") != NULL);

    if (A) {
        mat_get(A, 0, 0, &dv);
        check_matrix_fromstring_expr_double("bracketed symbolic [radius]", dv, 3.0, 1e-18);
        mat_get(A, 0, 1, &dv);
        check_matrix_fromstring_expr_double("bracketed symbolic [scale]*x", dv, 8.0, 1e-18);
        mat_get(A, 1, 1, &dv);
        check_matrix_fromstring_expr_double("bracketed symbolic [offset]", dv, 7.0, 1e-18);
    }

    mat_bindings_free(bindings);
    mat_free(A);
}

static void test_mat_symbolic_derivative_helpers_by_name(void)
{
    mat_bindings_t *bindings = NULL;
    matrix_t *A = mat_from_string_expr("([radius], x*y; y, c1)", &bindings);
    matrix_t *Dr = NULL;
    matrix_t *Ix = NULL;
    matrix_t *DIx = NULL;
    expr_t *dtr = NULL;
    expr_t *ddet = NULL;
    expr_t *dv = NULL;

    check_bool("mat symbolic helpers source non-null", A != NULL);
    check_bool("mat symbolic helpers set x", test_mat_bindings_set_d(bindings, "x", 2.0) == 0);
    check_bool("mat symbolic helpers set y", test_mat_bindings_set_d(bindings, "y", 3.0) == 0);
    check_bool("mat symbolic helpers set [radius]", test_mat_bindings_set_d(bindings, "[radius]", 5.0) == 0);
    check_bool("mat symbolic helpers set c₁", test_mat_bindings_set_d(bindings, "c₁", 7.0) == 0);

    Dr = mat_deriv_by_name(A, bindings, "[radius]");
    Ix = mat_integrate_by_name(A, bindings, "x");
    DIx = mat_deriv_by_name(Ix, bindings, "x");
    dtr = mat_deriv_trace_by_name(A, bindings, "[radius]");
    ddet = mat_deriv_det_by_name(A, bindings, "[radius]");

    check_bool("mat_deriv_by_name([radius]) not NULL", Dr != NULL);
    check_bool("mat_integrate_by_name(x) not NULL", Ix != NULL);
    check_bool("mat_deriv_by_name(mat_integrate_by_name(A,x),x) not NULL", DIx != NULL);
    check_bool("mat_deriv_trace_by_name([radius]) not NULL", dtr != NULL);
    check_bool("mat_deriv_det_by_name([radius]) not NULL", ddet != NULL);
    check_bool("mat_deriv_by_name missing symbol returns NULL", mat_deriv_by_name(A, bindings, "missing") == NULL);

    if (Dr) {
        mat_get(Dr, 0, 0, &dv);
        check_matrix_fromstring_expr_double("mat_deriv_by_name [0,0] = 1", dv, 1.0, 1e-18);
        mat_get(Dr, 0, 1, &dv);
        check_matrix_fromstring_expr_double("mat_deriv_by_name [0,1] = 0", dv, 0.0, 1e-18);
        mat_get(Dr, 1, 0, &dv);
        check_matrix_fromstring_expr_double("mat_deriv_by_name [1,0] = 0", dv, 0.0, 1e-18);
        mat_get(Dr, 1, 1, &dv);
        check_matrix_fromstring_expr_double("mat_deriv_by_name [1,1] = 0", dv, 0.0, 1e-18);
    }

    if (DIx) {
        mat_get(DIx, 0, 0, &dv);
        check_matrix_fromstring_expr_double("matrix integral derivative [0,0] = [radius]", dv, 5.0, 1e-18);
        mat_get(DIx, 0, 1, &dv);
        check_matrix_fromstring_expr_double("matrix integral derivative [0,1] = xy", dv, 6.0, 1e-18);
        mat_get(DIx, 1, 0, &dv);
        check_matrix_fromstring_expr_double("matrix integral derivative [1,0] = y", dv, 3.0, 1e-18);
        mat_get(DIx, 1, 1, &dv);
        check_matrix_fromstring_expr_double("matrix integral derivative [1,1] = c₁", dv, 7.0, 1e-18);
    }

    if (dtr)
        check_matrix_fromstring_expr_double("mat_deriv_trace_by_name([radius]) = 1", dtr, 1.0, 1e-18);
    if (ddet)
        check_matrix_fromstring_expr_double("mat_deriv_det_by_name([radius]) = c₁", ddet, 7.0, 1e-18);

    expr_free(ddet);
    expr_free(dtr);
    mat_free(DIx);
    mat_free(Ix);
    mat_free(Dr);
    mat_bindings_free(bindings);
    mat_free(A);
}

static void test_mat_symbolic_jacobian_helper_by_names(void)
{
    mat_bindings_t *bindings = NULL;
    matrix_t *A = mat_from_string_expr("(x, x*y)", &bindings);
    const char *names[2] = {"x", "y"};
    matrix_t *J = NULL;
    expr_t *dv = NULL;

    check_bool("mat symbolic Jacobian helper source non-null", A != NULL);
    check_bool("mat symbolic Jacobian helper set x", test_mat_bindings_set_d(bindings, "x", 2.0) == 0);
    check_bool("mat symbolic Jacobian helper set y", test_mat_bindings_set_d(bindings, "y", 3.0) == 0);

    J = mat_jacobian_by_names(A, bindings, names, 2);
    check_bool("mat_jacobian_by_names not NULL", J != NULL);
    check_bool("mat_jacobian_by_names rows", J && mat_get_row_count(J) == 2);
    check_bool("mat_jacobian_by_names cols", J && mat_get_col_count(J) == 2);
    check_bool("mat_jacobian_by_names missing symbol returns NULL",
               mat_jacobian_by_names(A, bindings, (const char *const[]){"x", "missing"}, 2) == NULL);

    if (J) {
        mat_get(J, 0, 0, &dv);
        check_matrix_fromstring_expr_double("mat_jacobian_by_names [0,0] = 1", dv, 1.0, 1e-18);
        mat_get(J, 0, 1, &dv);
        check_matrix_fromstring_expr_double("mat_jacobian_by_names [0,1] = 0", dv, 0.0, 1e-18);
        mat_get(J, 1, 0, &dv);
        check_matrix_fromstring_expr_double("mat_jacobian_by_names [1,0] = y", dv, 3.0, 1e-18);
        mat_get(J, 1, 1, &dv);
        check_matrix_fromstring_expr_double("mat_jacobian_by_names [1,1] = x", dv, 2.0, 1e-18);
    }

    mat_free(J);
    mat_bindings_free(bindings);
    mat_free(A);
}

static void test_mat_symbolic_matrix_calculus_helpers_by_name(void)
{
    mat_bindings_t *bindings = NULL;
    matrix_t *A = mat_from_string_expr("(x, 1; y, 2)", &bindings);
    expr_t *x_binding = NULL;
    expr_t *y_binding = NULL;
    expr_t *b_entries[2] = {NULL, NULL};
    matrix_t *B = NULL;
    matrix_t *dAi = NULL;
    matrix_t *dAbi = NULL;
    matrix_t *dX = NULL;
    matrix_t *dXb = NULL;
    expr_t *dv = NULL;

    check_bool("mat symbolic calculus by-name source A non-null", A != NULL);
    x_binding = mat_bindings_get(bindings, "x");
    y_binding = mat_bindings_get(bindings, "y");
    check_bool("mat symbolic calculus by-name shared x binding present", x_binding != NULL);
    check_bool("mat symbolic calculus by-name shared y binding present", y_binding != NULL);
    if (x_binding && y_binding) {
        b_entries[0] = x_binding;
        b_entries[1] = y_binding;
        B = mat_create_expr(2, 1, b_entries);
    }
    check_bool("mat symbolic calculus by-name source B non-null", B != NULL);

    check_bool("mat symbolic calculus by-name set A x", test_mat_bindings_set_d(bindings, "x", 2.0) == 0);
    check_bool("mat symbolic calculus by-name set A y", test_mat_bindings_set_d(bindings, "y", 3.0) == 0);

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
        check_matrix_fromstring_expr_double("mat_deriv_inverse_by_name [0,0]", dv, -4.0, 1e-18);
        mat_get(dAi, 1, 0, &dv);
        check_matrix_fromstring_expr_double("mat_deriv_inverse_by_name [1,0]", dv, 6.0, 1e-18);
    }

    if (dAbi) {
        mat_get(dAbi, 0, 1, &dv);
        check_matrix_fromstring_expr_double("mat_deriv_block_inverse_by_name [0,1]", dv, 2.0, 1e-18);
        mat_get(dAbi, 1, 1, &dv);
        check_matrix_fromstring_expr_double("mat_deriv_block_inverse_by_name [1,1]", dv, -3.0, 1e-18);
    }

    if (dX) {
        mat_get(dX, 0, 0, &dv);
        check_matrix_fromstring_expr_double("mat_deriv_solve_by_name [0,0]", dv, 0.0, 1e-18);
        mat_get(dX, 1, 0, &dv);
        check_matrix_fromstring_expr_double("mat_deriv_solve_by_name [1,0]", dv, 0.0, 1e-18);
    }

    if (dXb) {
        mat_get(dXb, 0, 0, &dv);
        check_matrix_fromstring_expr_double("mat_deriv_block_solve_by_name [0,0]", dv, 0.0, 1e-18);
        mat_get(dXb, 1, 0, &dv);
        check_matrix_fromstring_expr_double("mat_deriv_block_solve_by_name [1,0]", dv, 0.0, 1e-18);
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
    TEST_RUN_CASE(test_mat_from_string_numeric_num_real, NULL);
    TEST_RUN_CASE(test_mat_from_string_numeric_num_complex, NULL);
    TEST_RUN_CASE(test_mat_from_string_compact_columns, NULL);
    TEST_RUN_CASE(test_mat_from_text_constructors, NULL);
    TEST_RUN_CASE(test_mat_from_string_symbolic_numeric_fallback, NULL);
    TEST_RUN_CASE(test_mat_from_string_symbolic_insufficient_bindings, NULL);
    TEST_RUN_CASE(test_mat_from_string_symbolic_builtin_constants, NULL);
    TEST_RUN_CASE(test_mat_from_string_symbolic_number_bindings, NULL);
    TEST_RUN_CASE(test_mat_from_string_symbolic_wrapped, NULL);
    TEST_RUN_CASE(test_mat_from_string_symbolic_bare, NULL);
    TEST_RUN_CASE(test_mat_from_string_symbolic_at_aliases, NULL);
    TEST_RUN_CASE(test_mat_from_string_symbolic_math_conventions, NULL);
    TEST_RUN_CASE(test_mat_from_string_bracketed_names, NULL);
    TEST_RUN_CASE(test_mat_symbolic_derivative_helpers_by_name, NULL);
    TEST_RUN_CASE(test_mat_symbolic_jacobian_helper_by_names, NULL);
    TEST_RUN_CASE(test_mat_symbolic_matrix_calculus_helpers_by_name, NULL);
    TEST_RUN_CASE(test_mat_expression_from_string, NULL);
    TEST_RUN_CASE(test_mat_expression_registered_function_aliases, NULL);
    TEST_RUN_CASE(test_mat_from_string_invalid_syntax, NULL);
}
