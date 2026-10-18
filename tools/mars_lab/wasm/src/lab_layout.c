/**
 * @file lab_layout.c
 * @brief Result-card projection and responsive SVG layout in C/WebAssembly.
 *
 * Drives card zoom, exclusive expansion, error state and native compact/wrapped
 * rendering through scoped browser DOM capabilities. The host supplies CSS/SVG
 * measurements and DOM operations; C owns the choices and update order. Opaque
 * handles are valid only during one synchronous entry point and are never retained.
 * No mathematical text or SVG grammar is interpreted here.
 */
#include "lab_dom.h"
#include "lab_view.h"
#include "lab_result.h"
#include "lab_plot_controls.h"
#include "lab_plot_pointer.h"
#include "lab_layout.h"

static browser_handle lab_layout_read(node_handle node, const char *key)
{
    return lab_dom_read(node, LAB_DOM_DATASET, key);
}

static void lab_layout_text(node_handle node, lab_dom_accessor_t kind, const char *key, const char *text)
{
    lab_dom_write(node, kind, key, lab_dom_string(text));
}

static void lab_layout_number(node_handle node, lab_dom_accessor_t kind, const char *key, double number, const char *unit)
{
    lab_dom_write(node, kind, key, lab_dom_format(number, "", unit));
}

static double lab_layout_base(browser_handle card, const char *key, double fallback)
{
    double value = lab_dom_css(card, key);
    return value ? value : fallback;
}

static double lab_layout_width(node_handle node)
{
    double width = lab_dom_measure(node, LAB_DOM_MEASURE_CLIENT_WIDTH) - lab_dom_css(node, "padding-left") -
                   lab_dom_css(node, "padding-right");
    return width > 0 ? width : 0;
}

static double lab_layout_intrinsic(browser_handle svg, unsigned axis)
{
    double value = lab_dom_measure(svg, 1 + axis);
    if (!value)
        value = lab_dom_measure(svg, 3 + axis);
    if (!value)
        value = lab_dom_measure(svg, 5 + axis);
    return value > 1 ? value : 1;
}

/* Ask the browser to parse SVG; read only intrinsic absolute length or view-box geometry. */
double lab_layout_markup_width(browser_handle markup)
{
    int box = lab_dom_create("div");
    lab_dom_write(box, LAB_DOM_HTML, "", markup);
    int svg = lab_dom_query(box, "svg");
    if (!svg)
        return 0;
    double width = lab_dom_measure(svg, LAB_DOM_MEASURE_SVG_WIDTH);
    return width > 0 ? width : lab_dom_measure(svg, LAB_DOM_MEASURE_VIEWBOX_WIDTH);
}

static void lab_layout_frame(browser_handle card, double scale, double zoom)
{
    if (lab_dom_query(card, ".complex-grid")) {
        int buttons = lab_dom_all(card, ".complex-panel button");
        for (unsigned i = 0; i < lab_dom_count(buttons); ++i)
            lab_dom_listen(lab_dom_item(buttons, i), "click", 31, 0);
        lab_plot_controls_sync();
        return;
    }
    int frame = lab_dom_query(card, ".rendered-zoom-frame");
    int svg = frame ? lab_dom_query(frame, "svg") : 0;
    if (!svg)
        return;
    if (!lab_dom_length(lab_layout_read(svg, "baseWidth")) || !lab_dom_length(lab_layout_read(svg, "baseHeight"))) {
        int previous = lab_dom_read(svg, LAB_DOM_STYLE, "transform");
        lab_layout_text(svg, LAB_DOM_STYLE, "transform", "none");
        lab_layout_number(svg, LAB_DOM_DATASET, "baseWidth", lab_layout_intrinsic(svg, 0), "");
        lab_layout_number(svg, LAB_DOM_DATASET, "baseHeight", lab_layout_intrinsic(svg, 1), "");
        lab_dom_write(svg, LAB_DOM_STYLE, "transform", previous);
    }
    double width = lab_dom_number(lab_layout_read(svg, "baseWidth"));
    double height = lab_dom_number(lab_layout_read(svg, "baseHeight"));
    width = width ? width : 1;
    height = height ? height : 1;
    int matrix = lab_dom_closest(frame, "#value.matrix-tex-value");
    if (matrix)
        scale = lab_view_matrix_scale(scale / zoom, zoom, lab_layout_width(matrix), width);
    /* Native plot series identify graphics which fit the viewport at 100% zoom. */
    int plot = lab_dom_query(svg, "[data-series]");
    if (plot)
        lab_plot_controls_sync();
    if (lab_dom_query(card, "#plotRange"))
        lab_dom_class(card, "plot-card", !!plot);
    if (plot) {
        double available = lab_layout_width(lab_dom_closest(frame, ".rendered"));
        if (available > 0)
            scale = available / width * zoom;
    }
    lab_layout_number(frame, LAB_DOM_STYLE, "width", width * scale, "px");
    lab_layout_number(frame, LAB_DOM_STYLE, "height", height * scale, "px");
    lab_layout_number(svg, LAB_DOM_STYLE, "width", plot ? width * scale : width, "px");
    lab_layout_number(svg, LAB_DOM_STYLE, "height", plot ? height * scale : height, "px");
    lab_dom_write(svg, LAB_DOM_STYLE, "transform", plot ? lab_dom_string("none") : lab_dom_format(scale, "scale(", ")"));
    if (plot)
        lab_layout_text(svg, LAB_DOM_STYLE, "margin-bottom", "0");
}

