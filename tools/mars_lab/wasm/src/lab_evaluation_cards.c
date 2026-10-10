/**
 * @file lab_evaluation_cards.c
 * @brief Evaluation result selection and mode presentation for the browser.
 *
 * Selects exact native card representations, composes solver diagnostics, merges
 * weather sections and projects mode-specific controls. Browser adapters deliver
 * selected values through the existing card services, preserving their asynchronous
 * cancellation and diagnostic boundaries. No mathematics is parsed or rewritten.
 * All records, cache entries and nodes are borrowed for one synchronous call;
 * returned records contain browser values, never retained WASM handle numbers.
 */
#include "lab_dom.h"
#include "lab_evaluation_dom.h"
#include "lab_layout.h"
#include "lab_result.h"
#include "lab_select.h"
#include "lab_workspace_dom.h"
#include "lab_evaluation_cards.h"

static int lab_evaluation_cards_or(int value, int fallback)
{
    return lab_dom_truth(value) ? value : fallback;
}

static int lab_evaluation_cards_field(int data, const char *key)
{
    return lab_evaluation_cards_or(lab_dom_get(data, key), lab_dom_string(""));
}

static int lab_evaluation_cards_compact(int caches, int source)
{
    int editor = lab_dom_map_get(lab_dom_get(caches, "editors"), lab_dom_clean(source, 0));
    return editor ? lab_dom_get(editor, "display") : source;
}

static void lab_evaluation_cards_pair(int plan, const char *short_key, const char *full_key, int short_text, int full)
{
    lab_dom_set(plan, short_key, short_text);
    lab_dom_set(plan, full_key, full);
}

static int lab_evaluation_cards_join(int first, int second, const char *separator)
{
    if (!lab_dom_truth(first))
        return second;
    if (!lab_dom_truth(second))
        return first;
    return lab_dom_join(lab_dom_join(first, lab_dom_string(separator), ""), second, "");
}

static int lab_evaluation_cards_label(int data, const char *key, const char *label)
{
    int value = lab_evaluation_cards_field(data, key);
    return lab_dom_truth(value) ? lab_dom_join(lab_dom_string(label), value, "") : value;
}

static void lab_evaluation_cards_expression(int plan, int data, int text, int caches, unsigned mode)
{
    int expression = lab_evaluation_cards_field(data, "expression");
    if (mode == 7)
        expression = lab_evaluation_cards_or(expression, text);
    int full = lab_evaluation_cards_or(lab_dom_get(data, "full_display_expression"), expression);
    int compact =
        mode == 7 || lab_dom_truth(expression) ? lab_evaluation_cards_compact(caches, expression) : lab_dom_string("");
    lab_evaluation_cards_pair(plan, "expression", "full_expression",
                              lab_evaluation_cards_or(lab_dom_get(data, "display_expression"), compact), full);
    lab_dom_set(plan, "input",
                mode == 7 ? full : lab_evaluation_cards_or(lab_dom_get(data, "editor_expression"), full));
    int function = lab_evaluation_cards_field(data, "function");
    lab_evaluation_cards_pair(plan, "function", "full_function",
                              lab_evaluation_cards_or(lab_dom_get(data, "display_function"), function),
                              lab_evaluation_cards_or(lab_dom_get(data, "full_display_function"), function));
    lab_dom_set(plan, "value", lab_evaluation_cards_field(data, "value"));
    int calculus = lab_dom_get(lab_dom_get(data, "presentation"), "calculus");
    lab_dom_set(plan, "derivative", lab_evaluation_cards_field(lab_dom_get(calculus, "derivative"), "expression"));
    int flag = lab_evaluation_cards_or(lab_dom_get(data, "differentiable"), lab_dom_string("yes"));
    lab_dom_set(plan, "differentiable",
                lab_dom_scalar(1, !lab_dom_equal(lab_dom_clean(flag, 1), lab_dom_string("no"))));
    lab_dom_set(plan, "unchanged",
                lab_dom_scalar(1, lab_dom_equal(lab_dom_clean(expression, 0), lab_dom_clean(text, 0))));
}

static void lab_evaluation_cards_equation(int plan, int data, int text, int caches, unsigned mode)
{
    (void)text;
    (void)mode;
    int equation = lab_evaluation_cards_field(data, "equation");
    int full = lab_evaluation_cards_or(lab_dom_get(data, "full_display_equation"), equation);
    int display = lab_evaluation_cards_or(lab_dom_get(data, "display_equation"), equation);
    lab_evaluation_cards_pair(plan, "expression", "full_expression", display, full);
    lab_dom_set(plan, "input", lab_dom_clean(lab_evaluation_cards_or(full, display), 0));
    int function = lab_evaluation_cards_field(data, "function");
    lab_evaluation_cards_pair(plan, "function", "full_function", function, function);
    lab_dom_set(plan, "value", lab_evaluation_cards_field(lab_dom_get(data, "presentation"), "equation_solution_text"));
    int shown = lab_dom_get(data, "display_TeX"), full_TeX = lab_dom_get(data, "full_display_TeX");
    int expand = lab_dom_truth(shown) && lab_dom_truth(full_TeX) && !lab_dom_equal(shown, full_TeX) &&
                 lab_result_can_expand(lab_dom_get(caches, "expansions"), shown, full_TeX);
    lab_dom_set(plan, "expandable", lab_dom_scalar(1, expand));
}

