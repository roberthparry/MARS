#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "test_number.h"

#define NUMBER_PARITY_PRECISION 512u
#define NUMBER_PARITY_LOOSE_TOL "1e-40"
#define NUMBER_PARITY_TIGHT_TOL "1e-70"
#define NUMBER_PARITY_QCOMPLEX_TOL "1e-12"
#define NUMBER_PARITY_BRANCH_TOL "1e-30"

static number_t number_text(const char *text)
{
    return num_create_from_string(text);
}

static void assert_number_close_text(const char *label, number_t got, const char *expected_text,
                                     const char *tolerance_text)
{
    number_t expected = number_text(expected_text);
    number_t tolerance = number_text(tolerance_text);
    number_t error = num_abs(num_sub(got, expected));
    string_t *error_text = num_to_string(error);

    printf(C_WHITE C_BOLD "%s" C_RESET "\n", label ? label : "<unspecified>");
    printf("    expected = %s\n", expected_text);
    printf("    tol      = %s\n", tolerance_text);
    printf("    error    = %s\n\n", error_text ? string_c_str(error_text) : "<format-error>");
    ASSERT_TRUE(num_le(error, tolerance));

    string_free(error_text);
    num_destroy(&error);
    num_destroy(&tolerance);
    num_destroy(&expected);
}

static void assert_number_real_imag_prefix(const char *label, number_t value, const char *real_prefix,
                                           const char *imag_prefix)
{
    char real_label[160];
    char imag_label[160];
    number_t real = num_real_part(value);
    number_t imag = num_imag_part(value);

    snprintf(real_label, sizeof(real_label), "%s.real", label);
    snprintf(imag_label, sizeof(imag_label), "%s.imag", label);
    assert_number_string_prefix(real_label, real, real_prefix);
    assert_number_string_prefix(imag_label, imag, imag_prefix);

    num_destroy(&imag);
    num_destroy(&real);
}

static void assert_number_real_imag_close(const char *label, number_t value, const char *real_text,
                                          const char *imag_text, const char *tolerance_text)
{
    char real_label[160];
    char imag_label[160];
    number_t real = num_real_part(value);
    number_t imag = num_imag_part(value);

    snprintf(real_label, sizeof(real_label), "%s.real", label);
    snprintf(imag_label, sizeof(imag_label), "%s.imag", label);
    assert_number_close_text(real_label, real, real_text, tolerance_text);
    assert_number_close_text(imag_label, imag, imag_text, tolerance_text);

    num_destroy(&imag);
    num_destroy(&real);
}

static void test_number_real_arithmetic_parity(void)
{
    NUM_SCOPE(scope);
    number_t a = number_text("1.5");
    number_t b = number_text("0.25");
    number_t c = number_text("2.25");
    number_t d = number_text("3.5");
    number_t eighth_num = number_text("1");
    number_t eighth_den = number_text("8");
    number_t pow_base = number_text("1.5");
    number_t two = number_text("2");
    number_t add = num_add(a, b);
    number_t mul = num_mul(b, c);
    number_t sqrt_c = num_sqrt(c);
    number_t sub = num_sub(d, two);
    number_t div = num_div(eighth_num, eighth_den);
    number_t pow = num_pow(pow_base, two);
    number_t scaled = num_ldexp(pow, 3);
    number_t pow10 = num_pow10(3);

    assert_number_string("1.5 + 0.25", add, "1.75");
    assert_number_string("0.25 * 2.25", mul, "0.5625");
    assert_number_string("sqrt(2.25)", sqrt_c, "1.5");
    assert_number_string("3.5 - 2", sub, "1.5");
    assert_number_string("1 / 8", div, "⅛");
    assert_number_string("1.5 ^ 2", pow, "2.25");
    assert_number_string("ldexp(2.25, 3)", scaled, "18");
    assert_number_string("10^3", pow10, "1000");

    num_destroy(&pow10);
    num_destroy(&scaled);
    num_destroy(&pow);
    num_destroy(&div);
    num_destroy(&sub);
    num_destroy(&sqrt_c);
    num_destroy(&mul);
    num_destroy(&add);
    num_destroy(&two);
    num_destroy(&pow_base);
    num_destroy(&eighth_den);
    num_destroy(&eighth_num);
    num_destroy(&d);
    num_destroy(&c);
    num_destroy(&b);
    num_destroy(&a);
}

