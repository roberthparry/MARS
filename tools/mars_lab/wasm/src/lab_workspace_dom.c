/**
 * @file lab_workspace_dom.c
 * @brief Workspace controls, visibility and editor projection for the C/WASM Lab.
 *
 * Applies mode and precision policy, help selection, result-card visibility,
 * busy-state ownership, conditional editor sizing and calculus action markup.
 * Calculus labels use the shared binding formatter, with temporary handles released per name.
 * Captures and restores opaque DOM snapshots without interpreting mathematical
 * text. Browser values and nodes are borrowed only within a synchronous call;
 * returned snapshot objects belong to the host and no handles are retained.
 * Decoded snapshot validation preserves state identity and uses the supplied mode catalogue.
 * Result restoration normalises opaque metadata, clones variable lists and
 * projects reusable input before returning globals for calculus-control refresh.
 * Request lifetimes and asynchronous browser callbacks remain with their adapters.
 */
#include "lab_mode.h"
#include "lab_dom.h"
#include "lab_binding.h"
#include "lab_layout.h"
#include "lab_plot_controls.h"
#include "lab_plot_slices.h"
#include "lab_precision.h"
#include "lab_result.h"
#include "lab_view.h"
#include "lab_workspace.h"
#include "lab_workspace_dom.h"

/** @brief Calculus action stored on each generated variable button. */
typedef enum {
    LAB_WORKSPACE_DERIVATIVE = 0, /**< Differentiate with respect to the button's variable. */
    LAB_WORKSPACE_INTEGRAL = 1    /**< Integrate with respect to the button's variable. */
} lab_workspace_calculus_action_t;

