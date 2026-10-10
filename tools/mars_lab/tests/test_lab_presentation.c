/**
 * @file test_lab_presentation.c
 * @brief Native presentation metadata and exact editor-text regressions.
 *
 * Exercises the public presentation adapter without workers, rendering processes
 * or network access. Literal expectations distinguish numerical solutions from
 * symbolic constants and preserve authored matrix cells and binding values.
 */
#include <string.h>

#include "expression.h"
#include "lab_presentation.h"
#include "test_harness.h"
#include "test_lab_support.h"

static json_t *lab_pres_test_request(const char *literal, unsigned *status)
{
    json_t *payload = test_lab_json(literal);
    json_t *result = lab_presentation_request(payload, status);
    json_free(payload);
    return result;
}

static json_t *lab_pres_test_analyse_text(const string_t *text, unsigned *status)
{
    json_t *payload = test_lab_json("{\"action\":\"editor\"}");
    string_t *key = string_new_with("text");
    json_t *value = text ? json_new_string(text) : NULL;
    bool ok = payload && key && value && json_object_set(payload, key, value);
    json_t *result = ok ? lab_presentation_request(payload, status) : NULL;
    json_free(value);
    string_free(key);
    json_free(payload);
    return result;
}

static void test_lab_presentation_matrix(void)
{
    unsigned status = 0;
    json_t *result = lab_pres_test_request(
        "{\"action\":\"matrix\",\"text\":\"1.5·(1/3, f(x,y); 1e-20, [a+b]) + (2, 3; 4, 5)\"}", &status);
    const json_t *terms = test_lab_member(result, "terms");
    const json_t *first = json_array_get(terms, 0);
    const json_t *rows = test_lab_member(first, "rows");
    const string_t *cell = json_string_value(json_array_get(json_array_get(rows, 0), 1));
    bool ok = status == 200 && test_lab_ok(result, true) && json_array_size(terms) == 2 &&
              !strcmp(test_lab_text(first, "factor"), "1.5") && json_array_size(rows) == 2 && cell &&
              !strcmp(string_c_str(cell), "f(x,y)") &&
              strstr(test_lab_text(result, "html"), "class=\"matrix-factor\">1.5</span>") &&
              strstr(test_lab_text(result, "html"), "style=\"--matrix-columns:2\"") &&
              strstr(test_lab_text(result, "html"), "class=\"matrix-cell\">f(x,y)</span>") &&
              strstr(test_lab_text(result, "html"), "class=\"matrix-sum-operator\">+</span>");
    json_free(result);
    result = lab_pres_test_request("{\"action\":\"matrix\",\"text\":\"(<img src=x>, &; μ, 4)\"}", &status);
    ok = ok && status == 200 && strstr(test_lab_text(result, "html"), "&lt;img src=x&gt;") &&
         strstr(test_lab_text(result, "html"), "&amp;") && !strstr(test_lab_text(result, "html"), "<img");
    json_free(result);
    TEST_ASSERT_TRUE(ok, "native matrix layout retains decimal factors and nested cell arguments");
}

static void test_lab_presentation_metadata(void)
{
    json_t *fields = test_lab_json("{\"derivative\":\"d/dx = 2*x\",\"solutions\":\"x = 1/3\\ny ≈ 2+3i\\nz = π\","
                                   "\"solver\":\"a_b\\\\c\"}");
    bool ok = lab_presentation_adapt(fields);
    const json_t *metadata = test_lab_member(fields, "presentation");
    const json_t *calculus = test_lab_member(metadata, "calculus_lines");
    const json_t *solutions = test_lab_member(metadata, "solution_lines");
    bool first_numeric = false, second_numeric = false, third_numeric = true;
    ok = ok && !strcmp(test_lab_text(json_array_get(calculus, 0), "expression"), "2*x") &&
         json_array_size(solutions) == 3 &&
         json_bool_value(test_lab_member(json_array_get(solutions, 0), "numeric"), &first_numeric) && first_numeric &&
         json_bool_value(test_lab_member(json_array_get(solutions, 1), "numeric"), &second_numeric) && second_numeric &&
         json_bool_value(test_lab_member(json_array_get(solutions, 2), "numeric"), &third_numeric) && !third_numeric &&
         !strcmp(test_lab_text(metadata, "solver_TeX"),
                 "\\begin{aligned}[t]&\\text{solver: a\\_b\\textbackslash{}c}\\end{aligned}") &&
         !strcmp(test_lab_text(fields, "derivative"), "d/dx = 2*x");
    json_free(fields);
    TEST_ASSERT_TRUE(ok, "native metadata classifies literals, extracts calculus and escapes TeX exactly once");
}

