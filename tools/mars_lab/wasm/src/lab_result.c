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

static int lab_result_or(int first, int second)
{
    return lab_dom_truth(first) ? first : second;
}

static int lab_result_field(int object, const char *key)
{
    return lab_result_or(lab_dom_get(object, key), lab_dom_string(""));
}

static int lab_result_data(int node, const char *key)
{
    return lab_dom_read(node, 3, key);
}

static int lab_result_true(int value)
{
    return lab_dom_type(value) == 1 && lab_dom_truth(value);
}

static void lab_result_text(int node, const char *text)
{
    lab_dom_write(node, 0, "", lab_dom_string(text));
}

static void lab_result_limit(int map, unsigned limit)
{
    while (lab_dom_map_size(map) > limit) {
        unsigned mark = lab_dom_mark();
        lab_dom_map_delete(map, lab_dom_map_first(map));
        lab_dom_release(mark);
    }
}

static void lab_result_editor(int cache, int editor)
{
    int text = lab_dom_get(editor, "text");
    if (lab_dom_type(text) != 3)
        return;
    lab_dom_map_set(cache, lab_dom_clean(text, 0), editor);
    int expression = lab_dom_get(editor, "expression");
    if (lab_dom_type(expression) == 3)
        lab_dom_map_set(cache, lab_dom_clean(expression, 0), editor);
    lab_result_limit(cache, 1024);
}

/* Install an editor's exact native aliases with insertion-order eviction. */
void lab_result_editor_install(int cache, int editor)
{
    lab_result_editor(cache, editor);
}

static void lab_result_entries(int cache, int entries, const char *key, const char *value, unsigned limit)
{
    if (lab_dom_type(entries) != 4)
        return;
    for (unsigned i = 0; i < lab_dom_count(entries); ++i) {
        unsigned mark = lab_dom_mark();
        int entry = lab_dom_item(entries, i), source = lab_dom_get(entry, key);
        if (lab_dom_type(source) == 3)
            lab_dom_map_set(cache, source, value ? lab_dom_get(entry, value) : entry);
        lab_dom_release(mark);
    }
    lab_result_limit(cache, limit);
}