static void test_number_real_elementary_parity(void)
{
    NUM_SCOPE(scope);
    number_t one = number_text("1");
    number_t zero = number_text("0");
    number_t complex_zero = number_text("0 + 0i");
    number_t half = number_text("0.5");
    number_t exact_half = number_text("1/2");
    number_t two = number_text("2");
    number_t neg_one = number_text("-1");
    number_t sin_half = num_sin(half);
    number_t cos_half = num_cos(half);
    number_t tan_half = num_tan(half);
    number_t sec_half = num_sec(half);
    number_t cosec_half = num_cosec(half);
    number_t cot_half = num_cot(half);
    number_t sinh_half = num_sinh(half);
    number_t cosh_half = num_cosh(half);
    number_t tanh_half = num_tanh(half);
    number_t sech_half = num_sech(half);
    number_t cosech_half = num_cosech(half);
    number_t coth_half = num_coth(half);
    number_t atan_half = num_atan(half);
    number_t asin_half = num_asin(half);
    number_t asin_two = num_asin(two);
    number_t acos_half = num_acos(half);
    number_t acos_two = num_acos(two);
    number_t asec_two = num_asec(two);
    number_t acosec_two = num_acosec(two);
    number_t asec_exact_half = num_asec(exact_half);
    number_t acosec_exact_half = num_acosec(exact_half);
    number_t asec_zero = num_asec(zero);
    number_t acosec_zero = num_acosec(zero);
    number_t asec_complex_zero = num_asec(complex_zero);
    number_t acosec_complex_zero = num_acosec(complex_zero);
    number_t asec_inf = num_asec(NUM_INF);
    number_t acosec_inf = num_acosec(NUM_INF);
    number_t acot_zero = num_acot(zero);
    number_t acot_inf = num_acot(NUM_INF);
    number_t acot_ninf = num_acot(NUM_NINF);
    number_t acot_one = num_acot(one);
    number_t asinh_half = num_asinh(half);
    number_t acosh_two = num_acosh(two);
    number_t acosh_exact_half = num_acosh(exact_half);
    number_t atanh_half = num_atanh(half);
    number_t atanh_two = num_atanh(two);
    number_t asech_half = num_asech(half);
    number_t asech_two = num_asech(two);
    number_t acosech_one = num_acosech(one);
    number_t acoth_two = num_acoth(two);
    number_t acoth_exact_half = num_acoth(exact_half);
    number_t atan2_quad_ii = num_atan2(half, neg_one);
    number_t sec_cos = num_mul(sec_half, cos_half);
    number_t cosec_sin = num_mul(cosec_half, sin_half);
    number_t cot_tan = num_mul(cot_half, tan_half);
    number_t sech_cosh = num_mul(sech_half, cosh_half);
    number_t cosech_sinh = num_mul(cosech_half, sinh_half);
    number_t coth_tanh = num_mul(coth_half, tanh_half);
    number_t sin_pair = num_new();
    number_t cos_pair = num_new();
    number_t sinh_pair = num_new();
    number_t cosh_pair = num_new();

    ASSERT_EQ_INT(num_sincos(half, &sin_pair, &cos_pair), 0);
    ASSERT_EQ_INT(num_sinhcosh(half, &sinh_pair, &cosh_pair), 0);

    assert_number_string_prefix("sin(0.5)", sin_half, "0.479425538604203000273287935215571");
    assert_number_string_prefix("cos(0.5)", cos_half, "0.877582561890372716116281582603829");
    assert_number_string_prefix("tan(0.5)", tan_half, "0.546302489843790513255179465780285");
    assert_number_close_text("sec(0.5) * cos(0.5)", sec_cos, "1", NUMBER_PARITY_TIGHT_TOL);
    assert_number_close_text("cosec(0.5) * sin(0.5)", cosec_sin, "1", NUMBER_PARITY_TIGHT_TOL);
    assert_number_close_text("cot(0.5) * tan(0.5)", cot_tan, "1", NUMBER_PARITY_TIGHT_TOL);
    assert_number_string_prefix("sinh(0.5)", sinh_half, "0.521095305493747361622425626411491");
    assert_number_string_prefix("cosh(0.5)", cosh_half, "1.127625965206380785226225161402672");
    assert_number_string_prefix("tanh(0.5)", tanh_half, "0.462117157260009758502318483643672");
    assert_number_close_text("sech(0.5) * cosh(0.5)", sech_cosh, "1", NUMBER_PARITY_TIGHT_TOL);
    assert_number_close_text("cosech(0.5) * sinh(0.5)", cosech_sinh, "1", NUMBER_PARITY_TIGHT_TOL);
    assert_number_close_text("coth(0.5) * tanh(0.5)", coth_tanh, "1", NUMBER_PARITY_TIGHT_TOL);
    assert_number_string_prefix("atan(0.5)", atan_half, "0.463647609000806116214256231461214");
    assert_number_string_prefix("asin(0.5)", asin_half, "0.523598775598298873077107230546583");
    assert_number_close_text("asin(2)", asin_two,
                             "1.5707963267948966192313216916397514420985846996875529104874722961539082031 + "
                             "1.3169578969248167086250463473079684440269819714675164797684722569204601854i",
                             NUMBER_PARITY_TIGHT_TOL);
    assert_number_string_prefix("acos(0.5)", acos_half, "1.04719755119659774615421446109316");
    assert_number_close_text("acos(2)", acos_two,
                             "-1.3169578969248167086250463473079684440269819714675164797684722569204601854i",
                             NUMBER_PARITY_TIGHT_TOL);
    assert_number_string_prefix("asec(2)", asec_two, "1.04719755119659774615421446109316");
    assert_number_string_prefix("acosec(2)", acosec_two, "0.523598775598298873077107230546583");
    assert_number_close_text("asec(1/2) = acos(2)", num_sub(asec_exact_half, acos_two), "0",
                             NUMBER_PARITY_TIGHT_TOL);
    assert_number_close_text("acosec(1/2) = asin(2)", num_sub(acosec_exact_half, asin_two), "0",
                             NUMBER_PARITY_TIGHT_TOL);
    ASSERT_TRUE(num_is_nan(asec_zero));
    ASSERT_TRUE(num_is_nan(acosec_zero));
    ASSERT_TRUE(num_is_nan(asec_complex_zero));
    ASSERT_TRUE(num_is_nan(acosec_complex_zero));
    assert_number_string_prefix("asec(inf)", asec_inf, "1.57079632679489661923132169163975");
    assert_number_string("acosec(inf)", acosec_inf, "0");
    assert_number_string_prefix("acot(0)", acot_zero, "1.57079632679489661923132169163975");
    assert_number_string("acot(inf)", acot_inf, "0");
    assert_number_string_prefix("acot(-inf)", acot_ninf, "3.14159265358979323846264338327950");
    assert_number_string_prefix("acot(1)", acot_one, "0.785398163397448309615660845819875");
    assert_number_string_prefix("asinh(0.5)", asinh_half, "0.481211825059603447497758913424368");
    assert_number_string_prefix("acosh(2)", acosh_two, "1.316957896924816708625046347307968");
    assert_number_real_imag_close("acosh(1/2)", acosh_exact_half, "0", "1.047197551196597746154214461093168",
                                  NUMBER_PARITY_BRANCH_TOL);
    assert_number_string_prefix("atanh(0.5)", atanh_half, "0.549306144334054845697622618461262");
    assert_number_real_imag_close("atanh(2)", atanh_two, "0.549306144334054845697622618461262",
                                  "1.570796326794896619231321691639751", NUMBER_PARITY_BRANCH_TOL);
    assert_number_string_prefix("asech(0.5)", asech_half, "1.316957896924816708625046347307968");
    assert_number_close_text("asech(2) = acosh(1/2)", num_sub(asech_two, acosh_exact_half), "0",
                             NUMBER_PARITY_TIGHT_TOL);
    assert_number_string_prefix("acosech(1)", acosech_one, "0.881373587019543025232609324979792");
    assert_number_string_prefix("acoth(2)", acoth_two, "0.549306144334054845697622618461262");
    assert_number_close_text("acoth(1/2) = atanh(2)", num_sub(acoth_exact_half, atanh_two), "0",
                             NUMBER_PARITY_TIGHT_TOL);
    assert_number_string_prefix("atan2(0.5, -1)", atan2_quad_ii, "2.67794504458898712224838715181828");
    assert_number_string_prefix("sincos(0.5).sin", sin_pair, "0.479425538604203000273287935215571");
    assert_number_string_prefix("sincos(0.5).cos", cos_pair, "0.877582561890372716116281582603829");
    assert_number_string_prefix("sinhcosh(0.5).sinh", sinh_pair, "0.521095305493747361622425626411491");
    assert_number_string_prefix("sinhcosh(0.5).cosh", cosh_pair, "1.127625965206380785226225161402672");

    num_destroy(&cosh_pair);
    num_destroy(&sinh_pair);
    num_destroy(&cos_pair);
    num_destroy(&sin_pair);
    num_destroy(&coth_tanh);
    num_destroy(&cosech_sinh);
    num_destroy(&sech_cosh);
    num_destroy(&cot_tan);
    num_destroy(&cosec_sin);
    num_destroy(&sec_cos);
    num_destroy(&atan2_quad_ii);
    num_destroy(&acoth_exact_half);
    num_destroy(&acoth_two);
    num_destroy(&acosech_one);
    num_destroy(&asech_two);
    num_destroy(&asech_half);
    num_destroy(&atanh_two);
    num_destroy(&atanh_half);
    num_destroy(&acosh_exact_half);
    num_destroy(&acosh_two);
    num_destroy(&asinh_half);
    num_destroy(&acot_one);
    num_destroy(&acot_ninf);
    num_destroy(&acot_inf);
    num_destroy(&acot_zero);
    num_destroy(&acosec_inf);
    num_destroy(&asec_inf);
    num_destroy(&acosec_complex_zero);
    num_destroy(&asec_complex_zero);
    num_destroy(&acosec_zero);
    num_destroy(&asec_zero);
    num_destroy(&acosec_two);
    num_destroy(&asec_two);
    num_destroy(&acosec_exact_half);
    num_destroy(&asec_exact_half);
    num_destroy(&acos_half);
    num_destroy(&asin_half);
    num_destroy(&atan_half);
    num_destroy(&coth_half);
    num_destroy(&cosech_half);
    num_destroy(&sech_half);
    num_destroy(&tanh_half);
    num_destroy(&cosh_half);
    num_destroy(&sinh_half);
    num_destroy(&cot_half);
    num_destroy(&cosec_half);
    num_destroy(&sec_half);
    num_destroy(&tan_half);
    num_destroy(&cos_half);
    num_destroy(&sin_half);
    num_destroy(&neg_one);
    num_destroy(&two);
    num_destroy(&half);
    num_destroy(&complex_zero);
    num_destroy(&zero);
    num_destroy(&exact_half);
    num_destroy(&one);
}

