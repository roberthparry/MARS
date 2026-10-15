/**
 * @file lab_worker_equation_contour.c
 * @brief Bounded real implicit-curve sampling for two-coordinate Lab equations.
 *
 * Evaluates the native residual on a triangular grid and refines sign-changing
 * edges before handing disconnected segments to plot_t. Non-finite values and
 * pole-like crossings are rejected. A coarse expanding search frames discovered
 * components; finite sampling cannot certify completeness or find every isolated
 * zero or very small component. This isolated worker never rewrites browser text.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MARS_LAB_WORKER_INTERNAL_ACCESS
#include "lab_worker_internal.h"

typedef struct {
    double x, y, value;
} contour_vertex_t;

typedef struct {
    lab_worker_implicit_t *sampling;
    plot_point_t *points;
    size_t count, capacity;
    double extent[4];
} contour_t;

static double evaluate(contour_t *contour, double x, double y)
{
    double coordinates[] = {x, y};
    return lab_worker_implicit_eval(contour->sampling, coordinates);
}

static void clear_points(contour_t *contour)
{
    for (size_t i = 0; i < contour->count; ++i) {
        num_destroy(&contour->points[i].x);
        num_destroy(&contour->points[i].y);
    }
    contour->count = 0;
    contour->extent[0] = contour->extent[2] = INFINITY;
    contour->extent[1] = contour->extent[3] = -INFINITY;
}

static bool crossing(contour_t *contour, contour_vertex_t a, contour_vertex_t b, contour_vertex_t *point)
{
    if (!isfinite(a.value) || !isfinite(b.value) || ((a.value > 0) == (b.value > 0)))
        return false;
    double scale = fmax(fabs(a.value), fabs(b.value));
    contour_vertex_t best = fabs(a.value) < fabs(b.value) ? a : b;
    for (unsigned i = 0; i < 18 && best.value != 0; ++i) {
        contour_vertex_t mid = {.x = (a.x + b.x) / 2, .y = (a.y + b.y) / 2};
        mid.value = evaluate(contour, mid.x, mid.y);
        if (!isfinite(mid.value))
            return false;
        if (fabs(mid.value) < fabs(best.value))
            best = mid;
        if ((mid.value > 0) == (a.value > 0))
            a = mid;
        else
            b = mid;
    }
    /* A sign change across a pole does not converge to a residual zero. */
    if (fabs(best.value) > scale * 0.0001)
        return false;
    *point = best;
    return true;
}

static bool triangle(contour_t *contour, const contour_vertex_t vertices[3])
{
    contour_vertex_t ends[2];
    size_t count = 0;
    for (unsigned i = 0; i < 3 && count < 2; ++i)
        if (crossing(contour, vertices[i], vertices[(i + 1) % 3], &ends[count]))
            ++count;
    if (count != 2 || (ends[0].x == ends[1].x && ends[0].y == ends[1].y))
        return true;
    if (contour->count + 2 > contour->capacity)
        return false;
    for (unsigned i = 0; i < 2; ++i) {
        contour->points[contour->count++] = (plot_point_t){lab_worker_implicit_coordinate(contour->sampling, 0, ends[i].x),
                                                         lab_worker_implicit_coordinate(contour->sampling, 1, ends[i].y),
                                                         i == 0};
        contour->extent[0] = fmin(contour->extent[0], ends[i].x);
        contour->extent[1] = fmax(contour->extent[1], ends[i].x);
        contour->extent[2] = fmin(contour->extent[2], ends[i].y);
        contour->extent[3] = fmax(contour->extent[3], ends[i].y);
    }
    return true;
}

/* Refine cells whose curvature could hide a closed component between grid vertices. */
static bool cell(contour_t *contour, const contour_vertex_t corners[4], unsigned depth)
{
    contour_vertex_t centre = {.x = (corners[0].x + corners[2].x) / 2,
                               .y = (corners[0].y + corners[2].y) / 2};
    centre.value = evaluate(contour, centre.x, centre.y);
    double minimum = fabs(centre.value), average = 0;
    bool same = true, finite = isfinite(centre.value);
    for (unsigned i = 0; i < 4; ++i) {
        minimum = fmin(minimum, fabs(corners[i].value));
        average += corners[i].value / 4;
        same &= (corners[i].value > 0) == (centre.value > 0);
        finite &= isfinite(corners[i].value);
    }
    double curvature = fabs(centre.value - average);
    if (depth && finite && (!same || (curvature > 0 && minimum <= 2 * curvature))) {
        contour_vertex_t middle[4];
        for (unsigned i = 0; i < 4; ++i) {
            unsigned next = (i + 1) % 4;
            middle[i] = (contour_vertex_t){.x = (corners[i].x + corners[next].x) / 2,
                                           .y = (corners[i].y + corners[next].y) / 2};
            middle[i].value = evaluate(contour, middle[i].x, middle[i].y);
        }
        for (unsigned i = 0; i < 4; ++i) {
            contour_vertex_t child[] = {corners[i], middle[i], centre, middle[(i + 3) % 4]};
            if (!cell(contour, child, depth - 1))
                return false;
        }
        return true;
    }
    for (unsigned i = 0; i < 4; ++i) {
        contour_vertex_t vertices[] = {corners[i], corners[(i + 1) % 4], centre};
        if (!triangle(contour, vertices))
            return false;
    }
    return true;
}

