/**
 * @file lab_surface_math.h
 * @brief Bounded, normalised display geometry for the freestanding browser renderer.
 *
 * These routines operate on camera angles and unit-volume coordinates, never on
 * mathematical plot bounds. Native MARS number_t owns those at working precision.
 * Boolean parameters use the wasm32 i32 ABI; browser callers must supply exactly 0 or 1.
 */
#ifndef MARS_LAB_SURFACE_MATH_H
#define MARS_LAB_SURFACE_MATH_H

#include <stdbool.h>

#include "lab_handle.h"

/**
 * @brief Scoped browser handle to a quaternion array in [x, y, z, w] order.
 *
 * Borrows the array for the current synchronous bridge call; it is not a pointer
 * into WebAssembly memory. This alias documents the role without changing the i32 ABI.
 */
typedef browser_handle quaternion_handle;

/** @brief Camera geometry operations; numeric values are fixed by the browser ABI. */
typedef enum {
    LAB_SURFACE_CAMERA_FROM_ANGLES = 0, /**< Convert degree angles to a quaternion. */
    LAB_SURFACE_CAMERA_ROWS = 1,        /**< Convert a quaternion to three rotation rows. */
    LAB_SURFACE_CAMERA_TO_ANGLES = 2,   /**< Convert a quaternion to degree angles. */
    LAB_SURFACE_CAMERA_DRAG = 3,        /**< Rotate a quaternion using a screen-space gesture. */
    LAB_SURFACE_CAMERA_MULTIPLY = 4     /**< Multiply and normalise two quaternions. */
} lab_surface_camera_action_t;

/**
 * @brief Evaluate the direction of a normalised display vector.
 * @param y Finite vertical vector component.
 * @param x Finite horizontal vector component in the same units as y.
 * @return Direction in radians in [-pi, pi]; every signed-zero vector returns positive zero.
 */
double lab_surface_atan2(double y, double x);

/**
 * @brief Evaluate a bounded relative binary display scale.
 * @param exponent Base-two exponent, clamped to -1022..1023, including infinities; NaN propagates.
 * @return Double-precision two raised to the clamped exponent, or NaN for NaN input.
 */
double lab_surface_exp2(double exponent);

/**
 * @brief Recover the binary scale of a finite positive gesture ratio.
 * @param value Dimensionless gesture scale ratio.
 * @return Double-precision base-two logarithm, or zero for a non-positive or non-finite ratio.
 */
double lab_surface_log2(double value);

/**
 * @brief Dispatch camera conversions and gestures using scoped browser arrays.
 * @param action Camera operation from lab_surface_camera_action_t; operand roles follow its ABI codes below.
 * @param first Scoped [azimuth, elevation, roll] array in degrees for action 0; [x, y, z, w] quaternion otherwise.
 * @param second Scoped previous angles in degrees for action 2, starting [clientX, clientY] in CSS pixels for
 * action 3, or right-hand quaternion for action 4; unused for actions 0 and 1.
 * @param third Scoped ending [clientX, clientY] array in CSS pixels for action 3; unused otherwise.
 * @param box Scoped rectangle with left, top, width and height in CSS pixels for trackball action 3.
 * @param roll True selects horizontal roll instead of trackball rotation for action 3; unused otherwise.
 * @details Publishes a quaternion, three row arrays (action 1), or three degree angles (action 2) through
 * lab_dom_return. Unknown action codes publish a four-zero array; no scoped handle is retained.
 */
void lab_surface_camera(lab_surface_camera_action_t action, browser_handle first, browser_handle second,
                        browser_handle third, browser_handle box, bool roll);

/**
 * @brief Read and normalise a scoped quaternion into a fixed display rotation.
 * @param quaternion Scoped browser [x, y, z, w] quaternion array.
 * @param[out] matrix Caller-owned storage for nine doubles, written in row-major order.
 */
void lab_surface_rotation(quaternion_handle quaternion, double matrix[9]);

#endif
