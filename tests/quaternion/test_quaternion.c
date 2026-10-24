/**
 * @file test_quaternion.c
 * @brief Native fixed-precision quaternion algebra, rotation and interpolation regressions.
 *
 * Checks Hamilton ordering, aliasing, extreme magnitudes, atomic rejection,
 * orthonormal matrices, shortest arcs and repeated camera composition. The
 * documented rotation example runs last, after ordinary module checks.
 */
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "quaternion-t.h"
#include "test_harness.h"

TEST_SUITE_CONFIG(TEST_CONFIG_GLOBAL);

static bool near(const double *a, const double *b, unsigned count)
{
    for (unsigned i = 0; i < count; ++i) if (!isfinite(a[i]) || fabs(a[i] - b[i]) > 1e-12) return false;
    return true;
}

static void test_quaternion_algebra(void)
{
    quaternion_t i = {1, 0, 0, 0}, j = {0, 1, 0, 0}, k = {0, 0, 1, 0}, result, identity;
    quaternion_identity(identity);
    TEST_ASSERT_TRUE(quaternion_is_unit(identity, 0), "identity has exact unit length");
    TEST_ASSERT_TRUE(quaternion_multiply(result, i, j) && near(result, k, 4), "i times j is k");
    k[2] = -1;
    TEST_ASSERT_TRUE(quaternion_multiply(result, j, i) && near(result, k, 4), "j times i is minus k");
    TEST_ASSERT_TRUE(quaternion_multiply(i, i, i) && i[3] == -1 && i[0] == 0, "both operands can alias output");
    quaternion_t q = {1, 2, 3, 4}, inverse;
    TEST_ASSERT_TRUE(quaternion_inverse(inverse, q) && quaternion_multiply(result, q, inverse) && near(result, identity, 4),
                     "non-unit inverse preserves Hamilton identity");
    TEST_ASSERT_TRUE(quaternion_conjugate(q, q) && q[0] == -1 && q[1] == -2 && q[2] == -3 && q[3] == 4,
                     "conjugation aliases without normalising");
    quaternion_t scaled = {0, 0, 0, 2};
    TEST_ASSERT_TRUE(quaternion_multiply(scaled, scaled, scaled) && scaled[3] == 4, "multiply is not silently normalised");
}

static void test_quaternion_range(void)
{
    quaternion_t huge = {DBL_MAX, DBL_MAX, DBL_MAX, DBL_MAX};
    quaternion_t tiny = {DBL_TRUE_MIN, DBL_TRUE_MIN, 0, 0}, result, inverse, identity;
    quaternion_identity(identity);
    TEST_ASSERT_TRUE(quaternion_normalise(result, huge) && result[0] == .5 && result[3] == .5,
                     "normalisation avoids squared-norm overflow");
    TEST_ASSERT_TRUE(quaternion_normalise(result, tiny) && quaternion_is_unit(result, 1e-14) &&
                     fabs(result[0] - sqrt(.5)) < 1e-14, "subnormal inputs do not collapse to zero");
    quaternion_t large = {0, 0, 0, DBL_MAX}, small = {0, 0, 0, DBL_MIN};
    TEST_ASSERT_TRUE(quaternion_inverse(inverse, large) && quaternion_multiply(result, large, inverse) &&
                     near(result, identity, 4), "large inverse avoids overflow");
    TEST_ASSERT_TRUE(quaternion_inverse(inverse, small) && quaternion_multiply(result, small, inverse) &&
                     near(result, identity, 4), "small representable inverse avoids premature overflow");
    quaternion_t tiny_turn = {1e-20, 0, 0, 1};
    TEST_ASSERT_TRUE(quaternion_multiply(result, tiny_turn, identity) && result[0] == tiny_turn[0],
                     "identity product does not lose tiny rotation components to cancellation");
    quaternion_t mixed = {DBL_MAX, 0, 0, DBL_MAX};
    TEST_ASSERT_TRUE(quaternion_multiply(result, mixed, small) && result[0] == DBL_MAX * DBL_MIN &&
                     result[3] == DBL_MAX * DBL_MIN, "finite products do not overflow preparatory component sums");
}