static void test_number_real_special_parity(void)
{
    NUM_SCOPE(scope);
    number_t half = number_text("0.5");
    number_t one = number_text("1");
    number_t two = number_text("2");
    number_t three = number_text("3");
    number_t two_point_345 = number_text("2.345");
    number_t two_point_5 = number_text("2.5");
    number_t three_point_5 = number_text("3.5");
    number_t five_point_5 = number_text("5.5");
    number_t neg_tenth = number_text("-0.1");
    number_t gamma5 = num_gamma(number_text("5"));
    number_t gamma_2345 = num_gamma(two_point_345);
    number_t lgamma_2345 = num_lgamma(two_point_345);
    number_t digamma_2345 = num_digamma(two_point_345);
    number_t trigamma_2345 = num_trigamma(two_point_345);
    number_t tetragamma_2345 = num_tetragamma(two_point_345);
    number_t erf_half = num_erf(half);
    number_t erfc_half = num_erfc(half);
    number_t erfinv_half = num_erfinv(half);
    number_t erfcinv_half = num_erfcinv(half);
    number_t gammainv_three = num_gammainv(three);
    number_t w0_one = num_lambert_w0(one);
    number_t wm1_neg_tenth = num_lambert_wm1(neg_tenth);
    number_t beta_2_3 = num_beta(two, three);
    number_t logbeta_25_35 = num_logbeta(two_point_5, three_point_5);
    number_t binomial_55_25 = num_binomial(five_point_5, two_point_5);
    number_t beta_pdf = num_beta_pdf(half, two_point_5, three_point_5);
    number_t logbeta_pdf = num_logbeta_pdf(half, two_point_5, three_point_5);
    number_t normal_pdf = num_normal_pdf(half);
    number_t normal_logpdf = num_normal_logpdf(half);
    number_t normal_cdf0 = num_normal_cdf(NUM_ZERO);
    number_t lower_1_1 = num_gammainc_lower(one, one);
    number_t upper_1_1 = num_gammainc_upper(one, one);
    number_t p_1_1 = num_gammainc_P(one, one);
    number_t q_1_1 = num_gammainc_Q(one, one);
    number_t ei_one = num_Ei(one);
    number_t e1_one = num_E1(one);

    assert_number_string("gamma(5)", gamma5, "24");
    assert_number_string_prefix("gamma(2.345)", gamma_2345, "1.199297829415319285526815335887956");
    assert_number_string_prefix("lgamma(2.345)", lgamma_2345, "0.181736243377572037978629332299959");
    assert_number_string_prefix("digamma(2.345)", digamma_2345, "0.624166816851114101398494286434486");
    assert_number_string_prefix("trigamma(2.345)", trigamma_2345, "0.529868755482033898168534051754644");
    assert_number_string_prefix("tetragamma(2.345)", tetragamma_2345, "-0.275072127759200054732994928499661");
    assert_number_string_prefix("erf(0.5)", erf_half, "0.520499877813046537682746653891964");
    assert_number_string_prefix("erfc(0.5)", erfc_half, "0.479500122186953462317253346108035");
    assert_number_string_prefix("erfinv(0.5)", erfinv_half, "0.47693627620446987");
    assert_number_string_prefix("erfcinv(0.5)", erfcinv_half, "0.47693627620446987");
    assert_number_string_prefix("gammainv(3)", gammainv_three, "3.4058699863095669");
    assert_number_string_prefix("lambert_w0(1)", w0_one, "0.567143290409783872999968662210355");
    assert_number_string_prefix("lambert_wm1(-0.1)", wm1_neg_tenth, "-3.57715206395729721840939196351199");
    assert_number_string_prefix("beta(2, 3)", beta_2_3, "0.083333333333333333333333333333333");
    assert_number_string_prefix("logbeta(2.5, 3.5)", logbeta_25_35, "-3.30183526996205260979918438338982");
    assert_number_string("binomial(5.5, 2.5)", binomial_55_25, "14.4375");
    assert_number_string_prefix("beta_pdf(0.5, 2.5, 3.5)", beta_pdf, "1.697652726313550248201426809306819");
    assert_number_string_prefix("logbeta_pdf(0.5, 2.5, 3.5)", logbeta_pdf, "0.529246547722271372130255897557121");
    assert_number_string_prefix("normal_pdf(0.5)", normal_pdf, "0.352065326764299477774680441596517");
    assert_number_string_prefix("normal_logpdf(0.5)", normal_logpdf, "-1.04393853320467274178032973640561");
    assert_number_string("normal_cdf(0)", normal_cdf0, "0.5");
    assert_number_string_prefix("gammainc_lower(1, 1)", lower_1_1, "0.632120558828557678404476229838539");
    assert_number_string_prefix("gammainc_upper(1, 1)", upper_1_1, "0.367879441171442321595523770161460");
    assert_number_string_prefix("gammainc_P(1, 1)", p_1_1, "0.632120558828557678404476229838539");
    assert_number_string_prefix("gammainc_Q(1, 1)", q_1_1, "0.367879441171442321595523770161460");
    assert_number_string_prefix("ei(1)", ei_one, "1.895117816355936755466520934331634");
    assert_number_string_prefix("e1(1)", e1_one, "0.219383934395520273677163775460121");

    num_destroy(&e1_one);
    num_destroy(&ei_one);
    num_destroy(&q_1_1);
    num_destroy(&p_1_1);
    num_destroy(&upper_1_1);
    num_destroy(&lower_1_1);
    num_destroy(&normal_cdf0);
    num_destroy(&normal_logpdf);
    num_destroy(&normal_pdf);
    num_destroy(&logbeta_pdf);
    num_destroy(&beta_pdf);
    num_destroy(&binomial_55_25);
    num_destroy(&logbeta_25_35);
    num_destroy(&beta_2_3);
    num_destroy(&wm1_neg_tenth);
    num_destroy(&w0_one);
    num_destroy(&gammainv_three);
    num_destroy(&erfcinv_half);
    num_destroy(&erfinv_half);
    num_destroy(&erfc_half);
    num_destroy(&erf_half);
    num_destroy(&tetragamma_2345);
    num_destroy(&trigamma_2345);
    num_destroy(&digamma_2345);
    num_destroy(&lgamma_2345);
    num_destroy(&gamma_2345);
    num_destroy(&gamma5);
    num_destroy(&neg_tenth);
    num_destroy(&five_point_5);
    num_destroy(&three_point_5);
    num_destroy(&two_point_5);
    num_destroy(&two_point_345);
    num_destroy(&three);
    num_destroy(&two);
    num_destroy(&one);
    num_destroy(&half);
}