static void lab_evaluation_cards_diffequation(int plan, int data, int text, int caches, unsigned mode)
{
    (void)mode;
    int problem = lab_evaluation_cards_or(lab_dom_get(data, "problem"), text);
    lab_evaluation_cards_pair(plan, "expression", "full_expression", problem, problem);
    lab_dom_set(plan, "input", lab_evaluation_cards_or(lab_dom_get(data, "input"), text));
    int symmetry = lab_evaluation_cards_label(data, "symmetry", "Symmetry: ");
    int steps = lab_evaluation_cards_field(data, "steps");
    int details = lab_evaluation_cards_join(symmetry, steps, "\n\n");
    if (!lab_dom_truth(details)) {
        details = lab_evaluation_cards_join(lab_evaluation_cards_label(data, "solver", "solver: "),
                                            lab_evaluation_cards_label(data, "status", "status: "), "\n");
        details = lab_evaluation_cards_join(details, lab_evaluation_cards_field(data, "diagnostic"), "\n");
    }
    lab_evaluation_cards_pair(plan, "function", "full_function", details, details);
    int cached = lab_dom_map_get(lab_dom_get(caches, "solver"), details);
    int source = lab_evaluation_cards_or(lab_dom_get(data, "steps_left_TeX"),
                                         lab_evaluation_cards_or(lab_dom_get(data, "steps_TeX"), cached));
    lab_dom_set(plan, "solver_source", lab_evaluation_cards_or(source, lab_dom_string("")));
    lab_dom_set(plan, "value",
                lab_evaluation_cards_or(lab_dom_get(data, "solutions"),
                                        lab_evaluation_cards_or(lab_dom_get(data, "diagnostic"),
                                                                lab_evaluation_cards_field(data, "status"))));
}

static void lab_evaluation_cards_matrix(int plan, int data, int text, int caches, unsigned mode)
{
    (void)text;
    (void)caches;
    (void)mode;
    lab_dom_set(plan, "scalar",
                lab_dom_truth(lab_dom_get(data, "scalar")) ? lab_evaluation_cards_field(data, "result")
                                                           : lab_dom_string(""));
}

static void lab_evaluation_cards_integrator(int plan, int data, int text, int caches, unsigned mode)
{
    (void)text;
    (void)caches;
    (void)mode;
    int expression = lab_evaluation_cards_field(data, "expression");
    lab_evaluation_cards_pair(plan, "expression", "full_expression", expression, expression);
    lab_dom_set(plan, "input", lab_evaluation_cards_field(data, "antiderivative"));
    int metadata = lab_dom_get(lab_dom_get(data, "presentation"), "integrator");
    int details = lab_evaluation_cards_field(metadata, "detail_text");
    lab_evaluation_cards_pair(plan, "function", "full_function", details, details);
    lab_dom_set(plan, "value", lab_evaluation_cards_field(metadata, "value_text"));
}

static void lab_evaluation_cards_datetime(int plan, int data, int text, int caches, unsigned mode)
{
    (void)text;
    (void)caches;
    (void)mode;
    static const struct {
        const char *node, *button, *sections, *text;
    } cards[] = {{"#rendered", "", "overview_sections", "overview"},
                 {"#parsed", "#parsedMore", "range_sections", "range"},
                 {"#functionStyle", "#functionMore", "calendar_sections", "calendar"},
                 {"#value", "", "solar_sections", "solar"}};
    int records = lab_dom_object(4);
    for (unsigned i = 0; i < sizeof cards / sizeof *cards; ++i) {
        unsigned mark = lab_dom_mark();
        int card = lab_dom_object(5);
        lab_dom_set(card, "element", lab_dom_query(0, cards[i].node));
        lab_dom_set(card, "button", *cards[i].button ? lab_dom_query(0, cards[i].button) : 0);
        lab_dom_set(card, "sections", lab_dom_get(data, cards[i].sections));
        lab_dom_set(card, "text", lab_evaluation_cards_field(data, cards[i].text));
        lab_dom_push(records, card);
        lab_dom_release(mark);
    }
    lab_dom_set(plan, "calendar", records);
}