static void test_lab_presentation_calculus_cards(void)
{
    json_t *fields = test_lab_json(
        "{\"derivative\":\"d/dx = 2*x\",\"derivative_function\":\"exact derivative\","
        "\"display_derivative_function\":\"short derivative\",\"full_display_derivative_function\":\"full derivative\","
        "\"derivative_TeX\":\"2x\",\"derivative_svg\":\"derivative svg\",\"derivative_value\":\"0\","
        "\"derivative_values\":\"x=0\",\"integral\":\"∫dx = x^2/2\",\"integral_function\":\"exact integral\","
        "\"integral_TeX\":\"compact\",\"integral_wrapped_TeX\":\"wrapped\",\"integral_svg\":\"integral svg\","
        "\"integral_wrapped_svg\":\"wrapped svg\",\"integral_render_error\":\"renderer unavailable\"}");
    bool ok = fields && lab_presentation_adapt(fields);
    const json_t *cards = test_lab_member(test_lab_member(fields, "presentation"), "calculus");
    const json_t *derivative = test_lab_member(cards, "derivative"), *integral = test_lab_member(cards, "integral");
    ok = ok && !strcmp(test_lab_text(derivative, "expression"), "2*x") &&
         !strcmp(test_lab_text(derivative, "function"), "short derivative") &&
         !strcmp(test_lab_text(derivative, "full_function"), "full derivative") &&
         !strcmp(test_lab_text(derivative, "wrapped_TeX"), "2x") &&
         !strcmp(test_lab_text(derivative, "svg"), "derivative svg") &&
         !strcmp(test_lab_text(derivative, "value"), "0") &&
         !strcmp(test_lab_text(derivative, "value_title"), "Values") &&
         !strcmp(test_lab_text(integral, "expression"), "x^2/2") &&
         !strcmp(test_lab_text(integral, "function"), "exact integral") &&
         !strcmp(test_lab_text(integral, "full_function"), "exact integral") &&
         !strcmp(test_lab_text(integral, "TeX"), "compact") &&
         !strcmp(test_lab_text(integral, "wrapped_TeX"), "wrapped") &&
         !strcmp(test_lab_text(integral, "wrapped_svg"), "wrapped svg") &&
         !strcmp(test_lab_text(integral, "render_error"), "renderer unavailable") &&
         !strcmp(test_lab_text(integral, "value_title"), "Value") && !*test_lab_text(integral, "value");
    json_free(fields);
    fields = test_lab_json("{\"derivative\":\"d/dx = 1/3\",\"integral\":\"unrecognised result\"}");
    ok = fields && lab_presentation_adapt(fields) && ok;
    cards = test_lab_member(test_lab_member(fields, "presentation"), "calculus");
    derivative = test_lab_member(cards, "derivative");
    integral = test_lab_member(cards, "integral");
    ok = ok && !strcmp(test_lab_text(derivative, "function"), "1/3") &&
         !strcmp(test_lab_text(derivative, "full_function"), "1/3") && !*test_lab_text(integral, "expression") &&
         !*test_lab_text(integral, "function");
    json_free(fields);
    TEST_ASSERT_TRUE(ok, "native calculus cards choose exact function fallbacks, TeX variants and value labels");
}

static void test_lab_presentation_editor_calculus(void)
{
    unsigned status = 0;
    json_t *result = lab_pres_test_request(
        "{\"action\":\"editor\",\"operation\":\"calculus\",\"calculus\":\"derivative\",\"name\":\"x\","
        "\"text\":\"{ (x, 0; 0, x^2) | x = ?; a = 1/3; x > 0 }\"}",
        &status);
    const json_t *editor = test_lab_member(result, "editor");
    bool ok = status == 200 && !strcmp(test_lab_text(editor, "body"), "Dx((x, 0; 0, x^2))") &&
              json_array_size(test_lab_member(editor, "bindings")) == 2 &&
              strstr(test_lab_text(editor, "expression"), "x > 0") &&
              strstr(test_lab_text(editor, "expression"), "1/3");
    json_free(result);
    result = lab_pres_test_request(
        "{\"action\":\"editor\",\"operation\":\"calculus\",\"calculus\":\"integral\",\"name\":\"x\","
        "\"text\":\"(x, 0; 0, x^2)\"}",
        &status);
    ok = ok && status == 200 && !strcmp(test_lab_text(test_lab_member(result, "editor"), "body"), "@S(x, 0; 0, x^2)dx");
    json_free(result);
    result = lab_pres_test_request(
        "{\"action\":\"editor\",\"operation\":\"calculus\",\"calculus\":\"derivative\",\"name\":\"x+1\","
        "\"text\":\"(x, 0; 0, x^2)\"}",
        &status);
    ok = ok && status == 400 && test_lab_ok(result, false);
    json_free(result);
    result = lab_pres_test_request(
        "{\"action\":\"editor\",\"operation\":\"calculus\",\"calculus\":\"unknown\",\"name\":\"x\","
        "\"text\":\"x\"}",
        &status);
    ok = ok && status == 400 && test_lab_ok(result, false);
    json_free(result);
    TEST_ASSERT_TRUE(ok,
                     "native calculus input construction retains bindings and conditions and validates names/actions");
}

