#ifndef MARS_ALMANAC_CARTESIAN_H
#define MARS_ALMANAC_CARTESIAN_H

#include <stdbool.h>

typedef struct cartesian3_t {
    double x;
    double y;
    double z;
} cartesian3_t;

cartesian3_t cartesian_add(const cartesian3_t *a, const cartesian3_t *b);
cartesian3_t cartesian_scale(const cartesian3_t *value, double scale);
double cartesian_length(const cartesian3_t *value);
bool cartesian_normalize_in_place(cartesian3_t *value);
cartesian3_t cartesian_subtract(const cartesian3_t *a, const cartesian3_t *b);
cartesian3_t cartesian_negate(const cartesian3_t *value);
cartesian3_t cartesian_cross(const cartesian3_t *a, const cartesian3_t *b);
double cartesian_dot(const cartesian3_t *a, const cartesian3_t *b);
cartesian3_t cartesian_rotate_x(const cartesian3_t *value, double radians);
cartesian3_t cartesian_rotate_z(const cartesian3_t *value, double radians);

#endif /* MARS_ALMANAC_CARTESIAN_H */
