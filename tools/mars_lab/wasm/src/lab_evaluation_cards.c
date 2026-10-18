/**
 * @file lab_evaluation_cards.c
 * @brief Evaluation result selection and kind presentation for the browser.
 *
 * Selects exact native card representations, composes solver diagnostics, merges
 * weather sections and projects kind-specific controls. Browser adapters deliver
 * selected values through the existing card services, preserving their asynchronous
 * cancellation and diagnostic boundaries. No mathematics is parsed or rewritten.
 * All records, cache entries and nodes are borrowed for one synchronous call;
 * returned records contain browser values, never retained WASM handle numbers.
 */
#include "lab_mode.h"
#include "lab_dom.h"
#include "lab_evaluation_dom.h"
#include "lab_layout.h"
#include "lab_result.h"
#include "lab_select.h"
#include "lab_workspace_dom.h"
#include "lab_evaluation_cards.h"

static browser_handle lab_evaluation_cards_or(browser_handle value, browser_handle fallback)
{
    return lab_dom_truth(value) ? value : fallback;
}

static browser_handle lab_evaluation_cards_field(browser_handle data, const char *key)
{
    return lab_evaluation_cards_or(lab_dom_get(data, key), lab_dom_string(""));
}

static browser_handle lab_evaluation_cards_compact(browser_handle caches, browser_handle source)
{
    int editor = lab_dom_map_get(lab_dom_get(caches, "editors"), lab_dom_clean(source, LAB_DOM_CLEAN_TRIM));
    return editor ? lab_dom_get(editor, "display") : source;
}

static void lab_evaluation_cards_pair(browser_handle plan, const char *short_key, const char *full_key,
                                browser_handle short_text, browser_handle full)
{
    lab_dom_set(plan, short_key, short_text);
    lab_dom_set(plan, full_key, full);
}

static browser_handle lab_evaluation_cards_join(browser_handle first, browser_handle second, const char *separator)
{
    if (!lab_dom_truth(first))
        return second;
    if (!lab_dom_truth(second))
        return first;
    return lab_dom_join(lab_dom_join(first, lab_dom_string(separator), ""), second, "");
}

static browser_handle lab_evaluation_cards_label(browser_handle data, const char *key, const char *label)
{
    int value = lab_evaluation_cards_field(data, key);
    return lab_dom_truth(value) ? lab_dom_join(lab_dom_string(label), value, "") : value;
}

static void lab_evaluation_cards_expression(browser_handle plan, browser_handle data, browser_handle text,
                                browser_handle caches, lab_evaluation_card_kind_t kind)
{
    int expression = lab_evaluation_cards_field(data, "expression");
    if (kind == LAB_EVALUATION_CARDS_GOAL)
        expression = lab_evaluation_cards_or(expression, text);
    int full = lab_evaluation_cards_or(lab_dom_get(data, "full_display_expression"), expression);
    int compact = kind == LAB_EVALUATION_CARDS_GOAL || lab_dom_truth(expression)
                      ? lab_evaluation_cards_compact(caches, expression)
                      : lab_dom_string("");
    lab_evaluation_cards_pair(plan, "expression", "full_expression",
                              lab_evaluation_cards_or(lab_dom_get(data, "display_expression"), compact), full);
    lab_dom_set(plan, "input", kind == LAB_EVALUATION_CARDS_GOAL
                                   ? full
                                   : lab_evaluation_cards_or(lab_dom_get(data, "editor_expression"), full));
    int function = lab_evaluation_cards_field(data, "function");
    lab_evaluation_cards_pair(plan, "function", "full_function",
                              lab_evaluation_cards_or(lab_dom_get(data, "display_function"), function),
                              lab_evaluation_cards_or(lab_dom_get(data, "full_display_function"), function));
    lab_dom_set(plan, "value", lab_evaluation_cards_field(data, "value"));
    int calculus = lab_dom_get(lab_dom_get(data, "presentation"), "calculus");
    lab_dom_set(plan, "derivative", lab_evaluation_cards_field(lab_dom_get(calculus, "derivative"), "expression"));
    int flag = lab_evaluation_cards_or(lab_dom_get(data, "differentiable"), lab_dom_string("yes"));
    lab_dom_set(plan, "differentiable",
                lab_dom_scalar(LAB_VALUE_BOOLEAN,
                               !lab_dom_equal(lab_dom_clean(flag, LAB_DOM_CLEAN_LOWER), lab_dom_string("no"))));
    lab_dom_set(plan, "unchanged",
                lab_dom_scalar(LAB_VALUE_BOOLEAN,
                               lab_dom_equal(lab_dom_clean(expression, LAB_DOM_CLEAN_TRIM),
                                             lab_dom_clean(text, LAB_DOM_CLEAN_TRIM))));
}

