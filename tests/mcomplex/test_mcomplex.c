#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mcomplex.h"

#define TEST_CONFIG_MODE TEST_CONFIG_GLOBAL
#define TEST_CONFIG_MAIN
#include "test_harness.h"

static int format_mfloat_pretty_value(char *buf, size_t buf_size, const mfloat_t *value);
static int format_mfloat_precise_value(char *buf, size_t buf_size, const mfloat_t *value);
static int format_mcomplex_parts(const mcomplex_t *value,
                                 int (*part_formatter)(char *buf, size_t buf_size, const mfloat_t *value),
                                 char *real_buf, size_t real_buf_size,
                                 char *imag_buf, size_t imag_buf_size,
                                 char *imag_sign,
                                 const char **imag_digits);
static void print_mcomplex_formatted_lines(const char *label,
                                           const mcomplex_t *value,
                                           int (*part_formatter)(char *buf, size_t buf_size, const mfloat_t *value),
                                           int allow_single_line);
static void print_mcomplex_value(const char *label, const mcomplex_t *value);
static void print_mcomplex_input(const char *label, const char *text);
static void print_mcomplex_error_check(const char *label,
                                       const mcomplex_t *got,
                                       const char *expected_real,
                                       const char *expected_imag);
static int mfloat_meets_precision(const mfloat_t *got,
                                  const char *expected_text,
                                  int relative_mode);
static int mcomplex_meets_precision(const mcomplex_t *got,
                                    const char *expected_real,
                                    const char *expected_imag);
static mcomplex_t *create_real_mcomplex(const char *real_text);

static int format_mfloat_pretty_value(char *buf, size_t buf_size, const mfloat_t *value)
{
    char fixed_buf[512];

    if (!buf || buf_size == 0 || !value)
        return -1;

    if (mf_sprintf(fixed_buf, sizeof(fixed_buf), "%mf", value) < 0)
        return -1;

    if (!mf_is_zero(value) && strlen(fixed_buf) > 48u)
        return mf_sprintf(buf, buf_size, "%MF", value);

    if (snprintf(buf, buf_size, "%s", fixed_buf) >= (int)buf_size)
        return -1;

    return (int)strlen(buf);
}

static int format_mfloat_precise_value(char *buf, size_t buf_size, const mfloat_t *value)
{
    char fmt[32];
    size_t precision_bits;
    int decimal_digits;

    if (!buf || buf_size == 0 || !value)
        return -1;

    precision_bits = mf_get_precision(value);
    decimal_digits = (int)ceil((double)precision_bits * 0.3010299956639812);
    if (decimal_digits < 1)
        decimal_digits = 1;
    if (decimal_digits > 240)
        decimal_digits = 240;

    snprintf(fmt, sizeof(fmt), "%%.%dmf", decimal_digits);
    return mf_sprintf(buf, buf_size, fmt, value);
}

static int format_mcomplex_parts(const mcomplex_t *value,
                                 int (*part_formatter)(char *buf, size_t buf_size, const mfloat_t *value),
                                 char *real_buf, size_t real_buf_size,
                                 char *imag_buf, size_t imag_buf_size,
                                 char *imag_sign,
                                 const char **imag_digits)
{
    if (!value || !part_formatter || !real_buf || real_buf_size == 0u ||
        !imag_buf || imag_buf_size == 0u || !imag_sign || !imag_digits)
        return -1;

    if (part_formatter(real_buf, real_buf_size, mc_real(value)) < 0 ||
        part_formatter(imag_buf, imag_buf_size, mc_imag(value)) < 0)
        return -1;

    *imag_sign = (imag_buf[0] == '-') ? '-' : '+';
    *imag_digits = imag_buf;
    if (imag_buf[0] == '-')
        (*imag_digits)++;

    return 0;
}

static void print_mcomplex_formatted_lines(const char *label,
                                           const mcomplex_t *value,
                                           int (*part_formatter)(char *buf, size_t buf_size, const mfloat_t *value),
                                           int allow_single_line)
{
    char real_buf[1024];
    char imag_buf[1024];
    const char *imag_digits = NULL;
    char imag_sign = '+';
    size_t inline_len;

    if (!label || !value || !part_formatter) {
        printf("    %s.re = <format-error>\n", label ? label : "<null>");
        printf("    %s.im = <format-error>\n", label ? label : "<null>");
        return;
    }

    if (format_mcomplex_parts(value,
                              part_formatter,
                              real_buf, sizeof(real_buf),
                              imag_buf, sizeof(imag_buf),
                              &imag_sign, &imag_digits) != 0) {
        printf("    %s.re = <format-error>\n", label);
        printf("    %s.im = <format-error>\n", label);
        return;
    }

    inline_len = strlen(real_buf) + strlen(imag_digits) + 4u;
    if (allow_single_line && inline_len <= 48u) {
        printf("    %-10s = " C_WHITE "%s %c %si" C_RESET "\n",
               label, real_buf, imag_sign, imag_digits);
        return;
    }

    printf("    %-10s = " C_WHITE "  %s" C_RESET "\n", label, real_buf);
    printf("    %-10s   " C_WHITE "%c %si" C_RESET "\n", "", imag_sign, imag_digits);
}

static void print_mcomplex_value(const char *label, const mcomplex_t *value)
{
    size_t precision_bits;
    int decimal_digits;

    if (!value) {
        printf(C_CYAN "%s" C_RESET " = <null>\n", label);
        return;
    }
    precision_bits = mc_get_precision(value);
    decimal_digits = (int)ceil((double)precision_bits * 0.3010299956639812);
    if (decimal_digits < 1)
        decimal_digits = 1;

    printf(C_CYAN "%s" C_RESET "\n", label);
    printf("    precision = %zu bits (~%d digits)\n", precision_bits, decimal_digits);
    print_mcomplex_formatted_lines("pretty", value, format_mfloat_pretty_value, 1);
    print_mcomplex_formatted_lines("sci", value, format_mfloat_precise_value, 0);
    fflush(stdout);
}

static void print_mcomplex_input(const char *label, const char *text)
{
    printf(C_CYAN "%s" C_RESET " = " C_WHITE "%s" C_RESET "\n", label, text ? text : "<null>");
    fflush(stdout);
}

