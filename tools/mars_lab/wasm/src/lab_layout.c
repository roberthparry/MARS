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
#include "lab_result.h"
#include "lab_view.h"
#include "lab_layout.h"

static int lab_layout_read(int node, const char *key)
{
    return lab_dom_read(node, 3, key);
}

static void lab_layout_text(int node, unsigned kind, const char *key, const char *text)
{
    lab_dom_write(node, kind, key, lab_dom_string(text));
}

static void lab_layout_number(int node, unsigned kind, const char *key, double number, const char *unit)
{
    lab_dom_write(node, kind, key, lab_dom_format(number, "", unit));
}

static double lab_layout_base(int card, const char *key, double fallback)
{
    double value = lab_dom_css(card, key);
    return value ? value : fallback;
}

static double lab_layout_width(int node)
{
    double width = lab_dom_measure(node, 0) - lab_dom_css(node, "padding-left") - lab_dom_css(node, "padding-right");
    return width > 0 ? width : 0;
}

static double lab_layout_intrinsic(int svg, unsigned axis)
{
    double value = lab_dom_measure(svg, 1 + axis);
    if (!value)
        value = lab_dom_measure(svg, 3 + axis);
    if (!value)
        value = lab_dom_measure(svg, 5 + axis);
    return value > 1 ? value : 1;
}

/* Ask the browser to parse SVG; read only intrinsic absolute length or view-box geometry. */
double lab_layout_markup_width(int markup)
{
    int box = lab_dom_create("div");
    lab_dom_write(box, 1, "", markup);
    int svg = lab_dom_query(box, "svg");
    if (!svg)
        return 0;
    double width = lab_dom_measure(svg, 1);
    return width > 0 ? width : lab_dom_measure(svg, 5);
}

static void lab_layout_frame(int card, double scale, double zoom)
{
    int frame = lab_dom_query(card, ".rendered-zoom-frame");
    int svg = frame ? lab_dom_query(frame, "svg") : 0;
    if (!svg)
        return;
    if (!lab_dom_length(lab_layout_read(svg, "baseWidth")) || !lab_dom_length(lab_layout_read(svg, "baseHeight"))) {
        int previous = lab_dom_read(svg, 4, "transform");
        lab_layout_text(svg, 4, "transform", "none");
        lab_layout_number(svg, 3, "baseWidth", lab_layout_intrinsic(svg, 0), "");
        lab_layout_number(svg, 3, "baseHeight", lab_layout_intrinsic(svg, 1), "");
        lab_dom_write(svg, 4, "transform", previous);
    }
    double width = lab_dom_number(lab_layout_read(svg, "baseWidth"));
    double height = lab_dom_number(lab_layout_read(svg, "baseHeight"));
    width = width ? width : 1;
    height = height ? height : 1;
    int matrix = lab_dom_closest(frame, "#value.matrix-tex-value");
    if (matrix)
        scale = lab_view_matrix_scale(scale / zoom, zoom, lab_layout_width(matrix), width);
    lab_layout_number(frame, 4, "width", width * scale, "px");
    lab_layout_number(frame, 4, "height", height * scale, "px");
    lab_layout_number(svg, 4, "width", width, "px");
    lab_layout_number(svg, 4, "height", height, "px");
    lab_dom_write(svg, 4, "transform", lab_dom_format(scale, "scale(", ")"));
}

/* Project a card's C-owned zoom to CSS, SVG geometry and accessible controls. */
void lab_layout_zoom(int card)
{
    if (!card)
        return;
    int index = lab_view_card_zoom(lab_dom_card_id(card));
    if (index < 0)
        return;
    double zoom = lab_view_zoom(index);
    double scale = lab_layout_base(card, "--render-base-scale", 1.35) * zoom;
    lab_layout_number(card, 4, "--result-zoom", zoom, "");
    lab_layout_number(card, 4, "--result-font-size", lab_layout_base(card, "--result-base-font-rem", 0.92) * zoom,
                      "rem");
    lab_layout_number(card, 4, "--render-font-size", lab_layout_base(card, "--render-base-font-rem", 1.15) * zoom,
                      "rem");
    lab_layout_number(card, 4, "--render-zoom", scale, "");
    lab_layout_number(card, 4, "--render-margin-bottom",
                      lab_layout_base(card, "--render-base-margin-rem", 3) * (zoom > 1 ? zoom : 1), "rem");
    lab_layout_frame(card, scale, zoom);
    int buttons = lab_dom_all(card, "[data-zoom-reset]");
    for (unsigned i = 0; i < lab_dom_count(buttons); ++i) {
        int button = lab_dom_item(buttons, i);
        unsigned percent = (unsigned)(zoom * 100 + 0.5);
        lab_layout_number(button, 0, "", percent, "%");
        lab_dom_write(button, 2, "aria-label", lab_dom_format(percent, "Reset zoom from ", "%"));
    }
    buttons = lab_dom_all(card, "[data-zoom-step]");
    for (unsigned i = 0; i < lab_dom_count(buttons); ++i) {
        int button = lab_dom_item(buttons, i);
        int down = lab_dom_equal(lab_layout_read(button, "zoomStep"), lab_dom_string("-1"));
        lab_dom_disabled(button, down ? index <= 0 : (unsigned)index >= lab_view_zoom_count() - 1);
    }
}

