/**
 * @file lab_surface_math.c
 * @brief Small fixed-domain graphics operations for a freestanding WebAssembly viewport.
 *
 * Camera rotations and relative unit-volume scales need only display precision.
 * Bounded series with range reduction avoid a JavaScript libm dependency. These
 * are not mathematical expression evaluators: native MARS retains all actual
 * coordinate arithmetic and arbitrary-precision sampling. Square roots compile
 * directly to the WebAssembly f64.sqrt instruction.
 */
#include "lab_dom.h"
#include "lab_surface_math.h"

static const double pi = 3.14159265358979323846264338327950288;

/* Camera arguments are bounded before the convergent sine series. */
double lab_surface_sin(double x)
{
    x -= (long)(x / (2 * pi)) * (2 * pi);
    if (x > pi) x -= 2 * pi;
    if (x < -pi) x += 2 * pi;
    if (x > pi / 2) x = pi - x;
    if (x < -pi / 2) x = -pi - x;
    double term = x, sum = x;
    for (unsigned n = 1; n < 14; ++n) {
        term *= -x * x / ((2 * n) * (2 * n + 1));
        sum += term;
    }
    return sum;
}

/* Share the same reduced series for cosine. */
double lab_surface_cos(double x)
{
    return lab_surface_sin(x + pi / 2);
}

/* Half-angle reduction confines the alternating series to |x| <= tan(pi/8). */
double lab_surface_atan2(double y, double x)
{
    if (x == 0) return y > 0 ? pi / 2 : y < 0 ? -pi / 2 : 0;
    double t = __builtin_fabs(y / x);
    int inverse = t > 1;
    if (inverse) t = 1 / t;
    t /= 1 + __builtin_sqrt(1 + t * t);
    double term = t, sum = t;
    for (unsigned n = 1; n < 28; ++n) {
        term *= -t * t;
        sum += term / (2 * n + 1);
    }
    double angle = 2 * sum;
    if (inverse) angle = pi / 2 - angle;
    if (x < 0) angle = pi - angle;
    return y < 0 ? -angle : angle;
}

/* Only relative screen transforms use this bounded floating-point scale. */
double lab_surface_exp2(double x)
{
    if (x > 1023) x = 1023;
    if (x < -1022) x = -1022;
    int whole = (int)x;
    double fraction = (x - whole) * 0.6931471805599453094, sum = 1, term = 1;
    for (unsigned n = 1; n < 20; ++n) { term *= fraction / n; sum += term; }
    while (whole > 0) { sum *= 2; --whole; }
    while (whole < 0) { sum *= 0.5; ++whole; }
    return sum;
}

/* Recover a gesture exponent without handling absolute mathematical coordinates. */
double lab_surface_log2(double value)
{
    if (!(value > 0) || !__builtin_isfinite(value)) return 0;
    int exponent = 0;
    while (value >= 2) { value *= 0.5; ++exponent; }
    while (value < 1) { value *= 2; --exponent; }
    double t = (value - 1) / (value + 1), term = t, sum = t;
    for (unsigned n = 1; n < 28; ++n) { term *= t * t; sum += term / (2 * n + 1); }
    return exponent + 2 * sum / 0.6931471805599453094;
}

static void read_vector(int handle, double *values, unsigned count)
{
    for (unsigned i = 0; i < count; ++i) values[i] = lab_dom_to_number(lab_dom_item(handle, i));
}

static int vector(const double *values, unsigned count)
{
    int result = lab_dom_object(4);
    for (unsigned i = 0; i < count; ++i) lab_dom_push(result, lab_dom_numeric(values[i]));
    return result;
}

static void normalise(double q[4])
{
    double length = __builtin_sqrt(q[0]*q[0] + q[1]*q[1] + q[2]*q[2] + q[3]*q[3]);
    if (length > 0) for (unsigned i = 0; i < 4; ++i) q[i] /= length;
    else q[3] = 1;
}

static void multiply(const double a[4], const double b[4], double q[4])
{
    double result[] = {
        a[3]*b[0] + a[0]*b[3] + a[1]*b[2] - a[2]*b[1],
        a[3]*b[1] - a[0]*b[2] + a[1]*b[3] + a[2]*b[0],
        a[3]*b[2] + a[0]*b[1] - a[1]*b[0] + a[2]*b[3],
        a[3]*b[3] - a[0]*b[0] - a[1]*b[1] - a[2]*b[2]
    };
    for (unsigned i = 0; i < 4; ++i) q[i] = result[i];
    normalise(q);
}

