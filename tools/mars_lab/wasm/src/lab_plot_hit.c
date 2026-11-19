/**
 * @file lab_plot_hit.c
 * @brief Wheel admission within the visible native plot viewport or projected volume.
 *
 * Screen coordinates are inverted through the chart's current SVG transform,
 * including responsive fitting, expansion, scrolling and CSS rotation. Curves
 * use their plotting rectangle; retained 3D charts use the current native camera
 * frame's projected box. Titles, labels and surrounding card margins do not
 * capture wheel gestures. No sampled geometry or mathematical source is inspected.
 */
#include <math.h>

#include "lab_dom.h"
#include "lab_surface_geometry.h"
#include "lab_plot_pointer.h"

/* Admit wheel gestures only where plot coordinates are actually displayed. */
bool lab_plot_pointer_inside(chart_jsref chart, event_jsref event)
{
    if (!chart || !event)
        return false;
    double a = lab_dom_measure(chart, LAB_DOM_MEASURE_CTM_A), b = lab_dom_measure(chart, LAB_DOM_MEASURE_CTM_B);
    double c = lab_dom_measure(chart, LAB_DOM_MEASURE_CTM_C), d = lab_dom_measure(chart, LAB_DOM_MEASURE_CTM_D);
    double e = lab_dom_measure(chart, LAB_DOM_MEASURE_CTM_E), f = lab_dom_measure(chart, LAB_DOM_MEASURE_CTM_F);
    double px = lab_dom_to_number(lab_dom_get(event, "clientX")) - e;
    double py = lab_dom_to_number(lab_dom_get(event, "clientY")) - f;
    double determinant = a * d - b * c;
    if (!isfinite(determinant) || !determinant || !isfinite(px) || !isfinite(py))
        return false;
    double x = (px * d - py * c) / determinant, y = (py * a - px * b) / determinant;
    if (!isfinite(x) || !isfinite(y))
        return false;
    if (lab_dom_has(chart, "data-surface"))
        return lab_surface_contains(lab_dom_get(chart, "marsSurfaceFrame"), x, y);
    double left = lab_dom_to_number(lab_dom_read(chart, LAB_DOM_DATASET, "plotLeft"));
    double top = lab_dom_to_number(lab_dom_read(chart, LAB_DOM_DATASET, "plotTop"));
    double width = lab_dom_to_number(lab_dom_read(chart, LAB_DOM_DATASET, "plotWidth"));
    double height = lab_dom_to_number(lab_dom_read(chart, LAB_DOM_DATASET, "plotHeight"));
    return width > 0 && height > 0 && x >= left && x <= left + width && y >= top && y <= top + height;
}