/* Mutate native state before projecting; invalid/unregistered cards remain untouched. */
void lab_layout_set_zoom(int card, double index, int step)
{
    int id = lab_dom_card_id(card);
    int changed = step ? lab_view_card_step_zoom(id, index < 0 ? -1 : 1) : lab_view_card_set_zoom(id, index);
    if (changed < 0)
        return;
    lab_layout_zoom(card);
    lab_dom_schedule(0, 0);
}

/* Render, toggle or collapse exclusive expansion and its accessible button state. */
void lab_layout_expand(int card, unsigned operation)
{
    if (operation > 2)
        return;
    if (operation == 1 && lab_view_card_toggle(lab_dom_card_id(card)) == -2)
        return;
    if (operation == 2)
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
            lab_layout_text(button, 0, "", selected ? "Collapse" : "Expand");
            lab_layout_text(button, 2, "aria-expanded", selected ? "true" : "false");
        }
    }
    if (operation == 1)
        lab_dom_schedule(1, card);
}

/* Install only native markup, or an ordinary text fallback, and fit it on the next frame. */
void lab_layout_content(int svg, int fallback)
{
    int rendered = lab_dom_query(0, "#rendered");
    lab_layout_text(rendered, 0, "", "");
    if (lab_dom_length(svg)) {
        int frame = lab_dom_create("div");
        lab_dom_class(frame, "rendered-zoom-frame", 1);
        lab_dom_write(frame, 1, "", svg);
        lab_dom_append(rendered, frame);
    } else {
        lab_dom_write(rendered, 0, "", fallback);
    }
    int card = lab_dom_closest(rendered, ".result-card");
    if (card)
        lab_dom_schedule(1, card);
}

/* Update rendered source and markup together, only when native metadata enables fitting. */
void lab_layout_fit(void)
{
    int rendered = lab_dom_query(0, "#rendered");
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
    lab_dom_write(rendered, 3, "responsiveVariant", variant);
    int text = use_wrapped ? lab_layout_read(rendered, "wrappedTex") : 0;
    if (!lab_dom_length(text))
        text = lab_layout_read(rendered, "compactTex");
    lab_dom_write(rendered, 3, "displayTex", text);
    int fallback = lab_layout_read(rendered, "responsiveFallback");
    lab_layout_content(use_wrapped ? wrapped : compact,
                       lab_dom_length(fallback) ? fallback : lab_dom_string("No symbolic solution available"));
}

/* Select solver wrapping using current geometry, not the geometry at request time. */
int lab_layout_solver_wrapped(void)
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
void lab_layout_solver_install(int markup, int variant)
{
    int node = lab_dom_query(0, "#functionStyle");
    if (!lab_dom_length(markup) || lab_dom_equal(lab_layout_read(node, "solverVariant"), variant))
        return;
    lab_dom_write(node, 1, "", markup);
    lab_dom_class(node, "equation-function", 1);
    int svg = lab_dom_query(node, "svg");
    int width = lab_dom_read(svg, 2, "width");
    if (svg && lab_dom_length(width)) {
        lab_dom_write(svg, 4, "width", lab_dom_join(lab_dom_string("calc("), width, " * var(--solver-tex-scale))"));
        lab_layout_text(svg, 4, "max-width", "100%");
        lab_layout_text(svg, 4, "height", "auto");
    }
    lab_dom_write(node, 3, "solverVariant", variant);
}

/* Abbreviation is supplied by native presentation metadata, never inferred from displayed text. */
void lab_layout_more(int button, int enabled)
{
    lab_dom_class(button, "hidden", !enabled);
    lab_layout_text(button, 0, "", "Show more digits");
    lab_layout_text(button, 3, "expanded", "false");
}

/* CSS owns error colours; clearing also removes obsolete inline error styles. */
void lab_layout_error(int message, int error)
{
    int rendered = lab_dom_query(0, "#rendered");
    lab_dom_class(rendered, "error", error);
    if (error) {
        lab_result_render_invalidate();
        lab_dom_write(rendered, 0, "", lab_dom_length(message) ? message : lab_dom_string("Evaluation failed"));
        return;
    }
    lab_dom_class(rendered, "vertically-wrapped-tex", 0);
    static const char *const properties[] = {"color",      "background",  "border-color",
                                             "box-shadow", "text-shadow", "font-family"};
    for (unsigned i = 0; i < sizeof properties / sizeof *properties; ++i)
        lab_layout_text(rendered, 4, properties[i], "");
}