/* Project a card's C-owned zoom to CSS, SVG geometry and accessible controls. */
void lab_layout_zoom(browser_handle card)
{
    if (!card)
        return;
    int index = lab_view_card_zoom(lab_dom_card_id(card));
    if (index < 0)
        return;
    int curve = !!lab_dom_query(card, "[data-plot-chart][data-x-min], [data-surface][data-x-min]");
    double zoom = curve ? 1 : lab_view_zoom(index);
    /* Read inherited bases together before writes invalidate computed styles. */
    double scale = lab_layout_base(card, "--render-base-scale", 1.35) * zoom;
    double font = lab_layout_base(card, "--result-base-font-rem", 0.92) * zoom;
    double render_font = lab_layout_base(card, "--render-base-font-rem", 1.15) * zoom;
    double margin = lab_layout_base(card, "--render-base-margin-rem", 3) * (zoom > 1 ? zoom : 1);
    lab_layout_number(card, LAB_DOM_STYLE, "--result-zoom", zoom, "");
    lab_layout_number(card, LAB_DOM_STYLE, "--result-font-size", font, "rem");
    lab_layout_number(card, LAB_DOM_STYLE, "--render-font-size", render_font, "rem");
    lab_layout_number(card, LAB_DOM_STYLE, "--render-zoom", scale, "");
    lab_layout_number(card, LAB_DOM_STYLE, "--render-margin-bottom", margin, "rem");
    lab_layout_frame(card, scale, zoom);
    int buttons = lab_dom_all(card, "[data-zoom-reset]");
    for (unsigned i = 0; i < lab_dom_count(buttons); ++i) {
        int button = lab_dom_item(buttons, i);
        unsigned percent = (unsigned)(zoom * 100 + 0.5);
        lab_layout_number(button, LAB_DOM_TEXT, "", percent, "%");
        lab_dom_write(button, LAB_DOM_ATTRIBUTE, "aria-label", lab_dom_format(percent, "Reset zoom from ", "%"));
        if (curve) {
            lab_dom_write(button, LAB_DOM_TEXT, "", lab_dom_string("Reset"));
            lab_dom_write(button, LAB_DOM_ATTRIBUTE, "aria-label", lab_dom_string("Reset plot view"));
        }
    }
    buttons = lab_dom_all(card, "[data-zoom-step]");
    for (unsigned i = 0; i < lab_dom_count(buttons); ++i) {
        int button = lab_dom_item(buttons, i);
        int down = lab_dom_equal(lab_layout_read(button, "zoomStep"), lab_dom_string("-1"));
        lab_dom_disabled(button, !curve && (down ? index <= 0 : (unsigned)index >= lab_view_zoom_count() - 1));
    }
}

/* Mutate native state before projecting; invalid/unregistered cards remain untouched. */
void lab_layout_set_zoom(browser_handle card, double index, lab_layout_zoom_operation_t operation)
{
    int id = lab_dom_card_id(card);
    lab_view_zoom_direction_t direction = index < 0 ? LAB_VIEW_ZOOM_DECREASE : LAB_VIEW_ZOOM_INCREASE;
    int changed = operation != LAB_LAYOUT_ZOOM_ABSOLUTE ? lab_view_card_step_zoom(id, direction)
                                                       : lab_view_card_set_zoom(id, index);
    if (changed < 0)
        return;
    lab_layout_zoom(card);
    lab_dom_schedule(LAB_DOM_SCHEDULE_FIT, 0);
}

