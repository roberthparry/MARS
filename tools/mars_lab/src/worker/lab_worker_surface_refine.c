/**
 * @file lab_worker_surface_refine.c
 * @brief Bounded, conforming local refinement of explicit height-field facets.
 *
 * The worker keeps its uniform grid for range statistics and probes interpolation
 * error on a dyadic lattice. Only unresolved cells subdivide. Shared boundary
 * vertices stitch neighbouring levels without cracks. Coordinates and samples
 * retain the worker's numerical precision; doubles measure normalised error only.
 * This private helper leaves the original grid available for transport fallback.
 */
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

#define MARS_SHARED_COMPUTATION_INTERNAL_ACCESS
#include "internal/computation_internal.h"
#define MARS_LAB_WORKER_INTERNAL_ACCESS
#include "lab_worker_internal.h"

enum { REFINE_STEP = 8, REFINE_LEAVES = 10000, REFINE_POINTS = 120000, REFINE_FACETS = 180000 };

typedef struct {
    size_t x, y;
    number_t z;
    double height;
    bool boundary;
} surface_vertex_t;

typedef struct {
    size_t x, y, step;
} surface_cell_t;

typedef struct {
    size_t width, count, leaves, pending, used;
    uint32_t *indices;
    number_t *x, *y, *facets[3];
    number_t lower, span;
    surface_vertex_t *vertices;
    surface_cell_t *cells;
    lab_worker_sample_t *evaluation;
    bool ok, refined;
} surface_refinement_t;

static bool finite_real(number_t value)
{
    return num_is_real(value) && num_is_finite(value);
}

static size_t vertex(surface_refinement_t *state, size_t x, size_t y, const number_t *original)
{
    size_t slot = y * state->width + x;
    if (state->indices[slot])
        return state->indices[slot] - 1;
    if (state->count == REFINE_POINTS || !computation_step_current("plot.surface.refine")) {
        state->ok = false;
        return 0;
    }
    size_t index = state->count++;
    surface_vertex_t *point = &state->vertices[index];
    point->x = x;
    point->y = y;
    number_t coordinates[] = {state->x[x], state->y[y]};
    point->z = original ? num_clone(*original) : lab_worker_sample_eval(state->evaluation, coordinates);
    point->height = NAN;
    if (finite_real(point->z)) {
        number_t offset = num_sub(point->z, state->lower), relative = num_div(offset, state->span);
        point->height = num_to_double(relative);
        num_destroy(&relative);
        num_destroy(&offset);
    }
    state->indices[slot] = (uint32_t)index + 1;
    return index;
}

static void subdivide(surface_refinement_t *state, size_t x, size_t y, size_t step)
{
    if (!state->ok)
        return;
    --state->pending;
    size_t half = step / 2;
    size_t corners[] = {vertex(state, x, y, NULL), vertex(state, x + step, y, NULL),
                        vertex(state, x + step, y + step, NULL), vertex(state, x, y + step, NULL)};
    size_t probes[] = {vertex(state, x + half, y, NULL), vertex(state, x + step, y + half, NULL),
                       vertex(state, x + half, y + step, NULL), vertex(state, x, y + half, NULL),
                       vertex(state, x + half, y + half, NULL)};
    double error = 0;
    unsigned valid = 0;
    for (unsigned i = 0; i < 4; ++i)
        valid += isfinite(state->vertices[corners[i]].height) != 0;
    for (unsigned i = 0; i < 5; ++i) {
        double actual = state->vertices[probes[i]].height;
        if (!isfinite(actual))
            continue;
        ++valid;
        unsigned a = i < 4 ? i : 0, b = i < 4 ? (i + 1) % 4 : 2;
        double expected = (state->vertices[corners[a]].height + state->vertices[corners[b]].height) * 0.5;
        error = fmax(error, fabs(actual - expected));
    }
    if (!state->ok || !valid)
        return;
    /* A mixed real/non-real cell is a domain boundary, not an entirely missing patch. */
    if (step > 2 && (valid != 9 || error > 0.002) && state->leaves + state->pending + 4 <= REFINE_LEAVES) {
        state->refined = true;
        state->pending += 4;
        subdivide(state, x, y, half);
        subdivide(state, x + half, y, half);
        subdivide(state, x + half, y + half, half);
        subdivide(state, x, y + half, half);
        return;
    }
    /* Never bridge an unresolved boundary when the depth or work budget is reached. */
    if (valid != 9)
        return;
    state->cells[state->leaves++] = (surface_cell_t){x, y, step};
    for (unsigned i = 0; i < 4; ++i)
        state->vertices[corners[i]].boundary = true;
}

static void triangle(surface_refinement_t *state, size_t a, size_t b, size_t c)
{
    if (state->used + 3 > REFINE_FACETS) {
        state->ok = false;
        return;
    }
    size_t indices[] = {a, b, c};
    for (unsigned i = 0; i < 3; ++i) {
        surface_vertex_t *point = &state->vertices[indices[i]];
        state->facets[0][state->used] = state->x[point->x];
        state->facets[1][state->used] = state->y[point->y];
        state->facets[2][state->used++] = point->z;
    }
}

