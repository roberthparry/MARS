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

static void test_lab_extra_diffequation(void)
{
    expect_response("/diffequation-eval", "{\"diffequation\":\"Dx(y)=x*y; y(0)=1\"}", 200u, "solutions", "y");
}

static void test_lab_extra_matrix(void)
{
    expect_response("/matrix-eval", "{\"matrix\":\"(1,2;3,4)\",\"operation\":\"det\"}", 200u, "result", "-2");
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
    TEST_RUN_IN_GROUP(test_lab_extra_constant_identity, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_bindings, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_binding_edit, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_derivative, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_integral, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_equation, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_diffequation, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_matrix, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_integrator, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_function, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_goal_seek, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_render_TeX, tests, NULL);
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