/* Return one native card plan; mode 7 denotes goal results, otherwise normal worksheet mode indices apply. */
void lab_evaluation_cards(unsigned mode, int data, int text, int caches)
{
    typedef void (*lab_evaluation_cards_handler_t)(int, int, int, int, unsigned);
    static const lab_evaluation_cards_handler_t handlers[] = {[0] = lab_evaluation_cards_expression,
                                                              [1] = lab_evaluation_cards_equation,
                                                              [2] = lab_evaluation_cards_diffequation,
                                                              [3] = lab_evaluation_cards_matrix,
                                                              [4] = lab_evaluation_cards_integrator,
                                                              [5] = lab_evaluation_cards_datetime,
                                                              [6] = 0,
                                                              [7] = lab_evaluation_cards_expression};
    if (mode >= sizeof handlers / sizeof *handlers) {
        lab_dom_return(0);
        return;
    }
    int plan = lab_dom_object(5), empty = lab_dom_string("");
    static const char *const fields[] = {"expression", "full_expression", "input",
                                         "function",   "full_function",   "value"};
    for (unsigned i = 0; i < sizeof fields / sizeof *fields; ++i)
        lab_dom_set(plan, fields[i], empty);
    if (handlers[mode])
        handlers[mode](plan, data, text, caches, mode);
    lab_dom_return(plan);
}

/* Project mode-specific notes, controls and visibility after the text-card services have run. */
void lab_evaluation_cards_present(unsigned mode, int data)
{
    if (!mode) {
        lab_evaluation_notes(data);
        int value = lab_dom_read(lab_dom_query(0, "#value"), 0, "");
        lab_workspace_dom_value(lab_dom_length(lab_dom_clean(value, 0)) != 0);
    } else if (mode == 2) {
        lab_dom_class(lab_dom_query(0, "#functionStyle"), "equation-function", 0);
    } else if (mode == 3) {
        lab_dom_write(lab_dom_query(0, "#valueTitle"), 0, "", lab_dom_string("Value"));
        int operation = lab_dom_get(data, "operation"), select = lab_dom_query(0, "#matrixOperation");
        if (select && lab_dom_truth(operation)) {
            lab_dom_write(select, 5, "", lab_dom_clean(operation, 0));
            if (!lab_dom_length(lab_dom_read(select, 5, "")))
                lab_dom_write(select, 5, "", lab_dom_string("eval"));
            lab_select_sync(select);
            lab_workspace_dom_matrix(3);
        }
    } else if (mode == 5 || mode == 6) {
        lab_layout_more(lab_dom_query(0, "#renderedMore"), 0);
    }
}

static int lab_evaluation_cards_copy(int source)
{
    int copy = lab_dom_object(5), keys = lab_dom_keys(source);
    for (unsigned i = 0; i < lab_dom_count(keys); ++i) {
        unsigned mark = lab_dom_mark();
        int key = lab_dom_item(keys, i);
        lab_dom_key_set(copy, key, lab_dom_key_get(source, key));
        lab_dom_release(mark);
    }
    return copy;
}

/* Return the weather request state with native-evaluation date/location precedence and string coercion. */
void lab_evaluation_weather_state(int state, int data)
{
    int copy = lab_evaluation_cards_copy(state), fields = lab_dom_get(data, "fields");
    static const char *const keys[] = {"date", "latitude", "longitude"};
    for (unsigned i = 0; i < 3; ++i) {
        int value = lab_evaluation_cards_or(lab_dom_get(fields, keys[i]), lab_dom_get(state, keys[i]));
        lab_dom_set(copy, keys[i], lab_dom_text(value));
    }
    lab_dom_return(copy);
}

/* Replace exact Weather sections while preserving native order, markup and copy text. */
void lab_evaluation_weather_cards(int overview, int weather)
{
    int result = lab_dom_object(5), sections = lab_dom_object(4);
    const int sources[] = {overview, weather};
    int weather_title = lab_dom_string("Weather");
    for (unsigned source = 0; source < 2; ++source) {
        int entries = lab_dom_get(sources[source], "overview_sections");
        if (lab_dom_type(entries) != 4)
            continue;
        for (unsigned i = 0; i < lab_dom_count(entries); ++i) {
            unsigned mark = lab_dom_mark();
            int section = lab_dom_item(entries, i);
            int title = lab_dom_text(lab_evaluation_cards_field(section, "title"));
            if (source || !lab_dom_equal(title, weather_title))
                lab_dom_push(sections, section);
            lab_dom_release(mark);
        }
    }
    lab_dom_set(result, "sections", sections);
    lab_dom_set(result, "text",
                lab_evaluation_cards_join(lab_evaluation_cards_field(overview, "overview"),
                                          lab_evaluation_cards_field(weather, "overview"), "\n"));
    lab_dom_return(result);
}

/* Reveal an eligible goal target, or return the source/target pair for the browser's asynchronous request. */
void lab_evaluation_goal_start(unsigned mode, int text)
{
    lab_dom_return(0);
    if (mode || !lab_dom_truth(text))
        return;
    if (lab_dom_has_class(lab_dom_query(0, "#targetRow"), "hidden")) {
        lab_workspace_dom_target(1);
        return;
    }
    lab_workspace_dom_help(0);
    int result = lab_dom_object(5);
    int target = lab_dom_clean(lab_dom_read(lab_dom_query(0, "#goalTarget"), 5, ""), 0);
    lab_dom_set(result, "text", text);
    lab_dom_set(result, "target", lab_evaluation_cards_or(target, lab_dom_string("0")));
    lab_dom_return(result);
}
