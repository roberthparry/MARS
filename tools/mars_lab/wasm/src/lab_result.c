/**
 * @file lab_result.c
 * @brief Native result-card projection, presentation caches and browser copy policy.
 *
 * Installs server-authored text, HTML and SVG without interpreting mathematics.
 * This private browser module selects representations, bounds presentation caches,
 * controls digit expansion and rejects obsolete asynchronous render completions.
 * Browser imports provide generic DOM, object and Map operations. All handles are
 * borrowed within one synchronous call; persistent objects belong to the host.
 * Calendar markup is validated before changing the visible result, allowing the
 * JavaScript request boundary to preserve its diagnostic and recovery behaviour.
 */
#include "lab_dom.h"
#include "lab_binding.h"
#include "lab_layout.h"
#include "lab_requests.h"
#include "lab_view.h"
#include "lab_workspace.h"
#include "lab_result.h"

static browser_jsref lab_result_or(browser_jsref first, browser_jsref second)
{
    return lab_dom_truth(first) ? first : second;
}

static browser_jsref lab_result_field(browser_jsref object, const char *key)
{
    return lab_result_or(lab_dom_get(object, key), lab_dom_string(""));
}

static browser_jsref lab_result_data(node_jsref node, const char *key)
{
    return lab_dom_read(node, LAB_DOM_DATASET, key);
}

static int lab_result_true(browser_jsref value)
{
    return lab_dom_type(value) == LAB_VALUE_BOOLEAN && lab_dom_truth(value);
}

static void lab_result_text(node_jsref node, const char *text)
{
    lab_dom_write(node, LAB_DOM_TEXT, "", lab_dom_string(text));
}

static void lab_result_limit(browser_jsref map, unsigned limit)
{
    while (lab_dom_map_size(map) > limit) {
        unsigned mark = lab_dom_mark();
        lab_dom_map_delete(map, lab_dom_map_first(map));
        lab_dom_release(mark);
    }
}

static void lab_result_editor(browser_jsref cache, browser_jsref editor)
{
    int text = lab_dom_get(editor, "text");
    if (lab_dom_type(text) != LAB_VALUE_STRING)
        return;
    lab_dom_map_set(cache, lab_dom_clean(text, LAB_DOM_CLEAN_TRIM), editor);
    int expression = lab_dom_get(editor, "expression");
    if (lab_dom_type(expression) == LAB_VALUE_STRING)
        lab_dom_map_set(cache, lab_dom_clean(expression, LAB_DOM_CLEAN_TRIM), editor);
    lab_result_limit(cache, 1024);
}

/* Install an editor's exact native aliases with insertion-order eviction. */
void lab_result_editor_install(browser_jsref cache, browser_jsref editor)
{
    lab_result_editor(cache, editor);
}

static void lab_result_entries(browser_jsref cache, browser_jsref entries, const char *key, const char *value, unsigned limit)
{
    if (lab_dom_type(entries) != LAB_VALUE_ARRAY)
        return;
    for (unsigned i = 0; i < lab_dom_count(entries); ++i) {
        unsigned mark = lab_dom_mark();
        int entry = lab_dom_item(entries, i), source = lab_dom_get(entry, key);
        if (lab_dom_type(source) == LAB_VALUE_STRING)
            lab_dom_map_set(cache, source, value ? lab_dom_get(entry, value) : entry);
        lab_dom_release(mark);
    }
    lab_result_limit(cache, limit);
}

/* Cache native presentation metadata; malformed collection types never become iterators. */
void lab_result_presentation_install(browser_jsref caches, browser_jsref data)
{
    int editors = lab_dom_get(caches, "editors");
    lab_result_editor(editors, lab_dom_get(data, "editor"));
    int metadata = lab_dom_get(data, "presentation");
    int expansions = lab_dom_get(caches, "expansions"), entries = lab_dom_get(metadata, "expansions");
    if (lab_dom_type(entries) == LAB_VALUE_ARRAY) {
        for (unsigned i = 0; i < lab_dom_count(entries); ++i) {
            unsigned mark = lab_dom_mark();
            int entry = lab_dom_item(entries, i);
            int display = lab_dom_get(entry, "display"), full = lab_dom_get(entry, "full");
            if (lab_dom_type(display) == LAB_VALUE_STRING && lab_dom_type(full) == LAB_VALUE_STRING) {
                int variants = lab_dom_map_get(expansions, display);
                if (!variants) {
                    variants = lab_dom_map_new();
                    lab_dom_map_set(expansions, display, variants);
                }
                lab_dom_map_set(variants, full, lab_dom_get(entry, "can_expand"));
                lab_result_limit(variants, 16);
            }
            lab_dom_release(mark);
        }
    }
    lab_result_limit(expansions, 1024);
    entries = lab_dom_get(metadata, "editors");
    if (lab_dom_type(entries) == LAB_VALUE_ARRAY) {
        for (unsigned i = 0; i < lab_dom_count(entries); ++i) {
            unsigned mark = lab_dom_mark();
            lab_result_editor(editors, lab_dom_item(entries, i));
            lab_dom_release(mark);
        }
    }
    lab_result_entries(lab_dom_get(caches, "numeric"), lab_dom_get(metadata, "solution_lines"), "line", "numeric",
                       1024);
    lab_result_entries(lab_dom_get(caches, "compact"), lab_dom_get(metadata, "compact_texts"), "text", "display", 1024);
    int source = lab_dom_get(metadata, "solver_text"), TeX = lab_dom_get(metadata, "solver_TeX");
    if (lab_dom_type(source) == LAB_VALUE_STRING && lab_dom_type(TeX) == LAB_VALUE_STRING) {
        int cache = lab_dom_get(caches, "solver");
        lab_dom_map_set(cache, source, TeX);
        lab_result_limit(cache, 64);
    }
}