static void print_mcomplex_error_check(const char *label,
                                       const mcomplex_t *got,
                                       const char *expected_real,
                                       const char *expected_imag)
{
    mcomplex_t *expected = NULL;
    mfloat_t *real_err = NULL;
    mfloat_t *imag_err = NULL;
    char real_err_buf[256];
    char imag_err_buf[256];
    size_t precision_bits = 0;
    int decimal_digits = 0;

    expected = mc_new_prec(got ? mc_get_precision(got) : 0u);
    if (!got || !expected ||
        mf_set_string((mfloat_t *)mc_real(expected), expected_real) != 0 ||
        mf_set_string((mfloat_t *)mc_imag(expected), expected_imag) != 0 ||
        (real_err = mf_clone(mc_real(got))) == NULL ||
        (imag_err = mf_clone(mc_imag(got))) == NULL ||
        mf_sub(real_err, mc_real(expected)) != 0 ||
        mf_abs(real_err) != 0 ||
        mf_sub(imag_err, mc_imag(expected)) != 0 ||
        mf_abs(imag_err) != 0 ||
        mf_sprintf(real_err_buf, sizeof(real_err_buf), "%.6MF", real_err) < 0 ||
        mf_sprintf(imag_err_buf, sizeof(imag_err_buf), "%.6MF", imag_err) < 0) {
        printf(C_CYAN "%s" C_RESET "\n", label);
        printf("    expected   = <format-error>\n");
        printf("    got        = <format-error>\n");
        printf("    error.re = <format-error>\n");
        printf("    error.im = <format-error>\n");
        mf_free(real_err);
        mf_free(imag_err);
        mc_free(expected);
        return;
    }

    precision_bits = mc_get_precision(got);
    decimal_digits = (int)ceil((double)precision_bits * 0.3010299956639812);
    if (decimal_digits < 1)
        decimal_digits = 1;
    printf(C_CYAN "%s" C_RESET "\n", label);
    printf("    precision = %zu bits (~%d digits)\n", precision_bits, decimal_digits);
    print_mcomplex_formatted_lines("expected", expected, format_mfloat_precise_value, 0);
    print_mcomplex_formatted_lines("got", got, format_mfloat_precise_value, 0);
    print_mcomplex_formatted_lines("expected~", expected, format_mfloat_pretty_value, 1);
    print_mcomplex_formatted_lines("got~", got, format_mfloat_pretty_value, 1);
    printf("    error.re  = %s\n", real_err_buf);
    printf("    error.im  = %s\n", imag_err_buf);
    fflush(stdout);

    mf_free(real_err);
    mf_free(imag_err);
    mc_free(expected);
}

static int mfloat_meets_precision(const mfloat_t *got,
                                  const char *expected_text,
                                  int relative_mode)
{
    mfloat_t *expected = NULL;
    mfloat_t *error = NULL;
    mfloat_t *tol = NULL;
    size_t precision_bits;
    long tol_exp2;
    int ok = 0;

    if (!got || !expected_text)
        return 0;

    precision_bits = mf_get_precision(got);

    expected = mf_create_string(expected_text);
    error = mf_clone(got);
    tol = mf_create_long(1);
    if (!expected || !error || !tol)
        goto cleanup;

    if (mf_sub(error, expected) != 0 || mf_abs(error) != 0)
        goto cleanup;

    if (relative_mode && !mf_is_zero(expected)) {
        tol_exp2 = mf_get_exponent2(expected) +
                   (long)mf_get_mantissa_bits(expected) - 1l -
                   (long)precision_bits;
    } else {
        tol_exp2 = -(long)precision_bits;
    }

    if (mf_ldexp(tol, (int)tol_exp2) != 0)
        goto cleanup;

    ok = mf_le(error, tol);

cleanup:
    mf_free(expected);
    mf_free(error);
    mf_free(tol);
    return ok;
}

static int mcomplex_meets_precision(const mcomplex_t *got,
                                    const char *expected_real,
                                    const char *expected_imag)
{
    mfloat_t *real_expected = NULL;
    mfloat_t *imag_expected = NULL;
    int real_relative = 0;
    int imag_relative = 0;
    int ok = 0;

    if (!got || !expected_real || !expected_imag)
        return 0;

    real_expected = mf_create_string(expected_real);
    imag_expected = mf_create_string(expected_imag);
    if (!real_expected || !imag_expected)
        goto cleanup;

    real_relative = !mf_is_zero(real_expected);
    imag_relative = !mf_is_zero(imag_expected);
    ok = mfloat_meets_precision(mc_real(got), expected_real, real_relative) &&
         mfloat_meets_precision(mc_imag(got), expected_imag, imag_relative);

cleanup:
    mf_free(real_expected);
    mf_free(imag_expected);
    return ok;
}

static mcomplex_t *create_real_mcomplex(const char *real_text)
{
    mcomplex_t *value = NULL;

    if (!real_text)
        return NULL;

    value = mc_create_string(real_text);
    if (!value)
        return NULL;
    if (!mf_is_zero(mc_imag(value))) {
        mc_free(value);
        return NULL;
    }
    return value;
}

static void test_conversion_to_from_qcomplex(void)
{
    mcomplex_t *a = NULL;
    mcomplex_t *b = NULL;
    qcomplex_t q = qc_make(qf_from_string("3.1415926535897932384626433832795"),
                           qf_from_string("-2.7182818284590452353602874713527"));
    qcomplex_t qa;
    qcomplex_t qb;
    char got_buf[256];
    char expected_buf[256];
    char qc_buf[256];

    for (;;) {
        a = mc_create_qcomplex(q);
        ASSERT_NOT_NULL(a);
        print_mcomplex_value("mc_create_qcomplex value", a);

        qa = mc_to_qcomplex(a);
        qf_to_string(qa.re, got_buf, sizeof(got_buf));
        qf_to_string(q.re, expected_buf, sizeof(expected_buf));
        printf(C_CYAN "mc_to_qcomplex real part" C_RESET "\n");
        printf("    expected = %s\n", expected_buf);
        printf("    got      = " C_WHITE "%s" C_RESET "\n", got_buf);
        fflush(stdout);
        ASSERT_TRUE(strcmp(got_buf, expected_buf) == 0);

        qf_to_string(qa.im, got_buf, sizeof(got_buf));
        qf_to_string(q.im, expected_buf, sizeof(expected_buf));
        printf(C_CYAN "mc_to_qcomplex imag part" C_RESET "\n");
        printf("    expected = %s\n", expected_buf);
        printf("    got      = " C_WHITE "%s" C_RESET "\n", got_buf);
        fflush(stdout);
        ASSERT_TRUE(strcmp(got_buf, expected_buf) == 0);

        b = mc_new();
        ASSERT_NOT_NULL(b);
        ASSERT_EQ_INT(mc_set_qcomplex(b, qc_make(qf_from_string("1.5"), qf_from_string("0.25"))), 0);
        print_mcomplex_value("mc_set_qcomplex value", b);
        print_mcomplex_error_check("mc_set_qcomplex(1.5 + 0.25i)", b, "1.5", "0.25");
        ASSERT_TRUE(mcomplex_meets_precision(b, "1.5", "0.25"));

        qb = mc_to_qcomplex(b);
        qc_to_string(qb, qc_buf, sizeof(qc_buf));
        printf(C_CYAN "mc_to_qcomplex string" C_RESET "\n");
        printf("    expected = 1.5 + 0.25i\n");
        printf("    got      = " C_WHITE "%s" C_RESET "\n", qc_buf);
        fflush(stdout);
        ASSERT_TRUE(strstr(qc_buf, "1.5") != NULL);
        ASSERT_TRUE(strstr(qc_buf, "0.25") != NULL);
        break;
    }

    mc_free(a);
    mc_free(b);
}

