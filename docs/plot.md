# Native SVG plots

Include `plot-t.h` for the opaque `plot_t` class. It copies height grids, measured points or
[`sampler_t`](sampler.md) results into a scene, then generates a complete SVG
image **in memory** as a `string_t`. No plotting library, temporary file,
external process, browser mathematics or JavaScript renderer is required.

Curve series have individual clipped SVG windows. A client can temporarily
translate the rendered series during a drag without painting over axis labels;
freshly sampled data is still required to fill newly exposed parts of the domain.

## API

| API | Behaviour |
| --- | --- |
| `plot_options_default()` | 800 × 500 pixels, one million points, 32 series, grid and legend enabled. |
| `plot_new(options)` | Copy settings and UTF-8 title/axis labels; NULL selects defaults. |
| `plot_add_points(plot, points, count, name, style, colour)` | Atomically copy a non-empty measured series. |
| `plot_add_sampler(plot, samples, name, style, colour)` | Copy sample coordinates and explicit gap markers. |
| `plot_set_surface(plot, x, columns, y, rows, z)` | Copy one row-major height grid into an empty scene. |
| `plot_set_mesh(plot, x, y, z, count)` | Copy independent triangles, permitting multiple heights at the same horizontal coordinates. |
| `plot_shade_interval(plot, lower, upper)` | Shade line series between the curve and zero inside an oriented finite interval, preserving gaps. |
| `plot_set_height_bounds(plot, minimum, maximum)` | Clip a surface to an explicit height interval. |
| `plot_include_height(plot, height)` | Include a located finite turning-point height in automatic surface bounds without changing the mesh. |
| `plot_set_bounds(plot, xmin, xmax, ymin, ymax)` | Copy an explicit finite real increasing viewport. |
| `plot_auto_bounds(plot)` | Restore bounds derived from finite data. |
| `plot_series_count(plot)` | Inspect the number of copied series. |
| `plot_to_svg(plot)` | Return an independent complete SVG string; release with `string_free()`. |
| `plot_free(plot)` | Release the scene and its copies; NULL is harmless. |

`PLOT_LINES` connects adjacent finite points unless `break_before` is set.
`PLOT_POINTS` draws individual markers. A non-finite or complex coordinate breaks
a line and is omitted as a marker. Numeric 24-bit RGB colours prevent CSS or
markup injection. Styles use direct function dispatch. Series are traversed
once per bounds, copying or rendering stage, rather than searched by name.

`plot_set_mesh()` accepts finite real coordinate arrays whose length is a positive
multiple of three. Every consecutive triple forms an independent triangle; the
scene copies all vertices, subject to its point limit. Meshes use the same camera,
height clipping and depth ordering as height grids. Coordinate units
retain their proportions under a uniform camera scale rather than being
stretched independently to fill the drawing rectangle. Explicit horizontal mesh
bounds set the camera frame: supply facets already clipped to that horizontal
domain. This permits volume samplers to preserve their chosen viewport.

`plot_shade_interval()` adds signed shading and dashed interval markers to line
series. Automatic vertical limits include zero. Explicit viewport bounds still
win, and all shading is clipped before conversion to drawing coordinates. Reversed
limits reverse the positive/negative contribution colours. This is an annotation
of sampled segments, not a quadrature rule or a claim that an improper integral
converges. Discontinuities must be supplied as gaps by the sampler.

Inputs remain caller-owned and may be freed immediately after successful
insertion. SVG remains valid after the scene is destroyed. A failed insertion
leaves the existing scene unchanged. Protect mutable scenes externally if using
threads; the API does not change the underlying mathematical threading policy.

## Axes, precision and limits

Axes are linear. Automatic bounds cover finite points; a constant coordinate is
expanded by the greater of one unit or five per cent of its magnitude. An
explicit viewport clips line segments geometrically and omits outside markers.
The renderer draws solid bottom and left axes beside the tick labels, interior
zero guides when visible, rounded decimal tick steps, optional subtle dashed
grid lines, plain-text labels and an in-plot legend, with the top and right
edges open. Grid lines on the viewport edges are omitted. Legend entries
that do not fit vertically are omitted. Long titles or labels can overlap:
this is not a general document-layout engine.

Tick steps are 1, 2, 2.5 or 5 times a power of ten, targeting about five
intervals. Grid lines follow those ticks, not arbitrary subdivisions of the
viewport.