/* Retain exact-source lexical markup, never a browser-derived classification. */
void lab_result_syntax_install(browser_jsref caches, browser_jsref data)
{
    int metadata = lab_dom_get(data, "presentation");
    lab_result_entries(lab_dom_get(caches, "functions"), lab_dom_get(metadata, "function_syntax"), "source", 0, 64);
    lab_result_entries(lab_dom_get(caches, "headings"), lab_dom_get(metadata, "matrix_headings"), "source", 0, 64);
}

/* Require the native Boolean flag for this exact display/full pair. */
bool lab_result_can_expand(browser_jsref cache, browser_jsref display, browser_jsref full)
{
    int variants = lab_dom_map_get(cache, display);
    return variants && lab_result_true(lab_dom_map_get(variants, full));
}

/* Function classification is supplied by native metadata only. */
bool lab_result_is_function(browser_jsref cache, browser_jsref source)
{
    return lab_result_true(lab_dom_get(lab_dom_map_get(cache, source), "is_function"));
}

/* Reject mismatched markup and safely display the unmodified source as text. */
void lab_result_native_text(browser_jsref element, browser_jsref source, browser_jsref metadata)
{
    int html = lab_dom_get(metadata, "html");
    int use_html = lab_dom_equal(lab_dom_get(metadata, "source"), source) && lab_dom_type(html) == LAB_VALUE_STRING;
    lab_dom_write(element, use_html ? LAB_DOM_HTML : LAB_DOM_TEXT, "", use_html ? html : source);
}

/* Choose Function or matrix metadata for a card; explicit Function/heading choices bypass card identity. */
void lab_result_text_render(browser_jsref caches, browser_jsref element, browser_jsref source, lab_result_text_kind_t kind)
{
    int functions = lab_dom_get(caches, "functions");
    int function = kind == LAB_RESULT_TEXT_FUNCTION ||
                   (kind == LAB_RESULT_TEXT_AUTO && lab_dom_equal(element, lab_dom_query(0, "#functionStyle")) &&
                    lab_result_is_function(functions, source));
    int cache = function ? functions : lab_dom_get(caches, "headings");
    lab_result_native_text(element, source, lab_dom_map_get(cache, source));
}

/* Install both exact representations and reset the native-metadata-controlled expansion button. */
bool lab_result_expandable(browser_jsref caches, browser_jsref element, browser_jsref button, browser_jsref display,
                           browser_jsref full)
{
    lab_dom_delete(element, "labMatrixPresentationRequest");
    lab_result_text_render(caches, element, lab_result_or(display, full), LAB_RESULT_TEXT_AUTO);
    lab_dom_write(element, LAB_DOM_DATASET, "displayText", display);
    lab_dom_write(element, LAB_DOM_DATASET, "fullText", full);
    int enabled = lab_dom_truth(display) && lab_dom_truth(full) && !lab_dom_equal(display, full) &&
                  lab_result_can_expand(lab_dom_get(caches, "expansions"), display, full);
    lab_layout_more(button, enabled);
    return lab_dom_equal(element, lab_dom_query(0, "#functionStyle"));
}

/* Value text clears the explanatory note and retains the complete native numerical source. */
void lab_result_value(browser_jsref caches, browser_jsref full)
{
    lab_result_text(lab_dom_query(0, "#valueNote"), "");
    lab_dom_class(lab_dom_query(0, "#valueNoteCard"), "hidden", 1);
    lab_result_expandable(caches, lab_dom_query(0, "#value"), lab_dom_query(0, "#valueMore"), full, full);
}

