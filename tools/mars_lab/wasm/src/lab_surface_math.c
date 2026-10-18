/**
 * @file lab_surface_math.c
 * @brief Small fixed-domain graphics operations for a freestanding WebAssembly viewport.
 *
 * Camera rotations and relative unit-volume scales need only display precision.
 * Standard maths functions are linked from WASI libc, without JavaScript maths
 * callbacks or WASI system imports. These are not mathematical expression
 * evaluators: native MARS retains all actual coordinate arithmetic and
 * arbitrary-precision sampling. Square roots compile
 * directly to the WebAssembly f64.sqrt instruction.
 */
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 700
#endif

#include <math.h>

#include "lab_dom.h"
#include "lab_surface_math.h"

/* Preserve the zero-vector fallback; otherwise use standard quadrant handling. */
double lab_surface_atan2(double y, double x)
{
    if (x == 0 && y == 0)
        return 0;
    return atan2(y, x);
}

/* Only relative screen transforms use this bounded floating-point scale. */
double lab_surface_exp2(double x)
{
    if (x > 1023)
        x = 1023;
    if (x < -1022)
        x = -1022;
    return exp2(x);
}

/* Recover a gesture exponent without handling absolute mathematical coordinates. */
double lab_surface_log2(double value)
{
    if (!(value > 0) || !isfinite(value))
        return 0;
    return log2(value);
}

static void read_vector(browser_jsref handle, double *values, unsigned count)
{
    for (unsigned i = 0; i < count; ++i)
        values[i] = lab_dom_to_number(lab_dom_item(handle, i));
}

static browser_jsref vector(const double *values, unsigned count)
{
    int result = lab_dom_object(LAB_VALUE_ARRAY);
    for (unsigned i = 0; i < count; ++i)
        lab_dom_push(result, lab_dom_numeric(values[i]));
    return result;
}

static void normalise(double q[4])
{
    double length = __builtin_sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
    if (length > 0)
        for (unsigned i = 0; i < 4; ++i)
            q[i] /= length;
    else
        q[3] = 1;
}

static void multiply(const double a[4], const double b[4], double q[4])
{
    double result[] = {
        a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1],
        a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0],
        a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3],
        a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2]
    };
    for (unsigned i = 0; i < 4; ++i)
        q[i] = result[i];
    normalise(q);
}

static void from_angles(const double angles[3], double q[4])
{
    double a = angles[0] * M_PI / 360, e = M_PI / 4 + angles[1] * M_PI / 360, r = angles[2] * M_PI / 360;
    double az[] = {0, 0, sin(a), cos(a)};
    double el[] = {sin(e), 0, 0, cos(e)};
    double ro[] = {0, 0, sin(r), cos(r)};
    multiply(el, az, q);
    multiply(ro, q, q);
}

static void rows(double q[4], double m[9])
{
    normalise(q);
    double x = q[0], y = q[1], z = q[2], w = q[3];
    double values[] = {1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w),
                       2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w),
                       2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)};
    for (unsigned i = 0; i < 9; ++i)
        m[i] = values[i];
}

static double wrap(double angle)
{
    angle *= 180 / M_PI;
    angle = fmod(angle, 360);
    return angle < 0 ? angle + 360 : angle;
}

/* The graphics controller uses a fixed rotation matrix, never absolute coordinates. */
void lab_surface_rotation(quaternion_jsref quaternion, double matrix[9])
{
    double q[4];
    read_vector(quaternion, q, 4);
    rows(q, matrix);
}

static void to_angles(double q[4], const double previous[3], double out[3])
{
    double m[9];
    rows(q, m);
    double sine = -m[8];
    if (sine > 1)
        sine = 1;
    if (sine < -1)
        sine = -1;
    double cosine = __builtin_sqrt(1 - sine * sine), e = lab_surface_atan2(sine, cosine), a, r;
    if (cosine > 1e-7) {
        a = lab_surface_atan2(m[6], m[7]);
        r = lab_surface_atan2(m[2], -m[5]);
    } else {
        a = previous[0] * M_PI / 180;
        double angles[] = {previous[0], e * 180 / M_PI, 0}, base[4], residual[4];
        from_angles(angles, base);
        for (unsigned i = 0; i < 3; ++i)
            base[i] = -base[i];
        multiply(q, base, residual);
        r = 2 * lab_surface_atan2(residual[2], residual[3]);
    }
    out[0] = wrap(a);
    out[1] = e * 180 / M_PI;
    out[2] = wrap(r);
}