static void test_quaternion_rotations(void)
{
    const double axis[] = {1, 2, 3}, point[] = {2, -1, 4};
    quaternion_t q, next, product;
    double matrix[9], result[3], expected[3], intermediate[3];
    TEST_ASSERT_TRUE(quaternion_axis_angle(q, axis, .8) && quaternion_matrix3(matrix, q), "axis-angle supplies matrix");
    for (unsigned row = 0; row < 3; ++row) for (unsigned col = 0; col < 3; ++col) {
        double dot = 0;
        for (unsigned k = 0; k < 3; ++k) dot += matrix[row * 3 + k] * matrix[col * 3 + k];
        TEST_ASSERT_TRUE(fabs(dot - (row == col ? 1 : 0)) < 1e-12, "rotation rows are orthonormal");
    }
    const double y[] = {0, 1, 0};
    TEST_ASSERT_TRUE(quaternion_axis_angle(next, y, -.4) && quaternion_multiply(product, q, next) &&
        quaternion_rotate(result, product, point) && quaternion_rotate(intermediate, next, point) &&
        quaternion_rotate(expected, q, intermediate) && near(result, expected, 3), "right-hand rotation is applied first");
    memcpy(intermediate, point, sizeof point);
    TEST_ASSERT_TRUE(quaternion_rotate(intermediate, product, intermediate) && near(result, intermediate, 3),
                     "vector rotation can be performed in place");
    double overlapping[9] = {q[0], q[1], q[2], q[3]};
    TEST_ASSERT_TRUE(quaternion_matrix3(overlapping, overlapping) && near(matrix, overlapping, 9),
                     "matrix output may overlap quaternion storage");
    TEST_ASSERT_TRUE(quaternion_axis_angle(next, axis, .0001), "small rotation is valid");
    quaternion_identity(q);
    for (unsigned n = 0; n < 10000; ++n) {
        TEST_ASSERT_TRUE(quaternion_multiply(q, next, q) && quaternion_normalise(q, q), "repeated rotation remains finite");
    }
    TEST_ASSERT_TRUE(quaternion_is_unit(q, 1e-12), "repeated composition has no accumulated norm drift");
}

static void test_quaternion_directions(void)
{
    const double from[] = {1, 0, 0};
    const double targets[][3] = {{1, 0, 0}, {0, 1, 0}, {-1, 0, 0}, {-1, 1e-10, 0}, {0, 0, -1}};
    for (unsigned i = 0; i < sizeof targets / sizeof *targets; ++i) {
        quaternion_t q;
        double result[3];
        TEST_ASSERT_TRUE(quaternion_from_vectors(q, from, targets[i]) && quaternion_rotate(result, q, from) &&
                         near(result, targets[i], 3), "direction mapping handles coincident and antipodal vectors");
    }
}

static void test_quaternion_interpolation(void)
{
    const double axis[] = {0, 0, 1}, x[] = {1, 0, 0}, y[] = {0, 1, 0};
    quaternion_t start, end, result;
    quaternion_identity(start);
    double rotated[3];
    TEST_ASSERT_TRUE(quaternion_axis_angle(end, axis, acos(-1)) && quaternion_slerp(result, start, end, .5) &&
                     quaternion_rotate(rotated, result, x) && near(rotated, y, 3), "half turn interpolates to quarter turn");
    TEST_ASSERT_TRUE(quaternion_slerp(result, start, end, 0) && near(result, start, 4) &&
                     quaternion_slerp(result, start, end, 1) && near(result, end, 4), "interpolation endpoints are preserved");
    for (unsigned i = 0; i < 4; ++i) end[i] = -start[i];
    TEST_ASSERT_TRUE(quaternion_slerp(result, start, end, .5) && near(result, start, 4), "opposite signs mean the same rotation");
    TEST_ASSERT_TRUE(quaternion_slerp(result, start, end, 1) && near(result, end, 4), "exact endpoint sign is retained");
    TEST_ASSERT_TRUE(quaternion_axis_angle(end, axis, 1e-9) && quaternion_slerp(start, start, end, .5) &&
                     quaternion_is_unit(start, 1e-12), "near-coincident interpolation is stable and aliases its input");
}