/* Render, toggle or collapse exclusive expansion and its accessible button state. */
void lab_layout_expand(browser_handle card, lab_layout_expansion_t operation)
{
    if (operation == LAB_LAYOUT_EXPANSION_TOGGLE && lab_dom_has(card, "data-complex-panel")) {
        int expanded = !lab_dom_equal(lab_dom_read(card, LAB_DOM_DATASET, "expanded"), lab_dom_string("true"));
        int host = lab_dom_id("plotRendered"), state = lab_dom_get(host, "marsComplexExpanded");
        if (!state) {
            state = lab_dom_object(LAB_VALUE_OBJECT);
            lab_dom_set(host, "marsComplexExpanded", state);
        }
        int grid = lab_dom_closest(card, ".complex-grid"), panels = lab_dom_all(grid, ".complex-panel");
        /* The four projections share one expanded viewport, never separate expanded rows. */
        for (unsigned i = 0; i < lab_dom_count(panels); ++i) {
            int panel = lab_dom_item(panels, i), selected = expanded && lab_dom_equal(panel, card);
            lab_dom_key_set(state, lab_dom_read(panel, LAB_DOM_DATASET, "complexPanel"),
                            lab_dom_scalar(LAB_VALUE_BOOLEAN, selected));
            lab_layout_text(panel, LAB_DOM_DATASET, "expanded", selected ? "true" : "false");
            lab_dom_class(panel, "complex-expanded", selected);
            int button = lab_dom_query(panel, "button");
            lab_layout_text(button, LAB_DOM_TEXT, "", selected ? "Collapse" : "Expand");
            lab_layout_text(button, LAB_DOM_ATTRIBUTE, "aria-expanded", selected ? "true" : "false");
        }
        lab_dom_class(grid, "complex-focused", expanded);
        lab_dom_schedule(LAB_DOM_SCHEDULE_ZOOM, lab_dom_id("plotCard"));
        return;
    }
    if ((unsigned)operation > LAB_LAYOUT_EXPANSION_COLLAPSE)
        return;
    if (operation == LAB_LAYOUT_EXPANSION_TOGGLE && lab_view_card_toggle(lab_dom_card_id(card)) == -2)
        return;
    if (operation == LAB_LAYOUT_EXPANSION_COLLAPSE)
        lab_view_cards_collapse();
    int expanded = lab_view_card_expanded();
    lab_dom_class(lab_dom_query(0, "#labWorkspace"), "result-card-expanded", expanded >= 0);
    lab_dom_class(lab_dom_query(0, "#resultPane"), "card-expanded", expanded >= 0);
    int cards = lab_dom_all(0, ".result-card");
    for (unsigned i = 0; i < lab_dom_count(cards); ++i) {
        int entry = lab_dom_item(cards, i);
        int selected = expanded >= 0 && lab_dom_card_id(entry) == expanded;
        lab_dom_class(entry, "expanded-card", selected);
        int buttons = lab_dom_all(entry, "[data-expand-card]");
        for (unsigned j = 0; j < lab_dom_count(buttons); ++j) {
            int button = lab_dom_item(buttons, j);
            lab_layout_text(button, LAB_DOM_TEXT, "", selected ? "Collapse" : "Expand");
            lab_layout_text(button, LAB_DOM_ATTRIBUTE, "aria-expanded", selected ? "true" : "false");
        }
    }
    if (operation == LAB_LAYOUT_EXPANSION_TOGGLE)
        lab_dom_schedule(LAB_DOM_SCHEDULE_ZOOM, card);
}

