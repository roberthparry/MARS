/**
 * @file lab_binding.c
 * @brief Binding and integrator control presentation for the MARS Lab browser.
 *
 * Builds accessible controls from native metadata, orders constants, selects
 * opaque authored values and dispatches browser interactions. Structured row
 * policy lives in lab_binding_rows.c; editor selection and goal-start precedence
 * live in lab_binding_editor.c. No mathematical text is parsed or rewritten.
 * JavaScript supplies asynchronous transport and clipboard promises; native
 * continuations own their stale-response guards. Browser Unicode primitives preserve
 * the existing string and locale semantics. Handles are scoped to one call;
 * control metadata retains actual browser values, never integer handles.
 * Rendering and metadata copying visit every entry with bounded temporary
 * storage. Constant ordering uses a stable merge sort rather than repeated scans.
 */
#include "lab_dom.h"
#include "../include/lab_forms.h"
#include "lab_binding.h"

/* Compare exact browser values with a literal without interpreting binding text. */
int lab_binding_is(int value, const char *text)
{
    return lab_dom_equal(value, lab_dom_string(text));
}

/* Retain the existing truthy-value fallback shared by binding policies. */
int lab_binding_or(int value, const char *fallback)
{
    return lab_dom_truth(value) ? value : lab_dom_string(fallback);
}

static void lab_binding_text(int node, unsigned kind, const char *key, const char *text)
{
    lab_dom_write(node, kind, key, lab_dom_string(text));
}

static int lab_binding_child(int parent, const char *tag, const char *classes)
{
    int node = lab_dom_create(tag);
    lab_binding_text(node, 2, "class", classes);
    lab_dom_append(parent, node);
    return node;
}

static int lab_binding_caption(const char *prefix, int name, const char *suffix)
{
    return lab_dom_join(lab_dom_string(prefix), name, suffix);
}

static int lab_binding_key(int name)
{
    /* Prefix keys so inherited Object properties can never impersonate bindings. */
    return lab_binding_caption("#", lab_dom_text(name), "");
}

/* Return the trimmed opaque name shared by presentation and reconciliation. */
int lab_binding_name(int binding)
{
    return lab_dom_clean(lab_binding_or(lab_dom_get(binding, "name"), ""), 0);
}

static int lab_binding_name_text(int binding)
{
    int name = lab_dom_get(binding, "name");
    return lab_dom_text(lab_dom_truth(name) ? name : lab_binding_or(binding, ""));
}

static int lab_binding_compare_values(int left, int right)
{
    int a = lab_binding_name_text(left), b = lab_binding_name_text(right);
    int compared = lab_dom_locale_compare(a, b, 1);
    return compared ? compared : lab_dom_locale_compare(a, b, 0);
}

/* Preserve numeric, case-insensitive browser ordering with its original tie-break. */
int lab_binding_compare(int arguments)
{
    return lab_binding_compare_values(lab_dom_item(arguments, 0), lab_dom_item(arguments, 1));
}

static int lab_binding_label_value(int name)
{
    int text = lab_dom_text(lab_binding_or(name, ""));
    unsigned length = lab_dom_length(text);
    if (length >= 2 && lab_binding_is(lab_dom_slice(text, 0, 1), "[") &&
        lab_binding_is(lab_dom_slice(text, (int)length - 1, (int)length), "]"))
        return lab_dom_slice(text, 1, (int)length - 1);
    return text;
}

/* Remove only the identifier's outer display brackets, retaining its exact identity elsewhere. */
void lab_binding_label(int arguments)
{
    lab_dom_return(lab_binding_label_value(lab_dom_item(arguments, 0)));
}

static int lab_binding_is_unset(int value)
{
    int text = lab_dom_clean(value, 1);
    return !lab_dom_length(text) || lab_binding_is(text, "?") || lab_binding_is(text, "nan");
}

