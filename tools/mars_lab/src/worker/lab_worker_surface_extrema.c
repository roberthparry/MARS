/**
 * @file lab_worker_surface_extrema.c
 * @brief Bounded gradient/Hessian refinement of sampled surface turning points.
 *
 * Interior grid extrema seed local Newton searches confined to their neighbouring
 * cells. Only finite small-gradient points with a definite Hessian contribute
 * advisory height limits. Saddles, poles, unsupported derivatives and unresolved
 * candidates are ignored. This is not a global extremum or domain proof and does
 * not alter the rendered mesh. All expression graphs and bindings are private.
 */
#include <errno.h>

#define MARS_SHARED_COMPUTATION_INTERNAL_ACCESS
#include "internal/computation_internal.h"
#define MARS_LAB_WORKER_INTERNAL_ACCESS
#include "lab_worker_internal.h"

static bool finite_real(number_t value)
{
    return num_is_real(value) && num_is_finite(value);
}

static int candidate_kind(const number_t *z, size_t side, size_t row, size_t column)
{
    number_t centre = z[row * side + column];
    if (!finite_real(centre))
        return 0;
    bool maximum = true, minimum = true, strict_maximum = false, strict_minimum = false;
    for (size_t r = row - 1; r <= row + 1; ++r) {
        for (size_t c = column - 1; c <= column + 1; ++c) {
            number_t value = z[r * side + c];
            if (!finite_real(value))
                return 0;
            maximum &= num_ge(centre, value);
            minimum &= num_le(centre, value);
            strict_maximum |= num_gt(centre, value);
            strict_minimum |= num_lt(centre, value);
        }
    }
    return maximum && strict_maximum ? 1 : minimum && strict_minimum ? -1 : 0;
}

/* Refine only local candidates; no search may leave the adjacent grid cells. */
bool lab_worker_surface_extrema(plot_t *plot, const char *source, const char *first, const char *second,
                                const number_t *x, const number_t *y, const number_t *z, size_t side)
{
    NUM_SCOPE(numbers);
    expr_bindings_t *bindings = NULL;
    expr_t *formula = expr_from_string(source, &bindings);
    expr_t *axes[] = {bindings ? expr_bindings_get(bindings, first) : NULL,
                     bindings ? expr_bindings_get(bindings, second) : NULL};
    /* A surface constant along a declared axis has no isolated definite-Hessian extremum. */
    if (formula && (!axes[0] || !axes[1])) {
        expr_free(formula);
        expr_bindings_free(bindings);
        return computation_poll_current();
    }
    expr_t *hessian[3] = {NULL, NULL, NULL};
    bool ok = formula && axes[0] && axes[1];
    unsigned candidates = 0;
    for (size_t r = 1; ok && candidates < 8 && r + 1 < side; ++r) {
        for (size_t c = 1; ok && candidates < 8 && c + 1 < side; ++c) {
            if (!computation_step_current("plot.surface.extrema")) {
                ok = false;
                break;
            }
            int kind = candidate_kind(z, side, r, c);
            if (!kind)
                continue;
            ++candidates;
            if (!hessian[0]) {
                hessian[0] = expr_create_2nd_deriv(formula, axes[0], axes[0]);
                hessian[1] = expr_create_2nd_deriv(formula, axes[0], axes[1]);
                hessian[2] = expr_create_2nd_deriv(formula, axes[1], axes[1]);
            }
            if (!hessian[0] || !hessian[1] || !hessian[2])
                continue;
            number_t px = num_clone(x[c]), py = num_clone(y[r]);
            number_t width = num_sub(x[c + 1], x[c - 1]), depth = num_sub(y[r + 1], y[r - 1]);
            for (unsigned iteration = 0; iteration < 12; ++iteration) {
                if (!computation_step_current("plot.surface.stationary")) {
                    ok = false;
                    break;
                }
                expr_set_val(axes[0], px);
                expr_set_val(axes[1], py);
                const expr_t *variables[] = {axes[0], axes[1]};
                number_t value = NUM_NAN, gradient[] = {NUM_NAN, NUM_NAN};
                if (expr_eval_derivatives(formula, 2, variables, &value, gradient) != 0 ||
                    !finite_real(value) || !finite_real(gradient[0]) || !finite_real(gradient[1]))
                    break;
                number_t xx = expr_eval(hessian[0]), xy = expr_eval(hessian[1]), yy = expr_eval(hessian[2]);
                if (!finite_real(xx) || !finite_real(xy) || !finite_real(yy))
                    break;
                number_t determinant = num_sub(num_mul(xx, yy), num_mul(xy, xy));
                if (!finite_real(determinant) || !num_gt(determinant, NUM_ZERO) ||
                    (kind > 0 ? !num_lt(xx, NUM_ZERO) : !num_gt(xx, NUM_ZERO)))
                    break;
                number_t error = num_add(num_abs(num_mul(gradient[0], width)),
                                         num_abs(num_mul(gradient[1], depth)));
                number_t tolerance = num_mul(num_add(NUM_ONE, num_abs(value)), num_create_from_string("1e-7"));
                if (num_le(error, tolerance)) {
                    ok = plot_include_height(plot, value);
                    break;
                }
                number_t step_x = num_div(num_sub(num_mul(yy, gradient[0]), num_mul(xy, gradient[1])), determinant);
                number_t step_y = num_div(num_sub(num_mul(xx, gradient[1]), num_mul(xy, gradient[0])), determinant);
                number_t next_x = num_sub(px, step_x), next_y = num_sub(py, step_y);
                if (!finite_real(next_x) || !finite_real(next_y) ||
                    !num_gt(next_x, x[c - 1]) || !num_lt(next_x, x[c + 1]) ||
                    !num_gt(next_y, y[r - 1]) || !num_lt(next_y, y[r + 1]))
                    break;
                px = next_x;
                py = next_y;
            }
        }
    }
    for (size_t i = 0; i < 3; ++i)
        expr_free(hessian[i]);
    expr_free(formula);
    expr_bindings_free(bindings);
    return ok && computation_poll_current();
}