static void test_lab_presentation_unset(void)
{
    unsigned status = 0;
    json_t *result = lab_pres_test_request(
        "{\"action\":\"unset_constants\",\"expression\":\"{ 1e-20*x - C_1 + sin(C_1*x) | x = 1/3; C_1 = ?; x > 0 }\","
        "\"names\":[\"C_1\"]}",
        &status);
    bool ok =
        status == 200 && !strcmp(test_lab_text(result, "expression"), "{ 1e-20*x + sin(C_1*x) | x = 1/3; ; x > 0 }");
    json_free(result);
    TEST_ASSERT_TRUE(ok, "only bare additive constants are removed; exponents, products and conditions survive");
}

static void test_lab_presentation_rejections(void)
{
    unsigned status = 0;
    json_t *result = lab_pres_test_request("{\"action\":\"matrix\",\"text\":\"(1,2;3)\"}", &status);
    bool ok =
        status == 200 && json_array_size(test_lab_member(result, "terms")) == 0 && !*test_lab_text(result, "html");
    json_free(result);
    result = lab_pres_test_request("{\"action\":\"unset_constants\",\"expression\":\"{ x | C = ?\","
                                   "\"names\":[\"C\"]}",
                                   &status);
    ok = ok && status == 400 && test_lab_ok(result, false);
    json_free(result);
    result = lab_pres_test_request("{\"action\":\"matrix\",\"text\":42}", &status);
    ok = ok && status == 400 && test_lab_ok(result, false);
    json_free(result);
    TEST_ASSERT_TRUE(ok, "ragged layouts fall back to native text; malformed edits and non-text input are rejected");
}

static void test_lab_presentation_editor_analysis(void)
{
    unsigned status = 0;
    json_t *result = lab_pres_test_request("{\"action\":\"editor\",\"operation\":\"analyse\","
                                           "\"text\":\"{ f(x,a) | x = 1/3; c_10 = sqrt(2), c_2 = π; x > 0 }\"}",
                                           &status);
    const json_t *editor = test_lab_member(result, "editor");
    const json_t *bindings = test_lab_member(editor, "bindings");
    bool ok = status == 200 && json_array_size(bindings) == 3 && !strcmp(test_lab_text(editor, "body"), "f(x,a)") &&
              !strcmp(test_lab_text(json_array_get(bindings, 1), "value"), "sqrt(2)") &&
              !strcmp(test_lab_text(editor, "expression"), "{ f(x,a) | x = 1/3; c_2 = π, c_10 = sqrt(2); x > 0 }") &&
              !strcmp(test_lab_text(editor, "goal_expression"), "{ f(x,a) | x = ?; c_2 = π, c_10 = sqrt(2); x > 0 }") &&
              !strcmp(test_lab_text(test_lab_member(editor, "starts"), "x"), "1/3");
    json_free(result);
    result = lab_pres_test_request("{\"action\":\"editor\",\"operation\":\"goal_seek\",\"text\":\"{ x | x = 1/3 }\"}",
                                   &status);
    editor = test_lab_member(result, "editor");
    ok = ok && status == 200 && !strcmp(test_lab_text(editor, "expression"), "{ x | x = ? }") &&
         !strcmp(test_lab_text(test_lab_member(editor, "starts"), "x"), "1/3");
    json_free(result);
    TEST_ASSERT_TRUE(ok, "editor metadata preserves symbolic literals, conditions and exact goal starts");
}

