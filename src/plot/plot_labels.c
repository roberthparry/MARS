/**
 * @file plot_labels.c
 * @brief Bounded native tick-label spacing shared by curve and surface renderers.
 *
 * Conservative numeric glyph rectangles keep neighbouring labels apart without
 * relying on browser font measurements. Surface labels may move slightly outwards;
 * labels that still cannot fit are omitted, while their tick strokes remain.
 * Three endpoint names and at most twelve ticks per axis bound the collision scan.
 */
#define MARS_PLOT_INTERNAL_ACCESS
#include "plot_internal.h"

/* Reserve enough space for long scientific labels before placing the axes. */
double plot_tick_width(number_t lower, number_t upper, double font_size)
{
    number_t ticks[12];
    size_t count = plot_ticks(lower, upper, ticks);
    double width = 0;
    bool ok = count != 0;
    for (size_t i = 0; i < count; ++i) {
        string_t *label = plot_tick_label(ticks[i]);
        ok = ok && label;
        double measured = label ? string_length(label) * font_size * 0.8 : 0;
        if (measured > width)
            width = measured;
        string_free(label);
        num_destroy(&ticks[i]);
    }
    return ok ? width : -1;
}

/* Reserve readable numeric text, independently of SVG output size or camera angle. */
bool plot_tick_text(string_t *out, plot_tick_layout_t *layout, const string_t *label, unsigned axis,
                    double x, double y, int anchor, double dx, double dy)
{
    double width = string_length(label) * layout->font_size * 0.8;
    if (!width)
        return true;
    for (unsigned attempt = 0; attempt < 4; ++attempt) {
        double offset = attempt * layout->font_size * 1.5;
        double at_x = x + dx * offset, at_y = y + dy * offset;
        double left = at_x - (anchor < 0 ? width : anchor == 0 ? width / 2 : 0);
        plot_label_box_t box = {left, at_y - layout->font_size, left + width, at_y + layout->font_size * 0.3};
        bool fits = box.left >= 2 && box.top >= 2 && box.right <= layout->width - 2 &&
                    box.bottom <= layout->height - 2 &&
                    layout->count < sizeof(layout->boxes) / sizeof(layout->boxes[0]);
        for (size_t i = 0; fits && i < layout->count; ++i) {
            const plot_label_box_t *other = &layout->boxes[i];
            fits = box.right + 4 <= other->left || other->right + 4 <= box.left ||
                   box.bottom + 4 <= other->top || other->bottom + 4 <= box.top;
        }
        if (fits) {
            layout->boxes[layout->count++] = box;
            return string_append_format(out, "<text data-plot-label=\"%u\" x=\"%.3f\" y=\"%.3f\" "
                        "text-anchor=\"%s\">", axis, at_x, at_y,
                        anchor < 0 ? "end" : anchor > 0 ? "start" : "middle") >= 0 &&
                   plot_escape(out, label) && string_append_cstr(out, "</text>\n") == 0;
        }
        if (!dx && !dy)
            break;
    }
    return true;
}
