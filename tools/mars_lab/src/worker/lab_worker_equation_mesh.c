/**
 * @file lab_worker_equation_mesh.c
 * @brief Bounded native isosurfaces for equations with three free real coordinates.
 *
 * A tetrahedral decomposition of a sampled volume locates residual zero crossings.
 * Refined edges reject poles rather than drawing them as solutions. The resulting
 * independent facets use the shared plot camera and renderer. Finite sampling is
 * illustrative, not a completeness or existence proof for arbitrary equations.
 */
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MARS_LAB_WORKER_INTERNAL_ACCESS
#include "lab_worker_internal.h"

typedef struct {
    double coordinate[3], value;
    size_t index;
} mesh_vertex_t;

typedef struct {
    mesh_vertex_t vertex;
    unsigned char state;
} mesh_edge_t;

typedef struct {
    lab_worker_implicit_t *sampling;
    double extent[6];
    number_t *coordinates[3];
    size_t count, capacity;
    mesh_edge_t *edges;
    unsigned side;
    bool overflow, excessive_levels;
} equation_mesh_t;

static double evaluate(equation_mesh_t *mesh, const double coordinate[3])
{
    return lab_worker_implicit_eval(mesh->sampling, coordinate);
}

static bool refine_edge(equation_mesh_t *mesh, mesh_vertex_t a, mesh_vertex_t b, mesh_vertex_t *out)
{
    /* Require improvement over the better endpoint: a large pole must not make
     * the opposite endpoint look like an already-converged zero. */
    double scale = fmin(fabs(a.value), fabs(b.value));
    double rounding = 32 * DBL_EPSILON * fmin(1.0, fmax(fabs(a.value), fabs(b.value)));
    double tolerance = fmax(scale * 0.0001, rounding);
    *out = fabs(a.value) < fabs(b.value) ? a : b;
    /* Near an already-small endpoint, fourteen bisections can exhaust the budget
     * before a smooth crossing converges. Keep the residual test, but allow enough
     * bracket refinement for binary64 coordinates. Ordinary edges still exit early. */
    for (unsigned step = 0; step < 52 && fabs(out->value) > tolerance; ++step) {
        mesh_vertex_t middle = {0};
        double weight = a.value / (a.value - b.value);
        if (!isfinite(weight) || weight < 0.1 || weight > 0.9)
            weight = 0.5;
        for (unsigned axis = 0; axis < 3; ++axis)
            middle.coordinate[axis] = a.coordinate[axis] + weight * (b.coordinate[axis] - a.coordinate[axis]);
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
    /* A nearly exact grid endpoint cannot improve below evaluation round-off.
     * Scale the floor down for small residuals and cap it so a large pole cannot
     * inflate the acceptance threshold at the other endpoint. */
    return fabs(out->value) <= fmax(scale * 0.001, rounding);
}

static bool edge(equation_mesh_t *mesh, mesh_vertex_t a, mesh_vertex_t b, mesh_vertex_t *out)
{
    if (a.index > b.index) {
        mesh_vertex_t swap = a;
        a = b;
        b = swap;
    }
    /* This tetrahedral split uses the seven positive cube directions only. */
    size_t delta = b.index - a.index;
    size_t direction = delta % mesh->side + 2 * ((delta / mesh->side) % mesh->side) +
                       4 * (delta / ((size_t)mesh->side * mesh->side));
    mesh_edge_t *cached = &mesh->edges[a.index * 7 + direction - 1];
    if (!cached->state)
        cached->state = refine_edge(mesh, a, b, &cached->vertex) ? 1 : 2;
    *out = cached->vertex;
    return cached->state == 1;
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
    if (mesh->count + 3 * (count - 2) > mesh->capacity) {
        mesh->overflow = true;
        return false;
    }
    for (unsigned triangle = 1; triangle + 1 < count; ++triangle) {
        unsigned indices[] = {0, triangle, triangle + 1};
        for (unsigned i = 0; i < 3; ++i) {
            for (unsigned axis = 0; axis < 3; ++axis) {
                double value = polygon[indices[i]].coordinate[axis];
                mesh->coordinates[axis][mesh->count] = lab_worker_implicit_coordinate(mesh->sampling, axis, value);
                mesh->extent[2 * axis] = fmin(mesh->extent[2 * axis], value);
                mesh->extent[2 * axis + 1] = fmax(mesh->extent[2 * axis + 1], value);
            }
            ++mesh->count;
        }
    }
    return true;
}

static bool triangulate(equation_mesh_t *mesh, const mesh_vertex_t *grid, unsigned side)
{
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
    return ok;
}

static bool sample(equation_mesh_t *mesh, const double bounds[6], unsigned side)
{
    size_t count = (size_t)side * side * side;
    mesh_vertex_t *grid = calloc(count, sizeof(*grid));
    double *values = calloc(count, sizeof(*values));
    mesh->edges = calloc(count * 7, sizeof(*mesh->edges));
    mesh->side = side;
    bool ok = grid && values && mesh->edges;
    unsigned layers = lab_worker_implicit_layers(mesh->sampling);
    for (unsigned layer = 0; ok && layer < layers; ++layer) {
        double spacing = lab_worker_implicit_layer(mesh->sampling, layer);
        double minimum = INFINITY, maximum = -INFINITY;
        for (unsigned z = 0; z < side; ++z)
            for (unsigned y = 0; y < side; ++y)
                for (unsigned x = 0; x < side; ++x) {
                    unsigned indices[] = {x, y, z};
                    size_t index = ((size_t)z * side + y) * side + x;
                    mesh_vertex_t *v = &grid[index];
                    v->index = index;
                    for (unsigned axis = 0; axis < 3; ++axis)
                        v->coordinate[axis] = bounds[2 * axis] +
                            (bounds[2 * axis + 1] - bounds[2 * axis]) * indices[axis] / (side - 1);
                    values[index] = evaluate(mesh, v->coordinate);
                    if (isfinite(values[index])) {
                        minimum = fmin(minimum, values[index]);
                        maximum = fmax(maximum, values[index]);
                    }
                }
        double first = spacing ? ceil(minimum / spacing) : 0;
        double last = spacing ? floor(maximum / spacing) : 0;
        /* Keep level enumeration bounded even for extremely oscillatory authored views. */
        if (!isfinite(first) || !isfinite(last) || fabs(first) > 1e9 || fabs(last) > 1e9 || last - first > 256) {
            mesh->excessive_levels = true;
            ok = false;
            break;
        }
        for (long level = (long)first; ok && level <= (long)last; ++level) {
            double offset = level * spacing;
            lab_worker_implicit_level(mesh->sampling, offset);
            for (size_t i = 0; i < count; ++i)
                grid[i].value = values[i] - offset;
            memset(mesh->edges, 0, count * 7 * sizeof(*mesh->edges));
            ok = triangulate(mesh, grid, side);
        }
    }
    free(values);
    free(grid);
    free(mesh->edges);
    mesh->edges = NULL;
    return ok;
}

/* Expand only unauthored endpoints, about the current centre even in a local precision frame. */
static bool expand_automatic(double bounds[6], const char *const supplied[6])
{
    bool changed = false;
    for (unsigned axis = 0; axis < 3; ++axis) {
        unsigned lo = 2 * axis, hi = lo + 1;
        double span = bounds[hi] - bounds[lo];
        if (!*supplied[lo]) {
            bounds[lo] -= span * 1.5;
            changed = true;
        }
        if (!*supplied[hi]) {
            bounds[hi] += span * 1.5;
            changed = true;
        }
    }
    return changed;
}

static void clear_mesh(equation_mesh_t *mesh)
{
    for (unsigned axis = 0; axis < 3; ++axis) {
        for (size_t i = 0; i < mesh->count; ++i)
            num_destroy(&mesh->coordinates[axis][i]);
        mesh->extent[2 * axis] = INFINITY;
        mesh->extent[2 * axis + 1] = -INFINITY;
    }
    mesh->count = 0;
    mesh->overflow = false;
}

/* Probe only clipped, automatic endpoints. Unbounded surfaces must not force an
 * ever-growing viewport; callers retain the original box if probing stays open. */
static bool expand_clipped(const equation_mesh_t *mesh, double bounds[6], const char *const supplied[6])
{
    bool changed = false;
    for (unsigned axis = 0; axis < 3; ++axis) {
        unsigned lo = 2 * axis, hi = lo + 1;
        double span = bounds[hi] - bounds[lo];
        double tolerance = span * 1e-8;
        bool lower = !*supplied[lo] && mesh->extent[lo] <= bounds[lo] + tolerance;
        bool upper = !*supplied[hi] && mesh->extent[hi] >= bounds[hi] - tolerance;
        if (lower)
            bounds[lo] -= span * 0.5;
        if (upper)
            bounds[hi] += span * 0.5;
        changed |= lower || upper;
    }
    return changed;
}

/* Retry the complete volume, never a truncated prefix of its facets. */
static bool sample_bounded(equation_mesh_t *mesh, const double bounds[6], unsigned *side)
{
    for (;;) {
        if (sample(mesh, bounds, *side))
            return true;
        if (!mesh->overflow || *side <= 3)
            return false;
        clear_mesh(mesh);
        *side = *side > 6 ? *side - 4 : 3;
    }
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
    equation_mesh_t mesh = {.capacity = 196608,
                            .extent = {INFINITY, -INFINITY, INFINITY, -INFINITY, INFINITY, -INFINITY}};
    size_t precision = num_get_default_prec_bits();
    double bounds[] = {-2, 2, -2, 2, -2, 2};
    bool manual = !strcmp(argv[7], "manual");
    const char *supplied[] = {argv[5], argv[6], argv[12], argv[13], manual ? argv[8] : "", manual ? argv[9] : ""};
    const char *names[] = {expr_symbol_name(first), expr_symbol_name(second), expr_symbol_name(third)};
    mesh.sampling = lab_worker_implicit_new(argv[1], names, supplied, 3, bounds);
    num_set_default_prec_bits(lab_worker_implicit_precision(mesh.sampling));
    bool ok = mesh.sampling && (manual || !strcmp(argv[7], "auto") || !strcmp(argv[7], "all"));
    for (unsigned axis = 0; axis < 3; ++axis) {
        mesh.coordinates[axis] = calloc(mesh.capacity, sizeof(number_t));
        ok = ok && mesh.coordinates[axis];
    }
    bool preview = !strcmp(argv[17], "preview");
    unsigned side = preview ? 9 : 25;
    unsigned requested_side = side;
    if (ok)
        ok = sample_bounded(&mesh, bounds, &side);
    bool expanded = false;
    for (unsigned attempt = 0; ok && !mesh.count && attempt < 8; ++attempt) {
        if (!expand_automatic(bounds, supplied))
            break;
        expanded = true;
        ok = sample(&mesh, bounds, 13);
    }
    if (ok && mesh.count && expanded && side != 13) {
        clear_mesh(&mesh);
        ok = sample_bounded(&mesh, bounds, &side);
    }
    if (ok && mesh.count) {
        double original[6], probe[6];
        memcpy(original, bounds, sizeof(original));
        memcpy(probe, bounds, sizeof(probe));
        bool clipped = expand_clipped(&mesh, probe, supplied);
        bool searched = clipped;
        for (unsigned attempt = 0; clipped && ok && attempt < 3; ++attempt) {
            memcpy(bounds, probe, sizeof(probe));
            clear_mesh(&mesh);
            ok = sample(&mesh, bounds, 13);
            clipped = !mesh.count || expand_clipped(&mesh, probe, supplied);
        }
        if (searched) {
            if (!ok || clipped)
                memcpy(bounds, original, sizeof(original));
            clear_mesh(&mesh);
            mesh.excessive_levels = false;
            ok = sample_bounded(&mesh, bounds, &side);
        }
    }
    bool explicit_volume = true;
    for (unsigned i = 0; i < 6; ++i)
        explicit_volume &= *supplied[i] != '\0';
    ok = ok && (mesh.count || explicit_volume);
    /* Frame the discovered surface, without replacing any authored coordinate interval. */
    for (unsigned axis = 0; ok && mesh.count && axis < 3; ++axis) {
        if (*supplied[2 * axis] || *supplied[2 * axis + 1])
            continue;
        double lower = mesh.extent[2 * axis], upper = mesh.extent[2 * axis + 1];
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
        limits[i] = lab_worker_implicit_coordinate(mesh.sampling, i / 2, bounds[i]);
    ok = ok && plot_set_bounds(plot, limits[0], limits[1], limits[2], limits[3]) &&
         plot_set_height_bounds(plot, limits[4], limits[5]);
    string_t *svg = ok ? plot_to_svg(plot) : NULL;
    while (svg && !lab_worker_plot_fits(svg) && side > 3) {
        string_free(svg);
        plot_free(plot);
        plot = plot_new(&options);
        clear_mesh(&mesh);
        side = side > 6 ? side - 4 : 3;
        ok = plot && sample_bounded(&mesh, bounds, &side) &&
             plot_set_mesh(plot, mesh.coordinates[0], mesh.coordinates[1], mesh.coordinates[2], mesh.count) &&
             plot_set_view(plot, view[0], view[1]) && plot_set_roll(plot, view[2]) &&
             plot_set_bounds(plot, limits[0], limits[1], limits[2], limits[3]) &&
             plot_set_height_bounds(plot, limits[4], limits[5]);
        svg = ok ? plot_to_svg(plot) : NULL;
    }
    bool excessive_output = mesh.overflow || (svg && !lab_worker_plot_fits(svg));
    if (svg && (!lab_worker_plot_fits(svg) ||
                (side < requested_side && !lab_worker_plot_reduced(svg)))) {
        string_free(svg);
        svg = NULL;
    }
    if (svg && !lab_worker_plot_quality(svg, preview, side)) {
        string_free(svg);
        svg = NULL;
    }
    char *title = equ_to_TeX_body_wrapped(equation, 110);
    if (svg)
        printf("input %s\nexpression %s\nplot_title %s\nplot_title_TeX %s\nplot_svg %s\ndifferentiable no\n",
               argv[1], argv[1], argv[1], title ? title : "", string_c_str(svg));
    else if (mesh.excessive_levels || excessive_output)
        printf("error Too many surface sheets in this view; zoom in or narrow the coordinate bounds.\n");
    else
        printf("error No real isosurface found in the sampled volume; supply finite ordered coordinate bounds.\n");
    int rc = svg ? 0 : 1;
    free(title);
    string_free(svg);
    plot_free(plot);
    lab_worker_implicit_free(mesh.sampling);
    for (unsigned i = 0; i < 6; ++i)
        num_destroy(&limits[i]);
    for (unsigned axis = 0; axis < 3; ++axis) {
        for (size_t i = 0; i < mesh.count; ++i)
            num_destroy(&mesh.coordinates[axis][i]);
        free(mesh.coordinates[axis]);
    }
    num_set_default_prec_bits(precision);
    return rc;
}