/* Recognise UI unset sentinels without interpreting mathematical expressions. */
int lab_binding_unset(int arguments)
{
    return lab_binding_is_unset(lab_dom_item(arguments, 0));
}

static int lab_binding_value_text(int binding)
{
    int value = lab_dom_get(binding, "value");
    if (!lab_dom_truth(value))
        value = lab_binding_or(lab_dom_get(binding, "display"), "");
    value = lab_dom_clean(value, 0);
    return lab_binding_is_unset(value) ? lab_dom_string("") : value;
}

/* Select the complete authored value; abbreviations are not reconstructed in the browser. */
void lab_binding_value(int binding)
{
    lab_dom_return(lab_binding_value_text(binding));
}

static int lab_binding_input_value(int input)
{
    return lab_binding_or(lab_dom_clean(lab_dom_read(input, 5, ""), 0), "?");
}

/* Only an empty input becomes a question mark; exact symbolic input remains opaque. */
void lab_binding_normalised(int input)
{
    lab_dom_return(lab_binding_input_value(input));
}

static int lab_binding_input(int input, int trim)
{
    int binding = lab_dom_object(5);
    int name = lab_dom_read(input, 3, "bindingName");
    int kind = lab_binding_or(lab_dom_read(input, 3, "bindingKind"), "variable");
    lab_dom_set(binding, "name", trim ? lab_dom_clean(name, 0) : name);
    lab_dom_set(binding, "kind", trim ? lab_dom_clean(kind, 0) : kind);
    lab_dom_set(binding, "value", lab_binding_input_value(input));
    return binding;
}

/* Capture visible values with the same trimmed names and exact authored expressions. */
void lab_binding_visible(int root)
{
    int result = lab_dom_object(4), inputs = lab_dom_all(root, ".binding-value-input");
    unsigned count = lab_dom_count(inputs), mark = lab_dom_mark();
    for (unsigned i = 0; i < count; ++i) {
        int binding = lab_binding_input(lab_dom_item(inputs, i), 1);
        lab_dom_set(binding, "display", lab_dom_get(binding, "value"));
        if (lab_dom_truth(lab_dom_get(binding, "name")))
            lab_dom_push(result, binding);
        lab_dom_release(mark);
    }
    lab_dom_return(result);
}

/* Copy own metadata fields without allowing identifiers to mutate object prototypes. */
int lab_binding_copy(int source)
{
    int copy = lab_dom_object(5), keys = lab_dom_keys(source);
    unsigned count = lab_dom_count(keys), mark = lab_dom_mark();
    for (unsigned i = 0; i < count; ++i) {
        int key = lab_dom_item(keys, i);
        lab_dom_key_set(copy, key, lab_dom_key_get(source, key));
        lab_dom_release(mark);
    }
    return copy;
}

/* Count only structured arrays for binding and row policy consumers. */
unsigned lab_binding_array_count(int array)
{
    return lab_dom_type(array) == 4 ? lab_dom_count(array) : 0;
}

static void lab_binding_index(int index, int bindings)
{
    unsigned count = lab_dom_type(bindings) == 4 ? lab_dom_count(bindings) : 0, mark = lab_dom_mark();
    for (unsigned i = 0; i < count; ++i) {
        int binding = lab_dom_item(bindings, i);
        lab_dom_key_set(index, lab_binding_key(lab_binding_name(binding)), binding);
        lab_dom_release(mark);
    }
}

/* Overlay visible then native-authored values, preserving discovered metadata and unmatched object identity. */
int lab_binding_authored_values(int bindings, int authored, int visible)
{
    int result = lab_dom_object(4), index = lab_dom_object(5);
    lab_binding_index(index, authored);
    lab_binding_index(index, visible);
    unsigned count = lab_dom_type(bindings) == 4 ? lab_dom_count(bindings) : 0, mark = lab_dom_mark();
    for (unsigned i = 0; i < count; ++i) {
        int binding = lab_dom_item(bindings, i);
        int source = lab_dom_key_get(index, lab_binding_key(lab_binding_name(binding)));
        if (source) {
            binding = lab_binding_copy(binding);
            lab_dom_set(binding, "value", lab_dom_get(source, "value"));
            lab_dom_set(binding, "display", lab_dom_get(source, "display"));
        }
        lab_dom_push(result, binding);
        lab_dom_release(mark);
    }
    return result;
}