/* Validate every populated section before replacing the card, preserving recovery on missing metadata. */
bool lab_result_datetime(browser_jsref element, browser_jsref button, browser_jsref sections, browser_jsref fallback)
{
    unsigned count = lab_dom_type(sections) == LAB_VALUE_ARRAY ? lab_dom_count(sections) : 0;
    for (unsigned i = 0; i < count; ++i) {
        unsigned mark = lab_dom_mark();
        int section = lab_dom_item(sections, i), html = lab_dom_get(section, "html");
        int invalid = lab_dom_count(lab_dom_get(section, "rows")) &&
                      (lab_dom_type(html) != LAB_VALUE_STRING || !lab_dom_length(lab_dom_clean(html, LAB_DOM_CLEAN_TRIM)));
        lab_dom_release(mark);
        if (invalid)
            return false;
    }
    if (lab_dom_equal(element, lab_dom_query(0, "#rendered")))
        lab_result_render_invalidate();
    int text = lab_dom_clean(fallback, LAB_DOM_CLEAN_TRIM);
    lab_dom_write(element, LAB_DOM_TEXT, "", text);
    lab_dom_write(element, LAB_DOM_DATASET, "displayText", text);
    lab_dom_write(element, LAB_DOM_DATASET, "fullText", text);
    lab_layout_more(button, 0);
    int grid = lab_dom_create("div");
    lab_dom_class(grid, "datetime-section-grid", 1);
    /* A scratch object preserves accumulated HTML across released handle scopes. */
    int accumulator = lab_dom_object(LAB_VALUE_OBJECT);
    lab_dom_set(accumulator, "html", lab_dom_string(""));
    for (unsigned i = 0; i < count; ++i) {
        unsigned mark = lab_dom_mark();
        int html = lab_dom_get(lab_dom_item(sections, i), "html");
        if (lab_dom_type(html) == LAB_VALUE_STRING)
            lab_dom_set(accumulator, "html", lab_dom_join(lab_dom_get(accumulator, "html"), html, ""));
        lab_dom_release(mark);
    }
    int html = lab_dom_get(accumulator, "html");
    if (lab_dom_length(html)) {
        lab_dom_write(grid, LAB_DOM_HTML, "", html);
        lab_result_text(element, "");
        lab_dom_append(element, grid);
    }
    return true;
}

/* Use the native pretty expression without interpreting matrix syntax. */
void lab_result_matrix_expression(browser_jsref caches, browser_jsref data)
{
    int full = lab_result_or(lab_dom_get(data, "expression_pretty"),
                             lab_result_or(lab_dom_get(data, "expression"), lab_result_field(data, "result")));
    int display = lab_result_or(lab_dom_get(data, "display_expression_pretty"), full);
    int parsed = lab_dom_query(0, "#parsed");
    lab_dom_class(parsed, "matrix-pretty", 0);
    lab_dom_class(parsed, "matrix-expression-pretty", 0);
    lab_dom_class(parsed, "matrix-expression-text", 1);
    lab_result_expandable(caches, parsed, lab_dom_query(0, "#parsedMore"), display, full);
}

/* Render a matrix value's native SVG, retaining plain text for copying. */
void lab_result_matrix_value(browser_jsref caches, browser_jsref data)
{
    int full = lab_result_field(data, "value"), svg = lab_result_field(data, "value_svg");
    int value = lab_dom_query(0, "#value");
    lab_dom_class(value, "matrix-pretty", 0);
    lab_dom_class(value, "matrix-tex-value", lab_dom_truth(svg));
    lab_result_value(caches, full);
    if (!lab_dom_truth(svg))
        return;
    lab_result_text(value, "");
    int frame = lab_dom_create("span");
    lab_dom_class(frame, "rendered-zoom-frame", 1);
    lab_dom_write(frame, LAB_DOM_HTML, "", svg);
    lab_dom_append(value, frame);
    int card = lab_dom_closest(value, ".result-card");
    if (card)
        lab_dom_schedule(LAB_DOM_SCHEDULE_ZOOM, card);
}

/* Invalidate pending digit renders whenever the owning result changes. */
void lab_result_render_invalidate(void)
{
    lab_dom_delete(lab_dom_query(0, "#rendered"), "labResultRenderRequest");
    lab_dom_disabled(lab_dom_query(0, "#renderedMore"), 0);
}

/* Retire the restored card independently of its expired evaluation request. */
void lab_result_solver_invalidate(void)
{
    lab_dom_delete(lab_dom_query(0, "#functionStyle"), "labSolverRestoreOwner");
}

/* A restored solver card receives a fresh identity in the current mode generation. */
void lab_result_solver_restore(void)
{
    lab_result_solver_invalidate();
    int node = lab_dom_query(0, "#functionStyle");
    if (lab_workspace_mode() != 2 || !lab_dom_has_class(node, "equation-function") ||
        !lab_dom_truth(lab_result_data(node, "solverCompactSvg")))
        return;
    int owner = lab_dom_object(LAB_VALUE_OBJECT);
    lab_dom_set(owner, "context", lab_dom_numeric(lab_request_context()));
    lab_dom_set(node, "labSolverRestoreOwner", owner);
    lab_dom_remove(node, "data-solver-parent-token");
}

/* Return the host identity only while this restored solver still owns rendering. */
void lab_result_solver_owner(void)
{
    int node = lab_dom_query(0, "#functionStyle"), owner = lab_dom_get(node, "labSolverRestoreOwner");
    if (lab_workspace_mode() != 2 || !lab_dom_has_class(node, "equation-function") || !lab_dom_truth(owner) ||
        lab_dom_to_number(lab_dom_get(owner, "context")) != lab_request_context())
        owner = 0;
    lab_dom_return(owner);
}