static void test_lifecycle_and_constants(void)
{
    mcomplex_t *z = NULL;
    char *text = NULL;

    for (;;) {
        print_mcomplex_value("MC_ZERO", MC_ZERO);
        print_mcomplex_value("MC_ONE", MC_ONE);
        print_mcomplex_value("MC_I", MC_I);

        ASSERT_TRUE(mc_is_zero(MC_ZERO));
        ASSERT_TRUE(mc_eq(MC_ONE, MC_ONE));
        print_mcomplex_error_check("MC_I value check", MC_I, "0", "1");
        ASSERT_TRUE(mcomplex_meets_precision(MC_I, "0", "1"));

        print_mcomplex_input("input string", "3.5 - 2.25i");
        z = mc_create_string("3.5 - 2.25i");
        ASSERT_NOT_NULL(z);
        print_mcomplex_value("parsed z", z);
        print_mcomplex_error_check("mc_create_string(\"3.5 - 2.25i\")", z, "3.5", "-2.25");
        ASSERT_TRUE(mcomplex_meets_precision(z, "3.5", "-2.25"));

        text = mc_to_string(z);
        ASSERT_NOT_NULL(text);
        printf(C_CYAN "roundtrip string" C_RESET "\n");
        printf("    expected = 3.5 - 2.25i\n");
        printf("    got      = " C_WHITE "%s" C_RESET "\n", text);
        fflush(stdout);
        ASSERT_TRUE(strstr(text, "3.5") != NULL);
        ASSERT_TRUE(strstr(text, "2.25") != NULL);
        break;
    }

    free(text);
    mc_free(z);
}

static void test_arithmetic(void)
{
    mcomplex_t *a = NULL;
    mcomplex_t *b = NULL;

    for (;;) {
        print_mcomplex_input("a input", "3 + 4i");
        print_mcomplex_input("b input", "1 - 2i");
        a = mc_create_string("3 + 4i");
        b = mc_create_string("1 - 2i");
        ASSERT_NOT_NULL(a);
        ASSERT_NOT_NULL(b);
        print_mcomplex_value("a initial", a);
        print_mcomplex_value("b initial", b);

        ASSERT_EQ_INT(mc_add(a, b), 0);
        print_mcomplex_value("a after a += b", a);
        print_mcomplex_error_check("(3 + 4i) + (1 - 2i)", a, "4", "2");
        ASSERT_TRUE(mcomplex_meets_precision(a, "4", "2"));

        ASSERT_EQ_INT(mc_mul(a, b), 0);
        print_mcomplex_value("a after a *= b", a);
        print_mcomplex_error_check("(4 + 2i) * (1 - 2i)", a, "8", "-6");
        ASSERT_TRUE(mcomplex_meets_precision(a, "8", "-6"));
        break;
    }

    mc_free(a);
    mc_free(b);
}

static void test_transcendentals(void)
{
    mcomplex_t *z_exp = NULL;
    mcomplex_t *z_log = NULL;

    for (;;) {
        print_mcomplex_input("exp input", "0 + pi*i");
        z_exp = mc_create(MF_ZERO, MF_PI);
        ASSERT_NOT_NULL(z_exp);
        print_mcomplex_value("exp initial", z_exp);

        ASSERT_EQ_INT(mc_exp(z_exp), 0);
        print_mcomplex_value("after exp(z)", z_exp);
        print_mcomplex_error_check("exp(i*pi)", z_exp, "-1", "0");
        ASSERT_TRUE(mcomplex_meets_precision(z_exp, "-1", "0"));

        print_mcomplex_input("log input", "-1 + 0i");
        z_log = mc_create_string("-1 + 0i");
        ASSERT_NOT_NULL(z_log);
        print_mcomplex_value("log initial", z_log);

        ASSERT_EQ_INT(mc_log(z_log), 0);
        print_mcomplex_value("after log(z)", z_log);
        print_mcomplex_error_check(
            "log(-1 + 0i)", z_log, "0",
            "3.141592653589793238462643383279502884197169399375105820974944592307816406286208998628034825342117068");
        ASSERT_TRUE(mcomplex_meets_precision(
            z_log, "0",
            "3.141592653589793238462643383279502884197169399375105820974944592307816406286208998628034825342117068"));
        break;
    }

    mc_free(z_exp);
    mc_free(z_log);
}

static void test_special_functions(void)
{
    mcomplex_t *z = NULL;

    for (;;) {
        print_mcomplex_input("z input", "1 + 0i");
        z = mc_create_string("1 + 0i");
        ASSERT_NOT_NULL(z);
        print_mcomplex_value("z initial", z);

        ASSERT_EQ_INT(mc_gamma(z), 0);
        print_mcomplex_value("z after gamma(z)", z);
        print_mcomplex_error_check("gamma(1 + 0i)", z, "1", "0");
        ASSERT_TRUE(mcomplex_meets_precision(z, "1", "0"));

        ASSERT_EQ_INT(mc_erf(z), 0);
        print_mcomplex_value("z after erf(z)", z);
        print_mcomplex_error_check(
            "erf(1 + 0i)", z,
            "0.8427007929497148693412206350826092592960669979663029084599378978347172540960108412619833253481448885",
            "0");
        ASSERT_TRUE(mcomplex_meets_precision(
            z,
            "0.8427007929497148693412206350826092592960669979663029084599378978347172540960108412619833253481448885",
            "0"));
        break;
    }

    mc_free(z);
}

