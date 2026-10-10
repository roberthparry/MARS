/**
 * @file lab_workspace_dom.c
 * @brief Workspace controls, visibility and editor projection for the C/WASM Lab.
 *
 * Applies mode and precision policy, help selection, result-card visibility,
 * busy-state ownership, conditional editor sizing and calculus action markup.
 * Captures and restores opaque DOM snapshots without interpreting mathematical
 * text. Browser values and nodes are borrowed only within a synchronous call;
 * returned snapshot objects belong to the host and no handles are retained.
 * Result restoration normalises opaque metadata, clones variable lists and
 * projects reusable input before returning globals for calculus-control refresh.
 * Request lifetimes and asynchronous browser callbacks remain with their adapters.
 */
#include "lab_dom.h"
#include "lab_layout.h"
#include "lab_result.h"
#include "lab_view.h"
#include "lab_workspace.h"
#include "lab_workspace_dom.h"

/* Resolve the fixed control catalogue once; browser values outlive only their scoped integer handles. */
void lab_workspace_dom_references(void)
{
    static const struct {
        const char *name, *selector;
        unsigned many;
    } fields[] = {
        {"expr", "expr", 0},
        {"subtitle", "subtitle", 0},
        {"leftPaneTitle", "leftPaneTitle", 0},
        {"matrixControls", "matrixControls", 0},
        {"matrixOperation", "matrixOperation", 0},
        {"matrixOperand", "matrixOperand", 0},
        {"matrixOperandLabel", "matrixOperandLabel", 0},
        {"equationControls", "equationControls", 0},
        {"diffequationControls", "diffequationControls", 0},
        {"integratorControls", "integratorControls", 0},
        {"integratorBoundStack", "integratorBoundStack", 0},
        {"integratorIntervalCap", "integratorIntervalCap", 0},
        {"datetimeControls", "datetimeControls", 0},
        {"datetimeDate", "datetimeDate", 0},
        {"datetimeJdn", "datetimeJdn", 0},
        {"datetimeStart", "datetimeStart", 0},
        {"datetimeYear", "datetimeYear", 0},
        {"datetimeJurisdiction", "datetimeJurisdiction", 0},
        {"datetimeTown", "datetimeTown", 0},
        {"datetimeLatitude", "datetimeLatitude", 0},
        {"datetimeLongitude", "datetimeLongitude", 0},
        {"datetimeGmtOffset", "datetimeGmtOffset", 0},
        {"datetimeLocal", "datetimeLocal", 0},
        {"datetimeLocalBody", "datetimeLocalBody", 0},
        {"almanacControls", "almanacControls", 0},
        {"almanacDate", "almanacDate", 0},
        {"almanacTime", "almanacTime", 0},
        {"almanacZone", "almanacZone", 0},
        {"almanacJurisdiction", "almanacJurisdiction", 0},
        {"almanacTown", "almanacTown", 0},
        {"almanacLatitude", "almanacLatitude", 0},
        {"almanacLongitude", "almanacLongitude", 0},
        {"almanacElevation", "almanacElevation", 0},
        {"marsDatePicker", "marsDatePicker", 0},
        {"marsDatePickerMonth", "marsDatePickerMonth", 0},
        {"marsDatePickerYear", "marsDatePickerYear", 0},
        {"marsDatePickerWeekdays", "marsDatePickerWeekdays", 0},
        {"marsDatePickerGrid", "marsDatePickerGrid", 0},
        {"marsDatePickerToday", "marsDatePickerToday", 0},
        {"marsDatePickerClose", "marsDatePickerClose", 0},
        {"run", "run", 0},
        {"back", "back", 0},
        {"forward", "forward", 0},
        {"help", "help", 0},
        {"goalSeek", "goalSeek", 0},
        {"clear", "clear", 0},
        {"targetRow", "targetRow", 0},
        {"goalTarget", "goalTarget", 0},
        {"lessPrecision", "lessPrecision", 0},
        {"morePrecision", "morePrecision", 0},
        {"derivativeButtons", "derivativeButtons", 0},
        {"variableValues", "variableValues", 0},
        {"mobileAccess", "mobileAccess", 0},
        {"mobileTitle", "mobileTitle", 0},
        {"mobileHint", "mobileHint", 0},
        {"mobileUrl", "mobileUrl", 0},
        {"mobileQr", "mobileQr", 0},
        {"statusEl", "status", 0},
        {"inputCopy", "inputCopy", 0},
        {"labWorkspace", "labWorkspace", 0},
        {"rightPaneTitle", "rightPaneTitle", 0},
        {"resultUseInput", "resultUseInput", 0},
        {"resultPane", "resultPane", 0},
        {"helpPane", "helpPane", 0},
        {"rendered", "rendered", 0},
        {"renderedTitle", "renderedTitle", 0},
        {"renderedMore", "renderedMore", 0},
        {"parsed", "parsed", 0},
        {"parsedMore", "parsedMore", 0},
        {"functionStyle", "functionStyle", 0},
        {"functionTitle", "functionTitle", 0},
        {"functionMore", "functionMore", 0},
        {"functionRun", "functionRun", 0},
        {"functionRunResult", "functionRunResult", 0},
        {"functionRunOutput", "functionRunOutput", 0},
        {"valueCard", "valueCard", 0},
        {"valueNoteCard", "valueNoteCard", 0},
        {"valueNote", "valueNote", 0},
        {"value", "value", 0},
        {"valueTitle", "valueTitle", 0},
        {"valueMore", "valueMore", 0},
        {"labTextareas", "textarea", 1},
        {"modeTabs", ".mode-tab", 1},
        {"helpCards", "#helpPane .help-card", 1},
        {"copyButtons", ".copy-result", 1},
        {"moreDigitButtons", ".more-digits", 1},
        {"resultCards", ".result-card", 1},
    };
    int result = lab_dom_object(5);
    unsigned mark = lab_dom_mark();
    for (unsigned i = 0; i < sizeof fields / sizeof *fields; ++i) {
        int value = fields[i].many ? lab_dom_all(0, fields[i].selector) : lab_dom_id(fields[i].selector);
        if (fields[i].many) {
            int array = lab_dom_object(4);
            unsigned element_mark = lab_dom_mark();
            for (unsigned n = 0; n < lab_dom_count(value); ++n) {
                lab_dom_push(array, lab_dom_item(value, n));
                lab_dom_release(element_mark);
            }
            value = array;
        }
        lab_dom_set(result, fields[i].name, value);
        lab_dom_release(mark);
    }
    lab_dom_return(result);
}