/* Project a validated result family while preserving metadata getter and rendering order. */
static browser_jsref lab_result_render(browser_jsref caches, browser_jsref data, lab_result_kind_t kind)
{
    /** @brief Native source and optional expansion fields for each supported result family. */
    static const struct {
        const char *source, *display, *full;
        int copy_full;
    } fields[] = {
        [LAB_RESULT_EXPRESSION] = {"tex", "display_TeX", "full_display_TeX", 0},
        [LAB_RESULT_MATRIX]     = {"tex", 0, "full_TeX", 1},
        [LAB_RESULT_CALCULUS]   = {"TeX", 0, 0, 0},
    };
    /** @brief Native wrapped representations and their optional compact fallback. */
    static const struct {
        const char *text, *svg;
        int fallback;
    } wrapped_fields[] = {
        [LAB_RESULT_EXPRESSION] = {"display_wrapped_TeX", "display_wrapped_svg", 1},
        [LAB_RESULT_MATRIX]     = {"display_wrapped_TeX", "display_wrapped_svg", 1},
        [LAB_RESULT_CALCULUS]   = {"wrapped_TeX", "wrapped_svg", 0},
    };
    int node = lab_dom_query(0, "#rendered"), button = lab_dom_query(0, "#renderedMore");
    lab_dom_write(lab_dom_query(0, "#renderedTitle"), LAB_DOM_TEXT, "",
                  lab_result_or(lab_dom_get(data, "render_title"), lab_dom_string("Rendered TeX")));
    int source = lab_result_field(data, fields[kind].source);
    int display = source, full = source;
    if (fields[kind].display)
        display = lab_result_or(lab_dom_get(data, fields[kind].display), source);
    if (fields[kind].full)
        full = lab_result_or(lab_dom_get(data, fields[kind].full), source);
    if (fields[kind].copy_full)
        source = full;
    int svg = lab_result_field(data, "svg"), error = lab_result_field(data, "render_error");
    lab_result_render_invalidate();
    lab_layout_error(0, 0);
    lab_dom_write(node, LAB_DOM_DATASET, "displayTex", display);
    lab_dom_write(node, LAB_DOM_DATASET, "fullTex", full);
    lab_dom_write(node, LAB_DOM_DATASET, "displaySvg", svg);
    lab_dom_write(node, LAB_DOM_DATASET, "fullSvg", lab_dom_string(""));
    lab_dom_write(node, LAB_DOM_DATASET, "fullRenderError", lab_dom_string(""));
    lab_dom_write(node, LAB_DOM_DATASET, "renderError", error);
    int expression = lab_result_or(lab_dom_get(data, "expression"), lab_dom_string("Could not render result"));
    int fallback = lab_result_or(lab_dom_get(data, "display_expression"), expression);
    if (kind != LAB_RESULT_EXPRESSION) {
        int empty = kind == LAB_RESULT_CALCULUS ? lab_result_field(data, "expression")
                                               : lab_dom_string("No rendered TeX available");
        fallback = lab_result_or(error, lab_result_or(display, empty));
    }
    lab_dom_write(node, LAB_DOM_DATASET, "compactTex", display);
    lab_dom_write(node, LAB_DOM_DATASET, "compactSvg", svg);
    int wrapped = wrapped_fields[kind].fallback
                      ? lab_result_or(lab_dom_get(data, wrapped_fields[kind].text), display)
                      : lab_result_field(data, wrapped_fields[kind].text);
    lab_dom_write(node, LAB_DOM_DATASET, "wrappedTex", wrapped);
    lab_dom_write(node, LAB_DOM_DATASET, "wrappedSvg", lab_result_field(data, wrapped_fields[kind].svg));
    lab_dom_write(node, LAB_DOM_DATASET, "responsiveFallback", lab_result_or(error, fallback));
    int responsive = kind == LAB_RESULT_CALCULUS ||
                     lab_result_true(lab_dom_get(lab_dom_get(data, "presentation"), "responsive_fit"));
    lab_dom_write(node, LAB_DOM_DATASET, "responsiveFit", lab_dom_string(responsive ? "true" : "false"));
    lab_dom_class(node, "vertically-wrapped-tex", 0);
    lab_dom_remove(node, "data-responsive-variant");
    lab_layout_content(svg, fallback);
    if (kind != LAB_RESULT_MATRIX)
        lab_dom_schedule(LAB_DOM_SCHEDULE_FIT, 0);
    int expandable = lab_dom_truth(display) && lab_dom_truth(full) && !lab_dom_equal(display, full);
    int allowed = kind == LAB_RESULT_MATRIX ||
                  (kind == LAB_RESULT_EXPRESSION &&
                   lab_result_can_expand(lab_dom_get(caches, "expansions"), display, full));
    lab_layout_more(button, expandable && allowed);
    return source;
}

