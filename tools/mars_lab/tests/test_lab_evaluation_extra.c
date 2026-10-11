/**
 * @file test_lab_evaluation_extra.c
 * @brief Additional native Lab protocol, endpoint and binding regressions.
 *
 * Runs prebuilt mathematical workers through the native adapter, with separate
 * harness registrations for every migrated endpoint case. Each registration
 * honours tests/test_config.json and runs sequentially in the lab_native suite.
 * TeX success coverage requires latex/dvisvgm; rejection cases need no renderer.
 * No standalone main, conditional test build or Python runtime is used.
 */
#include <string.h>

#include "lab_presentation.h"
#define MARS_SHARED_LAB_EVALUATE_INTERNAL_ACCESS
#include "internal/lab_evaluate_internal.h"
#include "test_harness.h"
#include "test_lab_support.h"

static json_t *request(const char *route, const char *payload, unsigned *status)
{
    string_t *path = string_new_with(route);
    string_t *text = string_new_with(payload);
    json_t *input = json_from_text(text);
    json_t *result = lab_eval_request(path, input, status);
    json_free(input);
    string_free(text);
    string_free(path);
    return result;
}

static bool contains(const json_t *fields, const char *key, const char *expected)
{
    const string_t *value = json_string_value(lab_eval_get(fields, key));
    return value && string_find(value, expected) >= 0;
}

static void show_failure(const json_t *result)
{
    string_t *output = result ? json_to_string(result) : string_new_with("null");
    string_printf("%s\n", output ? string_c_str(output) : "allocation failure");
    string_free(output);
}

static void test_lab_extra_native_records(void)
{
    string_t *raw = string_new_with("input       {x | x=?}\nfunction    number f(x) {\n\nreturn x.\n}\n"
                                    "binding     variable\tx\tNAN\nbound_var   x\nbound_lower \nbound_upper 1\n"
                                    "bound_var   y\nbound_lower 0\nbound_upper 2\nd values    1\n");
    json_t *fields = lab_eval_fields(raw);
    bool ok = !strcmp(lab_eval_text(fields, "function"), "number f(x) {\n\nreturn x.\n}") &&
              !strcmp(lab_eval_text(fields, "bound_lower"), "\n0") &&
              !strcmp(lab_eval_text(fields, "derivative_values"), "1");
    json_free(fields);
    string_free(raw);
    TEST_ASSERT_TRUE(ok, "native multiline records preserve empty lines and bound slots");
    raw = string_new_with("{ [label = NAN,] + x | x = NAN; c = NAN }\nNAN\n");
    string_t *display = lab_eval_display_bindings(raw);
    ok = display && !strcmp(string_c_str(display), "{ [label = NAN,] + x | x = ?; c = ? }\nNAN\n");
    string_free(display);
    string_free(raw);
    TEST_ASSERT_TRUE(ok, "only unset envelope values change notation; quoted names and literal NAN remain intact");
}

static void test_lab_extra_editor_and_cards(void)
{
    const char *source = "{ x+@mu+@sigma+exp(-inf) | x=0; @mu=100, @sigma=15 }";
    json_t *payload = json_new_object();
    lab_eval_set(payload, "expression", source);
    string_t *route = string_new_with("/eval");
    unsigned status = 0u;
    json_t *fields = lab_eval_request(route, payload, &status);
    bool ok = status == 200u && !strcmp(lab_eval_text(fields, "editor_expression"), source) &&
              !strcmp(lab_eval_text(fields, "display_expression"), lab_eval_text(fields, "expression")) &&
              !strcmp(lab_eval_text(fields, "display_function"), lab_eval_text(fields, "function")) &&
              contains(fields, "display_expression", "x") && contains(fields, "display_TeX", "x") &&
              contains(fields, "display_function", "x") &&
              json_array_size(lab_eval_get(fields, "binding_values")) == 3u;
    if (!ok)
        show_failure(fields);
    json_free(fields);
    json_free(payload);
    string_free(route);
    TEST_ASSERT_TRUE(ok, "editor source preserves Greek aliases and -inf while native cards retain variables");
}