static void test_lab_presentation_editor_edits(void)
{
    unsigned status = 0;
    json_t *result =
        lab_pres_test_request("{\"action\":\"editor\",\"operation\":\"bindings\",\"mode\":\"merge\","
                              "\"text\":\"{ x + C | x = 1/3; C = 5, a = sqrt(2); a > 0 }\",\"unset_constants\":true,"
                              "\"bindings\":[{\"name\":\"C\",\"kind\":\"constant\",\"value\":\"?\"}]}",
                              &status);
    bool ok = status == 200 && !strcmp(test_lab_text(test_lab_member(result, "editor"), "expression"),
                                       "{ x | x = 1/3; a = sqrt(2); a > 0 }");
    json_free(result);
    result =
        lab_pres_test_request("{\"action\":\"editor\",\"operation\":\"kind\",\"text\":\"{ x+a | x=1/3; a=π; a>0 }\","
                              "\"name\":\"a\",\"kind\":\"variable\"}",
                              &status);
    ok = ok && status == 200 &&
         !strcmp(test_lab_text(test_lab_member(result, "editor"), "expression"), "{ x+a | x = 1/3, a = π; ; a>0 }");
    json_free(result);
    result = lab_pres_test_request("{\"action\":\"editor\",\"operation\":\"bindings\",\"text\":\"{ x | x=1 }\","
                                   "\"bindings\":[{\"name\":\"x\",\"kind\":\"variable\",\"value\":\"2; a=7\"}]}",
                                   &status);
    ok = ok && status == 400 && test_lab_ok(result, false);
    json_free(result);
    TEST_ASSERT_TRUE(ok, "native edits remove additive constants, move kinds and reject injected assignment groups");
}

static void test_lab_presentation_compaction(void)
{
    unsigned status = 0;
    json_t *result = lab_pres_test_request(
        "{\"action\":\"editor\",\"text\":\"{ x | x = 123456789012345678901234567890; a = 1/3 }\"}", &status);
    const json_t *editor = test_lab_member(result, "editor");
    bool ok = status == 200 &&
              !strcmp(test_lab_text(editor, "display"), "{ x | x = 1.2345678901234567890123...e+29; a = 1/3 }") &&
              !strcmp(test_lab_text(editor, "expression"), "{ x | x = 123456789012345678901234567890; a = 1/3 }");
    json_free(result);
    TEST_ASSERT_TRUE(ok, "native numeric compaction retains the complete authored source independently");
}

static void test_lab_presentation_condition_boundary(void)
{
    unsigned status = 0;
    json_t *result =
        lab_pres_test_request("{\"action\":\"unset_constants\",\"expression\":\"{ x+C_1 | x=?; C_1=?; x>0; C_1=0 }\","
                              "\"names\":[\"C_1\"]}",
                              &status);
    bool ok = status == 200 && !strcmp(test_lab_text(result, "expression"), "{ x | x=?; ; x>0; C_1=0 }");
    json_free(result);
    result = lab_pres_test_request("{\"action\":\"editor\",\"text\":\"{ x | x=?; Re(s)=0; C_1=0 }\"}", &status);
    const json_t *editor = test_lab_member(result, "editor");
    ok = ok && status == 200 && json_array_size(test_lab_member(editor, "bindings")) == 1 &&
         json_array_size(test_lab_member(editor, "conditions")) == 2 &&
         !strcmp(test_lab_text(editor, "expression"), "{ x | x = ?; ; Re(s)=0; C_1=0 }");
    json_free(result);
    TEST_ASSERT_TRUE(ok, "the first condition ends editable bindings, including later identifier equalities");
}

static void test_lab_presentation_embedded_nul(void)
{
    unsigned status = 0;
    json_t *result = lab_pres_test_request("{\"action\":\"editor\",\"text\":\"x\\u0000 + y\"}", &status);
    bool ok = status == 400 && test_lab_ok(result, false);
    json_free(result);
    result = lab_pres_test_request("{\"action\":\"editor\",\"operation\":\"bindings\",\"text\":\"{ x | x=? }\","
                                   "\"bindings\":[{\"name\":\"x\",\"kind\":\"variable\",\"value\":\"1\\u0000/3\"}]}",
                                   &status);
    ok = ok && status == 400 && test_lab_ok(result, false);
    json_free(result);
    result = lab_pres_test_request("{\"action\":\"editor\\u0000other\",\"text\":\"x\"}", &status);
    ok = ok && status == 400 && test_lab_ok(result, false);
    json_free(result);
    TEST_ASSERT_TRUE(ok, "embedded NUL in source or authored values is rejected rather than silently truncated");
}