/* Project Expression (0), Matrix (1) or Calculus (2) rendered metadata and return its copy source. */
void lab_result_rendered(browser_jsref caches, browser_jsref data, lab_result_kind_t kind)
{
    lab_dom_return((unsigned)kind <= LAB_RESULT_CALCULUS ? lab_result_render(caches, data, kind) : 0);
}

static void lab_result_value_visible(int visible)
{
    int card = lab_dom_closest(lab_dom_query(0, "#value"), ".result-card");
    if (!card)
        return;
    if (!visible && lab_view_card_expanded() == lab_dom_card_id(card))
        lab_layout_expand(0, LAB_LAYOUT_EXPANSION_COLLAPSE);
    lab_dom_class(card, "hidden", !visible);
    if (visible)
        lab_dom_remove(card, "hidden");
    else
        lab_dom_write(card, LAB_DOM_ATTRIBUTE, "hidden", lab_dom_string(""));
    lab_dom_write(card, LAB_DOM_STYLE, "display", lab_dom_string(""));
}

static browser_jsref lab_result_bindings_copy(browser_jsref bindings)
{
    int result = lab_dom_object(LAB_VALUE_ARRAY);
    if (lab_dom_type(bindings) != LAB_VALUE_ARRAY)
        return result;
    for (unsigned i = 0; i < lab_dom_count(bindings); ++i) {
        unsigned mark = lab_dom_mark();
        int binding = lab_dom_item(bindings, i), copy = lab_dom_object(LAB_VALUE_OBJECT), keys = lab_dom_keys(binding);
        for (unsigned j = 0; j < lab_dom_count(keys); ++j) {
            unsigned field_mark = lab_dom_mark();
            int key = lab_dom_item(keys, j);
            lab_dom_key_set(copy, key, lab_dom_key_get(binding, key));
            lab_dom_release(field_mark);
        }
        lab_dom_push(result, copy);
        lab_dom_release(mark);
    }
    return result;
}

/* Install complete Matrix (1) or Calculus (2) cards and return the host's legacy state values. */
void lab_result_display(browser_jsref caches, browser_jsref data, lab_result_kind_t kind)
{
    /** @brief Source and function representations for complete result-card families. */
    static const struct {
        const char *expression, *display, *full;
        int full_fallback;
    } fields[] = {
        [LAB_RESULT_MATRIX]   = {"result", "display_function", "full_display_function", 1},
        [LAB_RESULT_CALCULUS] = {"expression", 0, "full_function", 0},
    };
    if (kind != LAB_RESULT_MATRIX && kind != LAB_RESULT_CALCULUS) {
        lab_dom_return(0);
        return;
    }
    int expression = lab_result_field(data, fields[kind].expression);
    if (kind == LAB_RESULT_MATRIX) {
        lab_result_matrix_expression(caches, data);
        lab_result_matrix_value(caches, data);
    } else {
        lab_result_expandable(caches, lab_dom_query(0, "#parsed"), lab_dom_query(0, "#parsedMore"), expression,
                              expression);
        lab_result_value(caches, lab_result_field(data, "value"));
        lab_dom_write(lab_dom_query(0, "#valueTitle"), LAB_DOM_TEXT, "", lab_result_field(data, "value_title"));
    }
    int function = lab_result_field(data, "function");
    int display = fields[kind].display ? lab_result_or(lab_dom_get(data, fields[kind].display), function) : function;
    int full = fields[kind].full_fallback ? lab_result_or(lab_dom_get(data, fields[kind].full), function)
                                        : lab_result_field(data, fields[kind].full);
    lab_result_expandable(caches, lab_dom_query(0, "#functionStyle"), lab_dom_query(0, "#functionMore"), display, full);
    lab_result_input_set(expression);
    lab_result_value_visible(lab_dom_truth(lab_dom_get(data, "value")));
    int state = lab_dom_object(LAB_VALUE_OBJECT);
    lab_dom_set(state, "TeX", lab_result_render(caches, data, kind));
    lab_dom_set(state, "bindings", lab_result_bindings_copy(kind == LAB_RESULT_MATRIX ? lab_dom_get(data, "binding_values") : 0));
    lab_dom_return(state);
}

/* Toggle exact native text representations and the matching accessible label. */
void lab_result_text_digits(browser_jsref caches, browser_jsref element, browser_jsref button)
{
    int expanded = lab_dom_equal(lab_result_data(button, "expanded"), lab_dom_string("true"));
    int text =
        lab_result_or(lab_result_data(element, expanded ? "displayText" : "fullText"), lab_dom_read(element, LAB_DOM_TEXT, ""));
    lab_result_text_render(caches, element, text, LAB_RESULT_TEXT_AUTO);
    lab_result_text(button, expanded ? "Show more digits" : "Show fewer digits");
    lab_dom_write(button, LAB_DOM_DATASET, "expanded", lab_dom_string(expanded ? "false" : "true"));
}