static bool sample(contour_t *contour, const double bounds[4], size_t side)
{
    clear_points(contour);
    contour_vertex_t *grid = calloc(side * side, sizeof(*grid));
    if (!grid)
        return false;
    for (size_t row = 0; row < side; ++row) {
        double y = bounds[2] + (bounds[3] - bounds[2]) * row / (side - 1);
        for (size_t col = 0; col < side; ++col) {
            double x = bounds[0] + (bounds[1] - bounds[0]) * col / (side - 1);
            grid[row * side + col] = (contour_vertex_t){x, y, evaluate(contour, x, y)};
        }
    }
    bool ok = true;
    for (size_t row = 0; ok && row + 1 < side; ++row) {
        for (size_t col = 0; ok && col + 1 < side; ++col) {
            size_t at = row * side + col;
            contour_vertex_t corners[] = {grid[at], grid[at + 1], grid[at + side + 1], grid[at + side]};
            ok = cell(contour, corners, side == 33 ? 4 : 2);
        }
    }
    free(grid);
    return ok;
}

/* Sample only a private worker equation; restore its binding values and numerical precision on every exit. */
int lab_worker_equation_contour(equation_t *equation, expr_t *first, expr_t *second, char **argv)
{
    size_t saved_precision = num_get_default_prec_bits();
    contour_t contour = {.capacity = 65536};
    contour.points = calloc(contour.capacity, sizeof(*contour.points));
    double bounds[] = {-2, 2, -2, 2};
    bool manual = !strcmp(argv[7], "manual");
    bool fixed_x = *argv[5] || *argv[6];
    const char *names[] = {expr_symbol_name(first), expr_symbol_name(second)};
    const char *supplied[] = {argv[5], argv[6], manual ? argv[8] : "", manual ? argv[9] : ""};
    contour.sampling = lab_worker_implicit_new(argv[1], names, supplied, 2, bounds);
    num_set_default_prec_bits(lab_worker_implicit_precision(contour.sampling));
    bool ok = contour.sampling && contour.points &&
              (!strcmp(argv[7], "auto") || !strcmp(argv[7], "all") || manual) &&
              (!manual || (*argv[8] && *argv[9]));
    ok = ok && isfinite(bounds[0]) && isfinite(bounds[1]) && bounds[0] < bounds[1] && bounds[2] < bounds[3];
    bool valid_bounds = ok;
    double centre[] = {(bounds[0] + bounds[1]) / 2, (bounds[2] + bounds[3]) / 2};
    for (unsigned attempt = 0; ok && attempt < 5; ++attempt) {
        ok = sample(&contour, bounds, 33);
        if (contour.count || (fixed_x && manual))
            break;
        if (!fixed_x) {
            bounds[0] = centre[0] + (bounds[0] - centre[0]) * 4;
            bounds[1] = centre[0] + (bounds[1] - centre[0]) * 4;
        } else if (!*argv[5]) {
            bounds[0] = bounds[1] - (bounds[1] - bounds[0]) * 4;
        } else if (!*argv[6]) {
            bounds[1] = bounds[0] + (bounds[1] - bounds[0]) * 4;
        }
        if (!manual) {
            bounds[2] = centre[1] + (bounds[2] - centre[1]) * 4;
            bounds[3] = centre[1] + (bounds[3] - centre[1]) * 4;
        }
    }
    if (ok && contour.count && !fixed_x && !manual) {
        double span = fmax(contour.extent[1] - contour.extent[0], contour.extent[3] - contour.extent[2]);
        if (span > 0) {
            for (unsigned axis = 0; axis < 2; ++axis) {
                double centre = (contour.extent[2 * axis] + contour.extent[2 * axis + 1]) / 2;
                bounds[2 * axis] = centre - span * 0.6;
                bounds[2 * axis + 1] = centre + span * 0.6;
            }
        }
    }
    if (ok)
        ok = sample(&contour, bounds, !strcmp(argv[17], "preview") ? 33 : 129) && contour.count;
    plot_options_t options = plot_options_default();
    options.width = 600;
    options.height = 584;
    options.x_label = expr_symbol_name(first);
    options.y_label = expr_symbol_name(second);
    options.legend = false;
    options.axis_colour = 0x94b8b1;
    options.grid_colour = 0x82aaa3;
    options.label_colour = 0xb8ccc5;
    plot_t *plot = ok ? plot_new(&options) : NULL;
    number_t limits[4];
    for (unsigned i = 0; i < 4; ++i)
        limits[i] = lab_worker_implicit_coordinate(contour.sampling, i / 2, bounds[i]);
    ok = plot && plot_add_points(plot, contour.points, contour.count, argv[1], PLOT_LINES, 0x78d8cb) &&
         plot_set_bounds(plot, limits[0], limits[1], limits[2], limits[3]);
    string_t *svg = ok ? plot_to_svg(plot) : NULL;
    char *title = equ_to_TeX_body_wrapped(equation, 110);
    if (svg)
        printf("input %s\nexpression %s\nplot_title %s\nplot_title_TeX %s\nplot_svg %s\ndifferentiable no\n",
               argv[1], argv[1], argv[1], title ? title : "", string_c_str(svg));
    else
        printf("error %s\n", valid_bounds ? "No real contour found in the sampled region; try explicit coordinate bounds."
                                         : "Supply finite ordered coordinate bounds and bind remaining parameters.");
    int rc = svg ? 0 : 1;
    free(title);
    string_free(svg);
    plot_free(plot);
    clear_points(&contour);
    free(contour.points);
    lab_worker_implicit_free(contour.sampling);
    for (unsigned i = 0; i < 4; ++i)
        num_destroy(&limits[i]);
    num_set_default_prec_bits(saved_precision);
    return rc;
}