static void test_quaternion_rejections(void)
{
    quaternion_t saved = {1, 2, 3, 4}, result = {1, 2, 3, 4}, zero = {0}, invalid = {NAN, 0, 0, 1};
    const double axis[] = {0, 0, 0}, vector[] = {1, 0, 0};
    TEST_ASSERT_TRUE(!quaternion_normalise(result, zero) && !quaternion_normalise(result, invalid) &&
        !quaternion_multiply(result, invalid, saved) && !quaternion_conjugate(result, invalid) &&
        !quaternion_inverse(result, zero) && !quaternion_axis_angle(result, axis, 1) &&
        !quaternion_from_vectors(result, axis, vector) && !quaternion_slerp(result, saved, saved, NAN) &&
        !quaternion_slerp(result, saved, saved, -1) && !quaternion_slerp(result, saved, saved, 2) &&
        !memcmp(result, saved, sizeof result), "failed operations leave output byte-for-byte unchanged");
    double matrix[9] = {1, 2, 3, 4, 5, 6, 7, 8, 9}, before[9];
    memcpy(before, matrix, sizeof matrix);
    TEST_ASSERT_TRUE(!quaternion_matrix3(matrix, invalid) && !quaternion_rotate(matrix, zero, vector) &&
                     !memcmp(matrix, before, sizeof matrix), "failed transforms are atomic");
    TEST_ASSERT_TRUE(!quaternion_normalise(NULL, saved) && !quaternion_normalise(result, NULL) &&
        !quaternion_matrix3(NULL, saved) && !quaternion_rotate(NULL, saved, vector) &&
        !quaternion_from_vectors(result, NULL, vector) && !quaternion_inverse(NULL, saved) &&
        !quaternion_is_unit(NULL, 1e-6) && !quaternion_is_unit(saved, NAN), "null operands and invalid tolerance are rejected");
    TEST_ASSERT_TRUE(!quaternion_is_unit(zero, 2) && !quaternion_is_unit(saved, -1),
                     "zero is not a unit rotation even with loose tolerance");
    quaternion_t huge = {DBL_MAX, DBL_MAX, DBL_MAX, DBL_MAX}, tiny = {DBL_TRUE_MIN, 0, 0, 0};
    TEST_ASSERT_TRUE(!quaternion_multiply(result, huge, huge) && !quaternion_inverse(result, tiny) &&
                     !memcmp(result, saved, sizeof result), "unrepresentable results leave the destination unchanged");
    quaternion_identity(NULL);
}

static void test_quaternion_product_validation(void)
{
    const double bad[] = {NAN, INFINITY, -INFINITY};
    const quaternion_t partners[] = {{0, 0, 0, 0}, {0, 0, 0, 1}, {1, -2, 3, -4}};
    const quaternion_t saved = {5, 6, 7, 8};
    for (unsigned component = 0; component < 4; ++component) {
        for (unsigned kind = 0; kind < 3; ++kind) {
            quaternion_t invalid = {1, 2, 3, 4}, result;
            invalid[component] = bad[kind];
            for (unsigned partner = 0; partner < 3; ++partner) {
                memcpy(result, saved, sizeof result);
                TEST_ASSERT_TRUE(!quaternion_multiply(result, invalid, partners[partner]) &&
                    !quaternion_multiply(result, partners[partner], invalid) &&
                    !quaternion_multiply(result, invalid, invalid) && !memcmp(result, saved, sizeof result),
                    "single result validation rejects every non-finite operand component atomically");
            }
            quaternion_t before;
            memcpy(before, invalid, sizeof before);
            TEST_ASSERT_TRUE(!quaternion_multiply(invalid, invalid, partners[1]) &&
                !quaternion_conjugate(invalid, invalid) && !memcmp(invalid, before, sizeof before),
                "non-finite in-place operands remain byte-for-byte unchanged");
        }
    }
    quaternion_t result = {5, 6, 7, 8};
    TEST_ASSERT_TRUE(!quaternion_multiply(NULL, saved, saved) && !quaternion_multiply(result, NULL, saved) &&
        !quaternion_multiply(result, saved, NULL) && !quaternion_conjugate(result, NULL) &&
        !quaternion_conjugate(NULL, saved) && !memcmp(result, saved, sizeof result), "null checks precede all dereferences");
}

/* README example: docs/globe.md, allocation-free quarter-turn rotation. */
static void example_quaternion_rotation(void)
{
    quaternion_t rotation;
    double axis[] = {0, 0, 1}, vector[] = {1, 0, 0}, result[3];
    bool ok = quaternion_axis_angle(rotation, axis, acos(-1) / 2) && quaternion_rotate(result, rotation, vector);
    TEST_ASSERT_TRUE(ok && fabs(result[0]) < 1e-12 && fabs(result[1] - 1) < 1e-12 && fabs(result[2]) < 1e-12,
                     "README quarter-turn result");
    if (ok) printf("Rotated: %.0f %.0f %.0f\n", result[0], result[1], result[2]);
}

int tests_main(void)
{
    TEST_RUN_IN_GROUP(test_quaternion_algebra, tests, NULL);
    TEST_RUN_IN_GROUP(test_quaternion_range, tests, NULL);
    TEST_RUN_IN_GROUP(test_quaternion_rotations, tests, NULL);
    TEST_RUN_IN_GROUP(test_quaternion_directions, tests, NULL);
    TEST_RUN_IN_GROUP(test_quaternion_interpolation, tests, NULL);
    TEST_RUN_IN_GROUP(test_quaternion_rejections, tests, NULL);
    TEST_RUN_IN_GROUP(test_quaternion_product_validation, tests, NULL);
    TEST_RUN_OUTPUT_IN_GROUP(example_quaternion_rotation, readme_examples);
    return TESTS_EXIT_CODE();
}