static void lab_result_digits_show(browser_jsref rendered, browser_jsref button, int expanded)
{
    int svg = lab_result_data(rendered, expanded ? "fullSvg" : "displaySvg");
    int fallback = lab_result_data(rendered, expanded ? "fullRenderError" : "renderError");
    if (expanded)
        fallback = lab_result_or(fallback, lab_dom_string("No rendered TeX available"));
    lab_layout_content(svg, fallback);
    lab_result_text(button, expanded ? "Show fewer digits" : "Show more digits");
    lab_dom_write(button, LAB_DOM_DATASET, "expanded", lab_dom_string(expanded ? "true" : "false"));
}

/* Apply a cached toggle, or return an opaque request object for asynchronous SVG fetching. */
void lab_result_digits_begin(browser_jsref last_TeX)
{
    int rendered = lab_dom_query(0, "#rendered"), button = lab_dom_query(0, "#renderedMore");
    lab_dom_return(0);
    if (lab_dom_get(rendered, "labResultRenderRequest"))
        return;
    if (lab_dom_equal(lab_result_data(button, "expanded"), lab_dom_string("true"))) {
        lab_result_digits_show(rendered, button, 0);
        return;
    }
    if (lab_dom_length(lab_result_data(rendered, "fullSvg"))) {
        lab_result_digits_show(rendered, button, 1);
        return;
    }
    int request = lab_dom_object(LAB_VALUE_OBJECT);
    lab_dom_set(request, "TeX", lab_result_or(lab_result_data(rendered, "fullTex"), last_TeX));
    lab_dom_set(rendered, "labResultRenderRequest", request);
    lab_dom_disabled(button, 1);
    lab_dom_return(request);
}

/* Reject stale completions before changing the new card or its pending control state. */
bool lab_result_digits_finish(browser_jsref request, browser_jsref data, browser_jsref error)
{
    int rendered = lab_dom_query(0, "#rendered"), button = lab_dom_query(0, "#renderedMore");
    if (!request || !lab_dom_equal(lab_dom_get(rendered, "labResultRenderRequest"), request))
        return false;
    lab_dom_delete(rendered, "labResultRenderRequest");
    lab_dom_disabled(button, 0);
    lab_dom_write(rendered, LAB_DOM_DATASET, "fullSvg", lab_result_field(data, "svg"));
    lab_dom_write(rendered, LAB_DOM_DATASET, "fullRenderError", lab_result_or(error, lab_result_field(data, "render_error")));
    lab_result_digits_show(rendered, button, 1);
    return true;
}

static browser_jsref lab_result_parsed(void)
{
    int parsed = lab_dom_query(0, "#parsed");
    return lab_dom_clean(
        lab_result_or(lab_result_data(parsed, "fullText"),
                      lab_result_or(lab_result_data(parsed, "displayText"), lab_dom_read(parsed, LAB_DOM_TEXT, ""))),
        LAB_DOM_CLEAN_TRIM);
}

/* Select a copy representation by target; unknown targets and non-HTTP mobile links are empty. */
void lab_result_copy(browser_jsref target, browser_jsref last_TeX)
{
    int text = lab_dom_string("");
    if (lab_dom_equal(target, lab_dom_string("rendered"))) {
        int rendered = lab_dom_query(0, "#rendered");
        text = lab_dom_has_class(rendered, "error") ? lab_dom_read(rendered, LAB_DOM_TEXT, "") : last_TeX;
    } else if (lab_dom_equal(target, lab_dom_string("plot"))) {
        int plot = lab_dom_id("plotRendered");
        text = lab_dom_has_class(plot, "error") ? lab_dom_read(plot, LAB_DOM_TEXT, "") : lab_result_data(plot, "displaySvg");
    } else if (lab_dom_equal(target, lab_dom_string("expression"))) {
        text = lab_result_parsed();
    } else if (lab_dom_equal(target, lab_dom_string("function")) || lab_dom_equal(target, lab_dom_string("value"))) {
        int node = lab_dom_query(0, lab_dom_equal(target, lab_dom_string("function")) ? "#functionStyle" : "#value");
        text = lab_result_or(lab_result_data(node, "fullText"), lab_dom_read(node, LAB_DOM_TEXT, ""));
    } else if (lab_dom_equal(target, lab_dom_string("mobile"))) {
        int url = lab_dom_clean(lab_dom_read(lab_dom_query(0, "#mobileUrl"), LAB_DOM_TEXT, ""), LAB_DOM_CLEAN_TRIM);
        if (lab_dom_starts_with(url, "http://") || lab_dom_starts_with(url, "https://"))
            text = url;
    }
    lab_dom_return(text);
}

