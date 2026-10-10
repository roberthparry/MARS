/**
 * @file lab_view.c
 * @brief Mode presentation and result geometry policy for the C browser application.
 *
 * Owns mode titles, card visibility, control availability, per-card zoom and expansion state and
 * responsive variant selection, popup placement and conditional editor resizing.
 * The host supplies measured browser geometry
 * and applies returned decisions to the DOM. No text, SVG or mathematics is
 * parsed here. Static strings are borrowed for the lifetime of the module.
 */
#include "lab_view.h"

typedef struct {
    const char *title, *subtitle, *rendered, *parsed, *function, *value;
    unsigned flags;
} lab_view_mode_t;

enum { lab_view_aux = 1, lab_view_value = 2, lab_view_value_if_present = 4, lab_view_calculus = 8, lab_view_goal = 16 };

static const char lab_view_expression_subtitle[] =
    "Switch between expression, equation, differential-equation, matrix, and integrator experiments. "
    "Each mode runs through a local MARS worker binary and shows the result on the right.";
static const char lab_view_equation_subtitle[] = "Enter an equation on the left. The lab tries symbolic isolation "
                                                 "first, then numeric solving for all variable bindings.";
static const char lab_view_diffequation_subtitle[] =
    "Enter an ordinary differential equation and optional initial or boundary conditions. MARS selects a symbolic "
    "solver "
    "family and preserves arbitrary constants when conditions are absent.";
static const char lab_view_matrix_subtitle[] =
    "Enter a complete matrix expression on the left, press Evaluate, and inspect its TeX, expression, function, "
    "and numerical value.";
static const char lab_view_integrator_subtitle[] = "Enter an integrand expression on the left, stack one or more "
                                                   "integral rows, and use Free when a symbol should stay "
                                                   "as a parameter. Leave both bounds blank for an antiderivative, or "
                                                   "leave lower blank and fill upper to evaluate it there.";
static const char lab_view_datetime_subtitle[] =
    "Choose dates, a year, and a location. MARS datetime calculates calendar observances, moon phase, solar times, "
    "and optional local weather, with jurisdiction holidays added when available.";
static const char lab_view_almanac_subtitle[] =
    "Enter date and time in GMT, then zone, latitude, and longitude. The live almanac engine covers ";

static const lab_view_mode_t lab_view_modes[] = {
    {"Expression", lab_view_expression_subtitle, "Rendered TeX", "Expression", "Function", "Value",
     lab_view_aux | lab_view_value_if_present | lab_view_calculus | lab_view_goal},
    {"Equation", lab_view_equation_subtitle, "Rendered TeX", "Equation", "Function", "Solutions",
     lab_view_aux | lab_view_value},
    {"Differential Equation", lab_view_diffequation_subtitle, "Solution", "Differential Equation", "Solver",
     "Solutions", lab_view_aux | lab_view_value},
    {"Matrix", lab_view_matrix_subtitle, "Rendered TeX", "Expression", "Function", "Value",
     lab_view_aux | lab_view_calculus},
    {"Integrator", lab_view_integrator_subtitle, "Rendered TeX", "Integrand", "Exact result", "Integral",
     lab_view_aux | lab_view_value},
    {"Datetime", lab_view_datetime_subtitle, "Overview", "Date Range", "Calendar", "Solar And Moon",
     lab_view_aux | lab_view_value},
    {"Almanac", lab_view_almanac_subtitle, "Worksheet", "", "", "", 0}};
static const double lab_view_zooms[] = {0.5, 0.67, 0.8, 1, 1.25, 1.5, 2, 3, 4, 6, 8};
static unsigned lab_view_card_zooms[64], lab_view_card_count;
static int lab_view_expanded_card = -1;

/* Registration is bounded and explicit; the host retains DOM references, never state ownership. */
int lab_view_cards_reset(unsigned count)
{
    if (count > sizeof(lab_view_card_zooms) / sizeof(*lab_view_card_zooms))
        return 0;
    lab_view_card_count = count;
    lab_view_expanded_card = -1;
    for (unsigned i = 0; i < count; ++i)
        lab_view_card_zooms[i] = 3;
    return 1;
}

/* Expose the selected table index rather than a mutable host mirror. */
int lab_view_card_zoom(unsigned card)
{
    return card < lab_view_card_count ? (int)lab_view_card_zooms[card] : -1;
}