/* Publish the shared authored-value overlay for a browser call. */
void lab_binding_authored(int bindings, int authored, int visible)
{
    lab_dom_return(lab_binding_authored_values(bindings, authored, visible));
}

/* Snapshot input identity and untrimmed text for the asynchronous commit ownership checks. */
void lab_binding_snapshot(int inputs)
{
    int result = lab_dom_object(4);
    unsigned count = lab_dom_count(inputs), mark = lab_dom_mark();
    for (unsigned i = 0; i < count; ++i) {
        int input = lab_dom_item(inputs, i), item = lab_dom_object(5);
        lab_dom_set(item, "input", input);
        lab_dom_set(item, "text", lab_dom_read(input, 5, ""));
        lab_dom_set(item, "binding", lab_binding_input(input, 0));
        lab_dom_push(result, item);
        lab_dom_release(mark);
    }
    lab_dom_return(result);
}

/* Recheck exact input bytes after an await; connectivity is deliberately not an additional commit condition. */
int lab_binding_snapshot_current(int snapshot)
{
    unsigned count = lab_dom_count(snapshot), mark = lab_dom_mark();
    for (unsigned i = 0; i < count; ++i) {
        int item = lab_dom_item(snapshot, i);
        int equal = lab_dom_equal(lab_dom_read(lab_dom_get(item, "input"), 5, ""), lab_dom_get(item, "text"));
        lab_dom_release(mark);
        if (!equal)
            return 0;
    }
    return 1;
}

static void lab_binding_pair(int array, int key, int value)
{
    int pair = lab_dom_object(4);
    lab_dom_push(pair, key);
    lab_dom_push(pair, value);
    lab_dom_push(array, pair);
}

static int lab_binding_committed_values(int snapshot)
{
    int result = lab_dom_object(4);
    unsigned count = lab_dom_count(snapshot), mark = lab_dom_mark();
    for (unsigned i = 0; i < count; ++i) {
        int item = lab_dom_item(snapshot, i), input = lab_dom_get(item, "input");
        int binding = lab_dom_get(item, "binding"), value = lab_dom_get(binding, "value");
        int name = lab_dom_get(binding, "name"), unset = lab_binding_is_unset(value);
        lab_dom_write(input, 5, "", unset ? lab_dom_string("") : value);
        lab_dom_write(input, 2, "title", value);
        lab_binding_pair(result, name, unset ? 0 : value);
        lab_dom_release(mark);
    }
    return result;
}

/* Apply an accepted commit and return cache additions/deletions for the shared host state. */
void lab_binding_committed(int snapshot)
{
    lab_dom_return(lab_binding_committed_values(snapshot));
}

/* Normalise every accepted control before updating the exact-name authored cache, as in the host path. */
void lab_binding_committed_cache(int snapshot, int editor, int cache)
{
    lab_dom_write(editor, 3, "bindingRefreshValid", lab_dom_string("true"));
    int values = lab_binding_committed_values(snapshot);
    unsigned count = lab_dom_count(values), mark = lab_dom_mark();
    /* This second pass preserves the all-controls-before-cache boundary, including duplicate names. */
    for (unsigned i = 0; i < count; ++i) {
        int pair = lab_dom_item(values, i), name = lab_dom_item(pair, 0), value = lab_dom_item(pair, 1);
        if (lab_dom_type(value))
            lab_dom_map_set(cache, name, value);
        else
            lab_dom_map_delete(cache, name);
        lab_dom_release(mark);
    }
}