static void lab_workspace_dom_text(int node, unsigned kind, const char *key, const char *text)
{
    lab_dom_write(node, kind, key, lab_dom_string(text));
}

static void lab_workspace_dom_hidden(const char *selector, int hidden)
{
    lab_dom_class(lab_dom_query(0, selector), "hidden", hidden);
}

static int lab_workspace_dom_content(int node, unsigned kind)
{
    return lab_dom_length(lab_dom_clean(lab_dom_read(node, kind, ""), 0)) != 0;
}

/* Format precision from the controller, returning ordinary browser text to legacy callers. */
void lab_workspace_dom_precision(void)
{
    unsigned bits = lab_workspace_requested_precision(lab_workspace_mode());
    int digits = lab_dom_format(lab_workspace_digits(bits), "", " digits / ");
    lab_dom_return(lab_dom_join(digits, lab_dom_format(bits, "", " bits"), ""));
}

/* Copy immutable native labels for callers that inspect the catalogue without projecting it. */
void lab_workspace_dom_labels(unsigned mode)
{
    int labels = lab_dom_object(4);
    for (unsigned i = 0; i < 6; ++i)
        lab_dom_push(labels, lab_dom_string(lab_view_text(mode, i)));
    lab_dom_return(labels);
}

/* Compose a status label without exposing precision formatting policy to the host. */
void lab_workspace_dom_status(int text)
{
    unsigned bits = lab_workspace_requested_precision(lab_workspace_mode());
    int digits = lab_dom_format(lab_workspace_digits(bits), " · ", " digits / ");
    int status = lab_dom_join(text, digits, "");
    lab_dom_write(lab_dom_query(0, "#status"), 0, "", lab_dom_join(status, lab_dom_format(bits, "", " bits"), ""));
}

/* Apply one selected tab and its keyboard/accessibility state to the supplied registration list. */
void lab_workspace_dom_tabs(int tabs, int mode)
{
    for (unsigned i = 0; i < lab_dom_count(tabs); ++i) {
        unsigned mark = lab_dom_mark();
        int tab = lab_dom_item(tabs, i);
        int active = lab_dom_equal(lab_dom_read(tab, 3, "mode"), mode);
        lab_dom_class(tab, "active", active);
        lab_workspace_dom_text(tab, 2, "aria-selected", active ? "true" : "false");
        lab_workspace_dom_text(tab, 2, "tabindex", active ? "0" : "-1");
        lab_dom_release(mark);
    }
}

