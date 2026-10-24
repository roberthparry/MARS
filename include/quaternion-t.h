/**
 * @file quaternion-t.h
 * @brief Allocation-free binary64 quaternions and three-dimensional rotations.
 *
 * A quaternion is a caller-owned four-double value in [x,y,z,w] order, with the
 * scalar component last. Hamilton multiplication applies the right rotation
 * first when acting on column vectors. Rotation matrices are row-major, angles
 * are radians, and spherical interpolation follows the shortest rotation arc.
 * Algebraic multiplication is not implicitly normalised; the rotation operations
 * normalise their inputs. Inputs may alias outputs. Invalid or non-finite inputs
 * and non-finite arithmetic results fail without changing the output; zero
 * quaternions cannot represent rotations. Boolean operations return false for
 * NULL operands. Output buffers must have the documented writable extent.
 * Finite results retain ordinary binary64 rounding, including underflow of tiny
 * coefficients; this is not exact arithmetic or an error-bounded interval API.
 * All operations are re-entrant and use only stack storage and standard maths,
 * including in freestanding WASM builds. This fixed-precision geometry module
 * is independent of number_t and the arbitrary-precision symbolic matrix module.
 */
#ifndef MARS_QUATERNION_H
#define MARS_QUATERNION_H

#include <stdbool.h>

/**
 * @brief Caller-owned coefficients of x*i + y*j + z*k + w.
 *
 * Indices 0, 1 and 2 hold the vector part; index 3 holds the scalar part. This
 * fixed-size array has value storage, not an opaque allocation: declare it on
 * the stack or embed it in another object, and do not free it separately.
 * Function parameters borrow its storage for the duration of the call. A unit
 * value represents an active right-handed rotation; q and -q represent the
 * same rotation. The identity is {0, 0, 0, 1}.
 */
typedef double quaternion_t[4];

/**
 * @brief Initialise a quaternion to the exact identity rotation.
 * @param[out] output Writable storage for four doubles, or NULL for no operation.
 * Writes {0, 0, 0, 1}. No initialisation of the destination is required.
 */
void quaternion_identity(quaternion_t output);

/**
 * @brief Convert a finite, non-zero quaternion to unit length.
 * @param[out] output Writable four-double result; may be the same storage as value.
 * @param[in] value Borrowed coefficients, which need not already have unit length.
 * @return True on success; false for NULL storage, a zero value or non-finite coefficients.
 * Preserves the quaternion's direction and sign. A scaled norm avoids overflow
 * and underflow for extreme and subnormal inputs. Ordinary values use the direct
 * squared norm. Failure leaves every output coefficient unchanged.
 */
bool quaternion_normalise(quaternion_t output, const quaternion_t value);

/**
 * @brief Check a persisted or computed quaternion's unit-length invariant.
 * @param[in] value Borrowed four-double quaternion; it is never modified.
 * @param tolerance Finite, non-negative absolute tolerance on squared length, not on length.
 * @return True when the finite squared length differs from one by at most tolerance;
 * false for NULL, zero/non-finite input, a non-finite squared length or an invalid tolerance.
 * This check does not normalise or repair its input. Callers choose a tolerance
 * appropriate to their persistence or accumulated rounding-error policy.
 */
bool quaternion_is_unit(const quaternion_t value, double tolerance);

/**
 * @brief Compute the Hamilton product left * right.
 * @param[out] output Writable four-double product; may alias either or both inputs.
 * @param[in] left Borrowed left factor, not necessarily a unit quaternion.
 * @param[in] right Borrowed right factor, not necessarily a unit quaternion.
 * @return True for a finite product; false for NULL operands, non-finite inputs
 * or arithmetic which produces non-finite coefficients. Failure leaves output unchanged.
 * Multiplication is non-commutative and does not normalise the result. For unit
 * rotations acting on column vectors, right is applied first, then left.
 * Repeated camera composition can explicitly call quaternion_normalise to
 * control norm drift. A zero algebraic product is valid, but is not a rotation.
 * The direct kernel uses 16 scalar multiplications and 12 additions/subtractions;
 * validation adds no norm, division or implicit normalisation. Non-finite inputs
 * propagate to a non-finite result, so one result check also rejects those inputs.
 */
bool quaternion_multiply(quaternion_t output, const quaternion_t left, const quaternion_t right);

/**
 * @brief Form the conjugate {-x, -y, -z, w} without changing magnitude.
 * @param[out] output Writable four-double result; may alias value.
 * @param[in] value Borrowed finite quaternion, including zero or a non-unit value.
 * @return True on success; false for NULL operands or non-finite coefficients,
 * leaving output unchanged on failure.
 * The conjugate is the inverse only for unit quaternions. Use quaternion_inverse
 * for a general non-zero value.
 */