static void expect_response(const char *route, const char *payload, unsigned expected_status, const char *field,
                            const char *expected)
{
    unsigned status = 0u;
    json_t *response = request(route, payload, &status);
    bool ok = response && status == expected_status && contains(response, field, expected) &&
              test_lab_ok(response, expected_status == 200u);
    if (!ok) {
        string_printf("route %s: expected HTTP %u, received %u\n", route, expected_status, status);
        show_failure(response);
    }
    json_free(response);
    TEST_ASSERT_TRUE(ok, "native endpoint returns the expected status and field");
}

static void test_lab_extra_expression(void)
{
    expect_response("/eval", "{\"expression\":\"{ x^2 | x=3 }\",\"precision\":40}", 200u, "value", "9");
    expect_response("/eval", "{\"expression\":\"{ x^(1/3) | x=8 }\",\"precision\":40}", 200u, "value", "[2,");
}

static void test_lab_extra_help_examples(void)
{
    /* Help-card examples in assets/index.html: execute the public adapter paths, not browser arithmetic. */
    expect_response("/eval", "{\"expression\":\"sin(x)\"}", 200u, "evaluation_ready", "yes");
    expect_response("/eval", "{\"expression\":\"{ x+a | x=2; a=3 }\"}", 200u, "value", "5");
    expect_response("/eval", "{\"expression\":\"sum(k,1,4,k)\"}", 200u, "value", "10");
    expect_response("/eval", "{\"expression\":\"Laplace(exp(-t),t,s)\"}", 200u, "expression", "Re(s)");
    expect_response("/matrix-eval", "{\"matrix\":\"trace(1 2; 3 4)\",\"operation\":\"eval\"}",
                    200u, "value", "5");
    expect_response("/matrix-eval", "{\"matrix\":\"det(1 2; 3 4)\",\"operation\":\"eval\"}",
                    200u, "value", "-2");
    expect_response("/equation-eval", "{\"equation\":\"abs(x-3) = 0\"}", 200u, "solutions", "x = 3");
    expect_response("/equation-eval", "{\"equation\":\"abs(x-3) = 2\"}", 200u, "solutions", "x = 1");
    expect_response("/equation-eval", "{\"equation\":\"{ abs(x-3) = 2 | x=6 }\"}", 200u, "solutions", "x = 5");
    expect_response("/equation-eval", "{\"equation\":\"x^2 = 4\"}", 200u, "solutions", "x = -2");
    expect_response("/equation-eval", "{\"equation\":\"(x+1)^4 = 1\"}", 200u, "display_solutions", "± i");
}

static void test_lab_extra_constant_identity(void)
{
    expect_response("/eval", "{\"expression\":\"{ sin(x)^2+cos(x)^2 | x=? }\"}", 200u, "value", "1");
    expect_response("/eval", "{\"expression\":\"{ sin(x)^2+cos(x)^2-1 | x=? }\"}", 200u, "value", "0");
    unsigned status = 0u;
    json_t *response = request("/eval", "{\"expression\":\"{x+1|x=?}\"}", &status);
    bool ok = status == 200u && test_lab_ok(response, true) && !*test_lab_text(response, "value");
    json_free(response);
    TEST_ASSERT_TRUE(ok, "simplified-value fallback does not invent a value for a remaining unset variable");
}

static void test_lab_extra_bindings(void)
{
    expect_response("/eval", "{\"expression\":\"{x+c | x=?; c=2}\",\"action\":\"bindings\"}", 200u, "bindings",
                    "constant");
}

static void test_lab_extra_binding_edit(void)
{
    expect_response("/eval",
                    "{\"expression\":\"{x^2|x=2}\",\"action\":\"binding-edit\",\"wrt\":\"x\",\"binding_val"
                    "ue\":\"3\"}",
                    200u, "editor_expression", "3");
}

static void test_lab_extra_derivative(void)
{
    expect_response("/eval", "{\"expression\":\"{x^2|x=3}\",\"action\":\"derivative\",\"wrt\":\"x\"}", 200u,
                    "derivative", "x");
}

static void test_lab_extra_integral(void)
{
    expect_response("/eval", "{\"expression\":\"{x|x=?}\",\"action\":\"integral\",\"wrt\":\"x\"}", 200u, "integral",
                    "x");
}