/* Install the four titles and expose Function execution only for its native representation. */
void lab_workspace_dom_titles(int rendered, int parsed, int function, int value)
{
    lab_dom_write(lab_dom_query(0, "#renderedTitle"), 0, "", rendered);
    lab_dom_write(lab_dom_query(0, "#parsedTitle"), 0, "", parsed);
    lab_dom_write(lab_dom_query(0, "#functionTitle"), 0, "", function);
    lab_dom_write(lab_dom_query(0, "#valueTitle"), 0, "", value);
    lab_workspace_dom_hidden("#functionRun", !lab_dom_equal(function, lab_dom_string("Function")));
}

static void lab_workspace_dom_card(int card, int visible)
{
    if (!card)
        return;
    int id = lab_dom_card_id(card);
    if (!visible && id >= 0 && lab_view_card_expanded() == id)
        lab_layout_expand(0, 2);
    lab_dom_class(card, "hidden", !visible);
}

/* Hiding an auxiliary card relinquishes expansion without changing card zoom. */
void lab_workspace_dom_aux(int visible)
{
    static const char *const selectors[] = {"#parsed", "#functionStyle", "#value"};
    for (unsigned i = 0; i < sizeof selectors / sizeof *selectors; ++i) {
        unsigned mark = lab_dom_mark();
        lab_workspace_dom_card(lab_dom_closest(lab_dom_query(0, selectors[i]), ".result-card"), visible);
        lab_dom_release(mark);
    }
}

/* Keep the hidden attribute and class aligned while allowing expansion CSS to control display. */
void lab_workspace_dom_value(int visible)
{
    int card = lab_dom_query(0, "#valueCard");
    lab_workspace_dom_card(card, visible);
    if (visible)
        lab_dom_remove(card, "hidden");
    else
        lab_workspace_dom_text(card, 2, "hidden", "");
    lab_workspace_dom_text(card, 4, "display", "");
}

/* Project native mode policy and labels; supplied coverage text remains opaque catalogue data. */
void lab_workspace_dom_mode(unsigned mode, int coverage)
{
    if (mode >= 7)
        mode = 0;
    static const char *const panels[] = {"",
                                         "#equationControls",
                                         "#diffequationControls",
                                         "#matrixControls",
                                         "#integratorControls",
                                         "#datetimeControls",
                                         "#almanacControls"};
    static const char *const classes[] = {
        "", "", "diffequation-mode", "matrix-mode", "", "datetime-mode", "almanac-mode"};
    int body = lab_dom_query(0, "body");
    for (unsigned i = 1; i < 7; ++i) {
        lab_workspace_dom_hidden(panels[i], i != mode);
        if (*classes[i])
            lab_dom_class(body, classes[i], i == mode);
    }
    unsigned flags = lab_view_mode_flags(mode, lab_workspace_dom_content(lab_dom_query(0, "#value"), 0));
    lab_workspace_dom_hidden("#datetimeLocal",
                             mode != 5 || !lab_workspace_dom_content(lab_dom_query(0, "#datetimeLocalBody"), 0));
    if (!(flags & 16))
        lab_workspace_dom_hidden("#targetRow", 1);
    lab_workspace_dom_hidden("#derivativeButtons", !(flags & 8));
    lab_workspace_dom_hidden("#goalSeek", !(flags & 16));
    lab_workspace_dom_text(lab_dom_query(0, "#leftPaneTitle"), 0, "", lab_view_text(mode, 0));
    int subtitle = lab_dom_string(lab_view_text(mode, 1));
    lab_dom_write(lab_dom_query(0, "#subtitle"), 0, "", mode == 6 ? lab_dom_join(subtitle, coverage, ".") : subtitle);
    lab_workspace_dom_titles(lab_dom_string(lab_view_text(mode, 2)), lab_dom_string(lab_view_text(mode, 3)),
                             lab_dom_string(lab_view_text(mode, 4)), lab_dom_string(lab_view_text(mode, 5)));
    lab_workspace_dom_aux(!!(flags & 1));
    lab_workspace_dom_value(!!(flags & 2));
}

