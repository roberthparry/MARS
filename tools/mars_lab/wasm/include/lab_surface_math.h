/**
 * @file lab_surface_math.h
 * @brief Bounded, normalised display geometry for the freestanding browser renderer.
 *
 * These routines operate on camera angles and unit-volume coordinates, never on
 * mathematical plot bounds. Native MARS number_t owns those at working precision.
 */
#ifndef MARS_LAB_SURFACE_MATH_H
#define MARS_LAB_SURFACE_MATH_H

/** @brief Evaluate sine of a bounded camera angle in radians. */
double lab_surface_sin(double angle);

/** @brief Evaluate cosine of a bounded camera angle in radians. */
double lab_surface_cos(double angle);

/** @brief Evaluate the direction of a normalised display vector. */
double lab_surface_atan2(double y, double x);

/** @brief Evaluate a bounded relative binary display scale. */
double lab_surface_exp2(double exponent);

/** @brief Recover the binary scale of a finite positive gesture ratio. */
double lab_surface_log2(double value);

/** @brief Dispatch quaternion conversion, rows, angles, drag or multiplication from scoped arrays. */
void lab_surface_camera(unsigned action, int first, int second, int third, int box, int roll);

/** @brief Read a scoped unit quaternion into a fixed 3-by-3 display rotation. */
void lab_surface_rotation(int quaternion, double matrix[9]);

#endif