static void ball(const double p[2], browser_jsref box, double out[3])
{
    double width = lab_dom_to_number(lab_dom_get(box, "width"));
    double height = lab_dom_to_number(lab_dom_get(box, "height"));
    double radius = (width < height ? width : height) / 2;
    if (radius < 1)
        radius = 1;
    double u = (p[0] - lab_dom_to_number(lab_dom_get(box, "left")) - width / 2) / radius;
    double v = (p[1] - lab_dom_to_number(lab_dom_get(box, "top")) - height / 2) / radius;
    double d = u * u + v * v, z = -(d <= 0.5 ? __builtin_sqrt(1 - d) : 0.5 / __builtin_sqrt(d));
    double length = __builtin_sqrt(d + z * z);
    out[0] = u / length;
    out[1] = v / length;
    out[2] = z / length;
}

/** @brief Scoped operands and caller-owned arrays for one camera operation. */
struct lab_surface_camera_context {
    double *first;
    double *result;
    browser_jsref second, third, box;
    int roll;
};

/* Convert the already-read degree angles. */
static void camera_from_angles(struct lab_surface_camera_context *ctx)
{
    from_angles(ctx->first, ctx->result);
}

/* Convert the already-read quaternion to matrix rows. */
static void camera_rows(struct lab_surface_camera_context *ctx)
{
    rows(ctx->first, ctx->result);
}

/* Read previous angles only for the inverse conversion. */
static void camera_to_angles(struct lab_surface_camera_context *ctx)
{
    double previous[3];
    read_vector(ctx->second, previous, 3);
    to_angles(ctx->first, previous, ctx->result);
}

/* Read the right-hand quaternion only for multiplication. */
static void camera_multiply(struct lab_surface_camera_context *ctx)
{
    double second[4];
    read_vector(ctx->second, second, 4);
    multiply(ctx->first, second, ctx->result);
}

/* Apply horizontal roll or the trackball gesture to the current quaternion. */
static void camera_drag(struct lab_surface_camera_context *ctx)
{
    double from[2], to[2], b[4] = {0};
    read_vector(ctx->second, from, 2);
    read_vector(ctx->third, to, 2);
    if (ctx->roll) {
        double half = (to[0] - from[0]) * M_PI / 720;
        b[2] = sin(half);
        b[3] = cos(half);
    } else {
        double u[3], v[3];
        ball(from, ctx->box, u);
        ball(to, ctx->box, v);
        b[0] = u[1] * v[2] - u[2] * v[1];
        b[1] = u[2] * v[0] - u[0] * v[2];
        b[2] = u[0] * v[1] - u[1] * v[0];
        b[3] = 1 + u[0] * v[0] + u[1] * v[1] + u[2] * v[2];
        normalise(b);
    }
    multiply(b, ctx->first, ctx->result);
}

/* Bounded dispatch retains the first-operand reads and zero result for unknown actions. */
void lab_surface_camera(lab_surface_camera_action_t action, browser_jsref first, browser_jsref second,
                        browser_jsref third, browser_jsref box, bool roll)
{
    /** @brief Camera operation with first-operand and result sizes; nine results form three rows. */
    static const struct {
        void (*apply)(struct lab_surface_camera_context *);
        unsigned input_count, output_count;
    } operations[] = {
        [LAB_SURFACE_CAMERA_FROM_ANGLES] = {camera_from_angles, 3, 4},
        [LAB_SURFACE_CAMERA_ROWS]        = {camera_rows,        4, 9},
        [LAB_SURFACE_CAMERA_TO_ANGLES]   = {camera_to_angles,   4, 3},
        [LAB_SURFACE_CAMERA_DRAG]        = {camera_drag,        4, 4},
        [LAB_SURFACE_CAMERA_MULTIPLY]    = {camera_multiply,    4, 4},
    };
    double a[4] = {0}, result[9] = {0};
    int valid = (unsigned)action < sizeof operations / sizeof *operations;
    read_vector(first, a, valid ? operations[action].input_count : 4);
    struct lab_surface_camera_context ctx = {a, result, second, third, box, roll};
    if (valid)
        operations[action].apply(&ctx);
    unsigned output_count = valid ? operations[action].output_count : 4;
    if (output_count == 9) {
        int out = lab_dom_object(LAB_VALUE_ARRAY);
        for (unsigned i = 0; i < 3; ++i)
            lab_dom_push(out, vector(result + i * 3, 3));
        lab_dom_return(out);
    } else
        lab_dom_return(vector(result, output_count));
}