static void lab_evaluation_cards_equation(browser_handle plan, browser_handle data, browser_handle text,
                                browser_handle caches, lab_evaluation_card_kind_t kind)
{
    (void)text;
    (void)kind;
    int equation = lab_evaluation_cards_field(data, "equation");
    int full = lab_evaluation_cards_or(lab_dom_get(data, "full_display_equation"), equation);
    int display = lab_evaluation_cards_or(lab_dom_get(data, "display_equation"), equation);
    lab_evaluation_cards_pair(plan, "expression", "full_expression", display, full);
    lab_dom_set(plan, "input", lab_dom_clean(lab_evaluation_cards_or(full, display), LAB_DOM_CLEAN_TRIM));
    int function = lab_evaluation_cards_field(data, "function");
    lab_evaluation_cards_pair(plan, "function", "full_function", function, function);
    lab_dom_set(plan, "value", lab_evaluation_cards_field(lab_dom_get(data, "presentation"), "equation_solution_text"));
    int shown = lab_dom_get(data, "display_TeX"), full_TeX = lab_dom_get(data, "full_display_TeX");
    int expand = lab_dom_truth(shown) && lab_dom_truth(full_TeX) && !lab_dom_equal(shown, full_TeX) &&
                 lab_result_can_expand(lab_dom_get(caches, "expansions"), shown, full_TeX);
    lab_dom_set(plan, "expandable", lab_dom_scalar(LAB_VALUE_BOOLEAN, expand));
}