static void test_lab_extra_equation(void)
{
    expect_response("/equation-eval", "{\"equation\":\"x^2=4\",\"precision\":40}", 200u, "solutions", "2");
}

static void test_lab_extra_equation_real_surd_numerics(void)
{
    unsigned status = 0u;
    json_t *response = request("/equation-eval",
                               "{\"equation\":\"x^4-x^3-26x^2+4x+8=0\",\"precision\":78}", &status);
    const json_t *roots = test_lab_member(response, "numeric_solutions");
    bool ok = status == 200u && test_lab_ok(response, true) && json_array_size(roots) == 4u &&
              contains(response, "display_solutions", "(1 - 2·B ± √(211 + C - 73/B))/4") &&
              contains(response, "display_solutions", "(1 + 2·B ± √(211 + C + 73/B))/4") &&
              contains(response, "display_TeX", "8\\,\\mathfrak{Re}\\mathopen{}\\left\\{A\\right\\}") &&
              !contains(response, "display_TeX", "\\overline") && contains(response, "svg", "<svg");
    for (size_t i = 0u; i < json_array_size(roots); ++i) {
        const string_t *root = json_string_value(json_array_get(roots, i));
        ok = root && string_find(root, "i") < 0 && string_find(root, "≈") >= 0 &&
             string_find(root, "E") < 0 && string_find(root, "e") < 0 && string_find(root, "×10") < 0 && ok;
    }
    if (!ok)
        show_failure(response);
    json_free(response);
    TEST_ASSERT_TRUE(ok, "78-digit quartic approximations retain four real roots and their exact surds");

    response = request("/equation-eval", "{\"equation\":\"x=1+i/10^90\",\"precision\":78}", &status);
    roots = test_lab_member(response, "numeric_solutions");
    ok = status == 200u && test_lab_ok(response, true) && json_array_size(roots) == 1u;
    for (size_t i = 0u; i < json_array_size(roots); ++i) {
        const string_t *root = json_string_value(json_array_get(roots, i));
        ok = root && string_find(root, "×10⁻⁹⁰i") >= 0 && string_find(root, "E") < 0 && ok;
    }
    if (!ok)
        show_failure(response);
    json_free(response);
    TEST_ASSERT_TRUE(ok, "genuine imaginary components of 1e-90 are not suppressed at 78 digits");
}