/* Project availability of the reusable native expression; bindings remain owned by evaluation. */
void lab_result_input_set(browser_jsref text)
{
    int node = lab_dom_query(0, "#resultUseInput"), trimmed = lab_dom_clean(text, LAB_DOM_CLEAN_TRIM);
    int present = lab_dom_length(trimmed) != 0;
    lab_dom_write(node, LAB_DOM_DATASET, "inputText", trimmed);
    lab_dom_disabled(node, !present);
    lab_dom_class(node, "hidden", !present);
    lab_dom_write(node, LAB_DOM_ATTRIBUTE, "title",
                  lab_dom_string(present ? "Send this result to the input pane" : "No reusable result is available"));
}

/* Equation reuse always follows the displayed result; other modes prefer their explicit source. */
void lab_result_input_get(browser_jsref mode)
{
    int text = lab_result_parsed();
    if (!lab_dom_equal(mode, lab_dom_string("equation")))
        text = lab_result_or(lab_result_data(lab_dom_query(0, "#resultUseInput"), "inputText"), text);
    lab_dom_return(lab_dom_clean(text, LAB_DOM_CLEAN_TRIM));
}

/* Project copy feedback; the host owns only the reset timer. */
void lab_result_copy_flash(browser_jsref button, lab_copy_state_t state)
{
    /** @brief Labels for non-reset feedback; unknown states retain the failure label. */
    static const char *const labels[] = {
        [LAB_COPY_SUCCESS] = "Copied",
        [LAB_COPY_FAILURE] = "Failed",
    };
    int original = lab_result_or(lab_result_data(button, "originalLabel"), lab_dom_read(button, LAB_DOM_TEXT, ""));
    lab_dom_write(button, LAB_DOM_DATASET, "originalLabel", original);
    lab_dom_class(button, "copied", state == LAB_COPY_SUCCESS);
    lab_dom_class(button, "copy-failed", state == LAB_COPY_FAILURE);
    int label = original;
    if (state != LAB_COPY_RESET) {
        unsigned index = (unsigned)state <= LAB_COPY_FAILURE ? (unsigned)state : LAB_COPY_FAILURE;
        label = lab_dom_string(labels[index]);
    }
    lab_dom_write(button, LAB_DOM_TEXT, "", label);
}

/* Begin a native matrix-layout fetch with a host object identity and exact displayed source. */
void lab_result_pretty_begin(browser_jsref caches, browser_jsref element, browser_jsref button,
                             browser_jsref result, browser_jsref pretty)
{
    int text = lab_result_or(pretty, result), request = lab_dom_object(LAB_VALUE_OBJECT);
    lab_dom_set(request, "text", text);
    lab_dom_set(element, "labMatrixPresentationRequest", request);
    lab_dom_class(element, "matrix-pretty", 1);
    lab_dom_write(element, LAB_DOM_DATASET, "displayText", text);
    lab_dom_write(element, LAB_DOM_DATASET, "fullText", text);
    lab_layout_more(button, 0);
    lab_result_text_render(caches, element, text, LAB_RESULT_TEXT_HEADING);
    lab_dom_return(request);
}

/* Install only the matching asynchronous native matrix presentation. */
void lab_result_pretty_finish(browser_jsref element, browser_jsref request, browser_jsref data)
{
    if (!lab_dom_equal(lab_dom_get(element, "labMatrixPresentationRequest"), request) ||
        !lab_dom_has_class(element, "matrix-pretty") ||
        !lab_dom_equal(lab_result_data(element, "fullText"), lab_dom_get(request, "text")))
        return;
    int html = lab_dom_get(data, "html");
    if (lab_dom_type(html) == LAB_VALUE_STRING && lab_dom_length(html))
        lab_dom_write(element, LAB_DOM_HTML, "", html);
}

/* Clear the rendered pane as well as exclusive expansion, leaving request/state recovery to its owner. */
void lab_result_pane_clear(void)
{
    lab_layout_plot_camera(1);
    lab_dom_write(lab_dom_id("plotRendered"), LAB_DOM_TEXT, "", lab_dom_string(""));
    lab_dom_remove(lab_dom_id("plotRendered"), "data-plot-pending");
    lab_dom_class(lab_dom_id("plotCard"), "hidden", 1);
    lab_layout_expand(0, LAB_LAYOUT_EXPANSION_COLLAPSE);
    lab_result_render_invalidate();
    lab_result_text(lab_dom_query(0, "#rendered"), "");
    lab_layout_error(0, 0);
    lab_layout_more(lab_dom_query(0, "#renderedMore"), 0);
}