static void test_lab_presentation_empty_constants_roundtrip(void)
{
    unsigned status = 0;
    json_t *removed =
        lab_pres_test_request("{\"action\":\"unset_constants\",\"expression\":\"{ x+C_1 | x=?; C_1=?; C_1=0 }\","
                              "\"names\":[\"C_1\"]}",
                              &status);
    const string_t *source = json_string_value(test_lab_member(removed, "expression"));
    bool ok = status == 200 && source && !strcmp(string_c_str(source), "{ x | x=?; ; C_1=0 }");
    json_t *analysed = lab_pres_test_analyse_text(source, &status);
    const json_t *editor = test_lab_member(analysed, "editor");
    ok = ok && status == 200 && json_array_size(test_lab_member(editor, "bindings")) == 1 &&
         json_array_size(test_lab_member(editor, "conditions")) == 1 &&
         !strcmp(test_lab_text(editor, "expression"), "{ x | x = ?; ; C_1=0 }");
    source = json_string_value(test_lab_member(editor, "expression"));
    json_t *again = lab_pres_test_analyse_text(source, &status);
    editor = test_lab_member(again, "editor");
    ok = ok && status == 200 && json_array_size(test_lab_member(editor, "bindings")) == 1 &&
         json_array_size(test_lab_member(editor, "conditions")) == 1 &&
         !strcmp(test_lab_text(editor, "expression"), "{ x | x = ?; ; C_1=0 }");
    json_free(again);
    json_free(analysed);
    json_free(removed);

    /* The expression parser currently supports Re(...) > ..., not bare equality conditions. */
    removed =
        lab_pres_test_request("{\"action\":\"unset_constants\",\"expression\":\"{ x+C_1 | x=?; C_1=?; Re(x)>0 }\","
                              "\"names\":[\"C_1\"]}",
                              &status);
    source = json_string_value(test_lab_member(removed, "expression"));
    analysed = lab_pres_test_analyse_text(source, &status);
    source = json_string_value(test_lab_member(test_lab_member(analysed, "editor"), "expression"));
    expr_bindings_t *bindings = NULL;
    expr_t *expression = source ? expr_from_text(source, &bindings) : NULL;
    string_t *rendered = expression ? expr_to_text(expression, style_EXPRESSION) : NULL;
    ok = ok && status == 200 && source && !strcmp(string_c_str(source), "{ x | x = ?; ; Re(x)>0 }") && expression &&
         expr_bindings_count(bindings) == 1 && expr_bindings_get(bindings, "x") &&
         !expr_bindings_get(bindings, "C_1") && rendered && string_find(rendered, "Re(") >= 0;
    string_free(rendered);
    expr_free(expression);
    expr_bindings_free(bindings);
    json_free(analysed);
    json_free(removed);
    TEST_ASSERT_TRUE(ok, "empty constant sections preserve conditions across editor and native expression parsing");
}

static void test_lab_presentation_equation_solution_text(void)
{
    static const struct {
        const char *payload;
        const char *expected;
    } cases[] = {
        {"{\"solutions\":\" x = 1 \\n\\n y = π \",\"display_solutions\":\" x = 1 \\n y = π \","
         "\"numeric_solutions\":[\" \",\"x ≈ 1\",\" y ≈ 3.14 \"],\"interpretation_note\":\" Note \","
         "\"search_note\":\" Search \",\"family_note\":\" Family \"}",
         " Note \n\nx = 1\ny = π\ny ≈ 3.14\n\n Search \n\n Family "},
        {"{\"solutions\":\"x = π\\ny = π\",\"numeric_solutions\":[\"x ≈ 3.14\",\"y ≈ 3.14\"]}",
         "x = π\ny = π\n\nx ≈ 3.14\ny ≈ 3.14"},
        {"{\"status\":\"no solutions\",\"interpretation_note\":\"ignored\",\"search_note\":\"ignored\"}",
         "No solutions"},
        {"{\"status\":\"undetermined\",\"solutions\":\" \\n \",\"search_note\":\"search\\nnext\"}",
         "undetermined\n\nsearch\nnext"},
        {"{\"status\":\"unused\",\"interpretation_note\":\"note\"}", "note\n"},
        {"{\"numeric_solutions\":[\"x ≈ 2\",\"y ≈ 3\"],\"family_note\":\"family\"}", "x ≈ 2\n\ny ≈ 3\n\nfamily"},
        {"{\"solutions\":\"x = 1\",\"display_solutions\":\" \\n\",\"numeric_solutions\":[\"x ≈ 1\"],"
         "\"status\":\"solved\"}",
         "solved"},
        {"{}", ""}};
    bool ok = true;
    for (size_t i = 0u; i < sizeof(cases) / sizeof(*cases); ++i) {
        json_t *fields = test_lab_json(cases[i].payload);
        bool adapted = fields && lab_presentation_adapt(fields);
        ok = ok && adapted &&
             !strcmp(test_lab_text(test_lab_member(fields, "presentation"), "equation_solution_text"),
                     cases[i].expected);
        json_free(fields);
    }
    TEST_ASSERT_TRUE(ok,
                     "native equation solution text retains literal suppression, note spacing and blank-line policy");
}