/* Only matrix solve and multiply display the second operand. */
void lab_workspace_dom_matrix(unsigned mode)
{
    int operation = lab_dom_read(lab_dom_query(0, "#matrixOperation"), 5, "");
    int needed = mode == 3 && (lab_dom_equal(operation, lab_dom_string("solve")) ||
                               lab_dom_equal(operation, lab_dom_string("multiply")));
    lab_workspace_dom_hidden("#matrixOperand", !needed);
    lab_workspace_dom_hidden("#matrixOperandLabel", !needed);
}

/* Help tags are UI metadata, matched exactly after browser Unicode trimming. */
void lab_workspace_dom_help_cards(int cards, int mode)
{
    for (unsigned i = 0; i < lab_dom_count(cards); ++i) {
        unsigned mark = lab_dom_mark();
        int card = lab_dom_item(cards, i);
        int tags = lab_dom_split(lab_dom_read(card, 3, "helpModes"), ",");
        int restricted = 0, matches = 0;
        /* Visit the card's authored tags, retaining only one temporary string at a time. */
        for (unsigned j = 0; j < lab_dom_count(tags); ++j) {
            unsigned token_mark = lab_dom_mark();
            int tag = lab_dom_clean(lab_dom_item(tags, j), 0);
            restricted |= lab_dom_length(tag) != 0;
            matches |= lab_dom_equal(tag, mode);
            lab_dom_release(token_mark);
        }
        lab_dom_class(card, "hidden", restricted && !matches);
        lab_dom_release(mark);
    }
}

/* Show results, show help, or toggle; only explicit help/toggle actions replace status. */
void lab_workspace_dom_help(unsigned action)
{
    if (action > 2)
        return;
    int showing = action == 1 || (action == 2 && lab_dom_has_class(lab_dom_query(0, "#helpPane"), "hidden"));
    lab_workspace_dom_hidden("#resultPane", showing);
    lab_workspace_dom_hidden("#helpPane", !showing);
    lab_workspace_dom_text(lab_dom_query(0, "#rightPaneTitle"), 0, "", showing ? "Help" : "Result");
    int input = lab_dom_query(0, "#resultUseInput");
    lab_dom_class(input, "hidden", showing || !lab_dom_length(lab_dom_read(input, 3, "inputText")));
    lab_workspace_dom_text(lab_dom_query(0, "#help"), 0, "", showing ? "Result" : "Help");
    if (action)
        lab_workspace_dom_status(lab_dom_string(showing ? "Help" : "Ready"));
}

/* Reveal the goal input before queuing browser focus and selection effects. */
void lab_workspace_dom_target(int visible)
{
    lab_workspace_dom_hidden("#targetRow", !visible);
    lab_workspace_dom_text(lab_dom_query(0, "#goalSeek"), 0, "", visible ? "Run goal seek" : "Goal seek");
    if (visible) {
        int target = lab_dom_query(0, "#goalTarget");
        lab_dom_effect(target, 0);
        lab_dom_effect(target, 3);
        lab_workspace_dom_status(lab_dom_string("Enter target"));
    }
}

/* Project the native availability mask and the two explanatory disabled titles. */
void lab_workspace_dom_controls(unsigned mode, int busy, int ready, unsigned back, unsigned forward, int goal,
                                int minimum, int maximum)
{
    unsigned enabled = lab_view_controls(mode, busy, ready, back, forward, goal, minimum, maximum);
    static const char *const selectors[] = {"#run",      "#back",          "#forward",
                                            "#goalSeek", "#lessPrecision", "#morePrecision"};
    for (unsigned i = 0; i < sizeof selectors / sizeof *selectors; ++i)
        lab_dom_disabled(lab_dom_query(0, selectors[i]), !(enabled & (1u << i)));
    lab_workspace_dom_text(lab_dom_query(0, "#goalSeek"), 2, "title",
                           !(enabled & 8) && !busy && mode == 0 ? "Goal seek needs at least one variable binding" : "");
    lab_workspace_dom_text(lab_dom_query(0, "#morePrecision"), 2, "title",
                           !(enabled & 32) && !busy ? "Already at the current maximum precision setting" : "");
}