/* Select the requested kind without changing the native binding identity. */
void lab_binding_toggle(int binding)
{
    int result = lab_dom_object(5), name = lab_binding_name(binding);
    int kind = lab_dom_clean(lab_binding_or(lab_dom_get(binding, "kind"), "variable"), 0);
    lab_dom_set(result, "name", name);
    lab_dom_set(result, "nextKind", lab_dom_string(lab_binding_is(kind, "constant") ? "variable" : "constant"));
    lab_dom_return(result);
}

static int lab_binding_sort(int source)
{
    unsigned count = lab_dom_count(source);
    int target = lab_dom_object(4);
    unsigned mark = lab_dom_mark();
    for (unsigned width = 1; width < count; width *= 2) {
        for (unsigned begin = 0; begin < count; begin += width * 2) {
            unsigned middle = begin + width < count ? begin + width : count;
            unsigned end = middle + width < count ? middle + width : count;
            unsigned left = begin, right = middle;
            for (unsigned next = begin; next < end; ++next) {
                int take_left = right == end;
                if (left < middle && right < end)
                    take_left =
                        lab_binding_compare_values(lab_dom_item(source, left), lab_dom_item(source, right)) <= 0;
                unsigned selected = take_left ? left++ : right++;
                lab_dom_key_set(target, lab_dom_format(next, "", ""), lab_dom_item(source, selected));
                lab_dom_release(mark);
            }
        }
        int swap = source;
        source = target;
        target = swap;
    }
    return source;
}

static int lab_binding_button(int parent, const char *classes, const char *caption)
{
    int button = lab_binding_child(parent, "button", classes);
    lab_binding_text(button, 2, "type", "button");
    lab_binding_text(button, 0, "", caption);
    return button;
}

static void lab_binding_card(int root, int binding, int values, int kinds, int index)
{
    int raw_name = lab_dom_get(binding, "name"), label = lab_binding_label_value(raw_name);
    int kind = lab_binding_or(lab_dom_get(binding, "kind"), "variable"), value = lab_binding_value_text(binding);
    int constant = lab_binding_is(kind, "constant");
    lab_binding_pair(kinds, raw_name, kind);
    if (lab_dom_truth(value))
        lab_binding_pair(values, raw_name, value);
    int box = lab_binding_child(root, "div", constant ? "variable-value-box constant-value-box" : "variable-value-box");
    int name =
        lab_binding_child(box, "span", constant ? "variable-value-name constant-value-name" : "variable-value-name");
    lab_dom_write(name, 0, "", label);
    int field = lab_binding_child(box, "div", "binding-value-field");
    int input = lab_binding_child(field, "input", "variable-value-text binding-value-input");
    lab_binding_text(input, 2, "type", "text");
    lab_dom_write(input, 5, "", value);
    lab_dom_write(input, 2, "title", lab_dom_truth(value) ? value : lab_binding_or(lab_dom_get(binding, "value"), "?"));
    lab_dom_write(input, 3, "bindingName", lab_dom_text(raw_name));
    lab_dom_write(input, 3, "bindingKind", lab_dom_text(kind));
    lab_binding_text(input, 2, "autocomplete", "off");
    lab_binding_text(input, 2, "spellcheck", "false");
    lab_binding_text(input, 2, "placeholder", " ");
    lab_dom_write(input, 2, "aria-label", lab_binding_caption("Value of ", label, ""));
    int clear = lab_binding_button(field, "binding-value-clear", "×");
    int clear_label = lab_binding_caption("Clear ", label, "");
    lab_dom_write(clear, 2, "title", clear_label);
    lab_dom_write(clear, 2, "aria-label", clear_label);
    int actions = lab_binding_child(box, "div", "variable-value-actions");
    int toggle = lab_binding_button(actions, "card-action variable-toggle", constant ? "Variable" : "Constant");
    lab_dom_write(toggle, 2, "title",
                  lab_binding_caption("Treat ", label, constant ? " as a variable" : " as a constant"));
    int copy = lab_binding_button(actions, "card-action variable-copy", "Copy");
    int card = lab_dom_object(5);
    lab_dom_set(card, "input", input);
    lab_dom_set(card, "binding", binding);
    lab_dom_set(card, "name", raw_name);
    lab_dom_set(card, "kind", kind);
    lab_dom_set(card, "original", value);
    lab_dom_set(card, "copy", copy);
    lab_dom_set(card, "message", lab_binding_caption("Copied ", label, ""));
    lab_dom_set(box, "labBindingCard", card);
    int name_key = lab_binding_key(raw_name), by_kind = lab_dom_key_get(index, name_key);
    if (!by_kind) {
        by_kind = lab_dom_object(5);
        lab_dom_key_set(index, name_key, by_kind);
    }
    int kind_key = lab_binding_key(kind);
    /* Keep the first matching control, just as the original post-commit focus lookup did. */
    if (!lab_dom_key_get(by_kind, kind_key))
        lab_dom_key_set(by_kind, kind_key, input);
}

