/**
 * @file test_webgl.c
 * @brief Application-independent geographic bounds and GPU coverage backend regressions.
 *
 * Exercises public APIs without a browser, application configuration or database.
 * A recording backend verifies caller-selected texture units and resource cleanup.
 */
#include <math.h>
#include <string.h>

#include "globe-t.h"
#include "webgl-t.h"
#include "test_harness.h"

TEST_SUITE_CONFIG(TEST_CONFIG_GLOBAL);

typedef struct _recording_gpu_t {
    unsigned next_id, unit, created, deleted, draws;
} recording_gpu_t;

static uint32_t record_call(void *context, unsigned operation, const uint32_t arguments[4])
{
    recording_gpu_t *gpu = context;
    if (operation == WEBGL_GPU_CREATE || operation == WEBGL_GPU_UNIFORM) {
        ++gpu->created;
        return ++gpu->next_id;
    }
    if (operation == WEBGL_GPU_DELETE) ++gpu->deleted;
    if (operation == WEBGL_GPU_ACTIVE_TEXTURE) gpu->unit = arguments[0];
    if (operation == WEBGL_MASK_DRAW) ++gpu->draws;
    return operation == WEBGL_GPU_SHADER_OK || operation == WEBGL_GPU_PROGRAM_OK ||
           operation == WEBGL_GPU_FRAMEBUFFER_OK;
}

static void test_webgl_texture_unit_is_caller_owned(void)
{
    const webgl_mask_batch_t batch = {0, 3, 1};
    for (unsigned unit = 0; unit <= 7; unit += 7) {
        recording_gpu_t gpu = {0};
        webgl_backend_t backend = {&gpu, record_call};
        webgl_mask_renderer_t *renderer = webgl_mask_renderer_new(&backend);
        TEST_ASSERT_TRUE(renderer != NULL, "renderer constructs without any application facilities");
        uint32_t texture = webgl_mask_renderer_draw(renderer, &backend, &batch, 1, 3, 128, unit, 101, 100);
        TEST_ASSERT_TRUE(texture && gpu.unit == 0x84c0 + unit && gpu.draws == 2,
                         "caller chooses the texture unit independently of coverage rendering");
        webgl_mask_renderer_free(renderer, &backend);
        TEST_ASSERT_TRUE(gpu.created == gpu.deleted,
                         "cleanup exchanges the published texture for the adopted previous texture");
    }
}

static void test_globe_cap_has_no_application_zoom_policy(void)
{
    const double rotation[] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
    double view[6], saved[6], point[3];
    TEST_ASSERT_TRUE(globe_view_bounds(rotation, 13, view) && fabs(view[2] + 13) < 1e-12 &&
                     fabs(view[3] + 13) < 1e-12 && fabs(view[4] - 13) < 1e-12 && fabs(view[5] - 13) < 1e-12,
                     "caller selects an angular radius without application zoom or padding");
    memcpy(saved, view, sizeof view);
    TEST_ASSERT_TRUE(!globe_view_bounds(rotation, -1, view) && !memcmp(saved, view, sizeof view),
                     "invalid radius leaves caller output intact");
    TEST_ASSERT_TRUE(globe_view_bounds(rotation, 0, view) && view[2] == view[4] && view[3] == view[5],
                     "zero-radius caps are supported independently of application magnification limits");
    TEST_ASSERT_TRUE(globe_view_project(rotation, 90, 0, point) && fabs(point[1] + 1) < 1e-12,
                     "orthographic projection requires no application state");
}

int tests_main(void)
{
    TEST_RUN_IN_GROUP(test_webgl_texture_unit_is_caller_owned, tests, NULL);
    TEST_RUN_IN_GROUP(test_globe_cap_has_no_application_zoom_policy, tests, NULL);
    return TESTS_EXIT_CODE();
}