/* Clear card metadata and pending identities together; no stale copy source survives. */
void lab_result_details_clear(browser_jsref caches, browser_jsref mode)
{
    lab_layout_plot_camera(1);
    lab_dom_write(lab_dom_id("plotRendered"), LAB_DOM_TEXT, "", lab_dom_string(""));
    lab_dom_remove(lab_dom_id("plotRendered"), "data-plot-pending");
    lab_dom_class(lab_dom_id("plotCard"), "hidden", 1);
    lab_result_solver_invalidate();
    static const char *const nodes[] = {"#parsed", "#functionStyle", "#value", "#rendered", "#plotRendered"};
    static const char *const classes[] = {"matrix-pretty", "matrix-expression-pretty", "matrix-expression-text",
                                          "equation-function", "matrix-tex-value"};
    static const char *const attributes[] = {
        "data-full-text",          "data-display-text",       "data-matrix-expression", "data-matrix-display-result",
        "data-matrix-full-result", "data-matrix-pretty",      "data-matrix-bindings",   "data-compact-tex",
        "data-wrapped-tex",        "data-compact-svg",        "data-wrapped-svg",       "data-responsive-fallback",
        "data-responsive-fit",     "data-responsive-variant", "data-display-tex",       "data-full-tex",
        "data-display-svg",        "data-full-svg",           "data-render-error",      "data-full-render-error"};
    lab_result_render_invalidate();
    /* Fixed cards and attribute/class sets make these bounded projection loops deliberate. */
    for (unsigned i = 0; i < sizeof nodes / sizeof *nodes; ++i) {
        unsigned mark = lab_dom_mark();
        int node = lab_dom_query(0, nodes[i]);
        lab_dom_delete(node, "labMatrixPresentationRequest");
        for (unsigned j = 0; j < sizeof classes / sizeof *classes; ++j)
            lab_dom_class(node, classes[j], 0);
        for (unsigned j = 0; j < sizeof attributes / sizeof *attributes; ++j)
            lab_dom_remove(node, attributes[j]);
        if (i < 3)
            lab_result_text(node, "");
        lab_dom_release(mark);
    }
    lab_layout_more(lab_dom_query(0, "#parsedMore"), 0);
    lab_layout_more(lab_dom_query(0, "#functionMore"), 0);
    lab_layout_more(lab_dom_query(0, "#valueMore"), 0);
    lab_result_input_set(lab_dom_string(""));
    lab_result_value(caches, lab_dom_string(""));
    if (lab_dom_equal(mode, lab_dom_string("expression")))
        lab_result_value_visible(0);
}

/* Reset browser-owned references from one native record, preserving the binding-retention policy. */
void lab_result_reset(browser_jsref caches, browser_jsref mode, browser_jsref options)
{
    lab_result_details_clear(caches, mode);
    int state = lab_dom_object(LAB_VALUE_OBJECT), calls = lab_dom_object(LAB_VALUE_ARRAY);
    lab_dom_set(state, "resultInputBindings", lab_dom_object(LAB_VALUE_ARRAY));
    lab_dom_set(state, "lastTex", lab_dom_string(""));
    lab_dom_set(state, "lastDerivativeExpression", lab_dom_string(""));
    lab_dom_set(state, "currentVariables", lab_dom_object(LAB_VALUE_ARRAY));
    lab_dom_set(state, "currentDifferentiable", lab_dom_scalar(LAB_VALUE_BOOLEAN, 1));
    if (!lab_dom_truth(lab_dom_get(options, "keepBindings"))) {
        int call = lab_dom_object(LAB_VALUE_OBJECT);
        lab_dom_set(call, "service", lab_dom_string("clearVariableValues"));
        lab_dom_set(call, "args", lab_dom_object(LAB_VALUE_ARRAY));
        lab_dom_push(calls, call);
    }
    lab_dom_set(state, "calls", calls);
    lab_dom_return(state);
}

/* Each binding is visited once; temporary metadata handles are released before the next record. */
void lab_result_binding_snapshot(browser_jsref bindings)
{
    int copy = lab_dom_object(LAB_VALUE_ARRAY);
    unsigned mark = lab_dom_mark();
    for (unsigned i = 0; i < lab_binding_array_count(bindings); ++i) {
        if (lab_dom_index_present(bindings, i))
            lab_dom_push(copy, lab_binding_copy(lab_dom_item(bindings, i)));
        else {
            lab_dom_push(copy, 0);
            lab_dom_key_delete(copy, lab_dom_numeric(i));
        }
        lab_dom_release(mark);
    }
    lab_dom_return(copy);
}

/* Preserve the strong native almanac HTML guard before copy or worksheet projection. */
bool lab_result_almanac_variant(browser_jsref data, browser_jsref visibility)
{
    int variant = lab_dom_key_get(lab_dom_get(data, "almanac_presentation"), visibility);
    int html = lab_dom_get(variant, "html");
    if (lab_dom_type(html) != LAB_VALUE_STRING || !lab_dom_length(lab_dom_clean(html, LAB_DOM_CLEAN_TRIM))) {
        lab_dom_return(0);
        return false;
    }
    lab_dom_return(variant);
    return true;
}

/* Project only an already validated native worksheet variant. */
void lab_result_almanac_render(browser_jsref target, browser_jsref variant)
{
    if (lab_dom_equal(target, lab_dom_query(0, "#rendered")))
        lab_result_render_invalidate();
    lab_dom_write(target, LAB_DOM_DATASET, "copyText", lab_result_field(variant, "copy_text"));
    lab_dom_write(target, LAB_DOM_HTML, "", lab_result_field(variant, "html"));
}