Set `options.smart_y = true` to opt into a robust automatic vertical range.
Set `options.guard_gaps = true` to apply the same tail protection only when line
series contain explicit discontinuities or invalid points. This avoids fitting
extreme samples near asymptotes while retaining the full range of continuous
curves. It is a viewing heuristic, not a proof of unboundedness. Explicit manual
bounds override either policy. The default for this additional option is false.
The default remains Fit all: every finite sampled value contributes to the
bounds. Robust scaling takes at most one representative per horizontal bin
(129 bins per series), so dense adaptive clusters near poles do not dominate
the estimate. If the full Y span exceeds four times the 5th–95th percentile
span, it trims the tails to that central span plus 25 per cent padding.
Fewer than 20 representatives retain the full range. For a constant central
span, a small magnitude-based reference span permits isolated outliers to be cropped.
For derivative-assisted samplers, located stationary heights with resolved edges
on both sides are retained after trimming. This viewing heuristic may still crop
unresolved genuine peaks, not just singularities.
Clipping adds no visible annotation.
Explicit `plot_set_bounds()` overrides robust selection; recreating a scene
with `smart_y = false` restores Fit all. No data is discarded or changed.

Coordinates stay as `number_t` values through clipping, subtraction and normalisation;
only the resulting drawing fractions become doubles. Large offsets therefore
do not erase representable small differences. Both axes always label ticks in
decimal notation, never as fractions, rounded to six significant digits. This
display rounding does not change the coordinates. Scientific exponents appear
as `×10` followed by superscript digits rather than `e` notation. Label conversion
uses multiprecision arithmetic, including for exact rational ticks and values
outside the double range. SVG coordinates use a thread-local C numeric locale,
restored on return, and three decimal places in drawing units.

Width is limited to 320–8192 pixels and height to 240–8192. Scene limits are
configurable and count gaps as well as valid points. Invalid arguments give
`EINVAL`, scene limits `E2BIG`, allocation failure `ENOMEM`, no finite points
`ENODATA`, and an unrepresentable range or clipping calculation `ERANGE`.
Finite off-screen samples, including values beyond the double range near poles,
are clipped before drawing conversion. Invisible markers and segments outside
the same viewport edge are omitted. Crossing segments retain their visible
intersection; explicit sampler gaps are never joined.

Each curve series carries its XML-escaped name in an SVG `title`, providing a
native hover tooltip even when the visible legend is disabled.
Curve SVG viewports also carry `data-x-min`, `data-x-max`, `data-y-min` and
`data-y-max` attributes for UI coordinate inspection. These are double-precision
approximations of the native bounds, not a replacement for native numerical data.
All text is XML-escaped; invalid XML control characters are rejected. SVG uses
no scripts, external resources, generated IDs or caller-supplied markup. Its
axis, grid and tick/axis-label colours are controlled by `axis_colour`,
`grid_colour` and `label_colour` in `plot_options_t`. These are validated
24-bit RGB values; defaults are muted slate tones. Series colours are independent,
with rounded two-pixel strokes. Set `x_label` and `y_label` to empty strings to
omit axis titles while retaining numerical ticks. All colours are embedded in
the SVG, so they survive copying and saving without an external stylesheet.

[`computation_run()`](computation.md) supplies optional cooperative stopping.
Copying, bounds and drawing each reserve work per visited point. Failure or
stopping returns no partial SVG. The caller may save the completed result using
the [file module](file.md); generating or displaying a plot requires no file.

The renderer supports two-dimensional lines and markers and rectangular height
surfaces, not logarithmic axes, histograms, raster export or a browser interaction engine. The
sampler's gap detection is heuristic, not a continuity guarantee.

## Height surfaces

`plot_set_surface(plot, x, columns, y, rows, z)` copies a rectangular height
grid into an empty scene. Both coordinate arrays must be finite, real and
strictly increasing, with at least two elements. Heights are row-major:
`z[row * columns + column]`. Non-real or non-finite heights leave holes;
cells touching a missing vertex are not drawn. Finite steep facets are retained;
the renderer does not infer discontinuities from their relative height changes.
Callers with a function evaluator must sample domain gaps before supplying heights.
Surface SVGs include approximate independent-axis bounds and per-facet affine
coordinate maps for pointer-centred domain zoom. These describe the rendered
mesh, not the exact function between samples; nearly edge-on facets may omit the
map. Consumers can resample a narrower domain without enlarging the SVG artwork.
A surface counts as one series
and cannot be mixed with curves, replaced, or given explicit 2-D bounds.

The vertex count is subject to `max_points`. Library defaults fit every finite
height. Set `options.smart_y = true` before creating the scene to crop extreme
height tails automatically, using the same padded 5th–95th percentile policy
as curves, with equal weight for each finite grid sample. This is a viewing
heuristic, not a determination of where the function diverges. Constant heights receive a padded vertical
range. Normalisation retains `number_t` precision before the configurable oblique
projection. Faces are depth-ordered and shaded blue, teal and gold by height.
Three open axes have tick strokes, decimal labels and no enclosing box. Native
label layout reserves clearance between numbers, including shared corners.
Surface labels may move slightly outwards; labels that still cannot fit are
omitted without removing their tick marks. Curves also thin crowded tick labels.
Margins account for long scientific labels. Sampled meshes can miss small holes,
poles or rapid oscillations; joining finite vertices is not a continuity
guarantee. No drawable cells gives `ENODATA`, except when a manual height interval
excludes all facets: that viewport still displays its axes.