static void lab_workspace_dom_disable_list(int controls, int busy, int preserve)
{
    for (unsigned i = 0; i < lab_dom_count(controls); ++i) {
        unsigned mark = lab_dom_mark();
        int control = lab_dom_item(controls, i);
        if (!preserve) {
            lab_dom_disabled(control, busy);
        } else if (busy) {
            if (!lab_dom_truth(lab_dom_get(control, "disabled")))
                lab_workspace_dom_text(control, 3, "busyDisabled", "1");
            lab_dom_disabled(control, 1);
        } else if (lab_dom_equal(lab_dom_read(control, 3, "busyDisabled"), lab_dom_string("1"))) {
            lab_dom_disabled(control, 0);
            lab_dom_remove(control, "data-busy-disabled");
        }
        lab_dom_release(mark);
    }
}

/* Repeated busy notifications preserve the initial disabled state of calendar and bounds controls. */
void lab_workspace_dom_busy(int busy, int equation_variable, int copies, int digits)
{
    int retained = lab_dom_all(0, "#integratorBoundStack input, #integratorBoundStack button, "
                                  "#integratorBoundStack select, #datetimeControls input, "
                                  "#datetimeControls button, #datetimeControls select, "
                                  "#almanacControls input, #almanacControls button, #almanacControls select");
    lab_workspace_dom_disable_list(retained, busy, 1);
    int direct = lab_dom_all(0, "#goalTarget, #integratorIntervalCap, #variableValues input, "
                                "#variableValues button, #derivativeButtons button");
    lab_workspace_dom_disable_list(direct, busy, 0);
    lab_dom_disabled(equation_variable, busy);
    lab_workspace_dom_disable_list(copies, busy, 0);
    lab_workspace_dom_disable_list(digits, busy, 0);
}

/* Apply running styling without leaving a false aria-busy attribute behind. */
void lab_workspace_dom_running(int button, int running)
{
    lab_dom_class(button, "action-running", running);
    if (running)
        lab_workspace_dom_text(button, 2, "aria-busy", "true");
    else
        lab_dom_remove(button, "aria-busy");
}

/* Disconnected, collapsed and zero-height editors cannot acquire a manual resize grip. */
int lab_workspace_dom_editor_visible(int editor)
{
    return lab_dom_measure(editor, 12) != 0 && lab_dom_measure(editor, 10) > 0;
}

static void lab_workspace_dom_editor_clear(int editor)
{
    lab_dom_class(editor, "editor-manual-size", 0);
    lab_dom_class(editor, "editor-space-limited", 0);
    lab_workspace_dom_text(editor, 4, "height", "");
    lab_workspace_dom_text(editor, 4, "max-height", "");
    lab_dom_remove(editor, "data-automatic-height");
}

/* Clear only editor sizing state, preserving all other authored inline styles. */
void lab_workspace_dom_editor_reset(int editors)
{
    for (unsigned i = 0; i < lab_dom_count(editors); ++i) {
        unsigned mark = lab_dom_mark();
        lab_workspace_dom_editor_clear(lab_dom_item(editors, i));
        lab_dom_release(mark);
    }
}

/* Measure the registered editors then apply one shared viewport budget to the visible set. */
void lab_workspace_dom_editor_resize(int editors, double viewport)
{
    unsigned visible = 0;
    for (unsigned i = 0; i < lab_dom_count(editors); ++i) {
        unsigned mark = lab_dom_mark();
        visible += lab_workspace_dom_editor_visible(lab_dom_item(editors, i));
        lab_dom_release(mark);
    }
    for (unsigned i = 0; i < lab_dom_count(editors); ++i) {
        unsigned mark = lab_dom_mark();
        int editor = lab_dom_item(editors, i);
        if (!lab_workspace_dom_editor_visible(editor)) {
            lab_workspace_dom_editor_clear(editor);
            lab_dom_release(mark);
            continue;
        }
        int manual = lab_dom_has_class(editor, "editor-manual-size");
        const double *size = lab_view_editor_resize(viewport, visible, lab_dom_measure(editor, 11),
                                                    lab_dom_measure(editor, 10), lab_dom_measure(editor, 4),
                                                    lab_dom_number(lab_dom_read(editor, 3, "automaticHeight")), manual);
        if (size) {
            unsigned flags = (unsigned)size[0];
            if (flags & 1) {
                lab_dom_class(editor, "editor-manual-size", 1);
                lab_dom_class(editor, "editor-space-limited", !!(flags & 2));
                if (!manual) {
                    lab_dom_write(editor, 3, "automaticHeight", lab_dom_format(size[1], "", ""));
                    lab_dom_write(editor, 4, "height", lab_dom_format(size[1], "", "px"));
                }
                lab_dom_write(editor, 4, "max-height", lab_dom_format(size[2], "", "px"));
            } else {
                lab_workspace_dom_editor_clear(editor);
            }
        }
        lab_dom_release(mark);
    }
}