static void test_real_elementary_replacements(void)
{
    mcomplex_t *z = NULL;
    mcomplex_t *other = NULL;

    for (;;) {
        z = create_real_mcomplex("0.5");
        ASSERT_NOT_NULL(z);
        print_mcomplex_input("sin input", "0.5 + 0i");
        print_mcomplex_value("sin initial", z);
        ASSERT_EQ_INT(mc_sin(z), 0);
        print_mcomplex_value("after sin(z)", z);
        print_mcomplex_error_check(
            "sin(0.5 + 0i)", z,
            "0.4794255386042030002732879352155713880818033679406006751886166131255350002878148322096312746843482690861320910845057174178110937486099402827801539620461919246099572939322814005335463381880552285956701356998542336391210717207773801529",
            "0");
        ASSERT_TRUE(mcomplex_meets_precision(
            z,
            "0.4794255386042030002732879352155713880818033679406006751886166131255350002878148322096312746843482690861320910845057174178110937486099402827801539620461919246099572939322814005335463381880552285956701356998542336391210717207773801529",
            "0"));
        mc_free(z);
        z = NULL;

        z = create_real_mcomplex("0.5");
        ASSERT_NOT_NULL(z);
        print_mcomplex_input("cos input", "0.5 + 0i");
        print_mcomplex_value("cos initial", z);
        ASSERT_EQ_INT(mc_cos(z), 0);
        print_mcomplex_value("after cos(z)", z);
        print_mcomplex_error_check(
            "cos(0.5 + 0i)", z,
            "0.8775825618903727161162815826038296519916451971097440529976108683159507632742139474057941840846822583554784005931090539934138279768332802667997561209502240155876291568785907234769393109896167396770144089976491285702134682183845438182",
            "0");
        ASSERT_TRUE(mcomplex_meets_precision(
            z,
            "0.8775825618903727161162815826038296519916451971097440529976108683159507632742139474057941840846822583554784005931090539934138279768332802667997561209502240155876291568785907234769393109896167396770144089976491285702134682183845438182",
            "0"));
        mc_free(z);
        z = NULL;

        z = create_real_mcomplex("0.5");
        ASSERT_NOT_NULL(z);
        print_mcomplex_input("tan input", "0.5 + 0i");
        print_mcomplex_value("tan initial", z);
        ASSERT_EQ_INT(mc_tan(z), 0);
        print_mcomplex_value("after tan(z)", z);
        print_mcomplex_error_check(
            "tan(0.5 + 0i)", z,
            "0.5463024898437905132551794657802853832975517201797912461640913859329075105180258157151806482706562185891048626002641142654932300911684028432173909299109142166369407437884742689574104012579117568787459997245089182122377508438391608137",
            "0");
        ASSERT_TRUE(mcomplex_meets_precision(
            z,
            "0.5463024898437905132551794657802853832975517201797912461640913859329075105180258157151806482706562185891048626002641142654932300911684028432173909299109142166369407437884742689574104012579117568787459997245089182122377508438391608137",
            "0"));
        mc_free(z);
        z = NULL;

        z = create_real_mcomplex("0.5");
        ASSERT_NOT_NULL(z);
        print_mcomplex_input("atan input", "0.5 + 0i");
        print_mcomplex_value("atan initial", z);
        ASSERT_EQ_INT(mc_atan(z), 0);
        print_mcomplex_value("after atan(z)", z);
        print_mcomplex_error_check(
            "atan(0.5 + 0i)", z,
            "0.4636476090008061162142562314612144020285370542861202638109330887201978641657417053006002839848878925565298522511908375135058181816250111554715305699441056207193362661648801015325027559879258055168538891674782372865387939180125171996",
            "0");
        ASSERT_TRUE(mcomplex_meets_precision(
            z,
            "0.4636476090008061162142562314612144020285370542861202638109330887201978641657417053006002839848878925565298522511908375135058181816250111554715305699441056207193362661648801015325027559879258055168538891674782372865387939180125171996",
            "0"));
        mc_free(z);
        z = NULL;

        z = create_real_mcomplex("1");
        other = create_real_mcomplex("-1");
        ASSERT_NOT_NULL(z);
        ASSERT_NOT_NULL(other);
        print_mcomplex_input("atan2 input y", "1 + 0i");
        print_mcomplex_input("atan2 input x", "-1 + 0i");
        print_mcomplex_value("atan2 initial y", z);
        print_mcomplex_value("atan2 initial x", other);
        ASSERT_EQ_INT(mc_atan2(z, other), 0);
        print_mcomplex_value("after atan2(y,x)", z);
        print_mcomplex_error_check(
            "atan2(1 + 0i, -1 + 0i)", z,
            "2.356194490192344928846982537459627163147877049531329365731208444230862304714656748971026119006587800986611064884961729985320383457162936673794019556096360838087713077026453890829169733467211716197786473321608231749450084596356736175",
            "0");
        ASSERT_TRUE(mcomplex_meets_precision(
            z,
            "2.356194490192344928846982537459627163147877049531329365731208444230862304714656748971026119006587800986611064884961729985320383457162936673794019556096360838087713077026453890829169733467211716197786473321608231749450084596356736175",
            "0"));
        mc_free(z);
        mc_free(other);
        z = NULL;
        other = NULL;

        z = create_real_mcomplex("0.5");
        ASSERT_NOT_NULL(z);
        print_mcomplex_input("asin input", "0.5 + 0i");
        print_mcomplex_value("asin initial", z);
        ASSERT_EQ_INT(mc_asin(z), 0);
        print_mcomplex_value("after asin(z)", z);
        print_mcomplex_error_check(
            "asin(0.5 + 0i)", z,
            "0.5235987755982988730771072305465838140328615665625176368291574320513027343810348331046724708903528446636913477522137177745156407682584303719542265680214135195750473504503230868509266074371581591550636607381351626109889076880792747053",
            "0");
        ASSERT_TRUE(mcomplex_meets_precision(
            z,
            "0.5235987755982988730771072305465838140328615665625176368291574320513027343810348331046724708903528446636913477522137177745156407682584303719542265680214135195750473504503230868509266074371581591550636607381351626109889076880792747053",
            "0"));
        mc_free(z);
        z = NULL;

        z = create_real_mcomplex("0.5");
        ASSERT_NOT_NULL(z);
        print_mcomplex_input("acos input", "0.5 + 0i");
        print_mcomplex_value("acos initial", z);
        ASSERT_EQ_INT(mc_acos(z), 0);
        print_mcomplex_value("after acos(z)", z);
        print_mcomplex_error_check(
            "acos(0.5 + 0i)", z,
            "1.047197551196597746154214461093167628065723133125035273658314864102605468762069666209344941780705689327382695504427435549031281536516860743908453136042827039150094700900646173701853214874316318310127321476270325221977815376158549411",
            "0");
        ASSERT_TRUE(mcomplex_meets_precision(
            z,
            "1.047197551196597746154214461093167628065723133125035273658314864102605468762069666209344941780705689327382695504427435549031281536516860743908453136042827039150094700900646173701853214874316318310127321476270325221977815376158549411",
            "0"));
        mc_free(z);
        z = NULL;

        z = create_real_mcomplex("0.5");
        ASSERT_NOT_NULL(z);
        print_mcomplex_input("sinh input", "0.5 + 0i");
        print_mcomplex_value("sinh initial", z);
        ASSERT_EQ_INT(mc_sinh(z), 0);
        print_mcomplex_value("after sinh(z)", z);
        print_mcomplex_error_check(
            "sinh(0.5 + 0i)", z,
            "0.5210953054937473616224256264114915591059289826114805279460935764528022508902335923170644542741885934882214239811341359140666794448283313132498958147711911861109207062907779867237162829057943448262401667428326636169984336690720577785",
            "0");
        ASSERT_TRUE(mcomplex_meets_precision(
            z,
            "0.5210953054937473616224256264114915591059289826114805279460935764528022508902335923170644542741885934882214239811341359140666794448283313132498958147711911861109207062907779867237162829057943448262401667428326636169984336690720577785",
            "0"));
        mc_free(z);
        z = NULL;

        z = create_real_mcomplex("0.5");
        ASSERT_NOT_NULL(z);
        print_mcomplex_input("cosh input", "0.5 + 0i");
        print_mcomplex_value("cosh initial", z);
        ASSERT_EQ_INT(mc_cosh(z), 0);
        print_mcomplex_value("after cosh(z)", z);
        print_mcomplex_error_check(
            "cosh(0.5 + 0i)", z,
            "1.127625965206380785226225161402672012547847118098667483628985735187858770303982016315712065782178049514645213775173661090604487530391277846591075637718868610818501952807625927996232181753694900070628738593585802103842632987787742311",
            "0");
        ASSERT_TRUE(mcomplex_meets_precision(
            z,
            "1.127625965206380785226225161402672012547847118098667483628985735187858770303982016315712065782178049514645213775173661090604487530391277846591075637718868610818501952807625927996232181753694900070628738593585802103842632987787742311",
            "0"));
        mc_free(z);
        z = NULL;

        z = create_real_mcomplex("0.5");
        ASSERT_NOT_NULL(z);
        print_mcomplex_input("tanh input", "0.5 + 0i");
        print_mcomplex_value("tanh initial", z);
        ASSERT_EQ_INT(mc_tanh(z), 0);
        print_mcomplex_value("after tanh(z)", z);
        print_mcomplex_error_check(
            "tanh(0.5 + 0i)", z,
            "0.4621171572600097585023184836436725487302892803301130385527318158380809061404092787749490641519624905843489329862815491328822654618695978959571446116158785633291327041667769391973725679307702700373014486085992624095817836118928991466",
            "0");
        ASSERT_TRUE(mcomplex_meets_precision(
            z,
            "0.4621171572600097585023184836436725487302892803301130385527318158380809061404092787749490641519624905843489329862815491328822654618695978959571446116158785633291327041667769391973725679307702700373014486085992624095817836118928991466",
            "0"));
        mc_free(z);
        z = NULL;

        z = create_real_mcomplex("0.5");
        ASSERT_NOT_NULL(z);
        print_mcomplex_input("asinh input", "0.5 + 0i");
        print_mcomplex_value("asinh initial", z);
        ASSERT_EQ_INT(mc_asinh(z), 0);
        print_mcomplex_value("after asinh(z)", z);
        print_mcomplex_error_check(
            "asinh(0.5 + 0i)", z,
            "0.4812118250596034474977589134243684231351843343856605196610181688401638676082217744120094291227234749972318399582936564112725683237267376227530592418644097541824170072118371502238239374691872752432791930187970790035617267969445457523",
            "0");
        ASSERT_TRUE(mcomplex_meets_precision(
            z,
            "0.4812118250596034474977589134243684231351843343856605196610181688401638676082217744120094291227234749972318399582936564112725683237267376227530592418644097541824170072118371502238239374691872752432791930187970790035617267969445457523",
            "0"));
        mc_free(z);
        z = NULL;

        z = create_real_mcomplex("2");
        ASSERT_NOT_NULL(z);
        print_mcomplex_input("acosh input", "2 + 0i");
        print_mcomplex_value("acosh initial", z);
        ASSERT_EQ_INT(mc_acosh(z), 0);
        print_mcomplex_value("after acosh(z)", z);
        print_mcomplex_error_check(
            "acosh(2 + 0i)", z,
            "1.316957896924816708625046347307968444026981971467516479768472256920460185416443976074219013450101783556465436565604979319809816862106371532726763345709920676905831128776256958170470437336863711940955650446796732000825937475377912891",
            "0");
        ASSERT_TRUE(mcomplex_meets_precision(
            z,
            "1.316957896924816708625046347307968444026981971467516479768472256920460185416443976074219013450101783556465436565604979319809816862106371532726763345709920676905831128776256958170470437336863711940955650446796732000825937475377912891",
            "0"));
        mc_free(z);
        z = NULL;

        z = create_real_mcomplex("0.5");
        ASSERT_NOT_NULL(z);
        print_mcomplex_input("atanh input", "0.5 + 0i");
        print_mcomplex_value("atanh initial", z);
        ASSERT_EQ_INT(mc_atanh(z), 0);
        print_mcomplex_value("after atanh(z)", z);
        print_mcomplex_error_check(
            "atanh(0.5 + 0i)", z,
            "0.5493061443340548456976226184612628523237452789113747258673471668187471466093044834368078774068660443939850145329789328711840021129652599105264009353836387053015813845916906835896868494221804799518712851583979557605727959588753356733",
            "0");
        ASSERT_TRUE(mcomplex_meets_precision(
            z,
            "0.5493061443340548456976226184612628523237452789113747258673471668187471466093044834368078774068660443939850145329789328711840021129652599105264009353836387053015813845916906835896868494221804799518712851583979557605727959588753356733",
            "0"));
        break;
    }

    mc_free(z);
    mc_free(other);
}