/* Install only native markup, or an ordinary text fallback, and fit it on the next frame. */
static void lab_layout_content_at(browser_handle rendered, browser_handle svg, browser_handle fallback)
{
    lab_layout_text(rendered, LAB_DOM_TEXT, "", "");
    if (lab_dom_length(svg)) {
        int frame = lab_dom_create("div");
        lab_dom_class(frame, "rendered-zoom-frame", 1);
        lab_dom_write(frame, LAB_DOM_HTML, "", svg);
        lab_dom_append(rendered, frame);
        lab_plot_pointer_prepare(rendered);
    } else {
        lab_dom_write(rendered, LAB_DOM_TEXT, "", fallback);
    }
    int card = lab_dom_closest(rendered, ".result-card");
    lab_dom_class(card, "plot-card", !!lab_dom_query(rendered, "[data-series]"));
    if (card && lab_dom_query(rendered, "[data-series]"))
        lab_layout_zoom(card);
    if (card)
        lab_dom_schedule(LAB_DOM_SCHEDULE_ZOOM, card);
}

/* Keep mathematical rendering independent of the optional plot card. */
void lab_layout_content(browser_handle svg, browser_handle fallback)
{
    lab_layout_content_at(lab_dom_id("rendered"), svg, fallback);
}

static void complex_grid(node_handle node, browser_handle result)
{
    if (!lab_dom_length(lab_dom_get(result, "complex_plane")))
        return;
    static const char *const keys[] = {"svg", "complex_plane", "components", "amplitude"};
    static const char *const names[] = {"3-D curve", "Complex plane", "Components", "Amplitude"};
    lab_layout_text(node, LAB_DOM_TEXT, "", "");
    int grid = lab_dom_create("div");
    lab_dom_class(grid, "complex-grid", 1);
    lab_dom_append(node, grid);
    int focused = 0;
    for (unsigned i = 0; i < 4; ++i) {
        int panel = lab_dom_create("section"), heading = lab_dom_create("div");
        int title = lab_dom_create("span"), button = lab_dom_create("button"), body = lab_dom_create("div");
        lab_dom_class(panel, "complex-panel", 1);
        lab_layout_text(panel, LAB_DOM_DATASET, "complexPanel", keys[i]);
        lab_dom_class(heading, "card-title", 1);
        lab_layout_text(title, LAB_DOM_TEXT, "", names[i]);
        lab_dom_class(button, "card-action", 1);
        lab_layout_text(button, LAB_DOM_ATTRIBUTE, "type", "button");
        lab_layout_text(button, LAB_DOM_TEXT, "", "Expand");
        lab_layout_text(button, LAB_DOM_ATTRIBUTE, "aria-expanded", "false");
        int expanded = !focused && lab_dom_truth(lab_dom_get(lab_dom_get(node, "marsComplexExpanded"), keys[i]));
        focused = focused || expanded;
        lab_dom_class(panel, "complex-expanded", expanded);
        lab_layout_text(panel, LAB_DOM_DATASET, "expanded", expanded ? "true" : "false");
        lab_layout_text(button, LAB_DOM_TEXT, "", expanded ? "Collapse" : "Expand");
        lab_layout_text(button, LAB_DOM_ATTRIBUTE, "aria-expanded", expanded ? "true" : "false");
        lab_dom_listen(button, "click", 31, 0);
        lab_dom_class(body, "rendered", 1);
        lab_dom_append(heading, title);
        lab_dom_append(heading, button);
        lab_dom_append(panel, heading);
        lab_dom_append(panel, body);
        lab_dom_append(grid, panel);
        lab_layout_content_at(body, lab_dom_get(result, keys[i]),
                              lab_dom_string("Amplitude is undefined for distributions."));
        if (i > 0) {
            lab_layout_text(lab_dom_query(body, "svg[data-x-min]"), LAB_DOM_DATASET, "plotChart", "true");
            lab_plot_pointer_prepare(body);
        }
    }
    lab_dom_class(grid, "complex-focused", focused);
}

/* Move original controls, preserving listeners and values; snapshots must never clone their identifiers. */
void lab_layout_plot_camera(bool home)
{
    int camera = lab_dom_id("plotCamera");
    int parent = home ? 0 : lab_dom_query(lab_dom_id("plotRendered"), "[data-complex-panel=\"svg\"]");
    int docked = parent != 0;
    if (!parent)
        parent = lab_dom_id("plotRange");
    if (camera && parent && !lab_dom_equal(lab_dom_get(camera, "parentNode"), parent)) {
        if (docked)
            lab_dom_append(parent, camera);
        else
            lab_dom_prepend(parent, camera);
    }
}