static void triangulate(surface_refinement_t *state, surface_cell_t cell)
{
    size_t boundary[4 * REFINE_STEP], count = 0;
    /* Each edge visits at most eight direct-indexed lattice positions. */
    for (unsigned edge = 0; edge < 4; ++edge) {
        for (size_t offset = 0; offset < cell.step; ++offset) {
            size_t x = cell.x, y = cell.y;
            if (edge == 0)
                x += offset;
            else if (edge == 1) {
                x += cell.step;
                y += offset;
            } else if (edge == 2) {
                x += cell.step - offset;
                y += cell.step;
            } else {
                y += cell.step - offset;
            }
            uint32_t index = state->indices[y * state->width + x];
            if (index && state->vertices[index - 1].boundary)
                boundary[count++] = index - 1;
        }
    }
    if (count == 4) {
        triangle(state, boundary[0], boundary[1], boundary[2]);
        triangle(state, boundary[0], boundary[2], boundary[3]);
    } else {
        size_t centre = vertex(state, cell.x + cell.step / 2, cell.y + cell.step / 2, NULL);
        for (size_t i = 0; state->ok && i < count; ++i)
            triangle(state, centre, boundary[i], boundary[(i + 1) % count]);
    }
}

static void coordinates(number_t *destination, const number_t *source, size_t side)
{
    for (size_t cell = 0; cell + 1 < side; ++cell) {
        number_t span = num_sub(source[cell + 1], source[cell]);
        for (size_t i = 0; i < REFINE_STEP; ++i) {
            number_t fraction = num_create_from_frac((long)i, REFINE_STEP), offset = num_mul(span, fraction);
            destination[cell * REFINE_STEP + i] = num_add(source[cell], offset);
            num_destroy(&offset);
            num_destroy(&fraction);
        }
        num_destroy(&span);
    }
    destination[(side - 1) * REFINE_STEP] = num_clone(source[side - 1]);
}

bool lab_worker_surface_refine(plot_t *plot, const char *source, const char *first, const char *second,
                               const number_t *x, const number_t *y, const number_t *z, size_t side, size_t bits)
{
    if (side < 2 || (side - 1) * (side - 1) > REFINE_LEAVES)
        return true;
    number_t lower = NUM_NAN, upper = NUM_NAN;
    for (size_t i = 0; i < side * side; ++i) {
        if (!finite_real(z[i]))
            continue;
        if (num_is_nan(lower) || num_lt(z[i], lower))
            lower = z[i];
        if (num_is_nan(upper) || num_gt(z[i], upper))
            upper = z[i];
    }
    if (!finite_real(lower) || num_eq(lower, upper))
        return true;
    expr_bindings_t *bindings = NULL;
    expr_t *formula = expr_from_string(source, &bindings);
    expr_t *axes[] = {bindings ? expr_bindings_get(bindings, first) : NULL,
                      bindings ? expr_bindings_get(bindings, second) : NULL};
    /* A missing axis means this is not a genuine two-variable surface. */
    if (!formula || !axes[0] || !axes[1]) {
        expr_free(formula);
        expr_bindings_free(bindings);
        return computation_poll_current();
    }
    surface_refinement_t state = {.width = (side - 1) * REFINE_STEP + 1,
                                 .pending = (side - 1) * (side - 1), .lower = lower,
                                 .span = num_sub(upper, lower), .ok = true};
    state.indices = calloc(state.width * state.width, sizeof(*state.indices));
    state.vertices = calloc(REFINE_POINTS, sizeof(*state.vertices));
    state.cells = calloc(REFINE_LEAVES, sizeof(*state.cells));
    state.x = calloc(state.width, sizeof(*state.x));
    state.y = calloc(state.width, sizeof(*state.y));
    state.evaluation = lab_worker_sample_new(formula, axes, 2, bits);
    for (unsigned i = 0; i < 3; ++i)
        state.facets[i] = calloc(REFINE_FACETS, sizeof(number_t));
    state.ok = state.indices && state.vertices && state.cells && state.x && state.y && state.evaluation &&
               state.facets[0] && state.facets[1] && state.facets[2];
    if (state.ok) {
        coordinates(state.x, x, side);
        coordinates(state.y, y, side);
        for (size_t r = 0; state.ok && r < side; ++r)
            for (size_t c = 0; state.ok && c < side; ++c)
                vertex(&state, c * REFINE_STEP, r * REFINE_STEP, &z[r * side + c]);
        for (size_t r = 0; state.ok && r + 1 < side; ++r)
            for (size_t c = 0; state.ok && c + 1 < side; ++c)
                subdivide(&state, c * REFINE_STEP, r * REFINE_STEP, REFINE_STEP);
        for (size_t i = 0; state.ok && state.refined && i < state.leaves; ++i)
            triangulate(&state, state.cells[i]);
    }
    /* Budget exhaustion retains a complete uniform mesh, never a partial refinement. */
    bool ok = computation_poll_current();
    if (ok && state.ok && state.used)
        ok = plot_set_surface_facets(plot, state.facets[0], state.facets[1], state.facets[2], state.used);
    for (size_t i = 0; i < state.count; ++i)
        num_destroy(&state.vertices[i].z);
    for (size_t i = 0; i < state.width; ++i) {
        if (state.x)
            num_destroy(&state.x[i]);
        if (state.y)
            num_destroy(&state.y[i]);
    }
    for (unsigned i = 0; i < 3; ++i)
        free(state.facets[i]);
    free(state.x);
    free(state.y);
    free(state.indices);
    free(state.vertices);
    free(state.cells);
    num_destroy(&state.span);
    lab_worker_sample_free(state.evaluation);
    expr_free(formula);
    expr_bindings_free(bindings);
    return ok;
}
