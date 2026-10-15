/**
 * @file lab_worker_equation_mesh.c
 * @brief Bounded native isosurfaces for equations with three free real coordinates.
 *
 * A tetrahedral decomposition of a sampled volume locates residual zero crossings.
 * Refined edges reject poles rather than drawing them as solutions. The resulting
 * independent facets use the shared plot camera and renderer. Finite sampling is
 * illustrative, not a completeness or existence proof for arbitrary equations.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MARS_LAB_WORKER_INTERNAL_ACCESS
#include "lab_worker_internal.h"

typedef struct {
    double coordinate[3], value;
} mesh_vertex_t;

typedef struct {
    expr_t *residual, *variables[3];
    number_t *coordinates[3];
    size_t count, capacity;
} equation_mesh_t;

static double evaluate(equation_mesh_t *mesh, const double coordinate[3])
{
    for (unsigned axis = 0; axis < 3; ++axis) {
        number_t value = num_create_from_double(coordinate[axis]);
        expr_set_val(mesh->variables[axis], value);
        num_destroy(&value);
    }
    number_t value = expr_eval(mesh->residual);
    double result = num_is_real(value) && num_is_finite(value) ? num_to_double(value) : NAN;
    num_destroy(&value);
    return result;
}

static bool edge(equation_mesh_t *mesh, mesh_vertex_t a, mesh_vertex_t b, mesh_vertex_t *out)
{
    double scale = fmax(fabs(a.value), fabs(b.value));
    *out = fabs(a.value) < fabs(b.value) ? a : b;
    for (unsigned step = 0; step < 14 && out->value != 0; ++step) {
        mesh_vertex_t middle = {0};
        for (unsigned axis = 0; axis < 3; ++axis)
            middle.coordinate[axis] = (a.coordinate[axis] + b.coordinate[axis]) / 2;
        middle.value = evaluate(mesh, middle.coordinate);
        if (!isfinite(middle.value))
            return false;
        if (fabs(middle.value) < fabs(out->value))
            *out = middle;
        if ((middle.value > 0) == (a.value > 0))
            a = middle;
        else
            b = middle;
    }
    return fabs(out->value) <= scale * 0.001;
}

static bool tetrahedron(equation_mesh_t *mesh, const mesh_vertex_t vertices[4])
{
    unsigned positive[4], negative[4], np = 0, nn = 0;
    for (unsigned i = 0; i < 4; ++i) {
        if (!isfinite(vertices[i].value))
            return true;
        if (vertices[i].value > 0)
            positive[np++] = i;
        else
            negative[nn++] = i;
    }
    if (!np || !nn)
        return true;
    mesh_vertex_t polygon[4];
    unsigned count = np == 2 ? 4 : 3;
    for (unsigned i = 0; i < count; ++i) {
        unsigned a = np == 1 ? positive[0] : np == 3 ? positive[i] : positive[i / 2];
        unsigned b = nn == 1 ? negative[0] : nn == 3 ? negative[i] : negative[(i == 1 || i == 2) ? 1 : 0];
        if (!edge(mesh, vertices[a], vertices[b], &polygon[i]))
            return true;
    }
    if (mesh->count + 3 * (count - 2) > mesh->capacity)
        return false;
    for (unsigned triangle = 1; triangle + 1 < count; ++triangle) {
        unsigned indices[] = {0, triangle, triangle + 1};
        for (unsigned i = 0; i < 3; ++i) {
            for (unsigned axis = 0; axis < 3; ++axis)
                mesh->coordinates[axis][mesh->count] = num_create_from_double(polygon[indices[i]].coordinate[axis]);
            ++mesh->count;
        }
    }
    return true;
}

static bool sample(equation_mesh_t *mesh, const double bounds[6], unsigned side)
{
    size_t count = (size_t)side * side * side;
    mesh_vertex_t *grid = calloc(count, sizeof(*grid));
    if (!grid)
        return false;
    for (unsigned z = 0; z < side; ++z)
        for (unsigned y = 0; y < side; ++y)
            for (unsigned x = 0; x < side; ++x) {
                unsigned indices[] = {x, y, z};
                mesh_vertex_t *v = &grid[((size_t)z * side + y) * side + x];
                for (unsigned axis = 0; axis < 3; ++axis)
                    v->coordinate[axis] = bounds[2 * axis] +
                        (bounds[2 * axis + 1] - bounds[2 * axis]) * indices[axis] / (side - 1);
                v->value = evaluate(mesh, v->coordinate);
            }
    static const unsigned tetrahedra[][4] = {
        {0, 1, 3, 7}, {0, 3, 2, 7}, {0, 2, 6, 7}, {0, 6, 4, 7}, {0, 4, 5, 7}, {0, 5, 1, 7}
    };
    bool ok = true;
    for (unsigned z = 0; ok && z + 1 < side; ++z)
        for (unsigned y = 0; ok && y + 1 < side; ++y)
            for (unsigned x = 0; ok && x + 1 < side; ++x) {
                size_t base = ((size_t)z * side + y) * side + x;
                size_t offset[] = {0, 1, side, side + 1, side * side, side * side + 1,
                                   side * side + side, side * side + side + 1};
                for (unsigned t = 0; ok && t < 6; ++t) {
                    mesh_vertex_t vertices[4];
                    for (unsigned i = 0; i < 4; ++i)
                        vertices[i] = grid[base + offset[tetrahedra[t][i]]];
                    ok = tetrahedron(mesh, vertices);
                }
            }
    free(grid);
    return ok;
}

static bool read_bound(const char *text, double *out)
{
    if (!*text)
        return true;
    expr_t *expr = expr_from_string(text, NULL);
    number_t value = expr ? expr_eval(expr) : NUM_NAN;
    bool ok = num_is_real(value) && num_is_finite(value) && isfinite(num_to_double(value));
    if (ok)
        *out = num_to_double(value);
    num_destroy(&value);
    expr_free(expr);
    return ok;
}

/* Render an implicit three-coordinate residual without choosing a height-function branch. */
int lab_worker_equation_mesh(equation_t *equation, expr_t *first, expr_t *second, expr_t *third, char **argv)
{
    double view[3];
    for (unsigned i = 0; i < 3; ++i) {
        char *end = NULL;
        view[i] = strtod(argv[14 + i], &end);
        if (end == argv[14 + i] || *end || !isfinite(view[i]) || (i == 1 && (view[i] < 5 || view[i] > 85))) {
            printf("error Use a finite azimuth and roll, and an elevation from 5 to 85 degrees.\n");
            return 1;
        }
    }
    equation_mesh_t mesh = {.residual = expr_sub(equ_lhs(equation), equ_rhs(equation)),
                            .variables = {first, second, third}, .capacity = 196608};
    size_t precision = num_get_default_prec_bits();
    number_t saved[] = {expr_eval(first), expr_eval(second), expr_eval(third)};
    num_set_default_prec_bits(53);
    double bounds[] = {-2, 2, -2, 2, -2, 2};
    bool manual = !strcmp(argv[7], "manual");
    const char *supplied[] = {argv[5], argv[6], argv[12], argv[13], manual ? argv[8] : "", manual ? argv[9] : ""};
    bool ok = mesh.residual && (manual || !strcmp(argv[7], "auto") || !strcmp(argv[7], "all"));
    for (unsigned axis = 0; axis < 3; ++axis) {
        mesh.coordinates[axis] = calloc(mesh.capacity, sizeof(number_t));
        ok = ok && mesh.coordinates[axis] && read_bound(supplied[2 * axis], &bounds[2 * axis]) &&
             read_bound(supplied[2 * axis + 1], &bounds[2 * axis + 1]) && bounds[2 * axis] < bounds[2 * axis + 1];
    }
    if (ok)
        ok = sample(&mesh, bounds, !strcmp(argv[17], "preview") ? 17 : 33) && mesh.count;
    /* Frame the discovered surface, without replacing any authored coordinate interval. */
    for (unsigned axis = 0; ok && axis < 3; ++axis) {
        if (*supplied[2 * axis] || *supplied[2 * axis + 1])
            continue;
        double lower = INFINITY, upper = -INFINITY;
        for (size_t i = 0; i < mesh.count; ++i) {
            double value = num_to_double(mesh.coordinates[axis][i]);
            lower = fmin(lower, value);
            upper = fmax(upper, value);
        }
        double padding = fmax((upper - lower) * 0.1, 0.1);
        bounds[2 * axis] = lower - padding;
        bounds[2 * axis + 1] = upper + padding;
    }
    plot_options_t options = plot_options_default();
    options.width = 800;
    options.height = 650;
    options.x_label = expr_symbol_name(first);
    options.y_label = expr_symbol_name(second);
    options.z_label = expr_symbol_name(third);
    options.axis_colour = 0x94b8b1;
    options.label_colour = 0xb8ccc5;
    plot_t *plot = ok ? plot_new(&options) : NULL;
    ok = plot && plot_set_mesh(plot, mesh.coordinates[0], mesh.coordinates[1], mesh.coordinates[2], mesh.count) &&
         plot_set_view(plot, view[0], view[1]) && plot_set_roll(plot, view[2]);
    number_t limits[6];
    for (unsigned i = 0; i < 6; ++i)
        limits[i] = num_create_from_double(bounds[i]);
    ok = ok && plot_set_bounds(plot, limits[0], limits[1], limits[2], limits[3]) &&
         plot_set_height_bounds(plot, limits[4], limits[5]);
    string_t *svg = ok ? plot_to_svg(plot) : NULL;
    char *title = equ_to_TeX_body_wrapped(equation, 110);
    if (svg)
        printf("input %s\nexpression %s\nplot_title %s\nplot_title_TeX %s\nplot_svg %s\ndifferentiable no\n",
               argv[1], argv[1], argv[1], title ? title : "", string_c_str(svg));
    else
        printf("error No real isosurface found in the sampled volume; supply finite ordered coordinate bounds.\n");
    int rc = svg ? 0 : 1;
    free(title);
    string_free(svg);
    plot_free(plot);
    expr_free(mesh.residual);
    for (unsigned i = 0; i < 6; ++i)
        num_destroy(&limits[i]);
    for (unsigned axis = 0; axis < 3; ++axis) {
        for (size_t i = 0; i < mesh.count; ++i)
            num_destroy(&mesh.coordinates[axis][i]);
        free(mesh.coordinates[axis]);
        expr_set_val(mesh.variables[axis], saved[axis]);
        num_destroy(&saved[axis]);
    }
    num_set_default_prec_bits(precision);
    return rc;
}