/* Only native plot output and diagnostics enter the standalone plot card. */
void lab_layout_plot(browser_handle result, browser_handle error)
{
    int node = lab_dom_id("plotRendered"), svg = lab_dom_get(result, "svg");
    int projection = lab_dom_get(result, "gestureProjection");
    if (!error && lab_dom_length(projection)) {
        static const char *const keys[] = {"complex_plane", "components", "amplitude", "svg"};
        static const char *const selectors[] = {
            "[data-complex-panel=\"complex_plane\"] .rendered",
            "[data-complex-panel=\"components\"] .rendered",
            "[data-complex-panel=\"amplitude\"] .rendered",
            "[data-complex-panel=\"svg\"] .rendered"
        };
        int body = 0, image = 0;
        /* Each of the four fixed projections owns its local range gestures. */
        for (unsigned i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i)
            if (lab_dom_equal(projection, lab_dom_string(keys[i]))) {
                body = lab_dom_query(node, selectors[i]);
                image = lab_dom_get(result, keys[i]);
                break;
            }
        if (body && lab_dom_length(image)) {
            lab_layout_content_at(body, image, lab_dom_string(""));
            lab_layout_text(lab_dom_query(body, "svg[data-x-min]"), LAB_DOM_DATASET, "plotChart", "true");
            lab_plot_pointer_prepare(body);
            lab_dom_class(lab_dom_id("plotFeedback"), "hidden", 1);
            return;
        }
    }
    /* A failed navigation must not replace the coordinate-bearing chart with
     * text: its bounds and hit targets are needed to zoom back out or reset. */
    if (lab_dom_truth(error) && lab_dom_query(node, "[data-plot-chart][data-x-min], [data-surface][data-x-min]")) {
        lab_dom_write(node, LAB_DOM_ATTRIBUTE, "aria-busy", lab_dom_string("false"));
        lab_dom_write(node, LAB_DOM_DATASET, "plotPending", lab_dom_string("false"));
        int feedback = lab_dom_id("plotFeedback");
        lab_dom_write(feedback, LAB_DOM_TEXT, "", lab_dom_join(lab_dom_string("Previous view retained. "), error, ""));
        lab_dom_class(feedback, "hidden", 0);
        lab_layout_zoom(lab_dom_id("plotCard"));
        return;
    }
    lab_dom_class(lab_dom_id("plotCard"), "hidden", 0);
    lab_dom_class(node, "error", lab_dom_truth(error));
    lab_dom_write(node, LAB_DOM_ATTRIBUTE, "aria-busy", lab_dom_string(!result && !error ? "true" : "false"));
    lab_dom_write(node, LAB_DOM_DATASET, "plotPending", lab_dom_string(!result && !error ? "true" : "false"));
    lab_dom_write(node, LAB_DOM_DATASET, "displaySvg", lab_dom_truth(error) ? lab_dom_string("") : svg);
    lab_dom_write(node, LAB_DOM_DATASET, "displayTex", lab_dom_get(result, "TeX"));
    lab_layout_plot_camera(1);
    lab_layout_content_at(node, lab_dom_truth(error) ? lab_dom_string("") : svg,
                          !result && !error ? lab_dom_string("Generating plot…") : error);
    if (!error)
        complex_grid(node, result);
    lab_layout_plot_camera(0);
    lab_dom_class(lab_dom_id("plotCard"), "plot-card", 1);
    int notice = lab_dom_get(result, "notice");
    lab_dom_write(lab_dom_id("plotFeedback"), LAB_DOM_TEXT, "", notice);
    lab_dom_class(lab_dom_id("plotFeedback"), "hidden", !lab_dom_length(notice));
}

