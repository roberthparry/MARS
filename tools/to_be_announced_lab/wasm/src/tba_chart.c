/**
 * @file tba_chart.c
 * @brief Native chart scales, ticks and cursor hit testing for the browser.
 *
 * Bounded buffers avoid allocator churn. A single pass accumulates numeric
 * limits; ticks use binary search on monotone timestamps. Hit testing examines
 * each point once because distance depends on both coordinates.
 */
#include "tba_chart.h"

enum { TBA_CHART_CAPACITY = 32768 };
static double tba_chart_data[4][TBA_CHART_CAPACITY];
static unsigned tba_chart_count;
static double tba_chart_min, tba_chart_max, tba_chart_first, tba_chart_last;
static int tba_chart_timed, tba_chart_has_values;

static double tba_chart_abs(double value)
{
    return value < 0 ? -value : value;
}

/* Borrow fixed-capacity transfer storage without exposing implementation structs. */
double *tba_chart_buffer(unsigned kind)
{
    return kind < 4 ? tba_chart_data[kind] : NULL;
}

/* Validate capacity and initialise a new chart from the date buffer. */
int tba_chart_begin(unsigned count, double end)
{
    tba_chart_count = count <= TBA_CHART_CAPACITY ? count : 0;
    tba_chart_has_values = 0;
    tba_chart_timed = count > 1 && count <= TBA_CHART_CAPACITY;
    if (!tba_chart_count)
        return 0;
    tba_chart_first = tba_chart_data[0][0];
    tba_chart_last = tba_chart_data[0][count - 1];
    for (unsigned i = 0; i < count; ++i)
        if (!__builtin_isfinite(tba_chart_data[0][i]) || (i && tba_chart_data[0][i] < tba_chart_data[0][i - 1]))
            tba_chart_timed = 0;
    tba_chart_timed = tba_chart_timed && tba_chart_last > tba_chart_first;
    if (__builtin_isfinite(end) && end > tba_chart_last)
        tba_chart_last = end;
    return 1;
}

/* Each series is transferred once and contributes only finite values. */
void tba_chart_include(unsigned count)
{
    if (count > tba_chart_count)
        return;
    for (unsigned i = 0; i < count; ++i) {
        double value = tba_chart_data[1][i];
        if (!__builtin_isfinite(value))
            continue;
        if (!tba_chart_has_values || value < tba_chart_min)
            tba_chart_min = value;
        if (!tba_chart_has_values || value > tba_chart_max)
            tba_chart_max = value;
        tba_chart_has_values = 1;
    }
}

/* Pad constant series as well as ordinary ranges to avoid zero denominators. */
int tba_chart_finish(void)
{
    if (!tba_chart_has_values)
        return 0;
    double pad = (tba_chart_max - tba_chart_min) * 0.08;
    if (tba_chart_max == tba_chart_min) {
        pad = tba_chart_abs(tba_chart_min) * 0.1;
        if (pad < 1)
            pad = 1;
    }
    tba_chart_min -= pad;
    tba_chart_max += pad;
    return __builtin_isfinite(tba_chart_min) && __builtin_isfinite(tba_chart_max) &&
           __builtin_isfinite(tba_chart_max - tba_chart_min) && tba_chart_max > tba_chart_min;
}

/* Use fixed logical SVG dimensions; CSS scales the rendered chart. */
double tba_chart_x(unsigned index)
{
    if (index >= tba_chart_count)
        return __builtin_nan("");
    double unit = tba_chart_timed ? (tba_chart_data[0][index] - tba_chart_first) /
                                       (tba_chart_last - tba_chart_first) :
                  tba_chart_count > 1 ? (double)index / (tba_chart_count - 1) : 0.5;
    return 68 + unit * 944;
}

/* Vertical coordinates share the final padded scale. */
double tba_chart_y(double value)
{
    return 24 + (tba_chart_max - value) * 354 / (tba_chart_max - tba_chart_min);
}

/* Generate five evenly spaced axis values. */
double tba_chart_y_tick(unsigned index)
{
    return index < 5 ? tba_chart_max - (tba_chart_max - tba_chart_min) * index / 4 : __builtin_nan("");
}

/* Search rather than repeatedly scan all dates for each tick. */
int tba_chart_tick(unsigned slot, unsigned count)
{
    if (!count || count > 6 || slot >= count || !tba_chart_count)
        return -1;
    if (count == 1)
        return 0;
    if (!tba_chart_timed)
        return (int)((double)(tba_chart_count - 1) * slot / (count - 1) + 0.5);
    double target = tba_chart_first + (tba_chart_data[0][tba_chart_count - 1] - tba_chart_first) * slot / (count - 1);
    unsigned lo = 0, hi = tba_chart_count;
    while (lo < hi) {
        unsigned mid = lo + (hi - lo) / 2;
        if (tba_chart_data[0][mid] < target)
            lo = mid + 1;
        else
            hi = mid;
    }
    if (lo == tba_chart_count)
        return (int)lo - 1;
    if (lo && target - tba_chart_data[0][lo - 1] <= tba_chart_data[0][lo] - target)
        --lo;
    return (int)lo;
}

/* Expose the axis choice to the DOM renderer. */
int tba_chart_time_axis(void)
{
    return tba_chart_timed;
}

/* Expose the forecast boundary to browser-local date formatting. */
double tba_chart_end(void)
{
    return tba_chart_last;
}

/* Hover points need a bounded two-dimensional scan, not a date-only search. */
int tba_chart_nearest(unsigned count, double x, double y)
{
    if (count > TBA_CHART_CAPACITY || !__builtin_isfinite(x) || !__builtin_isfinite(y))
        return -1;
    double best = __builtin_inf();
    int found = -1;
    for (unsigned i = 0; i < count; ++i) {
        double dx = tba_chart_data[2][i] - x, dy = tba_chart_data[3][i] - y;
        double distance = dx * dx + dy * dy * 0.35;
        if (distance < best) {
            best = distance;
            found = (int)i;
        }
    }
    return found;
}
