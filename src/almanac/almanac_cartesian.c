#include <math.h>
#include <stdbool.h>

#include "almanac_cartesian.h"

cartesian3_t cartesian_add(const cartesian3_t *a, const cartesian3_t *b)
{
    cartesian3_t out;

    out.x = (a ? a->x : 0.0) + (b ? b->x : 0.0);
    out.y = (a ? a->y : 0.0) + (b ? b->y : 0.0);
    out.z = (a ? a->z : 0.0) + (b ? b->z : 0.0);
    return out;
}

cartesian3_t cartesian_scale(const cartesian3_t *value, double scale)
{
    cartesian3_t out;

    out.x = value ? value->x * scale : 0.0;
    out.y = value ? value->y * scale : 0.0;
    out.z = value ? value->z * scale : 0.0;
    return out;
}

double cartesian_length(const cartesian3_t *value)
{
    if (!value)
        return 0.0;
    return sqrt(value->x * value->x + value->y * value->y + value->z * value->z);
}

bool cartesian_normalize_in_place(cartesian3_t *value)
{
    double length;

    if (!value)
        return false;
    length = cartesian_length(value);
    if (length <= 0.0)
        return false;
    value->x /= length;
    value->y /= length;
    value->z /= length;
    return true;
}

cartesian3_t cartesian_subtract(const cartesian3_t *a, const cartesian3_t *b)
{
    cartesian3_t out;

    out.x = (a ? a->x : 0.0) - (b ? b->x : 0.0);
    out.y = (a ? a->y : 0.0) - (b ? b->y : 0.0);
    out.z = (a ? a->z : 0.0) - (b ? b->z : 0.0);
    return out;
}

cartesian3_t cartesian_negate(const cartesian3_t *value)
{
    cartesian3_t out;

    out.x = value ? -value->x : 0.0;
    out.y = value ? -value->y : 0.0;
    out.z = value ? -value->z : 0.0;
    return out;
}

cartesian3_t cartesian_cross(const cartesian3_t *a, const cartesian3_t *b)
{
    cartesian3_t out;

    out.x = (!a || !b) ? 0.0 : a->y * b->z - a->z * b->y;
    out.y = (!a || !b) ? 0.0 : a->z * b->x - a->x * b->z;
    out.z = (!a || !b) ? 0.0 : a->x * b->y - a->y * b->x;
    return out;
}

double cartesian_dot(const cartesian3_t *a, const cartesian3_t *b)
{
    if (!a || !b)
        return 0.0;
    return a->x * b->x + a->y * b->y + a->z * b->z;
}

cartesian3_t cartesian_rotate_x(const cartesian3_t *value, double radians)
{
    cartesian3_t out;
    double c;
    double s;

    sincos(radians, &s, &c);

    out.x = value ? value->x : 0.0;
    out.y = value ? value->y * c - value->z * s : 0.0;
    out.z = value ? value->y * s + value->z * c : 0.0;
    return out;
}

cartesian3_t cartesian_rotate_z(const cartesian3_t *value, double radians)
{
    cartesian3_t out;
    double c;
    double s;

    sincos(radians, &s, &c);

    out.x = value ? value->x * c - value->y * s : 0.0;
    out.y = value ? value->x * s + value->y * c : 0.0;
    out.z = value ? value->z : 0.0;
    return out;
}