static void test_lab_extra_equation_decimal_notation(void)
{
    const struct {
        const char *input;
        const char *expected;
    } cases[] = {
        {"x=sqrt(2)/10000", "0.00014142135623730950488"},
        {"x=-sqrt(2)/10000", "-0.00014142135623730950488"},
        {"x=sqrt(2)/100000", "1.4142135623730950488×10⁻⁵"},
        {"x=-sqrt(2)*1e-12", "-1.4142135623730950488×10⁻¹²"},
        {"x=sqrt(2)*1e20", "141421356237309504880"},
        {"x=sqrt(2)*1e5000", "1.4142135623730950488×10⁵⁰⁰⁰"},
        {"x=0.00001+0.0000002i", "1×10⁻⁵ + 2×10⁻⁷i"},
        {"x=-0.0000002i", "-2×10⁻⁷i"},
        {"x=1+i", "1 + i"},
        {"x=0", "0"},
    };
    for (size_t i = 0u; i < sizeof(cases) / sizeof(*cases); ++i) {
        string_t *payload = string_sprintf("{\"equation\":\"%s\",\"precision\":20}", cases[i].input);
        unsigned status = 0u;
        json_t *response = payload ? request("/equation-eval", string_c_str(payload), &status) : NULL;
        const json_t *roots = test_lab_member(response, "numeric_solutions");
        const string_t *root = json_string_value(json_array_get(roots, 0u));
        string_offset_t separator = root ? string_find(root, "≈ ") : -1;
        size_t prefix = 4u;
        if (separator < 0) {
            separator = root ? string_find(root, "= ") : -1;
            prefix = 2u;
        }
        string_view_t rhs = root && separator >= 0
            ? string_view_slice(string_view_all(root), (size_t)separator + prefix, string_byte_length(root))
            : string_view_empty();
        bool ok = status == 200u && test_lab_ok(response, true) && json_array_size(roots) == 1u &&
                  string_view_equals_literal(rhs, cases[i].expected);
        if (!ok)
            string_printf("%s: expected %s; received %S\n", cases[i].input, cases[i].expected, root);
        json_free(response);
        string_free(payload);
        TEST_ASSERT_TRUE(ok, "equation numerics use decimals or Unicode powers of ten, never E notation");
    }
    static const unsigned precisions[] = {17u, 78u};
    for (size_t p = 0; p < sizeof(precisions) / sizeof(*precisions); ++p) {
        unsigned status = 0u;
        string_t *payload = string_sprintf("{\"equation\":\"x^4-x^3-26x^2+4x+1/10000=0\",\"precision\":%u}", precisions[p]);
        json_t *response = payload ? request("/equation-eval", string_c_str(payload), &status) : NULL;
        string_free(payload);
        const json_t *roots = test_lab_member(response, "numeric_solutions");
        bool ok = status == 200u && test_lab_ok(response, true) && json_array_size(roots) == 4u &&
                  contains(response, "display_solutions", "A = ³√(") &&
                  contains(response, "display_TeX", "1957044392389164807") &&
                  contains(response, "display_TeX", "\\frac{\\sqrt{-1957044392389164807} - 8913952525}{500000}") &&
                  !contains(response, "display_TeX", "62500000000") &&
                  !contains(response, "display_TeX", "31312710.278226636912") && contains(response, "svg", "<svg");
        string_printf("  Compact quartic: %S\n", json_string_value(test_lab_member(response, "display_TeX")));
        bool small_root = false;
        for (size_t i = 0u; i < json_array_size(roots); ++i) {
            const string_t *root = json_string_value(json_array_get(roots, i));
            ok = root && string_find(root, " ×10") < 0 && ok;
            small_root = small_root || (root && string_find(root, "×10⁻⁵") >= 0);
        }
        json_free(response);
        TEST_ASSERT_TRUE(ok && small_root, "quartic surds remain reduced at double and higher precision");
    }
}

static void test_lab_extra_number_card_markup(void)
{
    static const struct { const char *route; const char *payload; const char *key; } cases[] = {
        {"/eval", "{\"expression\":\"exp(-20)\"}", "value"},
        {"/matrix-eval", "{\"matrix\":\"(exp(-20),0;0,0)\",\"operation\":\"trace\"}", "value"},
        {"/integrator-eval", "{\"expression\":\"exp(-20)\","
                             "\"bounds\":[{\"name\":\"x\",\"lo\":\"0\",\"hi\":\"1\"}]}", "value"},
        {"/function-run", "{\"source\":\"output(exp(-20)).\"}", "output"},
    };
    const unsigned precisions[] = {17u, 78u};
    for (size_t p = 0; p < 2u; ++p) {
        bool ok = true;
        for (size_t i = 0; i < sizeof(cases) / sizeof(*cases); ++i) {
            json_t *payload = test_lab_json(cases[i].payload);
            string_t *digits = string_sprintf("%u", precisions[p]);
            lab_eval_put(payload, "precision", json_new_number(digits));
            string_t *path = string_new_with(cases[i].route);
            unsigned status = 0u;
            json_t *result = lab_eval_request(path, payload, &status);
            bool adapted = result && lab_presentation_adapt(result);
            const json_t *metadata = test_lab_member(result, "presentation");
            const char *raw = test_lab_text(result, cases[i].key);
            bool displayed = false;
            if (!strcmp(cases[i].key, "output")) {
                displayed = strstr(test_lab_text(metadata, "output_display"), "×10⁻⁹") != NULL;
            } else {
                const json_t *entries = test_lab_member(metadata, "matrix_headings");
                /* Inspect the bounded native wire list used by the browser's source-keyed cache. */
                for (size_t j = 0; j < json_array_size(entries); ++j) {
                    const json_t *entry = json_array_get(entries, j);
                    if (!strcmp(test_lab_text(entry, "source"), raw))
                        displayed = strstr(test_lab_text(entry, "html"), "×10⁻⁹") != NULL;
                }
            }
            bool matched = adapted && status == 200u && test_lab_ok(result, true) && displayed;
            if (!matched) show_failure(result);
            ok = ok && matched;
            json_free(result);
            json_free(payload);
            string_free(path);
            string_free(digits);
        }
        TEST_ASSERT_TRUE(ok, "native workers retain executable values and supply power-of-ten card displays at both precisions");
    }
}