bool quaternion_conjugate(quaternion_t output, const quaternion_t value);

/**
 * @brief Compute the multiplicative inverse conjugate(value) / squared_length(value).
 * @param[out] output Writable four-double inverse; may alias value.
 * @param[in] value Borrowed finite, non-zero quaternion; unit length is not required.
 * @return True when the inverse is representable by finite binary64 coefficients;
 * false for NULL operands, zero/non-finite input or overflow, leaving output unchanged.
 * Scaling avoids overflow in the original squared norm. Extremely small values
 * can still have an inverse outside binary64 range; those values are rejected,
 * not silently replaced with the identity.
 */
bool quaternion_inverse(quaternion_t output, const quaternion_t value);

/**
 * @brief Construct a right-handed unit rotation about an axis.
 * @param[out] output Writable four-double quaternion; may overlap the input axis.
 * @param[in] axis Borrowed finite, non-zero three-vector; normalised internally.
 * @param angle Finite rotation angle in radians; negative angles reverse the rotation.
 * @return True on success; false for NULL operands, a zero/non-finite axis or a
 * non-finite angle, leaving output unchanged.
 * The vector coefficients are unit_axis * sin(angle/2), and the scalar is
 * cos(angle/2). A zero axis is invalid even for a zero angle; use
 * quaternion_identity when no axis is needed.
 */
bool quaternion_axis_angle(quaternion_t output, const double axis[3], double angle);

/**
 * @brief Construct the shortest rotation carrying one direction onto another.
 * @param[out] output Writable unit quaternion; may overlap either input vector.
 * @param[in] from Borrowed finite, non-zero source three-vector.
 * @param[in] to Borrowed finite, non-zero destination three-vector.
 * @return True on success; false for NULL operands or zero/non-finite vectors,
 * leaving output unchanged.
 * Magnitudes are ignored. Equal directions produce identity. Exactly opposite
 * directions have no unique shortest axis: the implementation deterministically
 * selects a perpendicular axis using the least-aligned coordinate axis. This
 * prevents a half-turn gesture from degenerating into a zero quaternion.
 */
bool quaternion_from_vectors(quaternion_t output, const double from[3], const double to[3]);

/**
 * @brief Convert a quaternion to a row-major rotation matrix.
 * @param[out] output Writable storage for nine doubles; may overlap value if it has that extent.
 * @param[in] value Borrowed finite, non-zero quaternion, normalised internally.
 * @return True on success; false for NULL operands or an invalid rotation,
 * leaving all nine output values unchanged on failure.
 * The result is orthonormal to binary64 rounding precision and multiplies column
 * vectors: out[row] = sum(matrix[3*row+column] * in[column]). Transposing it gives
 * the inverse rotation. Graphics APIs with column-major storage must account for
 * this convention explicitly; this function does not depend on WebGL.
 */
bool quaternion_matrix3(double output[9], const quaternion_t value);

/**
 * @brief Apply a quaternion's active rotation to a three-vector.
 * @param[out] output Writable three-double vector; may overlap either input.
 * @param[in] rotation Borrowed finite, non-zero quaternion, normalised internally.
 * @param[in] vector Borrowed finite three-vector; a zero vector is permitted.
 * @return True for a finite result; false for NULL operands, an invalid rotation,
 * non-finite vector components or arithmetic overflow, leaving output unchanged.
 * Uses the same row-major transform as quaternion_matrix3. The operation changes
 * direction, not magnitude, subject to ordinary floating-point rounding.
 */
bool quaternion_rotate(double output[3], const quaternion_t rotation, const double vector[3]);

/**
 * @brief Spherically interpolate rotations along their shortest arc.
 * @param[out] output Writable unit quaternion; may alias either input.
 * @param[in] start Borrowed finite, non-zero initial quaternion, normalised internally.
 * @param[in] end Borrowed finite, non-zero final quaternion, normalised internally.
 * @param fraction Finite interpolation fraction in [0,1]; extrapolation is rejected, not clamped.
 * @return True on success; false for NULL operands, invalid rotations or an invalid
 * fraction, leaving output unchanged.
 * Opposite quaternion signs describe the same rotation, so an equivalent endpoint
 * sign is chosen for the shorter arc. Near-coincident endpoints use normalised
 * linear interpolation to avoid dividing by a tiny sine. At exactly zero or one,
 * the corresponding normalised input's sign is preserved. Consequently the
 * coefficients can change sign at an endpoint without any jump in the rotation.
 */
bool quaternion_slerp(quaternion_t output, const quaternion_t start, const quaternion_t end, double fraction);

#endif