static void test_number_real_special_identities(void)
{
    NUM_SCOPE(scope);
    number_t one = number_text("1");
    number_t half = number_text("0.5");
    number_t two_point_345 = number_text("2.345");
    number_t two_point_5 = number_text("2.5");
    number_t three_point_5 = number_text("3.5");
    number_t neg_035 = number_text("-0.35");
    number_t lgamma_lhs = num_lgamma(number_text("3.345"));
    number_t lgamma_rhs = num_lgamma(two_point_345);
    number_t log_term = num_log(two_point_345);
    number_t lgamma_diff = num_sub(num_sub(lgamma_lhs, lgamma_rhs), log_term);
    number_t p = num_gammainc_P(half, one);
    number_t q = num_gammainc_Q(half, one);
    number_t pq_sum = num_sub(num_add(p, q), one);
    number_t w = num_productlog(neg_035);
    number_t w_exp_w = num_mul(w, num_exp(w));
    number_t productlog_error = num_sub(w_exp_w, neg_035);
    number_t beta = num_beta(two_point_5, three_point_5);
    number_t exp_logbeta = num_exp(num_logbeta(two_point_5, three_point_5));
    number_t beta_error = num_sub(beta, exp_logbeta);

    assert_number_close_text("lgamma(x + 1) - lgamma(x) - log(x)", lgamma_diff, "0", NUMBER_PARITY_TIGHT_TOL);
    assert_number_close_text("gammainc_P(0.5, 1) + gammainc_Q(0.5, 1) - 1", pq_sum, "0", NUMBER_PARITY_TIGHT_TOL);
    assert_number_close_text("productlog(x) * exp(productlog(x)) - x", productlog_error, "0", NUMBER_PARITY_TIGHT_TOL);
    assert_number_close_text("beta(2.5, 3.5) - exp(logbeta(2.5, 3.5))", beta_error, "0", NUMBER_PARITY_TIGHT_TOL);

    num_destroy(&beta_error);
    num_destroy(&exp_logbeta);
    num_destroy(&beta);
    num_destroy(&productlog_error);
    num_destroy(&w_exp_w);
    num_destroy(&w);
    num_destroy(&pq_sum);
    num_destroy(&q);
    num_destroy(&p);
    num_destroy(&lgamma_diff);
    num_destroy(&log_term);
    num_destroy(&lgamma_rhs);
    num_destroy(&lgamma_lhs);
    num_destroy(&neg_035);
    num_destroy(&three_point_5);
    num_destroy(&two_point_5);
    num_destroy(&two_point_345);
    num_destroy(&half);
    num_destroy(&one);
}