static void test_lab_extra_diffequation(void)
{
    expect_response("/diffequation-eval", "{\"diffequation\":\"Dx(y)=x*y; y(0)=1\"}", 200u, "solutions", "y");
}

static void test_lab_extra_matrix(void)
{
    expect_response("/matrix-eval", "{\"matrix\":\"(1,2;3,4)\",\"operation\":\"det\"}", 200u, "result", "-2");
    expect_response("/matrix-eval", "{\"matrix\":\"(1,2;3,4)\",\"operation\":\"trace\"}", 200u, "result", "5");
    expect_response("/matrix-eval", "{\"matrix\":\"(2,0;0,3)\",\"operation\":\"eigenvalues\"}", 200u, "result", "3");
    expect_response("/matrix-eval", "{\"matrix\":\"(2,0;0,3)\",\"operation\":\"eigendecompose\"}", 200u,
                    "result", "eigenvectors");
    /* Output slots must be safe to release when a numeric operation rejects a non-square matrix. */
    expect_response("/matrix-eval", "{\"matrix\":\"(1,2,3;4,5,6)\",\"operation\":\"det\"}", 422u, "error", "det");
    expect_response("/matrix-eval", "{\"matrix\":\"(1,2,3;4,5,6)\",\"operation\":\"trace\"}", 422u, "error", "trace");
    expect_response("/matrix-eval", "{\"matrix\":\"(1,2,3;4,5,6)\",\"operation\":\"eigenvalues\"}", 422u,
                    "error", "eigenvalues");
    expect_response("/matrix-eval", "{\"matrix\":\"(1,2,3;4,5,6)\",\"operation\":\"eigendecompose\"}", 422u,
                    "error", "eigendecompose");
}

static void test_lab_extra_integrator(void)
{
    unsigned status = 0u;
    json_t *response = request("/integrator-eval",
                               "{\"expression\":\"{x|x=?}\",\"bounds\":[{\"name\":\"x\",\"lo\":\"0\",\"hi\":\"1\"}],"
                               "\"precision\":40}",
                               &status);
    const json_t *bindings = test_lab_member(response, "binding_values");
    bool ok = status == 200u && test_lab_ok(response, true) && !strcmp(test_lab_text(response, "value"), "½") &&
              !strcmp(test_lab_text(response, "expression"), "x") && contains(response, "binding_expression", "x") &&
              !contains(response, "tex", "NAN") && json_array_size(bindings) == 1u &&
              !strcmp(test_lab_text(json_array_get(bindings, 0u), "name"), "x");
    if (!ok)
        show_failure(response);
    json_free(response);
    TEST_ASSERT_TRUE(ok, "exact half integral retains its root integrand and native editor binding");
    /* Exercise both infinity parsers and the numerical output slots under worker memory checks. */
    expect_response("/integrator-eval",
                    "{\"expression\":\"exp(x)\",\"bounds\":[{\"name\":\"x\",\"lo\":\"-inf\",\"hi\":\"0\"}],"
                    "\"precision\":40}", 200u, "value", "1");
    expect_response("/integrator-eval",
                    "{\"expression\":\"exp(-x)\",\"bounds\":[{\"name\":\"x\",\"lo\":\"0\",\"hi\":\"inf\"}],"
                    "\"precision\":40}", 200u, "value", "1");
    expect_response("/integrator-eval",
                    "{\"expression\":\"exp(Li(x))\",\"bounds\":[{\"name\":\"x\",\"lo\":\"0\",\"hi\":\"1\"}],"
                    "\"precision\":17}", 200u, "value", "0.6243299885");
}

static void test_lab_extra_function(void)
{
    expect_response("/function-run", "{\"source\":\"output(2+3).\",\"precision\":40}", 200u, "output", "5");
}

