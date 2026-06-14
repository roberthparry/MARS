#include "test_qfloat.h"

TEST_SUITE_CONFIG(TEST_CONFIG_GLOBAL);

static int qfloat_validity_equal(const void *actual, const void *expected, void *ctx);
static int qfloat_validity_format(const void *value, string_t *out, void *ctx);
static bool test_qfloat_suite_setup(void);

static const double qfloat_validity_rel_tol = 1e-28;
static const test_validity_contract_t qfloat_close_contract =
    TEST_VALIDITY_CONTRACT("qfloat-close",
                           qfloat_validity_equal,
                           qfloat_validity_format,
                           (void *)&qfloat_validity_rel_tol);

TEST_SUITE_SETUP(test_qfloat_suite_setup);

/* Helper to print qfloat_t */
void print_q(const char *label, qfloat_t x) {
    string_t *text = qf_to_string(x);
    printf("%s = %s\n", label, text ? string_c_str(text) : "<qfloat format failed>");
    string_free(text);
}

void test_qf_to_buffer(qfloat_t x, char *out, size_t out_size)
{
    string_t *text;

    if (!out || out_size == 0u)
        return;

    text = qf_to_string(x);
    if (!text) {
        out[0] = '\0';
        return;
    }

    snprintf(out, out_size, "%s", string_c_str(text));
    string_free(text);
}

/* Compare qfloat_t to double with tolerance */
int approx_equal(qfloat_t a, double b, double tol) {
    double diff = fabs(qf_to_double(a) - b);
    return diff < tol;
}

int qf_close(qfloat_t a, qfloat_t b, double rel)
{
    return qf_abs(qf_sub(a, b)).hi <= rel;
}

int qf_close_rel(qfloat_t a, qfloat_t b, double rel)
{
    return qf_abs(qf_sub(qf_div(a,b), (qfloat_t){1,0})).hi <= rel;
}

const test_validity_contract_t *qfloat_validity_contract_close(void)
{
    return &qfloat_close_contract;
}

bool test_assert_qfloat_close_tol(qfloat_t actual,
                                  qfloat_t expected,
                                  double rel_tol,
                                  const char *file,
                                  int line)
{
    const test_validity_contract_t contract =
        TEST_VALIDITY_CONTRACT("qfloat-close",
                               qfloat_validity_equal,
                               qfloat_validity_format,
                               &rel_tol);

    return test_assert_validity(&contract, &actual, &expected, file, line);
}

static int qfloat_validity_equal(const void *actual, const void *expected, void *ctx)
{
    const qfloat_t *a = (const qfloat_t *)actual;
    const qfloat_t *b = (const qfloat_t *)expected;
    const double rel = ctx ? *(const double *)ctx : 1e-28;

    return qf_eq(*a, *b) || qf_close(*a, *b, rel) || qf_close_rel(*a, *b, rel);
}

static int qfloat_validity_format(const void *value, string_t *out, void *ctx)
{
    int needed;
    char *text;

    (void)ctx;
    if (!out)
        return -1;

    needed = qf_sprintf(NULL, 0u, "%q", *(const qfloat_t *)value);
    if (needed < 0)
        return string_append_cstr(out, "<qfloat format failed>");

    text = malloc((size_t)needed + 1u);
    if (!text)
        return -1;

    if (qf_sprintf(text, (size_t)needed + 1u, "%q", *(const qfloat_t *)value) < 0 ||
        string_append_cstr(out, text) != 0) {
        free(text);
        return -1;
    }

    free(text);
    return 0;
}

static bool test_qfloat_suite_setup(void)
{
    test_register_validity_checker("qfloat-close", qfloat_validity_contract_close());
    return TEST_REQUIRE_VALIDITY_CHECKER("qfloat-close");
}

static void check_bool(const char *label, int cond)
{
    if (!cond)
        test_mark_failure(__FILE__, __LINE__, label);
    printf(cond ? C_GREEN "  OK: %s\n" C_RESET
                : C_RED   "  FAIL: %s\n" C_RESET, label);
}