/* Clear controls without discarding the separate authored-value cache. */
void lab_binding_clear(int root)
{
    lab_binding_text(root, 0, "", "");
    lab_dom_class(root, "hidden", 1);
    lab_dom_set(root, "labBindingIndex", lab_dom_object(5));
}

/* Build variable cards in discovery order, followed by stably collated constants. */
void lab_binding_render(int root, int bindings)
{
    lab_binding_clear(root);
    int result = lab_dom_object(5), values = lab_dom_object(4), kinds = lab_dom_object(4);
    int variables = lab_dom_object(4), constants = lab_dom_object(4), index = lab_dom_get(root, "labBindingIndex");
    lab_dom_set(result, "values", values);
    lab_dom_set(result, "kinds", kinds);
    unsigned count = lab_dom_count(bindings), mark = lab_dom_mark();
    for (unsigned i = 0; i < count; ++i) {
        int binding = lab_dom_item(bindings, i);
        lab_dom_push(lab_binding_is(lab_dom_get(binding, "kind"), "constant") ? constants : variables, binding);
        lab_dom_release(mark);
    }
    constants = lab_binding_sort(constants);
    mark = lab_dom_mark();
    for (unsigned group = 0; group < 2; ++group) {
        int list = group ? constants : variables;
        unsigned size = lab_dom_count(list);
        for (unsigned i = 0; i < size; ++i) {
            lab_binding_card(root, lab_dom_item(list, i), values, kinds, index);
            lab_dom_release(mark);
        }
    }
    lab_dom_class(root, "hidden", !count);
    lab_dom_return(result);
}

static int lab_binding_action(const char *name, int card, int prevent)
{
    int action = lab_dom_object(5);
    lab_dom_set(action, "action", lab_dom_string(name));
    lab_dom_set(action, "card", card);
    lab_dom_set(action, "prevent", lab_dom_numeric(prevent));
    return action;
}

static int lab_binding_key_event(int event, int input, int original, int card, int evaluate)
{
    int key = lab_dom_get(event, "key");
    if (lab_binding_is(key, "Enter")) {
        if (evaluate && (lab_dom_truth(lab_dom_get(event, "ctrlKey")) || lab_dom_truth(lab_dom_get(event, "metaKey"))))
            return lab_binding_action("evaluate", card, 1);
        lab_dom_effect(input, 4);
        return lab_binding_action("", card, 1);
    }
    if (lab_binding_is(key, "Escape")) {
        lab_dom_write(input, 5, "", original);
        lab_dom_effect(input, 4);
        return lab_binding_action("", card, 1);
    }
    return 0;
}