/* Build ordered derivative/integral actions from opaque names and native display labels. */
void lab_workspace_dom_derivatives(int names, int labels, int differentiable)
{
    int panel = lab_dom_query(0, "#derivativeButtons");
    lab_workspace_dom_text(panel, 0, "", "");
    if (!differentiable || !panel)
        return;
    static const char *const suffixes[] = {" derivative", " integral"};
    for (unsigned action = 0; action < 2; ++action) {
        for (unsigned i = 0; i < lab_dom_count(names); ++i) {
            unsigned mark = lab_dom_mark();
            int button = lab_dom_create("button"), label = lab_dom_create("i"), suffix = lab_dom_create("span");
            lab_dom_class(button, "secondary", 1);
            lab_workspace_dom_text(button, 2, "type", "button");
            lab_dom_write(button, 3, "variable", lab_dom_item(names, i));
            lab_dom_write(button, 3, "calculusAction", lab_dom_format(action, "", ""));
            lab_dom_write(label, 0, "", lab_dom_item(labels, i));
            lab_workspace_dom_text(suffix, 0, "", suffixes[action]);
            lab_dom_append(button, label);
            lab_dom_append(button, suffix);
            lab_dom_append(panel, button);
            lab_dom_release(mark);
        }
    }
}

static int lab_workspace_dom_copy_object(int source)
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

static int lab_workspace_dom_capture(int node, int button)
{
    int state = lab_dom_object(5);
    lab_dom_set(state, "className", lab_dom_get(node, "className"));
    if (button) {
        lab_dom_set(state, "textContent", lab_dom_get(node, "textContent"));
        lab_dom_set(state, "disabled", lab_dom_get(node, "disabled"));
    } else {
        lab_dom_set(state, "style", lab_dom_get(lab_dom_get(node, "style"), "cssText"));
        lab_dom_set(state, "innerHTML", lab_dom_get(node, "innerHTML"));
    }
    lab_dom_set(state, "dataset", lab_workspace_dom_copy_object(lab_dom_get(node, "dataset")));
    return state;
}

/* Capture only the presentation fields restored by a mode change, copying the dataset by value. */
void lab_workspace_dom_snapshot(int node, int button)
{
    lab_dom_return(lab_workspace_dom_capture(node, button));
}

/* Remove obsolete dataset entries before restoring the captured, opaque native presentation. */
void lab_workspace_dom_restore(int node, int state, int button)
{
    lab_dom_write(node, 2, "class", lab_dom_get(state, "className"));
    if (button) {
        lab_dom_write(node, 0, "", lab_dom_get(state, "textContent"));
        lab_dom_disabled(node, lab_dom_truth(lab_dom_get(state, "disabled")));
    } else {
        lab_dom_write(node, 2, "style", lab_dom_get(state, "style"));
        lab_dom_write(node, 1, "", lab_dom_get(state, "innerHTML"));
    }
    int dataset = lab_dom_get(node, "dataset"), keys = lab_dom_keys(dataset);
    for (unsigned i = 0; i < lab_dom_count(keys); ++i) {
        unsigned mark = lab_dom_mark();
        lab_dom_key_delete(dataset, lab_dom_item(keys, i));
        lab_dom_release(mark);
    }
    int saved = lab_dom_get(state, "dataset");
    keys = lab_dom_keys(saved);
    for (unsigned i = 0; i < lab_dom_count(keys); ++i) {
        unsigned mark = lab_dom_mark();
        int key = lab_dom_item(keys, i);
        lab_dom_key_set(dataset, key, lab_dom_key_get(saved, key));
        lab_dom_release(mark);
    }
}

/* A native rendered fragment or nonblank text in any auxiliary result is worth retaining. */
int lab_workspace_dom_has_result(void)
{
    static const char *const selectors[] = {"#rendered", "#parsed", "#functionStyle", "#value"};
    for (unsigned i = 0; i < sizeof selectors / sizeof *selectors; ++i)
        if (lab_workspace_dom_content(lab_dom_query(0, selectors[i]), i == 0 ? 1 : 0))
            return 1;
    return 0;
}