static void test_real_special_replacements(void)
{
    mcomplex_t *z = NULL;
    mcomplex_t *other = NULL;
    size_t expected_precision = 0u;

    for (;;) {
        z = create_real_mcomplex("0.5");
        ASSERT_NOT_NULL(z);
        print_mcomplex_input("erfinv input", "0.5 + 0i");
        print_mcomplex_value("erfinv initial", z);
        ASSERT_EQ_INT(mc_erfinv(z), 0);
        print_mcomplex_value("after erfinv(z)", z);
        print_mcomplex_error_check(
            "erfinv(0.5 + 0i)", z,
            "0.4769362762044698733814183536431305598089697490594706447038826959193834477746467334886959158699890099480330386734708686181554200754487317906163612464948982829944987416840339015704779831127126",
            "0");
        ASSERT_TRUE(mcomplex_meets_precision(
            z,
            "0.4769362762044698733814183536431305598089697490594706447038826959193834477746467334886959158699890099480330386734708686181554200754487317906163612464948982829944987416840339015704779831127126",
            "0"));
        mc_free(z);
        z = NULL;

        z = create_real_mcomplex("0.5");
        ASSERT_NOT_NULL(z);
        print_mcomplex_input("erfcinv input", "0.5 + 0i");
        print_mcomplex_value("erfcinv initial", z);
        ASSERT_EQ_INT(mc_erfcinv(z), 0);
        print_mcomplex_value("after erfcinv(z)", z);
        print_mcomplex_error_check(
            "erfcinv(0.5 + 0i)", z,
            "0.4769362762044698733814183536431305598089697490594706447038826959193834477746467334886959158699890099480330386734708686181554200754487317906163612464948982829944987416840339015704779831127126",
            "0");
        ASSERT_TRUE(mcomplex_meets_precision(
            z,
            "0.4769362762044698733814183536431305598089697490594706447038826959193834477746467334886959158699890099480330386734708686181554200754487317906163612464948982829944987416840339015704779831127126",
            "0"));
        mc_free(z);
        z = NULL;

        z = create_real_mcomplex("5");
        ASSERT_NOT_NULL(z);
        print_mcomplex_input("lgamma input", "5 + 0i");
        print_mcomplex_value("lgamma initial", z);
        ASSERT_EQ_INT(mc_lgamma(z), 0);
        print_mcomplex_value("after lgamma(z)", z);
        print_mcomplex_error_check(
            "lgamma(5 + 0i)", z,
            "3.178053830347945619646941601297055408873990960903515214096734362117675159127693113691205735802988151413974472127669922943424564933204911492150814125672505296395345481668495672750294814716035980317112620662889405386862953343268716072",
            "0");
        ASSERT_TRUE(mcomplex_meets_precision(
            z,
            "3.178053830347945619646941601297055408873990960903515214096734362117675159127693113691205735802988151413974472127669922943424564933204911492150814125672505296395345481668495672750294814716035980317112620662889405386862953343268716072",
            "0"));
        mc_free(z);
        z = NULL;

        z = create_real_mcomplex("1");
        ASSERT_NOT_NULL(z);
        print_mcomplex_input("digamma input", "1 + 0i");
        print_mcomplex_value("digamma initial", z);
        ASSERT_EQ_INT(mc_digamma(z), 0);
        print_mcomplex_value("after digamma(z)", z);
        print_mcomplex_error_check(
            "digamma(1 + 0i)", z,
            "-0.5772156649015328606065120900824024310421593359399235988057672348848677267776646709369470632917467495146314472498070824809605040144865428362241739976449235362535003337429373377376739427925952582470949160087352039481656708532331517767",
            "0");
        ASSERT_TRUE(mcomplex_meets_precision(
            z,
            "-0.5772156649015328606065120900824024310421593359399235988057672348848677267776646709369470632917467495146314472498070824809605040144865428362241739976449235362535003337429373377376739427925952582470949160087352039481656708532331517767",
            "0"));
        mc_free(z);
        z = NULL;

        z = create_real_mcomplex("1");
        ASSERT_NOT_NULL(z);
        print_mcomplex_input("trigamma input", "1 + 0i");
        print_mcomplex_value("trigamma initial", z);
        ASSERT_EQ_INT(mc_trigamma(z), 0);
        print_mcomplex_value("after trigamma(z)", z);
        print_mcomplex_error_check(
            "trigamma(1 + 0i)", z,
            "1.644934066848226436472415166646025189218949901206798437735558229370007470403200873833628900619758705304004318962337190679628724687005007787935102946330866276831733309367762605095251006872140054796811558794890360823277761919840756456",
            "0");
        ASSERT_TRUE(mcomplex_meets_precision(
            z,
            "1.644934066848226436472415166646025189218949901206798437735558229370007470403200873833628900619758705304004318962337190679628724687005007787935102946330866276831733309367762605095251006872140054796811558794890360823277761919840756456",
            "0"));
        mc_free(z);
        z = NULL;

        z = create_real_mcomplex("1");
        ASSERT_NOT_NULL(z);
        print_mcomplex_input("tetragamma input", "1 + 0i");
        print_mcomplex_value("tetragamma initial", z);
        ASSERT_EQ_INT(mc_tetragamma(z), 0);
        print_mcomplex_value("after tetragamma(z)", z);
        print_mcomplex_error_check(
            "tetragamma(1 + 0i)", z,
            "-2.4041138063191885707994763230228999815299725846809977635845431106836764115726261803729117472186705162923983155905214388369839919973465664275527936744158003229078835658987",
            "0");
        ASSERT_TRUE(mcomplex_meets_precision(
            z,
            "-2.4041138063191885707994763230228999815299725846809977635845431106836764115726261803729117472186705162923983155905214388369839919973465664275527936744158003229078835658987",
            "0"));
        mc_free(z);
        z = NULL;

        z = create_real_mcomplex("3");
        ASSERT_NOT_NULL(z);
        print_mcomplex_input("gammainv input", "3 + 0i");
        print_mcomplex_value("gammainv initial", z);
        ASSERT_EQ_INT(mc_gammainv(z), 0);
        print_mcomplex_value("after gammainv(z)", z);
        print_mcomplex_error_check(
            "gammainv(3 + 0i)", z,
            "3.4058699863095669246999292183755580095953957993289813943129416262771480469249543971039352849144736866311861261393708590757492573265835991956905840323578868826789864361657",
            "0");
        ASSERT_TRUE(mcomplex_meets_precision(
            z,
            "3.4058699863095669246999292183755580095953957993289813943129416262771480469249543971039352849144736866311861261393708590757492573265835991956905840323578868826789864361657",
            "0"));
        mc_free(z);
        z = NULL;

        z = create_real_mcomplex("1");
        ASSERT_NOT_NULL(z);
        expected_precision = mc_get_precision(z);
        print_mcomplex_input("lambert_w0 input", "1 + 0i");
        print_mcomplex_value("lambert_w0 initial", z);
        ASSERT_EQ_INT(mc_lambert_w0(z), 0);
        print_mcomplex_value("after lambert_w0(z)", z);
        ASSERT_EQ_LONG((long)mc_get_precision(z), (long)expected_precision);
        print_mcomplex_error_check(
            "lambert_w0(1 + 0i)", z,
            "0.5671432904097838729999686622103555497538157871865125081351310792230457930866845666932194469617522945576380249728667897854523584659400729956085164392899946143115714929598",
            "0");
        ASSERT_TRUE(mcomplex_meets_precision(
            z,
            "0.5671432904097838729999686622103555497538157871865125081351310792230457930866845666932194469617522945576380249728667897854523584659400729956085164392899946143115714929598",
            "0"));
        mc_free(z);
        z = NULL;

        z = create_real_mcomplex("-0.1");
        ASSERT_NOT_NULL(z);
        expected_precision = mc_get_precision(z);
        print_mcomplex_input("lambert_wm1 input", "-0.1 + 0i");
        print_mcomplex_value("lambert_wm1 initial", z);
        ASSERT_EQ_INT(mc_lambert_wm1(z), 0);
        print_mcomplex_value("after lambert_wm1(z)", z);
        ASSERT_EQ_LONG((long)mc_get_precision(z), (long)expected_precision);
        print_mcomplex_error_check(
            "lambert_wm1(-0.1 + 0i)", z,
            "-3.577152063957297218409391963511994880401796257793075923683527755791687236350575462861463655620846808017732465627597059470558844569051750534584923541827063499452631656593265232240273452302009544089866198954722805115875488714857591771",
            "0");
        ASSERT_TRUE(mcomplex_meets_precision(
            z,
            "-3.577152063957297218409391963511994880401796257793075923683527755791687236350575462861463655620846808017732465627597059470558844569051750534584923541827063499452631656593265232240273452302009544089866198954722805115875488714857591771",
            "0"));
        mc_free(z);
        z = NULL;

        z = create_real_mcomplex("0");
        ASSERT_NOT_NULL(z);
        print_mcomplex_input("normal_cdf input", "0 + 0i");
        print_mcomplex_value("normal_cdf initial", z);
        ASSERT_EQ_INT(mc_normal_cdf(z), 0);
        print_mcomplex_value("after normal_cdf(z)", z);
        print_mcomplex_error_check("normal_cdf(0 + 0i)", z, "0.5", "0");
        ASSERT_TRUE(mcomplex_meets_precision(z, "0.5", "0"));
        mc_free(z);
        z = NULL;

        z = create_real_mcomplex("0.5");
        ASSERT_NOT_NULL(z);
        print_mcomplex_input("normal_pdf input", "0.5 + 0i");
        print_mcomplex_value("normal_pdf initial", z);
        ASSERT_EQ_INT(mc_normal_pdf(z), 0);
        print_mcomplex_value("after normal_pdf(z)", z);
        print_mcomplex_error_check(
            "normal_pdf(0.5 + 0i)", z,
            "0.3520653267642994777746804415965176531103151803757119496554690179882231978367166074876695830352912992493563315873991226985732202993620550355837505163524984117606205891629138560900857072618466033122881469544464486893328511817392660585",
            "0");
        ASSERT_TRUE(mcomplex_meets_precision(
            z,
            "0.3520653267642994777746804415965176531103151803757119496554690179882231978367166074876695830352912992493563315873991226985732202993620550355837505163524984117606205891629138560900857072618466033122881469544464486893328511817392660585",
            "0"));
        mc_free(z);
        z = NULL;

        z = create_real_mcomplex("0.5");
        ASSERT_NOT_NULL(z);
        print_mcomplex_input("normal_logpdf input", "0.5 + 0i");
        print_mcomplex_value("normal_logpdf initial", z);
        ASSERT_EQ_INT(mc_normal_logpdf(z), 0);
        print_mcomplex_value("after normal_logpdf(z)", z);
        print_mcomplex_error_check(
            "normal_logpdf(0.5 + 0i)", z,
            "-1.043938533204672741780329736405617639861397473637783412817151540482765695927260397694743298635954197622005646624634337446366862881840793572155875915222681393603560742547358669046395905991380805630163234873094627374625518251694954477",
            "0");
        ASSERT_TRUE(mcomplex_meets_precision(
            z,
            "-1.043938533204672741780329736405617639861397473637783412817151540482765695927260397694743298635954197622005646624634337446366862881840793572155875915222681393603560742547358669046395905991380805630163234873094627374625518251694954477",
            "0"));
        mc_free(z);
        z = NULL;

        z = create_real_mcomplex("1");
        ASSERT_NOT_NULL(z);
        expected_precision = mc_get_precision(z);
        print_mcomplex_input("productlog input", "1 + 0i");
        print_mcomplex_value("productlog initial", z);
        ASSERT_EQ_INT(mc_productlog(z), 0);
        print_mcomplex_value("after productlog(z)", z);
        ASSERT_EQ_LONG((long)mc_get_precision(z), (long)expected_precision);
        print_mcomplex_error_check(
            "productlog(1 + 0i)", z,
            "0.5671432904097838729999686622103555497538157871865125081351310792230457930866845666932194469617522945576380249728667897854523584659400729956085164392899946143115714929598",
            "0");
        ASSERT_TRUE(mcomplex_meets_precision(
            z,
            "0.5671432904097838729999686622103555497538157871865125081351310792230457930866845666932194469617522945576380249728667897854523584659400729956085164392899946143115714929598",
            "0"));
        mc_free(z);
        z = NULL;

        z = create_real_mcomplex("1");
        ASSERT_NOT_NULL(z);
        print_mcomplex_input("ei input", "1 + 0i");
        print_mcomplex_value("ei initial", z);
        ASSERT_EQ_INT(mc_ei(z), 0);
        print_mcomplex_value("after ei(z)", z);
        print_mcomplex_error_check(
            "ei(1 + 0i)", z,
            "1.895117816355936755466520934331634269017060581732707591646228431882513834533804153548900710126138956971811095317944653742588149164163064688088186682538828669632338545095227555258481392212166459936359948543306285455761625228166868119",
            "0");
        ASSERT_TRUE(mcomplex_meets_precision(
            z,
            "1.895117816355936755466520934331634269017060581732707591646228431882513834533804153548900710126138956971811095317944653742588149164163064688088186682538828669632338545095227555258481392212166459936359948543306285455761625228166868119",
            "0"));
        mc_free(z);
        z = NULL;

        z = create_real_mcomplex("1");
        ASSERT_NOT_NULL(z);
        print_mcomplex_input("e1 input", "1 + 0i");
        print_mcomplex_value("e1 initial", z);
        ASSERT_EQ_INT(mc_e1(z), 0);
        print_mcomplex_value("after e1(z)", z);
        print_mcomplex_error_check(
            "e1(1 + 0i)", z,
            "0.2193839343955202736771637754601216490310472934069082075779786130735686985591415447222102510351372499547582346308741095901763785205370960099567044878767774129313472607957338658928051397881295371811343600593450128247655854623683249695",
            "0");
        ASSERT_TRUE(mcomplex_meets_precision(
            z,
            "0.2193839343955202736771637754601216490310472934069082075779786130735686985591415447222102510351372499547582346308741095901763785205370960099567044878767774129313472607957338658928051397881295371811343600593450128247655854623683249695",
            "0"));
        mc_free(z);
        z = NULL;

        z = create_real_mcomplex("2.5");
        other = create_real_mcomplex("3.5");
        ASSERT_NOT_NULL(z);
        ASSERT_NOT_NULL(other);
        print_mcomplex_input("beta input a", "2.5 + 0i");
        print_mcomplex_input("beta input b", "3.5 + 0i");
        print_mcomplex_value("beta initial a", z);
        print_mcomplex_value("beta initial b", other);
        ASSERT_EQ_INT(mc_beta(z, other), 0);
        print_mcomplex_value("after beta(a,b)", z);
        print_mcomplex_error_check(
            "beta(2.5 + 0i, 3.5 + 0i)", z,
            "0.036815538909255389513234102147806674424185578898927021339550131941107223511166511702672283109477934390415797888827527031020630991518170885528031555564005638095120516828538342044205777085425183065590413645650128621085157571818074002739688865",
            "0");
        ASSERT_TRUE(mcomplex_meets_precision(
            z,
            "0.036815538909255389513234102147806674424185578898927021339550131941107223511166511702672283109477934390415797888827527031020630991518170885528031555564005638095120516828538342044205777085425183065590413645650128621085157571818074002739688865",
            "0"));
        mc_free(z);
        mc_free(other);
        z = NULL;
        other = NULL;

        z = create_real_mcomplex("2.5");
        other = create_real_mcomplex("3.5");
        ASSERT_NOT_NULL(z);
        ASSERT_NOT_NULL(other);
        print_mcomplex_input("logbeta input a", "2.5 + 0i");
        print_mcomplex_input("logbeta input b", "3.5 + 0i");
        print_mcomplex_value("logbeta initial a", z);
        print_mcomplex_value("logbeta initial b", other);
        ASSERT_EQ_INT(mc_logbeta(z, other), 0);
        print_mcomplex_value("after logbeta(a,b)", z);
        print_mcomplex_error_check(
            "logbeta(2.5 + 0i, 3.5 + 0i)", z,
            "-3.301835269962052609799184383389828128309215704143981009717122670837516912654122678189667590882127703846032006869909630968067952132211068047929483063503043459566263883177244211440597836787902490076041110975295315326755455855413553872",
            "0");
        ASSERT_TRUE(mcomplex_meets_precision(
            z,
            "-3.301835269962052609799184383389828128309215704143981009717122670837516912654122678189667590882127703846032006869909630968067952132211068047929483063503043459566263883177244211440597836787902490076041110975295315326755455855413553872",
            "0"));
        mc_free(z);
        mc_free(other);
        z = NULL;
        other = NULL;

        z = create_real_mcomplex("5.5");
        other = create_real_mcomplex("2.5");
        ASSERT_NOT_NULL(z);
        ASSERT_NOT_NULL(other);
        print_mcomplex_input("binomial input a", "5.5 + 0i");
        print_mcomplex_input("binomial input b", "2.5 + 0i");
        print_mcomplex_value("binomial initial a", z);
        print_mcomplex_value("binomial initial b", other);
        ASSERT_EQ_INT(mc_binomial(z, other), 0);
        print_mcomplex_value("after binomial(a,b)", z);
        print_mcomplex_error_check("binomial(5.5 + 0i, 2.5 + 0i)", z, "14.4375", "0");
        ASSERT_TRUE(mcomplex_meets_precision(z, "14.4375", "0"));
        mc_free(z);
        mc_free(other);
        z = NULL;
        other = NULL;

        z = create_real_mcomplex("0.5");
        other = create_real_mcomplex("1");
        ASSERT_NOT_NULL(z);
        ASSERT_NOT_NULL(other);
        print_mcomplex_input("gammainc_P input s", "0.5 + 0i");
        print_mcomplex_input("gammainc_P input x", "1 + 0i");
        print_mcomplex_value("gammainc_P initial s", z);
        print_mcomplex_value("gammainc_P initial x", other);
        ASSERT_EQ_INT(mc_gammainc_P(z, other), 0);
        print_mcomplex_value("after gammainc_P(s,x)", z);
        print_mcomplex_error_check(
            "gammainc_P(0.5 + 0i, 1 + 0i)", z,
            "0.8427007929497148693412206350826092592960669979663029084599378978347172540960108412619833253481448885",
            "0");
        ASSERT_TRUE(mcomplex_meets_precision(
            z,
            "0.8427007929497148693412206350826092592960669979663029084599378978347172540960108412619833253481448885",
            "0"));
        mc_free(z);
        mc_free(other);
        z = NULL;
        other = NULL;

        z = create_real_mcomplex("1");
        other = create_real_mcomplex("1");
        ASSERT_NOT_NULL(z);
        ASSERT_NOT_NULL(other);
        print_mcomplex_input("gammainc_Q input s", "1 + 0i");
        print_mcomplex_input("gammainc_Q input x", "1 + 0i");
        print_mcomplex_value("gammainc_Q initial s", z);
        print_mcomplex_value("gammainc_Q initial x", other);
        ASSERT_EQ_INT(mc_gammainc_Q(z, other), 0);
        print_mcomplex_value("after gammainc_Q(s,x)", z);
        print_mcomplex_error_check(
            "gammainc_Q(1 + 0i, 1 + 0i)", z,
            "0.3678794411714423215955237701614608674458111310317678345078368016974614957448998033571472743459196437466273252768439952082469757927901290086266535894940987830921943673773381150486389911251456163449877199786844759579397473025498924955",
            "0");
        ASSERT_TRUE(mcomplex_meets_precision(
            z,
            "0.3678794411714423215955237701614608674458111310317678345078368016974614957448998033571472743459196437466273252768439952082469757927901290086266535894940987830921943673773381150486389911251456163449877199786844759579397473025498924955",
            "0"));
        break;
    }

    mc_free(z);
    mc_free(other);
}

int tests_main(void)
{
    RUN_TEST(test_lifecycle_and_constants, NULL);
    RUN_TEST(test_conversion_to_from_qcomplex, NULL);
    RUN_TEST(test_arithmetic, NULL);
    RUN_TEST(test_transcendentals, NULL);
    RUN_TEST(test_real_elementary_replacements, NULL);
    RUN_TEST(test_real_special_replacements, NULL);
    RUN_TEST(test_special_functions, NULL);
    return 0;
}
