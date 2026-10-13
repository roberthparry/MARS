# Native SVG plots

Include `plot-t.h` for the opaque `plot_t` class. It copies measured points or
[`sampler_t`](sampler.md) results into a scene, then generates a complete SVG
image **in memory** as a `string_t`. No plotting library, temporary file,
external process, browser mathematics or JavaScript renderer is required.

## API

| API | Behaviour |
| --- | --- |
| `plot_options_default()` | 800 × 500 pixels, one million points, 32 series, grid and legend enabled. |
| `plot_new(options)` | Copy settings and UTF-8 title/axis labels; NULL selects defaults. |
| `plot_add_points(plot, points, count, name, style, colour)` | Atomically copy a non-empty measured series. |
| `plot_add_sampler(plot, samples, name, style, colour)` | Copy sample coordinates and explicit gap markers. |
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

Inputs remain caller-owned and may be freed immediately after successful
insertion. SVG remains valid after the scene is destroyed. A failed insertion
leaves the existing scene unchanged. Protect mutable scenes externally if using
threads; the API does not change the underlying mathematical threading policy.

## Axes, precision and limits

Axes are linear. Automatic bounds cover finite points; a constant coordinate is
expanded by the greater of one unit or five per cent of its magnitude. An
explicit viewport clips line segments geometrically and omits outside markers.
The renderer draws a frame, zero axes when visible, rounded decimal tick steps,
optional grid lines, plain-text labels and an in-plot legend. Legend entries
that do not fit vertically are omitted. Long titles or labels can overlap:
this is not a general document-layout engine.

Tick steps are 1, 2, 2.5 or 5 times a power of ten, targeting about five
intervals. Grid lines follow those ticks, not arbitrary subdivisions of the
viewport.

Set `options.smart_y = true` to opt into a robust automatic vertical range.
The default remains Fit all: every finite sampled value contributes to the
bounds. Robust scaling takes at most one representative per horizontal bin
(129 bins per series), so dense adaptive clusters near poles do not dominate
the estimate. If the full Y span exceeds four times the 5th–95th percentile
span, it trims the tails to that central span plus 25 per cent padding.
Fewer than 20 representatives or a zero central span retain the full range.
This viewing heuristic may crop genuine peaks, not just singularities.
Every viewport that clips sampled Y values displays **Vertical range clipped**.
Explicit `plot_set_bounds()` overrides robust selection; recreating a scene
with `smart_y = false` restores Fit all. No data is discarded or changed.

Coordinates stay as `number_t` values through subtraction and normalisation;
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
`ENODATA`, and an unrepresentable range or normalised coordinate `ERANGE`.
Normalised coordinates with magnitude above 10¹⁰⁰ are rejected. Choose a
reasonable viewport rather than expecting invisible extreme outliers to render.

All text is XML-escaped; invalid XML control characters are rejected. SVG uses
no scripts, external resources, generated IDs or caller-supplied markup. Its
foreground axes inherit `currentColor`, making it suitable for light or dark
MARS Lab cards. The caller controls series colours.

[`computation_run()`](computation.md) supplies optional cooperative stopping.
Copying, bounds and drawing each reserve work per visited point. Failure or
stopping returns no partial SVG. The caller may save the completed result using
the [file module](file.md); generating or displaying a plot requires no file.

This first renderer supports two-dimensional lines and markers, not logarithmic
axes, surfaces, histograms, raster export or interactive zoom/resampling. The
sampler's gap detection is heuristic, not a continuity guarantee.

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