static void lab_evaluation_cards_diffequation(browser_handle plan, browser_handle data, browser_handle text,
                                browser_handle caches, lab_evaluation_card_kind_t kind)
{
    (void)kind;
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

static void lab_evaluation_cards_matrix(browser_handle plan, browser_handle data, browser_handle text,
                                browser_handle caches, lab_evaluation_card_kind_t kind)
{
    (void)text;
    (void)caches;
    (void)kind;
    lab_dom_set(plan, "scalar",
                lab_dom_truth(lab_dom_get(data, "scalar")) ? lab_evaluation_cards_field(data, "result")
                                                           : lab_dom_string(""));
}

static void lab_evaluation_cards_integrator(browser_handle plan, browser_handle data, browser_handle text,
                                browser_handle caches, lab_evaluation_card_kind_t kind)
{
    (void)text;
    (void)caches;
    (void)kind;
    int expression = lab_evaluation_cards_field(data, "expression");
    lab_evaluation_cards_pair(plan, "expression", "full_expression", expression, expression);
    lab_dom_set(plan, "input", lab_evaluation_cards_field(data, "antiderivative"));
    int metadata = lab_dom_get(lab_dom_get(data, "presentation"), "integrator");
    int details = lab_evaluation_cards_field(metadata, "detail_text");
    lab_evaluation_cards_pair(plan, "function", "full_function", details, details);
    lab_dom_set(plan, "value", lab_evaluation_cards_field(metadata, "value_text"));
}

static void lab_evaluation_cards_datetime(browser_handle plan, browser_handle data, browser_handle text,
                                browser_handle caches, lab_evaluation_card_kind_t kind)
{
    (void)text;
    (void)caches;
    (void)kind;
    static const struct {
        const char *node, *button, *sections, *text;
    } cards[] = {{"#rendered", "", "overview_sections", "overview"},
                 {"#parsed", "#parsedMore", "range_sections", "range"},
                 {"#functionStyle", "#functionMore", "calendar_sections", "calendar"},
                 {"#value", "", "solar_sections", "solar"}};
    int records = lab_dom_object(LAB_VALUE_ARRAY);
    for (unsigned i = 0; i < sizeof cards / sizeof *cards; ++i) {
        unsigned mark = lab_dom_mark();
        int card = lab_dom_object(LAB_VALUE_OBJECT);
        lab_dom_set(card, "element", lab_dom_query(0, cards[i].node));
        lab_dom_set(card, "button", *cards[i].button ? lab_dom_query(0, cards[i].button) : 0);
        lab_dom_set(card, "sections", lab_dom_get(data, cards[i].sections));
        lab_dom_set(card, "text", lab_evaluation_cards_field(data, cards[i].text));
        lab_dom_push(records, card);
        lab_dom_release(mark);
    }
    lab_dom_set(plan, "calendar", records);
}

/* Return one native card plan; kind 7 denotes goal results, otherwise normal worksheet kind indices apply. */
void lab_evaluation_cards(lab_evaluation_card_kind_t kind, browser_handle data, browser_handle text, browser_handle caches)
{
    /**
     * @brief Populate a card plan for the selected presentation kind.
     * @param plan Scoped destination card-plan record.
     * @param data Scoped native evaluation response.
     * @param text Scoped authored source used for fallback selection.
     * @param caches Scoped native-text caches.
     * @param kind Worksheet-compatible presentation kind or LAB_EVALUATION_CARDS_GOAL.
     */
    typedef void (*lab_evaluation_cards_handler_t)(browser_handle plan, browser_handle data, browser_handle text,
                                                  browser_handle caches,
                                                  lab_evaluation_card_kind_t kind);
    static const lab_evaluation_cards_handler_t handlers[] = {
        [LAB_EVALUATION_CARDS_EXPRESSION]            = lab_evaluation_cards_expression,
        [LAB_EVALUATION_CARDS_EQUATION]              = lab_evaluation_cards_equation,
        [LAB_EVALUATION_CARDS_DIFFERENTIAL_EQUATION] = lab_evaluation_cards_diffequation,
        [LAB_EVALUATION_CARDS_MATRIX]                = lab_evaluation_cards_matrix,
        [LAB_EVALUATION_CARDS_INTEGRATOR]            = lab_evaluation_cards_integrator,
        [LAB_EVALUATION_CARDS_DATETIME]              = lab_evaluation_cards_datetime,
        [LAB_EVALUATION_CARDS_ALMANAC]               = 0,
        [LAB_EVALUATION_CARDS_GOAL]                  = lab_evaluation_cards_expression,
    };
    if ((unsigned)kind >= sizeof handlers / sizeof *handlers) {
        lab_dom_return(0);
        return;
    }
    int plan = lab_dom_object(LAB_VALUE_OBJECT), empty = lab_dom_string("");
    static const char *const fields[] = {"expression", "full_expression", "input",
                                         "function",   "full_function",   "value"};
    for (unsigned i = 0; i < sizeof fields / sizeof *fields; ++i)
        lab_dom_set(plan, fields[i], empty);
    if (handlers[kind])
        handlers[kind](plan, data, text, caches, kind);
    lab_dom_return(plan);
}

/* Update notes before deriving value-card visibility from the installed text. */
static void lab_evaluation_cards_present_expression(browser_handle data)
{
    lab_evaluation_notes(data);
    int value = lab_dom_read(lab_dom_query(0, "#value"), LAB_DOM_TEXT, "");
    lab_workspace_dom_value(lab_dom_length(lab_dom_clean(value, LAB_DOM_CLEAN_TRIM)) != 0);
}

/* Clear equation formatting on the differential-equation detail card. */
static void lab_evaluation_cards_present_diffequation(browser_handle data)
{
    (void)data;
    lab_dom_class(lab_dom_query(0, "#functionStyle"), "equation-function", 0);
}

/* Synchronise a supplied matrix operation after updating the value title. */
static void lab_evaluation_cards_present_matrix(browser_handle data)
{
    lab_dom_write(lab_dom_query(0, "#valueTitle"), LAB_DOM_TEXT, "", lab_dom_string("Value"));
    int operation = lab_dom_get(data, "operation"), select = lab_dom_query(0, "#matrixOperation");
    if (select && lab_dom_truth(operation)) {
        lab_dom_write(select, LAB_DOM_VALUE, "", lab_dom_clean(operation, LAB_DOM_CLEAN_TRIM));
        if (!lab_dom_length(lab_dom_read(select, LAB_DOM_VALUE, "")))
            lab_dom_write(select, LAB_DOM_VALUE, "", lab_dom_string("eval"));
        lab_select_sync(select);
        lab_workspace_dom_matrix(LAB_MODE_MATRIX);
    }
}

/* Calendar presentations do not expose a rendered-card expansion control. */
static void lab_evaluation_cards_present_calendar(browser_handle data)
{
    (void)data;
    lab_layout_more(lab_dom_query(0, "#renderedMore"), 0);
}

/* Project mode-specific notes, controls and visibility after the text-card services have run. */
void lab_evaluation_cards_present(lab_mode_t mode, browser_handle data)
{
    /** @brief Optional presentation effects indexed by the worksheet ABI mode. */
    static void (*const handlers[LAB_MODE_COUNT])(browser_handle) = {
        [LAB_MODE_EXPRESSION]            = lab_evaluation_cards_present_expression,
        [LAB_MODE_EQUATION]              = 0,
        [LAB_MODE_DIFFERENTIAL_EQUATION] = lab_evaluation_cards_present_diffequation,
        [LAB_MODE_MATRIX]                = lab_evaluation_cards_present_matrix,
        [LAB_MODE_INTEGRATOR]            = 0,
        [LAB_MODE_DATETIME]              = lab_evaluation_cards_present_calendar,
        [LAB_MODE_ALMANAC]               = lab_evaluation_cards_present_calendar,
    };
    if ((unsigned)mode < sizeof handlers / sizeof *handlers && handlers[mode])
        handlers[mode](data);
}

static browser_handle lab_evaluation_cards_copy(browser_handle source)
{
    int copy = lab_dom_object(LAB_VALUE_OBJECT), keys = lab_dom_keys(source);
    for (unsigned i = 0; i < lab_dom_count(keys); ++i) {
        unsigned mark = lab_dom_mark();
        int key = lab_dom_item(keys, i);
        lab_dom_key_set(copy, key, lab_dom_key_get(source, key));
        lab_dom_release(mark);
    }
    return copy;
}

/* Return the weather request state with native-evaluation date/location precedence and string coercion. */
void lab_evaluation_weather_state(browser_handle state, browser_handle data)
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
void lab_evaluation_weather_cards(browser_handle overview, browser_handle weather)
{
    int result = lab_dom_object(LAB_VALUE_OBJECT), sections = lab_dom_object(LAB_VALUE_ARRAY);
    const int sources[] = {overview, weather};
    int weather_title = lab_dom_string("Weather");
    for (unsigned source = 0; source < 2; ++source) {
        int entries = lab_dom_get(sources[source], "overview_sections");
        if (lab_dom_type(entries) != LAB_VALUE_ARRAY)
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
void lab_evaluation_goal_start(lab_mode_t mode, browser_handle text)
{
    lab_dom_return(0);
    if (mode != LAB_MODE_EXPRESSION || !lab_dom_truth(text))
        return;
    if (lab_dom_has_class(lab_dom_query(0, "#targetRow"), "hidden")) {
        lab_workspace_dom_target(1);
        return;
    }
    lab_workspace_dom_help(LAB_WORKSPACE_SHOW_RESULTS);
    int result = lab_dom_object(LAB_VALUE_OBJECT);
    int target = lab_dom_clean(lab_dom_read(lab_dom_query(0, "#goalTarget"), LAB_DOM_VALUE, ""), LAB_DOM_CLEAN_TRIM);
    lab_dom_set(result, "text", text);
    lab_dom_set(result, "target", lab_evaluation_cards_or(target, lab_dom_string("0")));
    lab_dom_return(result);
}