`plot_set_height_bounds(plot, minimum, maximum)` sets an existing surface's height
viewport. Both bounds must be finite and real, with minimum strictly below maximum;
invalid arguments give `EINVAL` without changing the previous interval. Manual limits
override `smart_y`. Facets are geometrically intersected with the interval in native
precision before drawing conversion; out-of-range heights are not flattened into
artificial plateaus. Missing values remain holes. `plot_auto_bounds(plot)` restores
the scene's automatic policy without resampling or changing any data.

`plot_include_height(plot, height)` copies a finite real height that an external
native search has located. Repeated calls accumulate an advisory range included
in both automatic policies; manual height bounds still win. The rectangular mesh
is unchanged. Non-real/non-finite heights or a scene without a surface give `EINVAL`.

`plot_set_view(plot, azimuth, elevation)` changes the surface camera in degrees,
without changing or resampling the copied grid. Defaults are 315° around the
surface and 30° above it, with both horizontal axes increasing from left to right
and the height axis increasing from bottom to top. Finite azimuths wrap into
[0,360); elevation accepts 5° to 85°, keeping the axes readable. Invalid arguments give `EINVAL` without
changing the previous view. Call `plot_to_svg()` again to obtain the rotated
image. Projection, depth ordering, exposed axes and tick positions all follow
the camera. `plot_set_roll(plot, roll)` adds clockwise screen-plane rotation,
defaulting to 0°. Finite rolls wrap into [0,360); invalid rolls leave the camera
unchanged and give `EINVAL`. The rotated surface is fitted into the viewport,
while text remains upright. Curve scenes ignore both camera settings.

Each surface axis ends in an arrow pointing towards increasing values, labelled
with the copied `x_label`, `y_label` or `z_label` option (defaults `x`, `y`, `f`).
An empty label suppresses its name, not its arrow. Endpoint names take priority
over nearby tick numbers in the collision layout. The example below uses
120° azimuth, 40° elevation and 20° roll, with manual heights from 0 to 1.

```c
#include <stdbool.h>
#include <stdio.h>

#include "plot-t.h"

int main(void)
{
    plot_t *plot = plot_new(NULL);
    number_t axis[] = {NUM_NEG_ONE, NUM_ZERO, NUM_ONE};
    number_t heights[9];
    for (size_t r = 0; r < 3; ++r)
        for (size_t c = 0; c < 3; ++c)
            heights[r * 3 + c] = num_create_from_long((long)((r != 1) + (c != 1)));
    bool added = plot_set_surface(plot, axis, 3, axis, 3, heights) &&
                 plot_set_height_bounds(plot, NUM_ZERO, NUM_ONE) &&
                 plot_set_view(plot, 120, 40) && plot_set_roll(plot, 20);
    for (size_t i = 0; i < 9; ++i)
        num_destroy(&heights[i]);
    string_t *svg = added ? plot_to_svg(plot) : NULL;
    if (svg)
        printf("Surface SVG ready; series: %zu; height: 0 to 1\n", plot_series_count(plot));
    bool ok = svg != NULL;
    string_free(svg);
    plot_free(plot);
    return ok ? 0 : 1;
}
```

Output:

```text
Surface SVG ready; series: 1; height: 0 to 1
```

This small paraboloid is mirrored as `example_plot_surface` in
`tests/plot/test_plot.c`.

## Creating an SVG in memory

```c
#include <stdbool.h>
#include <stdio.h>

#include "plot-t.h"

int main(void)
{
    plot_options_t options = plot_options_default();
    options.title = "Measured curve";
    options.smart_y = true;
    plot_t *plot = plot_new(&options);
    plot_point_t points[] = {{NUM_ZERO, NUM_ZERO, true}, {NUM_HALF, NUM_ONE, false}, {NUM_ONE, NUM_ZERO, false}};
    bool ok = plot && plot_add_points(plot, points, 3, "Measurements", PLOT_LINES, 0x55aacc);
    string_t *svg = ok ? plot_to_svg(plot) : NULL;
    if (svg)
        printf("SVG ready; series: %zu\n", plot_series_count(plot));
    ok = svg != NULL;
    string_free(svg);
    plot_free(plot);
    return ok ? 0 : 1;
}
```

Output:

```text
SVG ready; series: 1
```

The SVG string contains axes, labels and the three-point curve. It can be sent
as an image response or included in an application-owned Protobuf message.
Mirrored as a README example in `tests/plot/test_plot.c`.