/* Cache native presentation metadata; malformed collection types never become iterators. */
void lab_result_presentation_install(int caches, int data)
{
    int editors = lab_dom_get(caches, "editors");
    lab_result_editor(editors, lab_dom_get(data, "editor"));
    int metadata = lab_dom_get(data, "presentation");
    int expansions = lab_dom_get(caches, "expansions"), entries = lab_dom_get(metadata, "expansions");
    if (lab_dom_type(entries) == 4) {
        for (unsigned i = 0; i < lab_dom_count(entries); ++i) {
            unsigned mark = lab_dom_mark();
            int entry = lab_dom_item(entries, i);
            int display = lab_dom_get(entry, "display"), full = lab_dom_get(entry, "full");
            if (lab_dom_type(display) == 3 && lab_dom_type(full) == 3) {
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
    if (lab_dom_type(entries) == 4) {
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
    if (lab_dom_type(source) == 3 && lab_dom_type(TeX) == 3) {
        int cache = lab_dom_get(caches, "solver");
        lab_dom_map_set(cache, source, TeX);
        lab_result_limit(cache, 64);
    }
}

/* Retain exact-source lexical markup, never a browser-derived classification. */
void lab_result_syntax_install(int caches, int data)
{
    int metadata = lab_dom_get(data, "presentation");
    lab_result_entries(lab_dom_get(caches, "functions"), lab_dom_get(metadata, "function_syntax"), "source", 0, 64);
    lab_result_entries(lab_dom_get(caches, "headings"), lab_dom_get(metadata, "matrix_headings"), "source", 0, 64);
}

/* Require the native Boolean flag for this exact display/full pair. */
int lab_result_can_expand(int cache, int display, int full)
{
    int variants = lab_dom_map_get(cache, display);
    return variants && lab_result_true(lab_dom_map_get(variants, full));
}

/* Function classification is supplied by native metadata only. */
int lab_result_is_function(int cache, int source)
{
    return lab_result_true(lab_dom_get(lab_dom_map_get(cache, source), "is_function"));
}

/* Reject mismatched markup and safely display the unmodified source as text. */
void lab_result_native_text(int element, int source, int metadata)
{
    int html = lab_dom_get(metadata, "html");
    int use_html = lab_dom_equal(lab_dom_get(metadata, "source"), source) && lab_dom_type(html) == 3;
    lab_dom_write(element, use_html ? 1 : 0, "", use_html ? html : source);
}

/* Choose Function or matrix metadata for a card; explicit kind 1/2 bypasses card identity. */
void lab_result_text_render(int caches, int element, int source, unsigned kind)
{
    int functions = lab_dom_get(caches, "functions");
    int function = kind == 1 || (!kind && lab_dom_equal(element, lab_dom_query(0, "#functionStyle")) &&
                                 lab_result_is_function(functions, source));
    int cache = function ? functions : lab_dom_get(caches, "headings");
    lab_result_native_text(element, source, lab_dom_map_get(cache, source));
}

/* Install both exact representations and reset the native-metadata-controlled expansion button. */
int lab_result_expandable(int caches, int element, int button, int display, int full)
{
    lab_dom_delete(element, "labMatrixPresentationRequest");
    lab_result_text_render(caches, element, lab_result_or(display, full), 0);
    lab_dom_write(element, 3, "displayText", display);
    lab_dom_write(element, 3, "fullText", full);
    int enabled = lab_dom_truth(display) && lab_dom_truth(full) && !lab_dom_equal(display, full) &&
                  lab_result_can_expand(lab_dom_get(caches, "expansions"), display, full);
    lab_layout_more(button, enabled);
    return lab_dom_equal(element, lab_dom_query(0, "#functionStyle"));
}

/* Value text clears the explanatory note and retains the complete native numerical source. */
void lab_result_value(int caches, int full)
{
    lab_result_text(lab_dom_query(0, "#valueNote"), "");
    lab_dom_class(lab_dom_query(0, "#valueNoteCard"), "hidden", 1);
    lab_result_expandable(caches, lab_dom_query(0, "#value"), lab_dom_query(0, "#valueMore"), full, full);
}

/* Validate every populated section before replacing the card, preserving recovery on missing metadata. */
int lab_result_datetime(int element, int button, int sections, int fallback)
{
    unsigned count = lab_dom_type(sections) == 4 ? lab_dom_count(sections) : 0;
    for (unsigned i = 0; i < count; ++i) {
        unsigned mark = lab_dom_mark();
        int section = lab_dom_item(sections, i), html = lab_dom_get(section, "html");
        int invalid = lab_dom_count(lab_dom_get(section, "rows")) &&
                      (lab_dom_type(html) != 3 || !lab_dom_length(lab_dom_clean(html, 0)));
        lab_dom_release(mark);
        if (invalid)
            return 0;
    }
    if (lab_dom_equal(element, lab_dom_query(0, "#rendered")))
        lab_result_render_invalidate();
    int text = lab_dom_clean(fallback, 0);
    lab_dom_write(element, 0, "", text);
    lab_dom_write(element, 3, "displayText", text);
    lab_dom_write(element, 3, "fullText", text);
    lab_layout_more(button, 0);
    int grid = lab_dom_create("div");
    lab_dom_class(grid, "datetime-section-grid", 1);
    /* A scratch object preserves accumulated HTML across released handle scopes. */
    int accumulator = lab_dom_object(5);
    lab_dom_set(accumulator, "html", lab_dom_string(""));
    for (unsigned i = 0; i < count; ++i) {
        unsigned mark = lab_dom_mark();
        int html = lab_dom_get(lab_dom_item(sections, i), "html");
        if (lab_dom_type(html) == 3)
            lab_dom_set(accumulator, "html", lab_dom_join(lab_dom_get(accumulator, "html"), html, ""));
        lab_dom_release(mark);
    }
    int html = lab_dom_get(accumulator, "html");
    if (lab_dom_length(html)) {
        lab_dom_write(grid, 1, "", html);
        lab_result_text(element, "");
        lab_dom_append(element, grid);
    }
    return 1;
}

/* Use the native pretty expression without interpreting matrix syntax. */
void lab_result_matrix_expression(int caches, int data)
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
void lab_result_matrix_value(int caches, int data)
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
    lab_dom_write(frame, 1, "", svg);
    lab_dom_append(value, frame);
    int card = lab_dom_closest(value, ".result-card");
    if (card)
        lab_dom_schedule(1, card);
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
    int owner = lab_dom_object(5);
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

static int lab_result_render(int caches, int data, unsigned kind)
{
    int node = lab_dom_query(0, "#rendered"), button = lab_dom_query(0, "#renderedMore");
    int source = lab_result_field(data, kind == 2 ? "TeX" : "tex");
    int display = source, full = source;
    if (!kind) {
        display = lab_result_or(lab_dom_get(data, "display_TeX"), source);
        full = lab_result_or(lab_dom_get(data, "full_display_TeX"), source);
    } else if (kind == 1) {
        full = lab_result_or(lab_dom_get(data, "full_TeX"), source);
        source = full;
    }
    int svg = lab_result_field(data, "svg"), error = lab_result_field(data, "render_error");
    lab_result_render_invalidate();
    lab_layout_error(0, 0);
    lab_dom_write(node, 3, "displayTex", display);
    lab_dom_write(node, 3, "fullTex", full);
    lab_dom_write(node, 3, "displaySvg", svg);
    lab_dom_write(node, 3, "fullSvg", lab_dom_string(""));
    lab_dom_write(node, 3, "fullRenderError", lab_dom_string(""));
    lab_dom_write(node, 3, "renderError", error);
    int expression = lab_result_or(lab_dom_get(data, "expression"), lab_dom_string("Could not render result"));
    int fallback = lab_result_or(lab_dom_get(data, "display_expression"), expression);
    if (kind) {
        int empty = kind == 2 ? lab_result_field(data, "expression") : lab_dom_string("No rendered TeX available");
        fallback = lab_result_or(error, lab_result_or(display, empty));
    }
    lab_dom_write(node, 3, "compactTex", display);
    lab_dom_write(node, 3, "compactSvg", svg);
    int wrapped = kind == 2 ? lab_result_field(data, "wrapped_TeX")
                            : lab_result_or(lab_dom_get(data, "display_wrapped_TeX"), display);
    lab_dom_write(node, 3, "wrappedTex", wrapped);
    lab_dom_write(node, 3, "wrappedSvg", lab_result_field(data, kind == 2 ? "wrapped_svg" : "display_wrapped_svg"));
    lab_dom_write(node, 3, "responsiveFallback", lab_result_or(error, fallback));
    int responsive = kind == 2 || lab_result_true(lab_dom_get(lab_dom_get(data, "presentation"), "responsive_fit"));
    lab_dom_write(node, 3, "responsiveFit", lab_dom_string(responsive ? "true" : "false"));
    lab_dom_class(node, "vertically-wrapped-tex", 0);
    lab_dom_remove(node, "data-responsive-variant");
    lab_layout_content(svg, fallback);
    if (kind != 1)
        lab_dom_schedule(0, 0);
    int expandable = lab_dom_truth(display) && lab_dom_truth(full) && !lab_dom_equal(display, full);
    int allowed = kind == 1 || (!kind && lab_result_can_expand(lab_dom_get(caches, "expansions"), display, full));
    lab_layout_more(button, expandable && allowed);
    return source;
}

/* Project Expression (0), Matrix (1) or Calculus (2) rendered metadata and return its copy source. */
void lab_result_rendered(int caches, int data, unsigned kind)
{
    lab_dom_return(kind <= 2 ? lab_result_render(caches, data, kind) : 0);
}

static void lab_result_value_visible(int visible)
{
    int card = lab_dom_closest(lab_dom_query(0, "#value"), ".result-card");
    if (!card)
        return;
    if (!visible && lab_view_card_expanded() == lab_dom_card_id(card))
        lab_layout_expand(0, 2);
    lab_dom_class(card, "hidden", !visible);
    if (visible)
        lab_dom_remove(card, "hidden");
    else
        lab_dom_write(card, 2, "hidden", lab_dom_string(""));
    lab_dom_write(card, 4, "display", lab_dom_string(""));
}

static int lab_result_bindings_copy(int bindings)
{
    int result = lab_dom_object(4);
    if (lab_dom_type(bindings) != 4)
        return result;
    for (unsigned i = 0; i < lab_dom_count(bindings); ++i) {
        unsigned mark = lab_dom_mark();
        int binding = lab_dom_item(bindings, i), copy = lab_dom_object(5), keys = lab_dom_keys(binding);
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
void lab_result_display(int caches, int data, unsigned kind)
{
    if (kind != 1 && kind != 2) {
        lab_dom_return(0);
        return;
    }
    int expression = lab_result_field(data, kind == 1 ? "result" : "expression");
    if (kind == 1) {
        lab_result_matrix_expression(caches, data);
        lab_result_matrix_value(caches, data);
    } else {
        lab_result_expandable(caches, lab_dom_query(0, "#parsed"), lab_dom_query(0, "#parsedMore"), expression,
                              expression);
        lab_result_value(caches, lab_result_field(data, "value"));
        lab_dom_write(lab_dom_query(0, "#valueTitle"), 0, "", lab_result_field(data, "value_title"));
    }
    int function = lab_result_field(data, "function");
    int display = kind == 1 ? lab_result_or(lab_dom_get(data, "display_function"), function) : function;
    int full = kind == 1 ? lab_result_or(lab_dom_get(data, "full_display_function"), function)
                         : lab_result_field(data, "full_function");
    lab_result_expandable(caches, lab_dom_query(0, "#functionStyle"), lab_dom_query(0, "#functionMore"), display, full);
    lab_result_input_set(expression);
    lab_result_value_visible(lab_dom_truth(lab_dom_get(data, "value")));
    int state = lab_dom_object(5);
    lab_dom_set(state, "TeX", lab_result_render(caches, data, kind));
    lab_dom_set(state, "bindings", lab_result_bindings_copy(kind == 1 ? lab_dom_get(data, "binding_values") : 0));
    lab_dom_return(state);
}

/* Toggle exact native text representations and the matching accessible label. */
void lab_result_text_digits(int caches, int element, int button)
{
    int expanded = lab_dom_equal(lab_result_data(button, "expanded"), lab_dom_string("true"));
    int text =
        lab_result_or(lab_result_data(element, expanded ? "displayText" : "fullText"), lab_dom_read(element, 0, ""));
    lab_result_text_render(caches, element, text, 0);
    lab_result_text(button, expanded ? "Show more digits" : "Show fewer digits");
    lab_dom_write(button, 3, "expanded", lab_dom_string(expanded ? "false" : "true"));
}

static void lab_result_digits_show(int rendered, int button, int expanded)
{
    int svg = lab_result_data(rendered, expanded ? "fullSvg" : "displaySvg");
    int fallback = lab_result_data(rendered, expanded ? "fullRenderError" : "renderError");
    if (expanded)
        fallback = lab_result_or(fallback, lab_dom_string("No rendered TeX available"));
    lab_layout_content(svg, fallback);
    lab_result_text(button, expanded ? "Show fewer digits" : "Show more digits");
    lab_dom_write(button, 3, "expanded", lab_dom_string(expanded ? "true" : "false"));
}

/* Apply a cached toggle, or return an opaque request object for asynchronous SVG fetching. */
void lab_result_digits_begin(int last_TeX)
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
    int request = lab_dom_object(5);
    lab_dom_set(request, "TeX", lab_result_or(lab_result_data(rendered, "fullTex"), last_TeX));
    lab_dom_set(rendered, "labResultRenderRequest", request);
    lab_dom_disabled(button, 1);
    lab_dom_return(request);
}

/* Reject stale completions before changing the new card or its pending control state. */
int lab_result_digits_finish(int request, int data, int error)
{
    int rendered = lab_dom_query(0, "#rendered"), button = lab_dom_query(0, "#renderedMore");
    if (!request || !lab_dom_equal(lab_dom_get(rendered, "labResultRenderRequest"), request))
        return 0;
    lab_dom_delete(rendered, "labResultRenderRequest");
    lab_dom_disabled(button, 0);
    lab_dom_write(rendered, 3, "fullSvg", lab_result_field(data, "svg"));
    lab_dom_write(rendered, 3, "fullRenderError", lab_result_or(error, lab_result_field(data, "render_error")));
    lab_result_digits_show(rendered, button, 1);
    return 1;
}

static int lab_result_parsed(void)
{
    int parsed = lab_dom_query(0, "#parsed");
    return lab_dom_clean(
        lab_result_or(lab_result_data(parsed, "fullText"),
                      lab_result_or(lab_result_data(parsed, "displayText"), lab_dom_read(parsed, 0, ""))),
        0);
}

/* Select a copy representation by target; unknown targets and non-HTTP mobile links are empty. */
void lab_result_copy(int target, int last_TeX)
{
    int text = lab_dom_string("");
    if (lab_dom_equal(target, lab_dom_string("rendered"))) {
        int rendered = lab_dom_query(0, "#rendered");
        text = lab_dom_has_class(rendered, "error") ? lab_dom_read(rendered, 0, "") : last_TeX;
    } else if (lab_dom_equal(target, lab_dom_string("expression"))) {
        text = lab_result_parsed();
    } else if (lab_dom_equal(target, lab_dom_string("function")) || lab_dom_equal(target, lab_dom_string("value"))) {
        int node = lab_dom_query(0, lab_dom_equal(target, lab_dom_string("function")) ? "#functionStyle" : "#value");
        text = lab_result_or(lab_result_data(node, "fullText"), lab_dom_read(node, 0, ""));
    } else if (lab_dom_equal(target, lab_dom_string("mobile"))) {
        int url = lab_dom_clean(lab_dom_read(lab_dom_query(0, "#mobileUrl"), 0, ""), 0);
        if (lab_dom_starts_with(url, "http://") || lab_dom_starts_with(url, "https://"))
            text = url;
    }
    lab_dom_return(text);
}

/* Project availability of the reusable native expression; bindings remain owned by evaluation. */
void lab_result_input_set(int text)
{
    int node = lab_dom_query(0, "#resultUseInput"), trimmed = lab_dom_clean(text, 0);
    int present = lab_dom_length(trimmed) != 0;
    lab_dom_write(node, 3, "inputText", trimmed);
    lab_dom_disabled(node, !present);
    lab_dom_class(node, "hidden", !present);
    lab_dom_write(node, 2, "title",
                  lab_dom_string(present ? "Send this result to the input pane" : "No reusable result is available"));
}

/* Equation reuse always follows the displayed result; other modes prefer their explicit source. */
void lab_result_input_get(int mode)
{
    int text = lab_result_parsed();
    if (!lab_dom_equal(mode, lab_dom_string("equation")))
        text = lab_result_or(lab_result_data(lab_dom_query(0, "#resultUseInput"), "inputText"), text);
    lab_dom_return(lab_dom_clean(text, 0));
}

/* Project copy feedback; the host owns only the reset timer. */
void lab_result_copy_flash(int button, unsigned state)
{
    int original = lab_result_or(lab_result_data(button, "originalLabel"), lab_dom_read(button, 0, ""));
    lab_dom_write(button, 3, "originalLabel", original);
    lab_dom_class(button, "copied", state == 1);
    lab_dom_class(button, "copy-failed", state == 2);
    lab_dom_write(button, 0, "", state ? lab_dom_string(state == 1 ? "Copied" : "Failed") : original);
}

/* Begin a native matrix-layout fetch with a host object identity and exact displayed source. */
void lab_result_pretty_begin(int caches, int element, int button, int result, int pretty)
{
    int text = lab_result_or(pretty, result), request = lab_dom_object(5);
    lab_dom_set(request, "text", text);
    lab_dom_set(element, "labMatrixPresentationRequest", request);
    lab_dom_class(element, "matrix-pretty", 1);
    lab_dom_write(element, 3, "displayText", text);
    lab_dom_write(element, 3, "fullText", text);
    lab_layout_more(button, 0);
    lab_result_text_render(caches, element, text, 2);
    lab_dom_return(request);
}

/* Install only the matching asynchronous native matrix presentation. */
void lab_result_pretty_finish(int element, int request, int data)
{
    if (!lab_dom_equal(lab_dom_get(element, "labMatrixPresentationRequest"), request) ||
        !lab_dom_has_class(element, "matrix-pretty") ||
        !lab_dom_equal(lab_result_data(element, "fullText"), lab_dom_get(request, "text")))
        return;
    int html = lab_dom_get(data, "html");
    if (lab_dom_type(html) == 3 && lab_dom_length(html))
        lab_dom_write(element, 1, "", html);
}

/* Clear the rendered pane as well as exclusive expansion, leaving request/state recovery to its owner. */
void lab_result_pane_clear(void)
{
    lab_layout_expand(0, 2);
    lab_result_render_invalidate();
    lab_result_text(lab_dom_query(0, "#rendered"), "");
    lab_layout_error(0, 0);
    lab_layout_more(lab_dom_query(0, "#renderedMore"), 0);
}

/* Clear card metadata and pending identities together; no stale copy source survives. */
void lab_result_details_clear(int caches, int mode)
{
    lab_result_solver_invalidate();
    static const char *const nodes[] = {"#parsed", "#functionStyle", "#value", "#rendered"};
    static const char *const classes[] = {"matrix-pretty", "matrix-expression-pretty", "matrix-expression-text",
                                          "equation-function", "matrix-tex-value"};
    static const char *const attributes[] = {
        "data-full-text",          "data-display-text",       "data-matrix-expression", "data-matrix-display-result",
        "data-matrix-full-result", "data-matrix-pretty",      "data-matrix-bindings",   "data-compact-tex",
        "data-wrapped-tex",        "data-compact-svg",        "data-wrapped-svg",       "data-responsive-fallback",
        "data-responsive-fit",     "data-responsive-variant", "data-display-tex",       "data-full-tex",
        "data-display-svg",        "data-full-svg",           "data-render-error",      "data-full-render-error"};
    lab_result_render_invalidate();
    /* Four cards and fixed attribute/class sets make these bounded projection loops deliberate. */
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
void lab_result_reset(int caches, int mode, int options)
{
    lab_result_details_clear(caches, mode);
    int state = lab_dom_object(5), calls = lab_dom_object(4);
    lab_dom_set(state, "resultInputBindings", lab_dom_object(4));
    lab_dom_set(state, "lastTex", lab_dom_string(""));
    lab_dom_set(state, "lastDerivativeExpression", lab_dom_string(""));
    lab_dom_set(state, "currentVariables", lab_dom_object(4));
    lab_dom_set(state, "currentDifferentiable", lab_dom_scalar(1, 1));
    if (!lab_dom_truth(lab_dom_get(options, "keepBindings"))) {
        int call = lab_dom_object(5);
        lab_dom_set(call, "service", lab_dom_string("clearVariableValues"));
        lab_dom_set(call, "args", lab_dom_object(4));
        lab_dom_push(calls, call);
    }
    lab_dom_set(state, "calls", calls);
    lab_dom_return(state);
}

/* Each binding is visited once; temporary metadata handles are released before the next record. */
void lab_result_binding_snapshot(int bindings)
{
    int copy = lab_dom_object(4);
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
int lab_result_almanac_variant(int data, int visibility)
{
    int variant = lab_dom_key_get(lab_dom_get(data, "almanac_presentation"), visibility);
    int html = lab_dom_get(variant, "html");
    if (lab_dom_type(html) != 3 || !lab_dom_length(lab_dom_clean(html, 0))) {
        lab_dom_return(0);
        return 0;
    }
    lab_dom_return(variant);
    return 1;
}

/* Project only an already validated native worksheet variant. */
void lab_result_almanac_render(int target, int variant)
{
    if (lab_dom_equal(target, lab_dom_query(0, "#rendered")))
        lab_result_render_invalidate();
    lab_dom_write(target, 3, "copyText", lab_result_field(variant, "copy_text"));
    lab_dom_write(target, 1, "", lab_result_field(variant, "html"));
}
