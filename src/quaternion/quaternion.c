/**
 * @file quaternion.c
 * @brief Fixed-size Hamilton algebra and robust binary64 camera rotations.
 *
 * Implements allocation-free quaternion operations for native and WASM callers.
 * Ordinary rotations use a direct norm; extreme magnitudes use scaling to avoid
 * overflow and underflow. Every operation computes into local storage before
 * publishing, preserving aliasing and failure atomicity without locks or state.
 */
#include <float.h>
#include <math.h>
#include <stddef.h>

#include "quaternion-t.h"

static bool quaternion_finite(const quaternion_t value)
{
    return value && isfinite(value[0]) && isfinite(value[1]) && isfinite(value[2]) && isfinite(value[3]);
}

static bool quaternion_publish(quaternion_t output, const quaternion_t value)
{
    if (!output || !quaternion_finite(value)) return false;
    for (unsigned i = 0; i < 4; ++i) output[i] = value[i];
    return true;
}

/* Initialise a caller-owned identity value. */
void quaternion_identity(quaternion_t output)
{
    const quaternion_t identity = {0, 0, 0, 1};
    quaternion_publish(output, identity);
}

/* Scale only when an ordinary squared norm could lose range or precision. */
bool quaternion_normalise(quaternion_t output, const quaternion_t value)
{
    if (!output || !quaternion_finite(value)) return false;
    quaternion_t result;
    double squared = value[0] * value[0] + value[1] * value[1] + value[2] * value[2] + value[3] * value[3];
    if (isfinite(squared) && squared >= DBL_MIN) {
        double inverse = 1 / sqrt(squared);
        for (unsigned i = 0; i < 4; ++i) result[i] = value[i] * inverse;
    } else {
        double scale = fmax(fmax(fabs(value[0]), fabs(value[1])), fmax(fabs(value[2]), fabs(value[3])));
        if (!scale) return false;
        squared = 0;
        for (unsigned i = 0; i < 4; ++i) { result[i] = value[i] / scale; squared += result[i] * result[i]; }
        double inverse = 1 / sqrt(squared);
        for (unsigned i = 0; i < 4; ++i) result[i] *= inverse;
    }
    return quaternion_publish(output, result);
}

/* Validate persisted unit rotations without modifying them. */
bool quaternion_is_unit(const quaternion_t value, double tolerance)
{
    if (!quaternion_finite(value) || !isfinite(tolerance) || tolerance < 0) return false;
    double squared = value[0] * value[0] + value[1] * value[1] + value[2] * value[2] + value[3] * value[3];
    return squared > 0 && isfinite(squared) && fabs(squared - 1) <= tolerance;
}

/* Preserve Hamilton algebra; callers choose whether to normalise a product. */
bool quaternion_multiply(quaternion_t output, const quaternion_t a, const quaternion_t b)
{
    if (!output || !a || !b) return false;
    /* 16 products and 12 sums/differences. Every input contributes to every result,
     * so any non-finite input necessarily fails the single publication check. */
    double x = a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1];
    double y = a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0];
    double z = a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3];
    double w = a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2];
    if (!isfinite(x) || !isfinite(y) || !isfinite(z) || !isfinite(w)) return false;
    /* Keep coefficients in registers until every result is validated; no temporary array or helper call. */
    output[0] = x;
    output[1] = y;
    output[2] = z;
    output[3] = w;
    return true;
}

/* Conjugation does not silently turn an arbitrary quaternion into a unit rotation. */
bool quaternion_conjugate(quaternion_t output, const quaternion_t value)
{
    if (!output || !value) return false;
    quaternion_t result = {-value[0], -value[1], -value[2], value[3]};
    return quaternion_publish(output, result);
}

/* Divide the scaled conjugate by its squared magnitude without overflowing the original norm. */
bool quaternion_inverse(quaternion_t output, const quaternion_t value)
{
    if (!output || !quaternion_finite(value)) return false;
    double scale = fmax(fmax(fabs(value[0]), fabs(value[1])), fmax(fabs(value[2]), fabs(value[3])));
    if (!scale) return false;
    quaternion_t result;
    double squared = 0;
    for (unsigned i = 0; i < 4; ++i) { result[i] = value[i] / scale; squared += result[i] * result[i]; }
    for (unsigned i = 0; i < 4; ++i) result[i] = (result[i] / squared) / scale * (i == 3 ? 1 : -1);
    return quaternion_publish(output, result);
}