/* Decide keyboard and click policy; the returned action only schedules host promise work. */
void lab_binding_event(int root, int event)
{
    int target = lab_dom_get(event, "target"), box = lab_dom_closest(target, ".variable-value-box");
    int card = lab_dom_get(box, "labBindingCard"), action = 0;
    if (!card || !lab_dom_equal(lab_dom_get(box, "parentNode"), root)) {
        lab_dom_return(0);
        return;
    }
    int type = lab_dom_get(event, "type"), input = lab_dom_get(card, "input");
    if (lab_dom_equal(target, input)) {
        if (lab_binding_is(type, "keydown"))
            action = lab_binding_key_event(event, input, lab_dom_get(card, "original"), card, 1);
        else if (lab_binding_is(type, "change"))
            action = lab_binding_action("commit", card, 0);
        else if (lab_binding_is(type, "input"))
            action = lab_binding_action("history", card, 0);
    } else if (lab_dom_closest(target, ".binding-value-clear")) {
        if (lab_binding_is(type, "pointerdown"))
            action = lab_binding_action("", card, 1);
        else if (lab_binding_is(type, "click")) {
            lab_binding_text(input, 5, "", "");
            lab_dom_effect(input, 0);
            action = lab_binding_action("clear", card, 0);
        }
    } else if (lab_binding_is(type, "click")) {
        if (lab_dom_closest(target, ".variable-copy"))
            action = lab_binding_action("copy", card, 0);
        else if (lab_dom_closest(target, ".variable-toggle"))
            action = lab_binding_action("toggle", card, 0);
    }
    lab_dom_return(action);
}

/* Restore focus by exact native name and kind after a commit may have replaced all controls. */
void lab_binding_refocus(int root, int card, int editor)
{
    int name = lab_dom_get(card, "name"), kind = lab_dom_get(card, "kind"), input = 0;
    if (lab_dom_type(name) == 3 && lab_dom_type(kind) == 3) {
        int index = lab_dom_get(root, "labBindingIndex");
        int by_kind = lab_dom_key_get(index, lab_binding_key(name));
        input = lab_dom_key_get(by_kind, lab_binding_key(kind));
    }
    lab_dom_effect(input ? input : editor, 0);
}

static void lab_binding_integrator_field(int item, const char *label, int value, const char *key, int bound,
                                         int disabled)
{
    int field = lab_binding_child(item, "div", disabled ? "integrator-bound-field disabled" : "integrator-bound-field");
    lab_dom_write(lab_binding_child(field, "label", ""), 0, "", lab_dom_string(label));
    int input = lab_binding_child(field, "input", "");
    lab_binding_text(input, 2, "spellcheck", "false");
    lab_binding_text(input, 2, "autocomplete", "off");
    lab_dom_write(input, 5, "", value);
    lab_binding_text(input, 2, "placeholder", bound ? "blank for none" : "");
    lab_binding_text(input, 3, key, "1");
    lab_dom_disabled(input, disabled);
    lab_dom_set(input, "labIntegratorOriginal", value);
}