/* Clamp through the same policy used by ordinary zoom arithmetic. */
int lab_view_card_set_zoom(unsigned card, double index)
{
    if (card >= lab_view_card_count)
        return -1;
    return (int)(lab_view_card_zooms[card] = lab_view_zoom_index(index));
}

/* Step the owned state directly; browser attributes cannot change the starting point. */
int lab_view_card_step_zoom(unsigned card, int direction)
{
    if (card >= lab_view_card_count)
        return -1;
    return (int)(lab_view_card_zooms[card] = lab_view_zoom_step(lab_view_card_zooms[card], direction));
}

/* A single selected index makes simultaneous expansion impossible. */
int lab_view_card_toggle(unsigned card)
{
    if (card >= lab_view_card_count)
        return -2;
    lab_view_expanded_card = lab_view_expanded_card == (int)card ? -1 : (int)card;
    return lab_view_expanded_card;
}

/* Return the controller's selection, not a DOM-derived guess. */
int lab_view_card_expanded(void)
{
    return lab_view_expanded_card;
}

/* Clearing a result or changing its visibility leaves the user's zoom untouched. */
void lab_view_cards_collapse(void)
{
    lab_view_expanded_card = -1;
}

static int lab_view_finite(double value)
{
    return value == value && value <= 1.7976931348623157e308 && value >= -1.7976931348623157e308;
}

/* Resolve editor resize policy without retaining browser handles or interpreting text. */
const double *lab_view_editor_resize(double viewport_height, unsigned visible_count, double scroll_height,
                                     double client_height, double current_height, double automatic_height, int manual)
{
    static double result[3];
    if (!lab_view_finite(viewport_height) || !lab_view_finite(scroll_height) || !lab_view_finite(client_height) ||
        !lab_view_finite(current_height) || !lab_view_finite(automatic_height) || viewport_height < 0 ||
        scroll_height < 0 || client_height < 0 || current_height < 0 || automatic_height < 0)
        return 0;
    double budget = viewport_height * 0.35;
    budget = budget < 96 ? 96 : (budget > 320 ? 320 : budget);
    budget /= visible_count ? visible_count : 1;
    int limited = scroll_height > client_height + 1;
    double base = manual ? automatic_height : current_height;
    double difference = current_height - base;
    int resized = difference > 2 || difference < -2;
    int enabled = limited || (manual && resized);
    double maximum = base + budget;
    if (!lab_view_finite(maximum))
        return 0;
    result[0] = enabled ? 3 : 0;
    result[1] = base;
    result[2] = maximum;
    return result;
}

/* Position the tooltip from measured rectangles, preserving its viewport edge policy. */
const double *lab_view_tooltip_rect(double viewport_width, double viewport_height, double anchor_left,
                                    double anchor_top, double anchor_width, double anchor_bottom, double tooltip_width,
                                    double tooltip_height)
{
    static double rect[2];
    if (!lab_view_finite(viewport_width) || !lab_view_finite(viewport_height) || !lab_view_finite(anchor_left) ||
        !lab_view_finite(anchor_top) || !lab_view_finite(anchor_width) || !lab_view_finite(anchor_bottom) ||
        !lab_view_finite(tooltip_width) || !lab_view_finite(tooltip_height) || viewport_width < 0 ||
        viewport_height < 0 || anchor_width < 0 || tooltip_width < 0 || tooltip_height < 0)
        return 0;
    double centre = anchor_left + (anchor_width - tooltip_width) / 2;
    double right = viewport_width - tooltip_width - 8;
    double left = centre < right ? centre : right;
    double below = anchor_bottom + 8;
    double top = below;
    if (below + tooltip_height > viewport_height - 8) {
        top = anchor_top - tooltip_height - 8;
        if (top < 8)
            top = 8;
    }
    if (!lab_view_finite(left) || !lab_view_finite(top))
        return 0;
    rect[0] = left > 8 ? left : 8;
    rect[1] = top;
    return rect;
}