/* Construct a rotation independently of the input axis magnitude. */
bool quaternion_axis_angle(quaternion_t output, const double axis[3], double angle)
{
    if (!output || !axis || !isfinite(angle)) return false;
    quaternion_t direction = {axis[0], axis[1], axis[2], 0};
    if (!quaternion_normalise(direction, direction)) return false;
    double sine = sin(angle / 2);
    quaternion_t result = {direction[0] * sine, direction[1] * sine, direction[2] * sine, cos(angle / 2)};
    return quaternion_publish(output, result);
}

/* Choose the shortest direction rotation, including a deterministic antipodal axis. */
bool quaternion_from_vectors(quaternion_t output, const double from[3], const double to[3])
{
    if (!output || !from || !to) return false;
    quaternion_t a = {from[0], from[1], from[2], 0}, b = {to[0], to[1], to[2], 0};
    if (!quaternion_normalise(a, a) || !quaternion_normalise(b, b)) return false;
    double axis[] = {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
    double sine = hypot(hypot(axis[0], axis[1]), axis[2]), cosine = a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
    if (sine > 0) return quaternion_axis_angle(output, axis, atan2(sine, cosine));
    if (cosine >= 0) { quaternion_identity(output); return true; }
    unsigned least = fabs(a[0]) < fabs(a[1]) ? 0 : 1;
    if (fabs(a[2]) < fabs(a[least])) least = 2;
    quaternion_t perpendicular = {0};
    perpendicular[(least + 1) % 3] = a[(least + 2) % 3];
    perpendicular[(least + 2) % 3] = -a[(least + 1) % 3];
    return quaternion_normalise(output, perpendicular);
}

/* Produce the same row-major rotation in desktop and freestanding builds. */
bool quaternion_matrix3(double output[9], const quaternion_t value)
{
    quaternion_t q;
    if (!output || !quaternion_normalise(q, value)) return false;
    double x = q[0], y = q[1], z = q[2], w = q[3];
    double result[] = {1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w),
                      2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w),
                      2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)};
    for (unsigned i = 0; i < 9; ++i) output[i] = result[i];
    return true;
}

/* Rotate a vector without exposing a temporary matrix to the caller. */
bool quaternion_rotate(double output[3], const quaternion_t rotation, const double vector[3])
{
    double matrix[9], result[3];
    if (!output || !vector || !quaternion_matrix3(matrix, rotation)) return false;
    for (unsigned i = 0; i < 3; ++i) {
        result[i] = matrix[i * 3] * vector[0] + matrix[i * 3 + 1] * vector[1] + matrix[i * 3 + 2] * vector[2];
        if (!isfinite(result[i])) return false;
    }
    for (unsigned i = 0; i < 3; ++i) output[i] = result[i];
    return true;
}

/* Interpolate unit rotations without taking the long path across an antipodal representation. */
bool quaternion_slerp(quaternion_t output, const quaternion_t start, const quaternion_t end, double fraction)
{
    quaternion_t a, b, result;
    if (!output || !isfinite(fraction) || fraction < 0 || fraction > 1 ||
        !quaternion_normalise(a, start) || !quaternion_normalise(b, end)) return false;
    if (fraction == 0) return quaternion_publish(output, a);
    if (fraction == 1) return quaternion_publish(output, b);
    double dot = a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];
    if (dot < 0) { dot = -dot; for (unsigned i = 0; i < 4; ++i) b[i] = -b[i]; }
    double left = 1 - fraction, right = fraction;
    if (dot < .9995) {
        double angle = acos(fmin(1, dot)), divisor = sin(angle);
        left = sin((1 - fraction) * angle) / divisor;
        right = sin(fraction * angle) / divisor;
    }
    for (unsigned i = 0; i < 4; ++i) result[i] = left * a[i] + right * b[i];
    return quaternion_normalise(output, result);
}