static void test_number_complex_arithmetic_parity(void)
{
    NUM_SCOPE(scope);
    number_t a = number_text("3 + 4i");
    number_t b = number_text("1 - 2i");
    number_t add = num_add(a, b);
    number_t mul = num_mul(add, b);
    number_t div = num_div(a, b);
    number_t conj = num_conj(div);
    number_t inv = num_inv(conj);
    number_t abs_inv = num_abs(inv);
    number_t cdouble_a = num_create_from_cdouble(3.0 + 4.0 * I);
    number_t cdouble_b = num_create_from_cdouble(1.0 - 2.0 * I);
    number_t cdouble_mul = num_mul(cdouble_a, cdouble_b);
    number_t cdouble_div = num_div(cdouble_a, cdouble_b);

    assert_number_real_imag_prefix("(3 + 4i) + (1 - 2i)", add, "4", "2");
    assert_number_real_imag_prefix("(4 + 2i) * (1 - 2i)", mul, "8", "-6");
    assert_number_real_imag_prefix("(3 + 4i) / (1 - 2i)", div, "-1", "2");
    assert_number_real_imag_prefix("conj(-1 + 2i)", conj, "-1", "-2");
    assert_number_real_imag_prefix("1 / (-1 - 2i)", inv, "-⅕", "⅖");
    assert_number_string_prefix("abs(-0.2 + 0.4i)", abs_inv, "0.447213595499957939281834733746255");
    assert_number_real_imag_prefix("cdouble (3 + 4i) * (1 - 2i)", cdouble_mul, "11", "-2");
    assert_number_real_imag_prefix("cdouble (3 + 4i) / (1 - 2i)", cdouble_div, "-1", "2");

    num_destroy(&cdouble_div);
    num_destroy(&cdouble_mul);
    num_destroy(&cdouble_b);
    num_destroy(&cdouble_a);
    num_destroy(&abs_inv);
    num_destroy(&inv);
    num_destroy(&conj);
    num_destroy(&div);
    num_destroy(&mul);
    num_destroy(&add);
    num_destroy(&b);
    num_destroy(&a);
}