/* Validate decoded data without cloning state or substituting the application's default mode catalogue. */
void lab_workspace_snapshot_validate(jsref envelope, jsref mode_names)
{
    lab_value_kind_t kind = lab_dom_type(envelope);
    if ((kind != LAB_VALUE_ARRAY && kind != LAB_VALUE_OBJECT) || !lab_dom_property_present(envelope, "state")) {
        lab_dom_return(lab_dom_numeric(-1));
        return;
    }
    int state = lab_dom_get(envelope, "state");
    if (!state) {
        /* Property reads collapse null and undefined handles; compare the original property instead. */
        int reference = lab_dom_object(LAB_VALUE_OBJECT);
        lab_dom_set(reference, "state", 0);
        lab_dom_return(lab_dom_properties_equal(envelope, "state", reference, "state") ? 0 : lab_dom_numeric(-2));
        return;
    }
    kind = lab_dom_type(state);
    if ((kind != LAB_VALUE_ARRAY && kind != LAB_VALUE_OBJECT) || !lab_dom_property_present(state, "mode") ||
        !lab_dom_property_present(state, "text")) {
        lab_dom_return(lab_dom_numeric(-2));
        return;
    }
    int mode = lab_dom_get(state, "mode");
    unsigned count = lab_dom_type(mode_names) == LAB_VALUE_ARRAY ? lab_dom_count(mode_names) : 0;
    if (lab_dom_type(mode) != LAB_VALUE_STRING || lab_dom_type(lab_dom_get(state, "text")) != LAB_VALUE_STRING || count > 7) {
        lab_dom_return(lab_dom_numeric(-2));
        return;
    }
    /* The caller supplies at most seven names; exact membership requires no fixed native mode mapping. */
    unsigned mark = lab_dom_mark();
    for (unsigned i = 0; i < count; ++i) {
        int matches = lab_dom_equal(mode, lab_dom_item(mode_names, i));
        lab_dom_release(mark);
        if (matches) {
            lab_dom_return(state);
            return;
        }
    }
    lab_dom_return(lab_dom_numeric(-2));
}

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
        {"precisionSlider", "precisionSlider", 0},
        {"precisionValue", "precisionValue", 0},
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
    int result = lab_dom_object(LAB_VALUE_OBJECT);
    unsigned mark = lab_dom_mark();
    for (unsigned i = 0; i < sizeof fields / sizeof *fields; ++i) {
        int value = fields[i].many ? lab_dom_all(0, fields[i].selector) : lab_dom_id(fields[i].selector);
        if (fields[i].many) {
            int array = lab_dom_object(LAB_VALUE_ARRAY);
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

static void lab_workspace_dom_text(node_jsref node, lab_dom_accessor_t kind, const char *key, const char *text)
{
    lab_dom_write(node, kind, key, lab_dom_string(text));
}

static void lab_workspace_dom_hidden(const char *selector, int hidden)
{
    lab_dom_class(lab_dom_query(0, selector), "hidden", hidden);
}

static int lab_workspace_dom_content(node_jsref node, lab_dom_accessor_t kind)
{
    return lab_dom_length(lab_dom_clean(lab_dom_read(node, kind, ""), LAB_DOM_CLEAN_TRIM)) != 0;
}

/* Format precision from the controller, returning ordinary browser text to legacy callers. */
void lab_workspace_dom_precision(void)
{
    unsigned bits = lab_workspace_requested_precision(lab_workspace_mode());
    int digits = lab_dom_format(lab_workspace_digits(bits), "", " digits / ");
    lab_dom_return(lab_dom_join(digits, lab_dom_format(bits, "", " bits"), ""));
}

/* Copy immutable native labels for callers that inspect the catalogue without projecting it. */
void lab_workspace_dom_labels(lab_mode_t mode)
{
    int labels = lab_dom_object(LAB_VALUE_ARRAY);
    for (lab_view_label_t field = LAB_VIEW_TITLE; field <= LAB_VIEW_VALUE; ++field)
        lab_dom_push(labels, lab_dom_string(lab_view_text(mode, field)));
    lab_dom_return(labels);
}

static void lab_workspace_dom_status_for_mode(lab_mode_t mode, jsref text, bool sky_ready)
{
    if (mode == LAB_MODE_DATETIME || mode == LAB_MODE_ALMANAC) {
        int status = lab_dom_query(0, "#status");
        if (sky_ready) {
            int source = lab_dom_query(0, "#homeSky");
            int html = lab_dom_read(source, LAB_DOM_HTML, "");
            if (lab_dom_length(html)) {
                lab_dom_write(status, LAB_DOM_HTML, "", html);
                return;
            }
        }
        lab_dom_write(status, LAB_DOM_TEXT, "", text);
        return;
    }
    unsigned bits = lab_workspace_requested_precision(mode);
    int digits = lab_dom_format(lab_workspace_digits(bits), " · ", " digits / ");
    int status = lab_dom_join(text, digits, "");
    lab_dom_write(lab_dom_query(0, "#status"), LAB_DOM_TEXT, "", lab_dom_join(status, lab_dom_format(bits, "", " bits"), ""));
}

/* Compose status only for the current mode; calendar presentation cannot leak into mathematics. */
void lab_workspace_dom_status(jsref text)
{
    lab_workspace_dom_status_for_mode(lab_workspace_mode(), text, true);
}

/* Apply one selected tab and its keyboard/accessibility state to the supplied registration list. */
void lab_workspace_dom_tabs(jsref tabs, jsref mode)
{
    for (unsigned i = 0; i < lab_dom_count(tabs); ++i) {
        unsigned mark = lab_dom_mark();
        int tab = lab_dom_item(tabs, i);
        int active = lab_dom_equal(lab_dom_read(tab, LAB_DOM_DATASET, "mode"), mode);
        lab_dom_class(tab, "active", active);
        lab_workspace_dom_text(tab, LAB_DOM_ATTRIBUTE, "aria-selected", active ? "true" : "false");
        lab_workspace_dom_text(tab, LAB_DOM_ATTRIBUTE, "tabindex", active ? "0" : "-1");
        lab_dom_release(mark);
    }
}

/* Install the four titles and expose Function execution only for its native representation. */
void lab_workspace_dom_titles(jsref rendered, jsref parsed, jsref function, jsref value)
{
    lab_dom_write(lab_dom_query(0, "#renderedTitle"), LAB_DOM_TEXT, "", rendered);
    lab_dom_write(lab_dom_query(0, "#parsedTitle"), LAB_DOM_TEXT, "", parsed);
    lab_dom_write(lab_dom_query(0, "#functionTitle"), LAB_DOM_TEXT, "", function);
    lab_dom_write(lab_dom_query(0, "#valueTitle"), LAB_DOM_TEXT, "", value);
    lab_workspace_dom_hidden("#functionRun", !lab_dom_equal(function, lab_dom_string("Function")));
}

static void lab_workspace_dom_card(jsref card, int visible)
{
    if (!card)
        return;
    int id = lab_dom_card_id(card);
    if (!visible && id >= 0 && lab_view_card_expanded() == id)
        lab_layout_expand(0, LAB_LAYOUT_EXPANSION_COLLAPSE);
    lab_dom_class(card, "hidden", !visible);
}

/* Hiding an auxiliary card relinquishes expansion without changing card zoom. */
void lab_workspace_dom_aux(bool visible)
{
    static const char *const selectors[] = {"#parsed", "#functionStyle", "#value"};
    for (unsigned i = 0; i < sizeof selectors / sizeof *selectors; ++i) {
        unsigned mark = lab_dom_mark();
        lab_workspace_dom_card(lab_dom_closest(lab_dom_query(0, selectors[i]), ".result-card"), visible);
        lab_dom_release(mark);
    }
}

/* Keep the hidden attribute and class aligned while allowing expansion CSS to control display. */
void lab_workspace_dom_value(bool visible)
{
    int card = lab_dom_query(0, "#valueCard");
    lab_workspace_dom_card(card, visible);
    if (visible)
        lab_dom_remove(card, "hidden");
    else
        lab_workspace_dom_text(card, LAB_DOM_ATTRIBUTE, "hidden", "");
    lab_workspace_dom_text(card, LAB_DOM_STYLE, "display", "");
}

/* Project native mode policy and labels; supplied coverage text remains opaque catalogue data. */
void lab_workspace_dom_mode(lab_mode_t mode, jsref coverage)
{
    lab_precision_close(0);
    if ((unsigned)mode >= LAB_MODE_COUNT)
        mode = LAB_MODE_EXPRESSION;
    int status = lab_dom_query(0, "#status"), status_mode = lab_dom_format(mode, "", "");
    if (!lab_dom_equal(lab_dom_read(status, LAB_DOM_DATASET, "workspaceMode"), status_mode)) {
        lab_dom_write(status, LAB_DOM_DATASET, "workspaceMode", status_mode);
        lab_workspace_dom_status_for_mode(mode, lab_dom_string("Ready"), true);
    }
    lab_workspace_dom_hidden(".precision-toolbar", mode == LAB_MODE_DATETIME || mode == LAB_MODE_ALMANAC);
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
    for (lab_mode_t panel = LAB_MODE_EQUATION; panel < LAB_MODE_COUNT; ++panel) {
        lab_workspace_dom_hidden(panels[panel], panel != mode);
        if (*classes[panel])
            lab_dom_class(body, classes[panel], panel == mode);
    }
    /* Move the original controls, preserving their listeners, focusability and native enablement policy. */
    int actions = lab_dom_id("workspaceActions");
    int home = lab_dom_id(mode == LAB_MODE_ALMANAC ? "almanacActions" : "workspaceActionsHome");
    if (actions && home && !lab_dom_equal(lab_dom_get(actions, "parentElement"), home))
        lab_dom_append(home, actions);
    unsigned flags = lab_view_mode_flags(mode, lab_workspace_dom_content(lab_dom_query(0, "#value"), LAB_DOM_TEXT));
    lab_workspace_dom_hidden(
        "#datetimeLocal",
        mode != LAB_MODE_DATETIME || !lab_workspace_dom_content(lab_dom_query(0, "#datetimeLocalBody"), LAB_DOM_TEXT));
    if (!(flags & 16))
        lab_workspace_dom_hidden("#targetRow", 1);
    lab_workspace_dom_hidden("#derivativeButtons",
                             !(flags & 8) && mode != LAB_MODE_EQUATION && mode != LAB_MODE_DIFFERENTIAL_EQUATION);
    lab_workspace_dom_hidden("#goalSeek", !(flags & 16));
    lab_workspace_dom_hidden("#run", mode == LAB_MODE_ALMANAC);
    lab_workspace_dom_hidden("#clear", mode == LAB_MODE_ALMANAC);
    lab_workspace_dom_hidden("#plotRange", mode > LAB_MODE_DIFFERENTIAL_EQUATION && mode != LAB_MODE_INTEGRATOR);
    lab_workspace_dom_hidden("#plotCard", (mode > LAB_MODE_DIFFERENTIAL_EQUATION && mode != LAB_MODE_INTEGRATOR) ||
                             !lab_workspace_dom_content(lab_dom_query(0, "#plotRendered"), LAB_DOM_TEXT));
    lab_workspace_dom_text(lab_dom_query(0, "#leftPaneTitle"), LAB_DOM_TEXT, "", lab_view_text(mode, LAB_VIEW_TITLE));
    int showing_help = lab_dom_has_class(lab_dom_query(0, "#labWorkspace"), "help-open");
    lab_workspace_dom_text(lab_dom_query(0, "#rightPaneTitle"), LAB_DOM_TEXT, "", showing_help ? "Help" : "Result");
    int subtitle = lab_dom_string(lab_view_text(mode, LAB_VIEW_SUBTITLE));
    lab_dom_write(lab_dom_query(0, "#subtitle"), LAB_DOM_TEXT, "",
                  mode == LAB_MODE_ALMANAC ? lab_dom_join(subtitle, coverage, ".") : subtitle);
    lab_workspace_dom_titles(lab_dom_string(lab_view_text(mode, LAB_VIEW_RENDERED)),
                             lab_dom_string(lab_view_text(mode, LAB_VIEW_PARSED)),
                             lab_dom_string(lab_view_text(mode, LAB_VIEW_FUNCTION)),
                             lab_dom_string(lab_view_text(mode, LAB_VIEW_VALUE)));
    lab_workspace_dom_aux(!!(flags & 1));
    lab_workspace_dom_value(!!(flags & 2));
}

/* Only matrix solve and multiply display the second operand. */
void lab_workspace_dom_matrix(lab_mode_t mode)
{
    int operation = lab_dom_read(lab_dom_query(0, "#matrixOperation"), LAB_DOM_VALUE, "");
    int needed = mode == LAB_MODE_MATRIX && (lab_dom_equal(operation, lab_dom_string("solve")) ||
                               lab_dom_equal(operation, lab_dom_string("multiply")));
    lab_workspace_dom_hidden("#matrixOperand", !needed);
    lab_workspace_dom_hidden("#matrixOperandLabel", !needed);
}

/* Help tags are UI metadata, matched exactly after browser Unicode trimming. */
void lab_workspace_dom_help_cards(jsref cards, jsref mode)
{
    for (unsigned i = 0; i < lab_dom_count(cards); ++i) {
        unsigned mark = lab_dom_mark();
        int card = lab_dom_item(cards, i);
        int tags = lab_dom_split(lab_dom_read(card, LAB_DOM_DATASET, "helpModes"), ",");
        int restricted = 0, matches = 0;
        /* Visit the card's authored tags, retaining only one temporary string at a time. */
        for (unsigned j = 0; j < lab_dom_count(tags); ++j) {
            unsigned token_mark = lab_dom_mark();
            int tag = lab_dom_clean(lab_dom_item(tags, j), LAB_DOM_CLEAN_TRIM);
            restricted |= lab_dom_length(tag) != 0;
            matches |= lab_dom_equal(tag, mode);
            lab_dom_release(token_mark);
        }
        lab_dom_class(card, "hidden", restricted && !matches);
        lab_dom_release(mark);
    }
}

/* Full-width help preserves the worksheet; its visible return action owns keyboard focus. */
void lab_workspace_dom_help(lab_workspace_help_action_t action)
{
    if ((unsigned)action > LAB_WORKSPACE_TOGGLE_HELP)
        return;
    int was_showing = !lab_dom_has_class(lab_dom_query(0, "#helpPane"), "hidden");
    int showing = action == LAB_WORKSPACE_SHOW_HELP || (action == LAB_WORKSPACE_TOGGLE_HELP && !was_showing);
    if (showing && lab_view_card_expanded() >= 0)
        lab_layout_expand(0, LAB_LAYOUT_EXPANSION_COLLAPSE);
    lab_dom_class(lab_dom_query(0, "#labWorkspace"), "help-open", showing);
    lab_workspace_dom_hidden("#resultPane", showing);
    lab_workspace_dom_hidden("#helpPane", !showing);
    lab_workspace_dom_hidden("#helpClose", !showing);
    lab_workspace_dom_text(lab_dom_query(0, "#rightPaneTitle"), LAB_DOM_TEXT, "", showing ? "Help" : "Result");
    int input = lab_dom_query(0, "#resultUseInput");
    lab_dom_class(input, "hidden", showing || !lab_dom_length(lab_dom_read(input, LAB_DOM_DATASET, "inputText")));
    lab_workspace_dom_text(lab_dom_query(0, "#help"), LAB_DOM_TEXT, "", showing ? "Result" : "Help");
    lab_workspace_dom_text(lab_dom_query(0, "#help"), LAB_DOM_ATTRIBUTE, "aria-expanded", showing ? "true" : "false");
    if (showing != was_showing)
        lab_dom_effect(lab_dom_query(0, showing ? "#helpClose" : "#help"), LAB_DOM_EFFECT_FOCUS);
    if (action != LAB_WORKSPACE_SHOW_RESULTS)
        lab_workspace_dom_status(lab_dom_string(showing ? "Help" : "Ready"));
}

/* Reveal the goal input before queuing browser focus and selection effects. */
void lab_workspace_dom_target(bool visible)
{
    lab_workspace_dom_hidden("#targetRow", !visible);
    lab_workspace_dom_text(lab_dom_query(0, "#goalSeek"), LAB_DOM_TEXT, "", visible ? "Run goal seek" : "Goal seek");
    if (visible) {
        int target = lab_dom_query(0, "#goalTarget");
        lab_dom_effect(target, LAB_DOM_EFFECT_FOCUS);
        lab_dom_effect(target, LAB_DOM_EFFECT_SELECT);
        lab_workspace_dom_status(lab_dom_string("Enter target"));
    }
}

/* Preview without assigning value: range inputs must retain their native pending change event. */
void lab_workspace_dom_precision_preview(unsigned bits)
{
    int slider = lab_dom_query(0, "#precisionSlider");
    int digits = lab_dom_format(lab_workspace_digits(bits), bits > 6656 ? "Custom: " : "", " digits");
    int description = lab_dom_join(digits, lab_dom_format(bits, " / ", " bits"), "");
    lab_dom_write(slider, LAB_DOM_ATTRIBUTE, "aria-valuetext", description);
    lab_dom_write(slider, LAB_DOM_ATTRIBUTE, "title", description);
    lab_dom_write(lab_dom_query(0, "#precisionValue"), LAB_DOM_TEXT, "", digits);
}

/* Refresh the range position only outside an active input preview. */
void lab_workspace_dom_precision_slider(unsigned bits)
{
    int slider = lab_dom_query(0, "#precisionSlider");
    lab_dom_write(slider, LAB_DOM_ATTRIBUTE, "min", lab_dom_string("0"));
    lab_dom_write(slider, LAB_DOM_ATTRIBUTE, "max", lab_dom_string("103"));
    lab_dom_write(slider, LAB_DOM_ATTRIBUTE, "step", lab_dom_string("1"));
    lab_dom_write(slider, LAB_DOM_VALUE, "", lab_dom_text(lab_dom_numeric(lab_workspace_precision_slider_position(bits))));
    lab_workspace_dom_precision_preview(bits);
}

/* Project native availability and refresh the slider from this mode's saved precision. */
void lab_workspace_dom_controls(lab_mode_t mode, bool busy, bool ready, unsigned back, unsigned forward, bool goal,
                                bool minimum, bool maximum)
{
    unsigned enabled = lab_view_controls(mode, busy, ready, back, forward, goal, minimum, maximum);
    static const char *const selectors[] = {"#run", "#back", "#forward", "#goalSeek"};
    for (unsigned i = 0; i < sizeof selectors / sizeof *selectors; ++i)
        lab_dom_disabled(lab_dom_query(0, selectors[i]), !(enabled & (1u << i)));
    lab_workspace_dom_text(lab_dom_query(0, "#goalSeek"), LAB_DOM_ATTRIBUTE, "title",
                           !(enabled & 8) && !busy && mode == LAB_MODE_EXPRESSION
                               ? "Goal seek needs at least one variable binding"
                               : "");
    lab_dom_disabled(lab_dom_query(0, "#precisionSlider"), busy);
    lab_dom_disabled(lab_dom_id("precisionCustomToggle"), busy);
    if (busy)
        lab_precision_close(0);
    lab_workspace_dom_precision_slider(lab_workspace_requested_precision(mode));
}

static void lab_workspace_dom_disable_list(jsref controls, int busy, int preserve)
{
    static const char *const locations[] = {
        "#datetimeJurisdiction", "#datetimeTown", "#almanacJurisdiction", "#almanacTown"
    };
    jsref shells[4] = {0};
    if (preserve)
        for (unsigned i = 0; i < 4; ++i)
            shells[i] = lab_dom_closest(lab_dom_query(0, locations[i]), ".select-shell");
    for (unsigned i = 0; i < lab_dom_count(controls); ++i) {
        unsigned mark = lab_dom_mark();
        int control = lab_dom_item(controls, i);
        /* Location choices supersede asynchronous calculations, including custom menu search and options. */
        int shell = preserve ? lab_dom_closest(control, ".select-shell") : 0;
        int location = 0;
        /* Four identity comparisons avoid scanning all descendants for every town row. */
        for (unsigned j = 0; shell && j < 4; ++j)
            location |= lab_dom_equal(shell, shells[j]);
        if (location || (preserve && lab_dom_closest(control,
                "#datetimeJurisdiction, #datetimeTown, #almanacJurisdiction, #almanacTown"))) {
            lab_dom_release(mark);
            continue;
        }
        if (!preserve) {
            lab_dom_disabled(control, busy);
        } else if (busy) {
            if (!lab_dom_truth(lab_dom_get(control, "disabled")))
                lab_workspace_dom_text(control, LAB_DOM_DATASET, "busyDisabled", "1");
            lab_dom_disabled(control, 1);
        } else if (lab_dom_equal(lab_dom_read(control, LAB_DOM_DATASET, "busyDisabled"), lab_dom_string("1"))) {
            lab_dom_disabled(control, 0);
            lab_dom_remove(control, "data-busy-disabled");
        }
        lab_dom_release(mark);
    }
}

/* Repeated busy notifications preserve the initial disabled state of calendar and bounds controls. */
void lab_workspace_dom_busy(bool busy, jsref equation_variable, jsref copies, jsref digits)
{
    lab_dom_class(lab_dom_id("worksheetPending"), "hidden", !busy);
    static const char *const retained[] = {"#integratorBoundStack", "#datetimeControls", "#almanacControls"};
    /* Scope each bounded control group so large result SVGs are never traversed. */
    for (unsigned i = 0; i < sizeof retained / sizeof *retained; ++i) {
        unsigned mark = lab_dom_mark();
        int parent = lab_dom_query(0, retained[i]);
        if (parent)
            /* Globe selections remain available; shared actions retain their separate native enablement policy. */
            lab_workspace_dom_disable_list(
                lab_dom_all(parent, "input, select, button:not(#almanacGlobe button):not(#workspaceActions button)"),
                busy, 1);
        lab_dom_release(mark);
    }
    lab_dom_disabled(lab_dom_query(0, "#goalTarget"), busy);
    lab_dom_disabled(lab_dom_query(0, "#integratorIntervalCap"), busy);
    int variables = lab_dom_query(0, "#variableValues");
    if (variables)
        lab_workspace_dom_disable_list(lab_dom_all(variables, "input, button"), busy, 0);
    int derivatives = lab_dom_query(0, "#derivativeButtons");
    if (derivatives)
        lab_workspace_dom_disable_list(lab_dom_all(derivatives, "button"), busy, 0);
    lab_dom_disabled(equation_variable, busy);
    /* Copy reads the displayed result; it never consumes the unfinished calculation. */
    lab_workspace_dom_disable_list(copies, 0, 0);
    lab_workspace_dom_disable_list(digits, busy, 0);
}

/* Apply running styling without leaving a false aria-busy attribute behind. */
void lab_workspace_dom_running(jsref button, bool running)
{
    lab_dom_class(button, "action-running", running);
    if (running)
        lab_workspace_dom_text(button, LAB_DOM_ATTRIBUTE, "aria-busy", "true");
    else
        lab_dom_remove(button, "aria-busy");
}

/* Disconnected, collapsed and zero-height editors cannot acquire a manual resize grip. */
bool lab_workspace_dom_editor_visible(jsref editor)
{
    return lab_dom_measure(editor, LAB_DOM_MEASURE_VISIBLE) != 0 && lab_dom_measure(editor, LAB_DOM_MEASURE_CLIENT_HEIGHT) > 0;
}

static void lab_workspace_dom_editor_clear(jsref editor)
{
    lab_dom_class(editor, "editor-manual-size", 0);
    lab_dom_class(editor, "editor-space-limited", 0);
    lab_workspace_dom_text(editor, LAB_DOM_STYLE, "height", "");
    lab_workspace_dom_text(editor, LAB_DOM_STYLE, "max-height", "");
    lab_dom_remove(editor, "data-automatic-height");
}

/* Clear only editor sizing state, preserving all other authored inline styles. */
void lab_workspace_dom_editor_reset(jsref editors)
{
    for (unsigned i = 0; i < lab_dom_count(editors); ++i) {
        unsigned mark = lab_dom_mark();
        lab_workspace_dom_editor_clear(lab_dom_item(editors, i));
        lab_dom_release(mark);
    }
}

/* Measure the registered editors then apply one shared viewport budget to the visible set. */
void lab_workspace_dom_editor_resize(jsref editors, double viewport)
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
        const double *size = lab_view_editor_resize(
            viewport, visible, lab_dom_measure(editor, LAB_DOM_MEASURE_SCROLL_HEIGHT),
            lab_dom_measure(editor, LAB_DOM_MEASURE_CLIENT_HEIGHT),
            lab_dom_measure(editor, LAB_DOM_MEASURE_RECT_HEIGHT),
            lab_dom_number(lab_dom_read(editor, LAB_DOM_DATASET, "automaticHeight")), manual);
        if (size) {
            unsigned flags = (unsigned)size[0];
            if (flags & 1) {
                lab_dom_class(editor, "editor-manual-size", 1);
                lab_dom_class(editor, "editor-space-limited", !!(flags & 2));
                if (!manual) {
                    lab_dom_write(editor, LAB_DOM_DATASET, "automaticHeight", lab_dom_format(size[1], "", ""));
                    lab_dom_write(editor, LAB_DOM_STYLE, "height", lab_dom_format(size[1], "", "px"));
                }
                lab_dom_write(editor, LAB_DOM_STYLE, "max-height", lab_dom_format(size[2], "", "px"));
            } else {
                lab_workspace_dom_editor_clear(editor);
            }
        }
        lab_dom_release(mark);
    }
}