static void test_lab_extra_goal_seek(void)
{
    expect_response("/goal_seek", "{\"expression\":\"{x|x=0}\",\"target\":\"2\",\"start\":{\"x\":\"0\"}}", 200u,
                    "value", "2");
}

static void test_lab_extra_render_TeX(void)
{
    expect_response("/render_TeX", "{\"tex\":\"\\\\frac{1}{2}+x^2\"}", 200u, "svg", "<svg");
    expect_response("/render_TeX", "{\"tex\":\"2\\\\mkern-2mu x+3\\\\mkern2mu y\"}", 200u, "svg", "<svg");
    expect_response("/eval", "{\"expression\":\"2*x\"}", 200u, "svg", "<svg");
    expect_response("/matrix-eval", "{\"matrix\":\"{ (1,2;3,4)^x | x = @pi }\"}", 200u, "svg", "<svg");
    expect_response("/diffequation-eval", "{\"diffequation\":\"y'+2y=0\"}", 200u, "svg", "<svg");
}

static void test_lab_extra_projection_fonts(void)
{
    static const struct { const char *route; const char *payload; const char *field; } cases[] = {
        {"/eval", "{\"expression\":\"Re(a)+Im(a)\"}", "display_TeX"},
        {"/equation-eval", "{\"equation\":\"x=Re(a)+Im(a)\"}", "display_TeX"},
        {"/diffequation-eval", "{\"diffequation\":\"y'=Re(a)+Im(a)\"}", "display_TeX"},
        {"/matrix-eval", "{\"matrix\":\"(Re(a),Im(a);0,1)\"}", "tex"},
        {"/integrator-eval", "{\"expression\":\"(Re(a)+Im(a))*x\",\"bounds\":[{\"name\":\"x\"}]}", "tex"}
    };
    for (size_t i = 0u; i < sizeof(cases) / sizeof(*cases); ++i) {
        unsigned status = 0u;
        json_t *response = request(cases[i].route, cases[i].payload, &status);
        bool ok = status == 200u && test_lab_ok(response, true) &&
                  contains(response, cases[i].field, "\\mathfrak{Re}") &&
                  contains(response, cases[i].field, "\\mathfrak{Im}") && contains(response, "svg", "<svg");
        if (!ok)
            show_failure(response);
        json_free(response);
        TEST_ASSERT_TRUE(ok, "every mathematical mode renders two-letter Fraktur projections");
    }
}

static void test_lab_extra_unsafe_TeX_input(void)
{
    expect_response("/render_TeX", "{\"tex\":\"\\\\input{/etc/passwd}\"}", 422u, "error", "unsupported");
}

static void test_lab_extra_unsafe_TeX_escape(void)
{
    expect_response("/render_TeX", "{\"tex\":\"^^5cinput{/etc/passwd}\"}", 422u, "error", "unsupported");
}

static void test_lab_extra_unsafe_TeX_environment(void)
{
    expect_response("/render_TeX", "{\"tex\":\"\\\\begin{filecontents}{x}data\\\\end{filecontents}\"}", 422u, "error",
                    "unsupported");
}

static void test_lab_extra_invalid_type(void)
{
    expect_response("/eval", "{\"expression\":3}", 400u, "error", "Input");
}

static void test_lab_extra_invalid_precision(void)
{
    expect_response("/eval", "{\"expression\":\"x\",\"precision\":true}", 400u, "error", "Precision");
}

static void test_lab_extra_excess_precision(void)
{
    expect_response("/eval", "{\"expression\":\"x\",\"precision\":315654}", 400u, "error", "Precision");
}

static void test_lab_extra_function_precision_limit(void)
{
    expect_response("/function-run", "{\"source\":\"output(1).\",\"precision\":10001}", 400u, "error",
                    "Function precision");
}

static void test_lab_extra_invalid_bounds(void)
{
    expect_response("/integrator-eval", "{\"expression\":\"x\",\"bounds\":[{\"name\":\"x\",\"lo\":\"0\"}]}", 400u,
                    "error", "Invalid");
}

static void test_lab_extra_invalid_operation(void)
{
    expect_response("/matrix-eval", "{\"matrix\":\"(1)\",\"operation\":\"shell\"}", 400u, "error", "Invalid");
}