/* Construct integrator rows from sanitised native fields; row-removal policy remains in lab_forms. */
void lab_binding_integrator_render(int root, int rows)
{
    lab_binding_text(root, 0, "", "");
    unsigned count = lab_dom_count(rows), bounds = 0, mark = lab_dom_mark();
    for (unsigned i = 0; i < count; ++i) {
        bounds += !lab_binding_is(lab_dom_get(lab_dom_item(rows, i), "kind"), "free");
        lab_dom_release(mark);
    }
    for (unsigned i = 0; i < count; ++i) {
        int row = lab_dom_item(rows, i), kind = lab_dom_get(row, "kind"), name = lab_dom_get(row, "name");
        int free_row = lab_binding_is(kind, "free");
        int item = lab_binding_child(root, "div", "integrator-bound-row");
        lab_dom_write(item, 3, "kind", kind);
        lab_dom_write(item, 3, "index", lab_dom_format(i, "", ""));
        int toggle = lab_binding_button(item, "card-action integrator-bound-toggle", free_row ? "Bound" : "Free");
        lab_dom_write(
            toggle, 2, "title",
            lab_binding_caption(free_row ? "Integrate with respect to " : "Leave ", name, free_row ? "" : " free"));
        lab_binding_integrator_field(item, "Variable", name, "integratorName", 0, 0);
        lab_binding_integrator_field(item, "Lower bound", lab_dom_get(row, "lo"), "integratorLower", 1, free_row);
        lab_binding_integrator_field(item, "Upper bound", lab_dom_get(row, "hi"), "integratorUpper", 1, free_row);
        int add = lab_binding_button(item, "card-action integrator-bound-add", "+");
        lab_binding_text(add, 2, "title", "Add another integral row");
        int remove = lab_binding_button(item, "card-action integrator-bound-remove", "−");
        lab_binding_text(remove, 2, "title", "Remove this row");
        lab_dom_disabled(remove, !lab_forms_row_removable(free_row, count, bounds));
        lab_dom_release(mark);
    }
}

/* Route integrator keyboard, invalidation and row-edit actions through native policy. */
void lab_binding_integrator_event(int root, int event)
{
    int target = lab_dom_get(event, "target"), item = lab_dom_closest(target, ".integrator-bound-row"), action = 0;
    if (!item || !lab_dom_equal(lab_dom_get(item, "parentNode"), root)) {
        lab_dom_return(0);
        return;
    }
    int type = lab_dom_get(event, "type");
    if (lab_binding_is(lab_dom_get(target, "tagName"), "INPUT")) {
        int card = lab_dom_object(5);
        lab_dom_set(card, "input", target);
        if (lab_binding_is(type, "keydown"))
            action = lab_binding_key_event(event, target, lab_dom_get(target, "labIntegratorOriginal"), card, 0);
        else if (lab_binding_is(type, "input"))
            action = lab_binding_action("invalidate", card, 0);
        else if (lab_binding_is(type, "change"))
            action = lab_binding_action("prepare", card, 0);
    } else if (lab_binding_is(type, "click")) {
        unsigned operation = lab_dom_closest(target, ".integrator-bound-toggle")   ? 1
                             : lab_dom_closest(target, ".integrator-bound-add")    ? 2
                             : lab_dom_closest(target, ".integrator-bound-remove") ? 3
                                                                                   : 0;
        if (operation) {
            action = lab_binding_action("edit", 0, 0);
            lab_dom_set(action, "item", item);
            lab_dom_set(action, "index", lab_dom_numeric(lab_dom_number(lab_dom_read(item, 3, "index"))));
            lab_dom_set(action, "operation", lab_dom_numeric(operation));
        }
    }
    lab_dom_return(action);
}

/* Update inactive prepared fields in place so a blur response cannot discard the next field's focus. */
void lab_binding_integrator_update(int root, int prepared)
{
    static const char *const selectors[] = {"[data-integrator-name]", "[data-integrator-lower]",
                                            "[data-integrator-upper]"};
    static const char *const keys[] = {"name", "lo", "hi"};
    int rows = lab_dom_all(root, ".integrator-bound-row"), active = lab_dom_active();
    unsigned count = lab_dom_count(rows), mark = lab_dom_mark();
    for (unsigned i = 0; i < count; ++i) {
        int row = lab_dom_item(rows, i), normalised = lab_dom_item(prepared, i);
        if (lab_dom_truth(normalised)) {
            for (unsigned j = 0; j < 3; ++j) {
                int field = lab_dom_query(row, selectors[j]), value = lab_dom_get(normalised, keys[j]);
                if (field && !lab_dom_equal(active, field) && !lab_dom_equal(lab_dom_read(field, 5, ""), value))
                    lab_dom_write(field, 5, "", value);
            }
        }
        lab_dom_release(mark);
    }
}