/* Update rendered source and markup together, only when native metadata enables fitting. */
void lab_layout_fit(void)
{
    lab_layout_zoom(lab_dom_id("plotCard"));
    int rendered = lab_dom_query(0, "#rendered");
    if (lab_dom_query(rendered, "svg [data-series]")) {
        lab_layout_zoom(lab_dom_closest(rendered, ".result-card"));
        return;
    }
    if (!lab_dom_equal(lab_layout_read(rendered, "responsiveFit"), lab_dom_string("true")))
        return;
    int compact = lab_layout_read(rendered, "compactSvg"), wrapped = lab_layout_read(rendered, "wrappedSvg");
    if (!lab_dom_length(compact))
        return;
    int card = lab_dom_closest(rendered, ".result-card");
    double scale = card ? lab_layout_base(card, "--render-base-scale", 1.35) *
                              lab_view_zoom(lab_view_card_zoom(lab_dom_card_id(card)))
                        : 1;
    int use_wrapped = lab_view_wrapped(lab_layout_markup_width(compact), scale, lab_layout_width(rendered),
                                       lab_dom_length(wrapped) != 0);
    int variant = lab_dom_string(use_wrapped ? "wrapped" : "compact");
    lab_dom_class(rendered, "vertically-wrapped-tex", use_wrapped);
    if (lab_dom_equal(lab_layout_read(rendered, "responsiveVariant"), variant))
        return;
    lab_dom_write(rendered, LAB_DOM_DATASET, "responsiveVariant", variant);
    int text = use_wrapped ? lab_layout_read(rendered, "wrappedTex") : 0;
    if (!lab_dom_length(text))
        text = lab_layout_read(rendered, "compactTex");
    lab_dom_write(rendered, LAB_DOM_DATASET, "displayTex", text);
    int fallback = lab_layout_read(rendered, "responsiveFallback");
    lab_layout_content(use_wrapped ? wrapped : compact,
                       lab_dom_length(fallback) ? fallback : lab_dom_string("No symbolic solution available"));
}

/* Select solver wrapping using current geometry, not the geometry at request time. */
bool lab_layout_solver_wrapped(void)
{
    int node = lab_dom_query(0, "#functionStyle");
    int compact = lab_layout_read(node, "solverCompactSvg"), wrapped = lab_layout_read(node, "solverWrappedTex");
    double scale = lab_layout_base(node, "--solver-tex-scale", 1.5);
    if (scale <= 0)
        scale = 1.5;
    int different = lab_dom_length(wrapped) && !lab_dom_equal(wrapped, lab_layout_read(node, "solverCompactTex"));
    return lab_view_wrapped(lab_layout_markup_width(compact), scale, lab_layout_width(node), different);
}

/* Browser HTML/SVG parsing handles any XML preamble; C applies the selected solver variant. */
void lab_layout_solver_install(browser_handle markup, browser_handle variant)
{
    int node = lab_dom_query(0, "#functionStyle");
    if (!lab_dom_length(markup) || lab_dom_equal(lab_layout_read(node, "solverVariant"), variant))
        return;
    lab_dom_write(node, LAB_DOM_HTML, "", markup);
    lab_dom_class(node, "equation-function", 1);
    int svg = lab_dom_query(node, "svg");
    int width = lab_dom_read(svg, LAB_DOM_ATTRIBUTE, "width");
    if (svg && lab_dom_length(width)) {
        lab_dom_write(svg, LAB_DOM_STYLE, "width", lab_dom_join(lab_dom_string("calc("), width, " * var(--solver-tex-scale))"));
        lab_layout_text(svg, LAB_DOM_STYLE, "max-width", "100%");
        lab_layout_text(svg, LAB_DOM_STYLE, "height", "auto");
    }
    lab_dom_write(node, LAB_DOM_DATASET, "solverVariant", variant);
}

/* Abbreviation is supplied by native presentation metadata, never inferred from displayed text. */
void lab_layout_more(browser_handle button, bool enabled)
{
    lab_dom_class(button, "hidden", !enabled);
    lab_layout_text(button, LAB_DOM_TEXT, "", "Show more digits");
    lab_layout_text(button, LAB_DOM_DATASET, "expanded", "false");
}

/* CSS owns error colours; clearing also removes obsolete inline error styles. */
void lab_layout_error(browser_handle message, bool error)
{
    int rendered = lab_dom_query(0, "#rendered");
    lab_dom_class(rendered, "error", error);
    if (error) {
        lab_result_render_invalidate();
        lab_dom_write(rendered, LAB_DOM_TEXT, "", lab_dom_length(message) ? message : lab_dom_string("Evaluation failed"));
        return;
    }
    lab_dom_class(rendered, "vertically-wrapped-tex", 0);
    static const char *const properties[] = {"color",      "background",  "border-color",
                                             "box-shadow", "text-shadow", "font-family"};
    for (unsigned i = 0; i < sizeof properties / sizeof *properties; ++i)
        lab_layout_text(rendered, LAB_DOM_STYLE, properties[i], "");
}