static void test_number_complex_elementary_parity(void)
{
    NUM_SCOPE(scope);
    number_t z = number_text("0.567 + 0.321i");
    number_t small = number_text("0.321 + 0.123i");
    number_t exp_i_pi = num_exp(num_mul(NUM_I, NUM_PI));
    number_t log_neg_one = num_log(number_text("-1 + 0i"));
    number_t log_neg_three = num_log(number_text("-3"));
    number_t log10_neg_one = num_log10(number_text("-1"));
    number_t log10_100 = num_log10(number_text("100"));
    number_t sin_z = num_sin(z);
    number_t cos_z = num_cos(z);
    number_t tan_z = num_tan(z);
    number_t sinh_z = num_sinh(z);
    number_t cosh_z = num_cosh(z);
    number_t tanh_z = num_tanh(z);
    number_t asin_small = num_asin(small);
    number_t acos_small = num_acos(small);
    number_t asinh_small = num_asinh(small);
    number_t acosh_input = number_text("2 + 0.5i");
    number_t acosh_value = num_acosh(acosh_input);
    number_t atanh_small = num_atanh(small);
    number_t sin_asin = num_sub(num_sin(asin_small), small);
    number_t cos_acos = num_sub(num_cos(acos_small), small);
    number_t sinh_asinh = num_sub(num_sinh(asinh_small), small);
    number_t cosh_acosh = num_sub(num_cosh(acosh_value), acosh_input);
    number_t tanh_atanh = num_sub(num_tanh(atanh_small), small);
    number_t log_roundtrip_input = number_text("0.75 + 1.25i");
    number_t log_roundtrip = num_sub(num_exp(num_log(log_roundtrip_input)), log_roundtrip_input);

    assert_number_real_imag_close("exp(i*pi)", exp_i_pi, "-1", "0", NUMBER_PARITY_LOOSE_TOL);
    assert_number_real_imag_close("log(-1 + 0i)", log_neg_one, "0", "3.141592653589793238462643383279502",
                                  NUMBER_PARITY_BRANCH_TOL);
    assert_number_real_imag_close("log(-3)", log_neg_three, "1.098612288668109691395245236922526",
                                  "3.141592653589793238462643383279502", NUMBER_PARITY_BRANCH_TOL);
    assert_number_real_imag_close("log10(-1)", log10_neg_one, "0", "1.364376353841841347485783625431355",
                                  NUMBER_PARITY_BRANCH_TOL);
    assert_number_string("log10(100)", log10_100, "2");
    assert_number_real_imag_prefix("sin(0.567 + 0.321i)", sin_z, "0.56501421", "0.27544272");
    assert_number_real_imag_prefix("cos(0.567 + 0.321i)", cos_z, "0.8873489", "-0.17538654");
    assert_number_real_imag_prefix("tan(0.567 + 0.321i)", tan_z, "0.5537574", "0.41986226");
    assert_number_real_imag_prefix("sinh(0.567 + 0.321i)", sinh_z, "0.5673337", "0.36760644");
    assert_number_real_imag_prefix("cosh(0.567 + 0.321i)", cosh_z, "1.1055846", "0.1886382");
    assert_number_real_imag_prefix("tanh(0.567 + 0.321i)", tanh_z, "0.55376346", "0.23801478");
    assert_number_close_text("sin(asin(z)) - z", sin_asin, "0", NUMBER_PARITY_LOOSE_TOL);
    assert_number_close_text("cos(acos(z)) - z", cos_acos, "0", NUMBER_PARITY_LOOSE_TOL);
    assert_number_close_text("sinh(asinh(z)) - z", sinh_asinh, "0", NUMBER_PARITY_LOOSE_TOL);
    assert_number_close_text("cosh(acosh(z)) - z", cosh_acosh, "0", NUMBER_PARITY_LOOSE_TOL);
    assert_number_close_text("tanh(atanh(z)) - z", tanh_atanh, "0", NUMBER_PARITY_LOOSE_TOL);
    assert_number_close_text("exp(log(0.75 + 1.25i)) - input", log_roundtrip, "0", NUMBER_PARITY_LOOSE_TOL);

    num_destroy(&log_roundtrip);
    num_destroy(&log_roundtrip_input);
    num_destroy(&tanh_atanh);
    num_destroy(&cosh_acosh);
    num_destroy(&sinh_asinh);
    num_destroy(&cos_acos);
    num_destroy(&sin_asin);
    num_destroy(&atanh_small);
    num_destroy(&acosh_value);
    num_destroy(&acosh_input);
    num_destroy(&asinh_small);
    num_destroy(&acos_small);
    num_destroy(&asin_small);
    num_destroy(&tanh_z);
    num_destroy(&cosh_z);
    num_destroy(&sinh_z);
    num_destroy(&tan_z);
    num_destroy(&cos_z);
    num_destroy(&sin_z);
    num_destroy(&log10_100);
    num_destroy(&log10_neg_one);
    num_destroy(&log_neg_one);
    num_destroy(&exp_i_pi);
    num_destroy(&small);
    num_destroy(&z);
}