void test_difficult_qfloat_cases(void)
{
    qfloat_t x = qf_from_string("2.3");
    qfloat_t lhs = qf_lgamma(qf_from_string("3.3"));
    qfloat_t rhs = qf_lgamma(x);
    qfloat_t tmp = qf_log(x);
    qfloat_t one = qf_from_double(1.0);
    qfloat_t y = qf_from_string("1e-20");
    qfloat_t s = qf_from_double(0.5);
    qfloat_t gx = qf_from_double(1.0);
    qfloat_t w = qf_productlog(qf_from_string("-0.35"));
    qfloat_t a = qf_from_string("2.5");
    qfloat_t b = qf_from_string("3.5");
    qfloat_t logb;
    qfloat_t beta;
    qfloat_t ident;

    printf(C_CYAN "TEST: difficult qfloat cases\n" C_RESET);

    lhs = qf_sub(qf_sub(lhs, rhs), tmp);
    print_q("    lgamma(3.3) - lgamma(2.3) - log(2.3)", lhs);
    check_bool("lgamma(3.3) - lgamma(2.3) - log(2.3) = 0",
               qf_abs(lhs).hi < 1e-28);

    y = qf_exp(qf_log(y));
    print_q("    exp(log(1e-20))", y);
    TEST_ASSERT_QFLOAT_CLOSE(y, qf_from_string("1e-20"));

    ident = qf_add(qf_gammainc_P(s, gx), qf_gammainc_Q(s, gx));
    print_q("    gammainc_P(0.5,1) + gammainc_Q(0.5,1)", ident);
    TEST_ASSERT_QFLOAT_CLOSE(ident, one);

    ident = qf_sub(qf_mul(w, qf_exp(w)), qf_from_string("-0.35"));
    print_q("    productlog(-0.35) * exp(productlog(-0.35)) - (-0.35)", ident);
    check_bool("productlog(-0.35) * exp(productlog(-0.35)) = -0.35",
               qf_abs(ident).hi < 1e-28);

    logb = qf_logbeta(a, b);
    beta = qf_beta(a, b);
    ident = qf_sub(qf_exp(logb), beta);
    print_q("    exp(logbeta(2.5,3.5)) - beta(2.5,3.5)", ident);
    TEST_ASSERT_QFLOAT_CLOSE(ident, qf_from_double(0.0));
}

int tests_main() {
    // qfloat_t x = qf_from_string("1.7724538509055160272981674833411451827975494561223871282138");
    // char* s = "QF_SQRT_PI";
    // printf("const qfloat_t %s = {\n    %.17g,\n    %.17g\n};\n", s, x.hi, x.lo);
    // printf("extern const qfloat_t %s;\n", s);
    // exit(0);

    printf(C_YELLOW "Running qfloat_t tests...\n\n" C_RESET);

    TEST_SECTION("Core and Elementary");

    TEST_RUN_IN_GROUP(test_arithmetic, tests, NULL);
    TEST_RUN_IN_GROUP(test_arithmetic_extensions, tests, NULL);
    TEST_RUN_IN_GROUP(test_strings, tests, NULL);
    TEST_RUN_IN_GROUP(test_printf, tests, NULL);
    TEST_RUN_IN_GROUP(test_vsprintf, tests, NULL);
    TEST_RUN_IN_GROUP(test_power, tests, NULL);
    TEST_RUN_IN_GROUP(test_trigonometric, tests, NULL);
    TEST_RUN_IN_GROUP(test_hyperbolic, tests, NULL);
    TEST_RUN_IN_GROUP(test_hypotenus, tests, NULL);

    TEST_SECTION("Special Functions");
    TEST_RUN_IN_GROUP(test_gamma_erf_erfc_erfinv_erfcinv_digamma, tests, NULL);
    TEST_RUN_IN_GROUP(test_lambert_w, tests, NULL);
    TEST_RUN_IN_GROUP(test_beta_logbeta_binomial_beta_pdf_logbeta_pdf_normal_pdf_cdf_logpdf, tests, NULL);
    TEST_RUN_IN_GROUP(test_gammainc_ei_e1, tests, NULL);
    TEST_RUN_IN_GROUP(test_difficult_qfloat_cases, tests, NULL);

    printf(C_YELLOW "\nRunning README examples...\n" C_RESET);
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(test_readme_examples, readme_examples,
                                  "qfloat,readme,output");

    printf("\n" C_YELLOW "Done.\n" C_RESET);

    return TESTS_EXIT_CODE();
}