static void test_lab_extra_calendar_fallback(void)
{
    unsigned status = 777u;
    json_t *response = request("/datetime-eval", "{}", &status);
    bool ok = !response && status == 777u;
    json_free(response);
    TEST_ASSERT_TRUE(ok, "unhandled calendar route preserves the caller's status");
}

static void test_lab_extra_equation_explicit_bindings(void)
{
    unsigned status = 0u;
    json_t *response = request("/equation-eval", "{\"equation\":\"{ x = c | x = ?; c = 2 }\"}", &status);
    const json_t *bindings = test_lab_member(response, "binding_values");
    bool variable = false;
    bool constant = false;
    /* Inspect the two records in this fixture; native order is deliberately unconstrained. */
    for (size_t i = 0u; i < json_array_size(bindings) && i < 2u; ++i) {
        const json_t *binding = json_array_get(bindings, i);
        if (!strcmp(test_lab_text(binding, "name"), "x"))
            variable = !strcmp(test_lab_text(binding, "kind"), "variable") &&
                       !strcmp(test_lab_text(binding, "value"), "NAN") && !*test_lab_text(binding, "display");
        if (!strcmp(test_lab_text(binding, "name"), "c"))
            constant = !strcmp(test_lab_text(binding, "kind"), "constant") &&
                       !strcmp(test_lab_text(binding, "value"), "2") && !strcmp(test_lab_text(binding, "display"), "2");
    }
    bool ok = status == 200u && test_lab_ok(response, true) && json_array_size(bindings) == 2u && variable && constant;
    if (!ok)
        show_failure(response);
    json_free(response);
    TEST_ASSERT_TRUE(ok, "native equation records populate unset variables and supplied constant controls");
}

static void test_lab_extra_equation_inferred_binding(void)
{
    unsigned status = 0u;
    json_t *response = request("/equation-eval", "{\"equation\":\"x = 2\"}", &status);
    const json_t *bindings = test_lab_member(response, "binding_values");
    const json_t *binding = json_array_get(bindings, 0u);
    bool ok = status == 200u && test_lab_ok(response, true) && json_array_size(bindings) == 1u &&
              !strcmp(test_lab_text(binding, "name"), "x") && !strcmp(test_lab_text(binding, "kind"), "variable");
    if (!ok)
        show_failure(response);
    json_free(response);
    TEST_ASSERT_TRUE(ok, "inferred equation bindings reach the UI without client expression parsing");
}

static void test_lab_extra_equation_greek_binding(void)
{
    unsigned status = 0u;
    json_t *response = request("/equation-eval", "{\"equation\":\"{ @mu = 2 | @mu = ? }\"}", &status);
    const json_t *bindings = test_lab_member(response, "binding_values");
    const json_t *binding = json_array_get(bindings, 0u);
    bool ok = status == 200u && test_lab_ok(response, true) && json_array_size(bindings) == 1u &&
              !strcmp(test_lab_text(binding, "name"), "μ") && !strcmp(test_lab_text(binding, "kind"), "variable");
    if (!ok)
        show_failure(response);
    json_free(response);
    TEST_ASSERT_TRUE(ok, "equation binding names use native Expression rendering for Greek aliases");
}

/* Register each case independently so test_config can select or suppress it. */
void test_lab_evaluation_extra_cases(void)
{
    TEST_RUN_IN_GROUP(test_lab_extra_native_records, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_editor_and_cards, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_expression, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_help_examples, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_constant_identity, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_bindings, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_binding_edit, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_derivative, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_integral, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_equation, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_equation_real_surd_numerics, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_equation_decimal_notation, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_number_card_markup, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_diffequation, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_matrix, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_integrator, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_function, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_goal_seek, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_render_TeX, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_projection_fonts, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_unsafe_TeX_input, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_unsafe_TeX_escape, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_unsafe_TeX_environment, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_invalid_type, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_invalid_precision, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_excess_precision, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_function_precision_limit, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_invalid_bounds, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_invalid_operation, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_calendar_fallback, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_equation_explicit_bindings, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_equation_inferred_binding, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_equation_greek_binding, tests, NULL);
}