static void from_angles(const double angles[3], double q[4])
{
    double a = angles[0] * pi / 360, e = pi / 4 + angles[1] * pi / 360, r = angles[2] * pi / 360;
    double az[] = {0, 0, lab_surface_sin(a), lab_surface_cos(a)};
    double el[] = {lab_surface_sin(e), 0, 0, lab_surface_cos(e)};
    double ro[] = {0, 0, lab_surface_sin(r), lab_surface_cos(r)};
    multiply(el, az, q);
    multiply(ro, q, q);
}

static void rows(double q[4], double m[9])
{
    normalise(q);
    double x = q[0], y = q[1], z = q[2], w = q[3];
    double values[] = {1-2*(y*y+z*z), 2*(x*y-z*w), 2*(x*z+y*w),
                       2*(x*y+z*w), 1-2*(x*x+z*z), 2*(y*z-x*w),
                       2*(x*z-y*w), 2*(y*z+x*w), 1-2*(x*x+y*y)};
    for (unsigned i = 0; i < 9; ++i) m[i] = values[i];
}

static double wrap(double angle)
{
    angle *= 180 / pi;
    angle -= (long)(angle / 360) * 360;
    return angle < 0 ? angle + 360 : angle;
}

/* The graphics controller uses a fixed rotation matrix, never absolute coordinates. */
void lab_surface_rotation(int quaternion, double matrix[9])
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
    if (sine > 1) sine = 1;
    if (sine < -1) sine = -1;
    double cosine = __builtin_sqrt(1 - sine*sine), e = lab_surface_atan2(sine, cosine), a, r;
    if (cosine > 1e-7) {
        a = lab_surface_atan2(m[6], m[7]);
        r = lab_surface_atan2(m[2], -m[5]);
    } else {
        a = previous[0] * pi / 180;
        double angles[] = {previous[0], e * 180 / pi, 0}, base[4], residual[4];
        from_angles(angles, base);
        for (unsigned i = 0; i < 3; ++i) base[i] = -base[i];
        multiply(q, base, residual);
        r = 2 * lab_surface_atan2(residual[2], residual[3]);
    }
    out[0] = wrap(a); out[1] = e * 180 / pi; out[2] = wrap(r);
}

static void ball(const double p[2], int box, double out[3])
{
    double width = lab_dom_to_number(lab_dom_get(box, "width"));
    double height = lab_dom_to_number(lab_dom_get(box, "height"));
    double radius = (width < height ? width : height) / 2;
    if (radius < 1) radius = 1;
    double u = (p[0] - lab_dom_to_number(lab_dom_get(box, "left")) - width/2) / radius;
    double v = (p[1] - lab_dom_to_number(lab_dom_get(box, "top")) - height/2) / radius;
    double d = u*u + v*v, z = -(d <= 0.5 ? __builtin_sqrt(1-d) : 0.5/__builtin_sqrt(d));
    double length = __builtin_sqrt(d + z*z);
    out[0] = u/length; out[1] = v/length; out[2] = z/length;
}

/* Bounded dispatch for five fixed graphics operations, not a mathematical parser. */
void lab_surface_camera(unsigned action, int first, int second, int third, int box, int roll)
{
    double a[4] = {0}, b[4] = {0}, result[9] = {0};
    read_vector(first, a, action == 0 ? 3 : 4);
    if (action == 0) from_angles(a, result);
    else if (action == 1) rows(a, result);
    else if (action == 2) { read_vector(second, b, 3); to_angles(a, b, result); }
    else if (action == 4) { read_vector(second, b, 4); multiply(a, b, result); }
    else if (action == 3) {
        double from[2], to[2];
        read_vector(second, from, 2); read_vector(third, to, 2);
        if (roll) {
            double half = (to[0]-from[0])*pi/720;
            b[2] = lab_surface_sin(half); b[3] = lab_surface_cos(half);
        } else {
            double u[3], v[3];
            ball(from, box, u); ball(to, box, v);
            b[0] = u[1]*v[2]-u[2]*v[1]; b[1] = u[2]*v[0]-u[0]*v[2]; b[2] = u[0]*v[1]-u[1]*v[0];
            b[3] = 1 + u[0]*v[0] + u[1]*v[1] + u[2]*v[2];
            normalise(b);
        }
        multiply(b, a, result);
    }
    if (action == 1) {
        int out = lab_dom_object(4);
        for (unsigned i = 0; i < 3; ++i) lab_dom_push(out, vector(result + i*3, 3));
        lab_dom_return(out);
    } else lab_dom_return(vector(result, action == 2 ? 3 : 4));
}
