/**
 * @file lab_editor.c
 * @brief Exact editor metadata lookup, compact presentation and retained-source selection.
 *
 * Projects opaque values supplied by the native mathematical server. Fallbacks
 * retain the distinction between falsey and nullish metadata, preserve authored
 * whitespace where required and do not infer abbreviated mathematical values.
 * Browser property capabilities retain undefined values and observable getter
 * ordering. Workspace source reads copy their output into scoped strings before
 * any subsequent read can reuse the buffer. All handles expire synchronously.
 */
#include "lab_dom.h"
#include "lab_workspace.h"
#include "lab_editor.h"

static jsref lab_editor_text(jsref value)
{
    return lab_dom_truth(value) ? lab_dom_text(value) : lab_dom_string("");
}

static jsref lab_editor_trim(jsref value)
{
    return lab_dom_clean(lab_editor_text(value), LAB_DOM_CLEAN_TRIM);
}

/* Only complete native source/display keys can resolve metadata. */
jsref lab_editor_lookup(jsref text, jsref editors)
{
    int metadata = lab_dom_map_get(editors, lab_editor_trim(text));
    return lab_dom_truth(metadata) ? metadata : 0;
}

static jsref lab_editor_expression(jsref text, jsref editors)
{
    int value = lab_dom_get(lab_editor_lookup(text, editors), "expression");
    return lab_dom_truth(value) ? value : lab_editor_trim(text);
}

static jsref lab_editor_restore(jsref text, jsref editors)
{
    int source = lab_editor_trim(text);
    int value = lab_dom_get(lab_editor_lookup(source, editors), "text");
    return lab_dom_truth(value) ? value : source;
}

static jsref lab_editor_body(jsref text, jsref editors)
{
    int value = lab_dom_get(lab_editor_lookup(text, editors), "body");
    return value ? value : lab_editor_trim(text);
}

static jsref lab_editor_bindings(jsref text, jsref editors)
{
    int editor = lab_editor_lookup(text, editors);
    return lab_dom_truth(lab_dom_get(editor, "wrapped")) ? editor : 0;
}

static jsref lab_editor_compact(jsref text, jsref editors)
{
    int source = lab_editor_text(text), editor = lab_editor_lookup(source, editors);
    int compact = lab_dom_object(LAB_VALUE_OBJECT);
    if (!editor) {
        lab_dom_set(compact, "display", source);
        lab_dom_set(compact, "bindings", lab_dom_object(LAB_VALUE_ARRAY));
        lab_dom_set(compact, "shortened", lab_dom_scalar(LAB_VALUE_BOOLEAN, 0));
        return compact;
    }
    lab_dom_property_copy(compact, "display", editor, "display");
    int bindings = lab_dom_get(editor, "bindings");
    lab_dom_set(compact, "bindings", lab_dom_truth(bindings) ? bindings : lab_dom_object(LAB_VALUE_ARRAY));
    lab_dom_set(compact, "shortened",
                lab_dom_scalar(LAB_VALUE_BOOLEAN, !lab_dom_properties_equal(editor, "display", editor, "expression")));
    return compact;
}

/* Resolve staged source before accessing other getters that might reuse workspace buffers. */
jsref lab_editor_current_value(view_jsref view)
{
    int raw = lab_dom_get(view, "rawEditor");
    if (!raw)
        return lab_dom_get(view, "expressionText");
    double staged = lab_dom_number(lab_dom_get(raw, "length"));
    int length = staged >= 0 && staged <= lab_workspace_capacity() && staged == (unsigned)staged
                     ? lab_workspace_source_resolve((unsigned)staged, 0)
                     : 0;
    if (length > 0)
        return lab_dom_utf8_string(lab_workspace_output(), (unsigned)length);
    return lab_editor_restore(lab_dom_get(raw, "text"), lab_dom_get(view, "editors"));
}

/* Boxed input values prevent numeric browser values being mistaken for scoped handles. */
void lab_editor_metadata(lab_editor_metadata_t kind, jsref inputs, jsref editors)
{
    static jsref (*const projections[])(jsref, jsref) = {
        [LAB_EDITOR_LOOKUP]     = lab_editor_lookup,
        [LAB_EDITOR_EXPRESSION] = lab_editor_expression,
        [LAB_EDITOR_RESTORE]    = lab_editor_restore,
        [LAB_EDITOR_BODY]       = lab_editor_body,
        [LAB_EDITOR_BINDINGS]   = lab_editor_bindings,
        [LAB_EDITOR_COMPACT]    = lab_editor_compact,
    };
    lab_dom_return((unsigned)kind < sizeof projections / sizeof *projections
                       ? projections[kind](lab_dom_item(inputs, 0), editors) : 0);
}

/* Share the exact same selection policy with setup and installation consumers inside C. */
void lab_editor_current(view_jsref view)
{
    lab_dom_return(lab_editor_current_value(view));
}

/* Structured worksheets do not inspect or validate the mathematical editor. */
void lab_editor_ready(jsref inputs, view_jsref view)
{
    int ready = !lab_dom_equal(lab_dom_item(inputs, 0), lab_dom_string("expression")) ||
                lab_dom_truth(lab_editor_current_value(view));
    lab_dom_return(lab_dom_scalar(LAB_VALUE_BOOLEAN, ready));
}