static void test_lab_presentation_integrator_text(void)
{
    static const struct {
        const char *payload;
        const char *detail;
        const char *value;
    } cases[] = {
        {"{\"antiderivative\":\"x^2/2\",\"symbolic\":\"1/2\",\"bound\":\"0 ≤ x ≤ 1\","
         "\"status\":\"SYMBOLIC result\",\"work_units\":\"2\",\"work_cap\":\"8\",\"value\":\"0.5\"}",
         "Antiderivative:\nx^2/2\n\nDefinite result:\n1/2\n\n0 ≤ x ≤ 1\nstatus: SYMBOLIC result", "0.5"},
        {"{\"antiderivative\":\"x^2/2\",\"symbolic\":\"x^2/2\",\"status\":\"Antiderivative\","
         "\"intervals\":\"2\"}",
         "Antiderivative:\nx^2/2\n\nstatus: Antiderivative", "Antiderivative"},
        {"{\"status\":\"Closed-Form\",\"work_units\":\"2\",\"work_cap\":\"8\"}", "status: Closed-Form", "Closed-Form"},
        {"{\"status\":\"FAST PATH complete\",\"work_units\":\"2\"}", "status: FAST PATH complete",
         "FAST PATH complete"},
        {"{\"status\":\"converged\",\"bound\":\"domain\\nrestriction\",\"work_units\":2,\"work_cap\":8,"
         "\"value\":\"1/3\",\"error\":\"1e-20\"}",
         "domain\nrestriction\nstatus: converged\nwork used: 2 / 8 (precision reached)", "1/3\nerror ≈ 1e-20"},
        {"{\"work_units\":8,\"work_cap\":\"8\",\"intervals\":99,\"max_intervals\":100}", "work used: 8 / 8", ""},
        {"{\"work_units\":0,\"work_cap\":0,\"intervals\":3,\"max_intervals\":9}",
         "work used: 3 / 9 (precision reached)", ""},
        {"{\"work_units\":\"0\",\"work_cap\":\"0\",\"intervals\":3}", "work used: 0 / 0", ""},
        {"{\"intervals\":4,\"error\":\"0\"}", "work used: 4", "error ≈ 0"},
        {"{\"work_cap\":9,\"status\":\"numerical\"}", "status: numerical", "numerical"},
        {"{}", "", ""}};
    bool ok = true;
    for (size_t i = 0u; i < sizeof(cases) / sizeof(*cases); ++i) {
        json_t *fields = test_lab_json(cases[i].payload);
        bool adapted = fields && lab_presentation_adapt(fields);
        const json_t *integrator = test_lab_member(test_lab_member(fields, "presentation"), "integrator");
        ok = ok && adapted && !strcmp(test_lab_text(integrator, "detail_text"), cases[i].detail) &&
             !strcmp(test_lab_text(integrator, "value_text"), cases[i].value);
        json_free(fields);
    }
    TEST_ASSERT_TRUE(
        ok, "native integrator cards preserve status policy, numeric counters, work fallback and exact spacing");
}

/* Register ordinary presentation checks before the suite's README examples. */
void test_lab_presentation_cases(void)
{
    TEST_RUN_IN_GROUP(test_lab_presentation_matrix, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_presentation_metadata, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_presentation_calculus_cards, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_presentation_equation_solution_text, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_presentation_integrator_text, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_presentation_unset, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_presentation_rejections, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_presentation_editor_analysis, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_presentation_editor_edits, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_presentation_editor_calculus, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_presentation_compaction, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_presentation_condition_boundary, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_presentation_embedded_nul, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_presentation_empty_constants_roundtrip, tests, NULL);
}