static const struct {
    const char *key, *selector;
    int button;
} lab_workspace_dom_result_fields[] = {{"rendered", "#rendered", 0},           {"parsed", "#parsed", 0},
                                       {"functionStyle", "#functionStyle", 0}, {"value", "#value", 0},
                                       {"renderedMore", "#renderedMore", 1},   {"parsedMore", "#parsedMore", 1},
                                       {"functionMore", "#functionMore", 1},   {"valueMore", "#valueMore", 1}};

/* Share the exact metadata defaults between snapshot capture and legacy snapshot restoration. */
static int lab_workspace_dom_result_metadata(int source)
{
    int state = lab_dom_object(5);
    static const char *const text_fields[] = {"lastTex", "lastDerivativeExpression"};
    for (unsigned i = 0; i < sizeof text_fields / sizeof *text_fields; ++i) {
        int value = lab_dom_get(source, text_fields[i]);
        lab_dom_set(state, text_fields[i], lab_dom_truth(value) ? value : lab_dom_string(""));
    }
    int variables = lab_dom_get(source, "currentVariables"), copy = lab_dom_object(4);
    unsigned count = lab_dom_type(variables) == 4 ? lab_dom_count(variables) : 0;
    unsigned mark = lab_dom_mark();
    /* Copy every opaque variable in order; bounded scopes also cover long discovery lists. */
    for (unsigned i = 0; i < count; ++i) {
        lab_dom_push(copy, lab_dom_item(variables, i));
        lab_dom_release(mark);
    }
    lab_dom_set(state, "currentVariables", copy);
    int differentiable = !lab_dom_equal(lab_dom_get(source, "currentDifferentiable"), lab_dom_scalar(1, 0));
    lab_dom_set(state, "currentDifferentiable", lab_dom_scalar(1, differentiable));
    return state;
}

/* Initialise the fixed mode catalogue with independently absent result snapshots. */
void lab_workspace_dom_result_states(void)
{
    static const char *const modes[] = {
        "expression", "equation", "diffequation", "matrix", "integrator", "datetime", "almanac"};
    int states = lab_dom_object(5);
    for (unsigned i = 0; i < sizeof modes / sizeof *modes; ++i)
        lab_dom_set(states, modes[i], 0);
    lab_dom_return(states);
}

/* Assemble a complete mode result snapshot with normalised, independently owned metadata. */
void lab_workspace_dom_result_save(int metadata)
{
    if (!lab_workspace_dom_has_result()) {
        lab_dom_return(0);
        return;
    }
    int state = lab_workspace_dom_result_metadata(metadata);
    for (unsigned i = 0; i < sizeof lab_workspace_dom_result_fields / sizeof *lab_workspace_dom_result_fields; ++i) {
        unsigned mark = lab_dom_mark();
        int node = lab_dom_query(0, lab_workspace_dom_result_fields[i].selector);
        lab_dom_set(state, lab_workspace_dom_result_fields[i].key,
                    lab_workspace_dom_capture(node, lab_workspace_dom_result_fields[i].button));
        lab_dom_release(mark);
    }
    lab_dom_set(state, "resultInputText", lab_dom_read(lab_dom_query(0, "#resultUseInput"), 3, "inputText"));
    lab_dom_return(state);
}

/* Restore cards and reusable input before returning fresh globals for calculus controls and fitting. */
void lab_workspace_dom_result_restore(int state)
{
    lab_result_render_invalidate();
    lab_result_solver_invalidate();
    if (!lab_dom_truth(state)) {
        lab_dom_return(0);
        return;
    }
    lab_layout_expand(0, 2);
    for (unsigned i = 0; i < sizeof lab_workspace_dom_result_fields / sizeof *lab_workspace_dom_result_fields; ++i) {
        unsigned mark = lab_dom_mark();
        int node = lab_dom_query(0, lab_workspace_dom_result_fields[i].selector);
        lab_workspace_dom_restore(node, lab_dom_get(state, lab_workspace_dom_result_fields[i].key),
                                  lab_workspace_dom_result_fields[i].button);
        lab_dom_release(mark);
    }
    lab_result_solver_restore();
    lab_result_input_set(lab_dom_get(state, "resultInputText"));
    int metadata = lab_workspace_dom_result_metadata(state);
    /* Reused result input never inherits the previous mode's evaluation bindings. */
    lab_dom_set(metadata, "resultInputBindings", lab_dom_object(4));
    lab_dom_return(metadata);
}
