/**
 * @file lab_worker_de_trajectory.c
 * @brief Plot representative numerical trajectories alongside exact parametric ODE solutions.
 *
 * Supplies initial-value choices and drawing limits to the differential-equation library. Numerical integration,
 * singularity handling and expression evaluation remain native library responsibilities. The worksheet's symbolic
 * coefficients and quadratures are not replaced by these explicitly labelled illustrative trajectories.
 */
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "diffequation-t.h"
#include "plot-t.h"
#define MARS_LAB_WORKER_INTERNAL_ACCESS
#include "lab_worker_internal.h"

/* The nine combinations represent both initial data of a second-order real family, including its equilibrium. */
int lab_worker_de_trajectory_plot(const diffequ_t *de, char **argv)
{
    number_t lower = NUM_NAN, upper = NUM_NAN, ymin = NUM_NAN, ymax = NUM_NAN;
    bool manual = !strcmp(argv[7], "manual");
    string_t *time_lower = !*argv[5] ? lab_worker_de_time_lower(de, argv[2]) : NULL;
    bool ok = lab_worker_plot_bounds(*argv[5] ? argv[5] : time_lower ? string_c_str(time_lower) : "-2",
                                     *argv[6] ? argv[6] : "2", &lower, &upper);
    if (ok && manual) ok = lab_worker_plot_bounds(argv[8], argv[9], &ymin, &ymax);
    double lo = num_to_double(lower), hi = num_to_double(upper);
    double origin = lo <= 0 && hi >= 0 ? 0 : lo + (hi - lo) / 2;
    number_t point = num_create_from_double(origin);
    plot_options_t configuration = plot_options_default();
    configuration.x_label = argv[2]; configuration.y_label = ""; configuration.title = "";
    configuration.legend = false; configuration.smart_y = !strcmp(argv[7], "auto");
    configuration.guard_gaps = true;
    configuration.axis_colour = 0x94b8b1; configuration.grid_colour = 0x82aaa3; configuration.label_colour = 0xb8ccc5;
    plot_t *plot = ok ? plot_new(&configuration) : NULL;
    ok = ok && plot;
    sampler_options_t settings = sampler_options_default();
    settings.intervals = !strcmp(argv[17], "preview") ? 128u : 512u;
    settings.absolute_tolerance = 1e-8; settings.relative_tolerance = 1e-7;
    static const unsigned colours[] = {0x78d8cb, 0xe8bd65, 0xd68cc4, 0x8caff0, 0xa9d879,
                                       0xf0a06a, 0xb2a1ec, 0xe0dbb5, 0xec8294};
    for (size_t i = 0; ok && i < 9u; ++i) {
        long initial = (long)(i / 3u) - 1, slope = (long)(i % 3u) - 1;
        number_t value = num_create_from_long(initial), velocity = num_create_from_long(slope);
        sampler_t *samples = de_sample_trajectory(de, point, value, velocity, lower, upper, &settings);
        char label[140];
        snprintf(label, sizeof label, "Numerical trajectory: value %ld, slope %ld at %s = %.6g",
                 initial, slope, argv[2], origin);
        ok = samples && plot_add_sampler(plot, samples, label, PLOT_LINES, colours[i]);
        sampler_free(samples); num_destroy(&velocity); num_destroy(&value);
    }
    if (ok && manual) ok = plot_set_bounds(plot, lower, upper, ymin, ymax);
    string_t *svg = ok ? plot_to_svg(plot) : NULL;
    string_t *title = svg ? de_to_text(de, style_LATEX) : NULL;
    if (svg && title) {
        printf("input %s\nexpression %s\nplot_title Representative numerical trajectories\n"
               "plot_title_TeX %s\\quad\\text{representative numerical trajectories}\n"
               "plot_svg %s\ndifferentiable no\n", argv[1], argv[1], string_c_str(title), string_c_str(svg));
    } else {
        printf("error Unable to sample real ODE trajectories within the selected bounds and numerical work limit.\n");
    }
    ok = svg && title;
    string_free(title); string_free(svg); plot_free(plot);
    string_free(time_lower);
    num_destroy(&point); num_destroy(&ymax); num_destroy(&ymin); num_destroy(&upper); num_destroy(&lower);
    return ok ? 0 : 1;
}