static void test_number_complex_special_parity(void)
{
    NUM_SCOPE(scope);
    number_t real_one = number_text("1 + 0i");
    number_t gamma_one = num_gamma(real_one);
    number_t erf_one = num_erf(real_one);
    number_t z = number_text("1.5 + 0.7i");
    number_t gamma_z = num_gamma(z);
    number_t exp_lgamma_z = num_exp(num_lgamma(z));
    number_t gamma_roundtrip = num_sub(gamma_z, exp_lgamma_z);
    number_t z_plus_one = num_add(z, NUM_ONE);
    number_t gamma_z_plus_one = num_gamma(z_plus_one);
    number_t z_gamma_z = num_mul(z, gamma_z);
    number_t gamma_recurrence = num_sub(gamma_z_plus_one, z_gamma_z);
    number_t erf_input = number_text("0.5 + 0.25i");
    number_t erf_z = num_erf(erf_input);
    number_t erfc_z = num_erfc(erf_input);
    number_t erf_erfc = num_sub(num_add(erf_z, erfc_z), NUM_ONE);
    number_t beta_a = number_text("1.25 + 0.5i");
    number_t beta_b = number_text("0.75 + 0.25i");
    number_t beta_value = num_beta(beta_a, beta_b);
    number_t exp_logbeta_value = num_exp(num_logbeta(beta_a, beta_b));
    number_t beta_identity = num_sub(beta_value, exp_logbeta_value);
    number_t inc_lower = num_gammainc_lower(beta_a, beta_b);
    number_t inc_upper = num_gammainc_upper(beta_a, beta_b);
    number_t inc_gamma = num_gamma(beta_a);
    number_t inc_sum = num_sub(num_add(inc_lower, inc_upper), inc_gamma);
    number_t inc_p = num_gammainc_P(beta_a, beta_b);
    number_t inc_q = num_gammainc_Q(beta_a, beta_b);
    number_t inc_pq = num_sub(num_add(inc_p, inc_q), NUM_ONE);
    number_t w_input = number_text("1 + 1i");
    number_t w = num_productlog(w_input);
    number_t w_roundtrip = num_sub(num_mul(w, num_exp(w)), w_input);

    assert_number_real_imag_prefix("gamma(1 + 0i)", gamma_one, "1", "0");
    assert_number_real_imag_prefix("erf(1 + 0i)", erf_one, "0.84270079294971486934122063508262", "0");
    assert_number_close_text("gamma(z) - exp(lgamma(z))", gamma_roundtrip, "0", NUMBER_PARITY_QCOMPLEX_TOL);
    assert_number_close_text("gamma(z + 1) - z * gamma(z)", gamma_recurrence, "0", NUMBER_PARITY_QCOMPLEX_TOL);
    assert_number_close_text("erf(z) + erfc(z) - 1", erf_erfc, "0", NUMBER_PARITY_QCOMPLEX_TOL);
    assert_number_close_text("beta(a,b) - exp(logbeta(a,b))", beta_identity, "0", NUMBER_PARITY_QCOMPLEX_TOL);
    assert_number_close_text("gammainc_lower + gammainc_upper - gamma", inc_sum, "0", NUMBER_PARITY_QCOMPLEX_TOL);
    assert_number_close_text("gammainc_P + gammainc_Q - 1", inc_pq, "0", NUMBER_PARITY_QCOMPLEX_TOL);
    assert_number_close_text("productlog(z) * exp(productlog(z)) - z", w_roundtrip, "0", NUMBER_PARITY_QCOMPLEX_TOL);

    num_destroy(&w_roundtrip);
    num_destroy(&w);
    num_destroy(&w_input);
    num_destroy(&inc_pq);
    num_destroy(&inc_q);
    num_destroy(&inc_p);
    num_destroy(&inc_sum);
    num_destroy(&inc_gamma);
    num_destroy(&inc_upper);
    num_destroy(&inc_lower);
    num_destroy(&beta_identity);
    num_destroy(&exp_logbeta_value);
    num_destroy(&beta_value);
    num_destroy(&beta_b);
    num_destroy(&beta_a);
    num_destroy(&erf_erfc);
    num_destroy(&erfc_z);
    num_destroy(&erf_z);
    num_destroy(&erf_input);
    num_destroy(&gamma_recurrence);
    num_destroy(&z_gamma_z);
    num_destroy(&gamma_z_plus_one);
    num_destroy(&z_plus_one);
    num_destroy(&gamma_roundtrip);
    num_destroy(&exp_lgamma_z);
    num_destroy(&gamma_z);
    num_destroy(&z);
    num_destroy(&erf_one);
    num_destroy(&gamma_one);
    num_destroy(&real_one);
}

void run_number_backend_parity_tests(void)
{
    size_t saved_precision = num_get_default_prec_bits();

    ASSERT_EQ_INT(num_set_default_prec_bits(NUMBER_PARITY_PRECISION), 0);

    TEST_RUN_SUBTEST(test_number_real_arithmetic_parity, "number,parity,real");
    TEST_RUN_SUBTEST(test_number_real_elementary_parity, "number,parity,real");
    TEST_RUN_SUBTEST(test_number_real_special_parity, "number,parity,real");
    TEST_RUN_SUBTEST(test_number_real_special_identities, "number,parity,real");
    TEST_RUN_SUBTEST(test_number_complex_arithmetic_parity, "number,parity,complex");
    TEST_RUN_SUBTEST(test_number_complex_elementary_parity, "number,parity,complex");
    TEST_RUN_SUBTEST(test_number_complex_special_parity, "number,parity,complex");

    ASSERT_EQ_INT(num_set_default_prec_bits(saved_precision), 0);
}