/* Build ordered calculus and expression-plot actions from opaque names and native display labels. */
void lab_workspace_dom_derivatives(jsref names, jsref labels, bool differentiable)
{
    int panel = lab_dom_query(0, "#derivativeButtons");
    lab_workspace_dom_text(panel, LAB_DOM_TEXT, "", "");
    if (!panel)
        return;
    static const char *const suffixes[] = {" derivative", " integral"};
    int plots = !lab_dom_has_class(lab_dom_query(0, "#plotRange"), "hidden");
    if (plots)
        lab_plot_controls_variables(names, labels);
    for (lab_workspace_calculus_action_t action = LAB_WORKSPACE_DERIVATIVE; action <= LAB_WORKSPACE_INTEGRAL;
         ++action) {
        if (!differentiable)
            continue;
        for (unsigned i = 0; i < lab_dom_count(names); ++i) {
            unsigned mark = lab_dom_mark();
            int button = lab_dom_create("button"), label = lab_dom_create("i"), suffix = lab_dom_create("span");
            lab_dom_class(button, "secondary", 1);
            lab_workspace_dom_text(button, LAB_DOM_ATTRIBUTE, "type", "button");
            lab_dom_write(button, LAB_DOM_DATASET, "variable", lab_dom_item(names, i));
            lab_dom_write(button, LAB_DOM_DATASET, "calculusAction", lab_dom_format(action, "", ""));
            lab_dom_write(label, LAB_DOM_TEXT, "", lab_dom_item(labels, i));
            lab_workspace_dom_text(suffix, LAB_DOM_TEXT, "", suffixes[action]);
            lab_dom_append(button, label);
            lab_dom_append(button, suffix);
            lab_dom_append(panel, button);
            lab_dom_release(mark);
        }
    }
    if (plots)
        lab_plot_controls_sync();
}