/* Resolve popup placement from measured geometry, leaving browser measurement and styles to the host. */
const double *lab_view_picker_rect(double viewport_width, double viewport_height, double anchor_left,
                                   double anchor_width, double anchor_bottom)
{
    static double rect[4];
    if (!lab_view_finite(viewport_width) || !lab_view_finite(viewport_height) || !lab_view_finite(anchor_left) ||
        !lab_view_finite(anchor_width) || !lab_view_finite(anchor_bottom) || viewport_width < 0 ||
        viewport_height < 0 || anchor_width < 0)
        return 0;
    double maximum = viewport_width > 24 ? viewport_width - 24 : 0;
    double minimum = maximum < 448 ? maximum : 448;
    double width = anchor_width > minimum ? anchor_width : minimum;
    if (width > maximum)
        width = maximum;
    double right = viewport_width - width - 12;
    double left = anchor_left < right ? anchor_left : right;
    double top = anchor_bottom + 8;
    double height = viewport_height - top - 12;
    rect[0] = width;
    rect[1] = left > 12 ? left : 12;
    rect[2] = top;
    rect[3] = height > 8 ? height : 8;
    return rect;
}

/* Borrow a mode label; invalid modes use Expression and invalid fields use an empty label. */
const char *lab_view_text(unsigned mode, unsigned field)
{
    const lab_view_mode_t *item = &lab_view_modes[mode < 7 ? mode : 0];
    const char *const fields[] = {item->title,  item->subtitle, item->rendered,
                                  item->parsed, item->function, item->value};
    return field < 6 ? fields[field] : "";
}

/* Measure a static mode label without exposing an unbounded host memory read. */
unsigned lab_view_text_length(unsigned mode, unsigned field)
{
    const char *text = lab_view_text(mode, field);
    unsigned length = 0;
    while (text[length])
        ++length;
    return length;
}

/* Return resolved card/action flags, using the caller's measured value presence. */
unsigned lab_view_mode_flags(unsigned mode, int has_value)
{
    unsigned flags = lab_view_modes[mode < 7 ? mode : 0].flags;
    if ((flags & lab_view_value_if_present) && has_value)
        flags |= lab_view_value;
    return flags;
}

/* Report the length of the fixed, ordered zoom table. */
unsigned lab_view_zoom_count(void)
{
    return sizeof(lab_view_zooms) / sizeof(*lab_view_zooms);
}

/* Round and saturate a requested index, with a non-finite request selecting 100%. */
unsigned lab_view_zoom_index(double index)
{
    if (!lab_view_finite(index))
        return 3;
    return index <= 0 ? 0 : index >= lab_view_zoom_count() - 1 ? lab_view_zoom_count() - 1 : (unsigned)(index + 0.5);
}

/* Return a scale for a requested zoom index. */
double lab_view_zoom(double index)
{
    return lab_view_zooms[lab_view_zoom_index(index)];
}

/* Step through the fixed zoom table without wrapping. */
unsigned lab_view_zoom_step(double index, int direction)
{
    return lab_view_zoom_index((double)lab_view_zoom_index(index) + (direction < 0 ? -1 : 1));
}

/* Fit a matrix at its base scale, then preserve the user's requested zoom factor. */
double lab_view_matrix_scale(double base, double zoom, double available, double width)
{
    if (!lab_view_finite(base) || base <= 0 || !lab_view_finite(zoom) || zoom <= 0 || !lab_view_finite(available) ||
        !lab_view_finite(width) || width <= 0)
        return 1;
    double fitted = (available > 1 ? available : 1) / width;
    return (base < fitted ? base : fitted) * zoom;
}

/* Choose the native wrapped rendering only when its compact counterpart exceeds the measured card. */
int lab_view_wrapped(double width, double scale, double available, int has_variant)
{
    return has_variant && lab_view_finite(width) && lab_view_finite(scale) && lab_view_finite(available) && width > 0 &&
           scale > 0 && width * scale > (available > 0 ? available : 0) + 1;
}

/* Return enabled button bits for run, back, forward, goal, less and more precision. */
unsigned lab_view_controls(unsigned mode, int busy, int ready, unsigned back, unsigned forward, int can_goal,
                           int minimum, int maximum)
{
    if (busy)
        return 0;
    return (!!ready) | ((back > 0) << 1) | ((forward > 0) << 2) | ((mode == 0 && can_goal) << 3) | ((!minimum) << 4) |
           ((!maximum) << 5);
}
