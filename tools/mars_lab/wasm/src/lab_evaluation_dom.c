/**
 * @file lab_evaluation_dom.c
 * @brief Projection of native solver renderings into the WASM worksheet.
 *
 * Chooses supplied TeX/SVG representations and diagnostic fallbacks for equation,
 * differential-equation and integrator cards. No mathematical text is parsed or
 * rewritten. C owns dataset/visibility policy; the browser supplies opaque native
 * records, DOM operations and asynchronous rendering when a representation is absent.
 */
#include "lab_dom.h"
#include "lab_layout.h"
#include "lab_result.h"
#include "lab_evaluation_dom.h"

static int lab_evaluation_field(int data, const char *key, int fallback)
{
    int value = lab_dom_get(data, key);
    return lab_dom_truth(value) ? value : fallback;
}

static void lab_evaluation_data(int node, const char *key, int value)
{
    lab_dom_write(node, 3, key, value);
}

/* Install only native-provided renderings, retaining exact copies for digit toggles. */
void lab_evaluation_render(unsigned mode, int data, int expandable)
{
    if (mode != 1 && mode != 2 && mode != 4) {
        lab_dom_return(0);
        return;
    }
    int rendered = lab_dom_query(0, "#rendered"), empty = lab_dom_string("");
    lab_result_render_invalidate();
    lab_evaluation_data(rendered, "responsiveFit", lab_dom_string(mode == 2 ? "true" : "false"));
    lab_dom_class(rendered, "vertically-wrapped-tex", 0);
    int base = lab_evaluation_field(data, "tex", empty);
    int shown = mode == 1 ? lab_evaluation_field(data, "display_TeX", base) : base;
    int full = lab_evaluation_field(data, mode == 1 ? "full_display_TeX" : "full_TeX", base);
    int last = mode == 4 ? full : base;
    int svg = lab_evaluation_field(data, "svg", empty);
    int error = lab_evaluation_field(data, "render_error", empty);
    int fallback = lab_dom_truth(base) ? base : lab_dom_string("No rendered TeX available");
    if (mode == 2) {
        last = lab_evaluation_field(
            data, "display_TeX",
            lab_evaluation_field(data, "solutions_TeX", lab_evaluation_field(data, "problem_TeX", empty)));
        shown = full = last;
        int status = lab_dom_get(data, "status");
        const char *title = lab_dom_equal(status, lab_dom_string("series"))   ? "Equation and local series"
                            : lab_dom_equal(status, lab_dom_string("solved")) ? "Equation and solutions"
                                                                              : "Reduction";
        lab_dom_write(lab_dom_query(0, "#renderedTitle"), 0, "", lab_dom_string(title));
        fallback = lab_dom_truth(last)
                       ? last
                       : lab_evaluation_field(data, "diagnostic", lab_dom_string("No symbolic solution available"));
        lab_evaluation_data(rendered, "compactTex", last);
        lab_evaluation_data(rendered, "wrappedTex", lab_evaluation_field(data, "display_wrapped_TeX", last));
        lab_evaluation_data(rendered, "compactSvg", svg);
        lab_evaluation_data(rendered, "wrappedSvg", lab_evaluation_field(data, "wrapped_svg", empty));
        lab_evaluation_data(rendered, "responsiveFallback", lab_dom_truth(error) ? error : fallback);
        lab_dom_remove(rendered, "data-responsive-variant");
    }
    lab_evaluation_data(rendered, "displayTex", shown);
    lab_evaluation_data(rendered, "fullTex", full);
    lab_evaluation_data(rendered, "displaySvg", svg);
    lab_evaluation_data(rendered, "fullSvg", empty);
    lab_evaluation_data(rendered, "renderError", error);
    lab_layout_content(svg, lab_dom_truth(error) ? error : fallback);
    int can_expand = mode == 1
                         ? expandable
                         : mode == 4 && lab_dom_truth(shown) && lab_dom_truth(full) && !lab_dom_equal(shown, full);
    lab_layout_more(lab_dom_query(0, "#renderedMore"), can_expand);
    lab_dom_return(last);
}

/* Display native expression notes and return its explicit differentiability decision. */
int lab_evaluation_notes(int data)
{
    int note = lab_evaluation_field(data, "value_note", lab_dom_string(""));
    lab_dom_write(lab_dom_query(0, "#valueNote"), 0, "", note);
    lab_dom_class(lab_dom_query(0, "#valueNoteCard"), "hidden", !lab_dom_length(lab_dom_clean(note, 0)));
    lab_dom_write(lab_dom_query(0, "#valueTitle"), 0, "",
                  lab_dom_string(lab_dom_truth(lab_dom_get(data, "root_value")) ? "Values" : "Value"));
    int differentiable = lab_evaluation_field(data, "differentiable", lab_dom_string("yes"));
    return !lab_dom_equal(lab_dom_clean(differentiable, 1), lab_dom_string("no"));
}

/* Retain the solver SVG and its request owner only after the host verifies freshness. */
void lab_evaluation_solver(int data, int source, int details, int svg, double token)
{
    int node = lab_dom_query(0, "#functionStyle"), empty = lab_dom_string("");
    lab_dom_class(node, "equation-function", 1);
    lab_evaluation_data(node, "solverCompactTex", source);
    lab_evaluation_data(node, "solverWrappedTex", lab_evaluation_field(data, "steps_wrapped_TeX", source));
    lab_evaluation_data(node, "solverCompactSvg", svg);
    lab_evaluation_data(node, "solverParentToken", lab_dom_format(token, "", ""));
    lab_evaluation_data(node, "solverWrappedSvg", empty);
    lab_dom_remove(node, "data-solver-variant");
    lab_layout_solver_install(svg, lab_dom_string("compact"));
    lab_evaluation_data(node, "fullText", details);
    lab_evaluation_data(node, "displayText", details);
}