/* Reuse native binding labels and the existing calculus renderer with bounded temporary handles. */
void lab_workspace_dom_calculus(jsref names, bool differentiable)
{
    unsigned scope = lab_dom_mark();
    int labels = lab_dom_object(LAB_VALUE_ARRAY);
    unsigned mark = lab_dom_mark();
    for (unsigned i = 0; i < lab_dom_count(names); ++i) {
        lab_dom_push(labels, lab_binding_label_value(lab_dom_item(names, i)));
        lab_dom_release(mark);
    }
    lab_workspace_dom_derivatives(names, labels, differentiable);
    lab_dom_release(scope);
}

static jsref lab_workspace_dom_copy_object(jsref source)
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

static jsref lab_workspace_dom_capture(node_jsref node, int button)
{
    int state = lab_dom_object(LAB_VALUE_OBJECT);
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
void lab_workspace_dom_snapshot(node_jsref node, bool button)
{
    lab_dom_return(lab_workspace_dom_capture(node, button));
}

/* Remove obsolete dataset entries before restoring the captured, opaque native presentation. */
void lab_workspace_dom_restore(node_jsref node, jsref state, bool button)
{
    lab_dom_write(node, LAB_DOM_ATTRIBUTE, "class", lab_dom_get(state, "className"));
    if (button) {
        lab_dom_write(node, LAB_DOM_TEXT, "", lab_dom_get(state, "textContent"));
        lab_dom_disabled(node, lab_dom_truth(lab_dom_get(state, "disabled")));
    } else {
        lab_dom_write(node, LAB_DOM_ATTRIBUTE, "style", lab_dom_get(state, "style"));
        lab_dom_write(node, LAB_DOM_HTML, "", lab_dom_get(state, "innerHTML"));
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
bool lab_workspace_dom_has_result(void)
{
    static const char *const selectors[] = {"#rendered", "#parsed", "#functionStyle", "#value"};
    for (unsigned i = 0; i < sizeof selectors / sizeof *selectors; ++i)
        if (lab_workspace_dom_content(lab_dom_query(0, selectors[i]), i == 0 ? LAB_DOM_HTML : LAB_DOM_TEXT))
            return true;
    return false;
}

static const struct {
    const char *key, *selector;
    int button;
} lab_workspace_dom_result_fields[] = {{"plotRendered", "#plotRendered", 0},
                                       {"rendered", "#rendered", 0},           {"parsed", "#parsed", 0},
                                       {"functionStyle", "#functionStyle", 0}, {"value", "#value", 0},
                                       {"renderedMore", "#renderedMore", 1},   {"parsedMore", "#parsedMore", 1},
                                       {"functionMore", "#functionMore", 1},   {"valueMore", "#valueMore", 1}};

static const char *const lab_workspace_dom_result_titles[] = {
    "#rightPaneTitle", "#renderedTitle", "#parsedTitle", "#functionTitle", "#valueTitle"};

/* Share the exact metadata defaults between snapshot capture and legacy snapshot restoration. */
static jsref lab_workspace_dom_result_metadata(jsref source)
{
    int state = lab_dom_object(LAB_VALUE_OBJECT);
    static const char *const text_fields[] = {"lastTex", "lastDerivativeExpression"};
    for (unsigned i = 0; i < sizeof text_fields / sizeof *text_fields; ++i) {
        int value = lab_dom_get(source, text_fields[i]);
        lab_dom_set(state, text_fields[i], lab_dom_truth(value) ? value : lab_dom_string(""));
    }
    int variables = lab_dom_get(source, "currentVariables"), copy = lab_dom_object(LAB_VALUE_ARRAY);
    unsigned count = lab_dom_type(variables) == LAB_VALUE_ARRAY ? lab_dom_count(variables) : 0;
    unsigned mark = lab_dom_mark();
    /* Copy every opaque variable in order; bounded scopes also cover long discovery lists. */
    for (unsigned i = 0; i < count; ++i) {
        lab_dom_push(copy, lab_dom_item(variables, i));
        lab_dom_release(mark);
    }
    lab_dom_set(state, "currentVariables", copy);
    int differentiable = !lab_dom_equal(lab_dom_get(source, "currentDifferentiable"), lab_dom_scalar(LAB_VALUE_BOOLEAN, 0));
    lab_dom_set(state, "currentDifferentiable", lab_dom_scalar(LAB_VALUE_BOOLEAN, differentiable));
    return state;
}

/* Initialise the fixed mode catalogue with independently absent result snapshots. */
void lab_workspace_dom_result_states(void)
{
    static const char *const modes[] = {
        "expression", "equation", "diffequation", "matrix", "integrator", "datetime", "almanac"};
    int states = lab_dom_object(LAB_VALUE_OBJECT);
    for (unsigned i = 0; i < sizeof modes / sizeof *modes; ++i)
        lab_dom_set(states, modes[i], 0);
    lab_dom_return(states);
}

/* Assemble a complete mode result snapshot with normalised, independently owned metadata. */
void lab_workspace_dom_result_save(jsref metadata)
{
    if (!lab_workspace_dom_has_result()) {
        lab_dom_return(0);
        return;
    }
    lab_layout_plot_camera(1);
    int state = lab_workspace_dom_result_metadata(metadata);
    int titles = lab_dom_object(LAB_VALUE_ARRAY);
    for (unsigned i = 0; i < sizeof lab_workspace_dom_result_titles / sizeof *lab_workspace_dom_result_titles; ++i) {
        int text = lab_dom_read(lab_dom_query(0, lab_workspace_dom_result_titles[i]), LAB_DOM_TEXT, "");
        if (!i && lab_dom_equal(text, lab_dom_string("Help")))
            text = lab_dom_string("Result");
        lab_dom_push(titles, text);
    }
    lab_dom_set(state, "titles", titles);
    for (unsigned i = 0; i < sizeof lab_workspace_dom_result_fields / sizeof *lab_workspace_dom_result_fields; ++i) {
        unsigned mark = lab_dom_mark();
        int node = lab_dom_query(0, lab_workspace_dom_result_fields[i].selector);
        lab_dom_set(state, lab_workspace_dom_result_fields[i].key,
                    lab_workspace_dom_capture(node, lab_workspace_dom_result_fields[i].button));
        lab_dom_release(mark);
    }
    lab_dom_set(state, "resultInputText", lab_dom_read(lab_dom_query(0, "#resultUseInput"), LAB_DOM_DATASET, "inputText"));
    lab_dom_set(state, "plotSlices", lab_dom_get(lab_dom_id("plotSlices"), "marsSliceRecords"));
    lab_dom_set(state, "plotSliceSelected", lab_dom_get(lab_dom_id("plotSlices"), "marsSliceSelected"));
    lab_dom_set(state, "plotSliceModes", lab_dom_get(lab_dom_id("plotSlices"), "marsSliceModes"));
    lab_layout_plot_camera(0);
    lab_dom_return(state);
}

/* Restore cards and reusable input before returning fresh globals for calculus controls and fitting. */
void lab_workspace_dom_result_restore(jsref state)
{
    lab_layout_plot_camera(1);
    lab_plot_slices_clear();
    lab_dom_set(lab_dom_id("plotSlices"), "marsSliceSelected", lab_dom_get(state, "plotSliceSelected"));
    lab_dom_set(lab_dom_id("plotSlices"), "marsSliceModes", lab_dom_get(state, "plotSliceModes"));
    lab_plot_slices_apply(lab_dom_get(state, "plotSlices"), lab_dom_string(""));
    lab_result_render_invalidate();
    lab_result_solver_invalidate();
    lab_layout_expand(0, LAB_LAYOUT_EXPANSION_COLLAPSE);
    int showing_help = lab_dom_has_class(lab_dom_query(0, "#labWorkspace"), "help-open");
    lab_workspace_dom_text(lab_dom_query(0, "#rightPaneTitle"), LAB_DOM_TEXT, "", showing_help ? "Help" : "Result");
    if (!lab_dom_truth(state)) {
        lab_dom_return(0);
        return;
    }
    int titles = lab_dom_get(state, "titles");
    for (unsigned i = 0; i < sizeof lab_workspace_dom_result_titles / sizeof *lab_workspace_dom_result_titles; ++i) {
        int text = lab_dom_item(titles, i);
        if (lab_dom_truth(text) && (i || !showing_help))
            lab_dom_write(lab_dom_query(0, lab_workspace_dom_result_titles[i]), LAB_DOM_TEXT, "", text);
    }
    for (unsigned i = 0; i < sizeof lab_workspace_dom_result_fields / sizeof *lab_workspace_dom_result_fields; ++i) {
        unsigned mark = lab_dom_mark();
        int node = lab_dom_query(0, lab_workspace_dom_result_fields[i].selector);
        lab_workspace_dom_restore(node, lab_dom_get(state, lab_workspace_dom_result_fields[i].key),
                                  lab_workspace_dom_result_fields[i].button);
        lab_dom_release(mark);
    }
    lab_workspace_dom_hidden("#plotCard", !lab_workspace_dom_content(lab_dom_id("plotRendered"), LAB_DOM_HTML));
    if (lab_dom_equal(lab_dom_read(lab_dom_id("plotRendered"), LAB_DOM_DATASET, "plotPending"), lab_dom_string("true")))
        lab_layout_plot(0, lab_dom_string("Plot cancelled when leaving this mode. Evaluate to generate it again."));
    else
        lab_dom_write(lab_dom_id("plotRendered"), LAB_DOM_ATTRIBUTE, "aria-busy", lab_dom_string("false"));
    /* Restored markup has fresh nodes: reconnect dynamic plot controls before accepting input. */
    lab_layout_plot_camera(0);
    lab_layout_zoom(lab_dom_id("plotCard"));
    lab_result_solver_restore();
    lab_result_input_set(lab_dom_get(state, "resultInputText"));
    /* Retained calendar results must restore their header after mode switching cleared it. */
    if (!showing_help)
        lab_workspace_dom_status(lab_dom_string("Ready"));
    int metadata = lab_workspace_dom_result_metadata(state);
    /* Reused result input never inherits the previous mode's evaluation bindings. */
    lab_dom_set(metadata, "resultInputBindings", lab_dom_object(LAB_VALUE_ARRAY));
    lab_dom_return(metadata);
}
