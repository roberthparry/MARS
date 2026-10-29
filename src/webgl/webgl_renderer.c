/**
 * @file webgl_renderer.c
 * @brief Portable native ownership and submission of double-buffered GPU coverage.
 *
 * Owns shader compilation policy, GPU handles, storage capacities, stencil execution
 * and publication. A synchronous backend forwards individual WebGL operations and
 * holds platform objects. Failed frames leave the caller's published texture intact.
 * One serial owner must use a renderer with one GPU context throughout its lifetime.
 */
#include <stdlib.h>

#include "webgl-t.h"

enum {
    gpu_stencil_test = 0x0b90, gpu_texture0 = 0x84c0, gpu_min_filter = 0x2801, gpu_mag_filter = 0x2800,
    gpu_linear = 0x2601, gpu_wrap_s = 0x2802, gpu_wrap_t = 0x2803, gpu_clamp_to_edge = 0x812f,
    gpu_blend = 0x0be2, gpu_cull_face = 0x0b44, gpu_depth_test = 0x0b71, gpu_colour_and_stencil = 0x4400
};

struct _webgl_mask_renderer_t {
    uint32_t resources[5], shade;
    unsigned pixels, texture_pixels, other_pixels;
    size_t capacity, command_capacity;
    webgl_mask_command_t *commands;
};

static uint32_t gpu(const webgl_backend_t *backend, unsigned operation, uint32_t a, uint32_t b,
                    uint32_t c, uint32_t d)
{
    const uint32_t arguments[4] = {a, b, c, d};
    return backend->call(backend->context, operation, arguments);
}

/* Release partial construction and successful renderer lifetimes identically. */
void webgl_mask_renderer_free(webgl_mask_renderer_t *renderer, const webgl_backend_t *backend)
{
    if (!renderer) return;
    for (unsigned kind = 0; kind < 5; ++kind)
        if (renderer->resources[kind]) gpu(backend, WEBGL_GPU_DELETE, kind, renderer->resources[kind], 0, 0);
    if (renderer->shade) gpu(backend, WEBGL_GPU_DELETE, WEBGL_GPU_UNIFORM_LOCATION, renderer->shade, 0, 0);
    free(renderer->commands);
    free(renderer);
}

/* Native code controls shader construction and all GPU object lifetimes. */
webgl_mask_renderer_t *webgl_mask_renderer_new(const webgl_backend_t *backend)
{
    if (!backend || !backend->call) return NULL;
    webgl_mask_renderer_t *renderer = calloc(1, sizeof *renderer);
    if (!renderer) return NULL;
    for (unsigned kind = 0; kind < 5; ++kind) {
        renderer->resources[kind] = gpu(backend, WEBGL_GPU_CREATE, kind, 0, 0, 0);
        if (!renderer->resources[kind]) goto failed;
    }
    uint32_t program = renderer->resources[WEBGL_GPU_PROGRAM];
    for (unsigned kind = WEBGL_GPU_VERTEX_SHADER; kind <= WEBGL_GPU_FRAGMENT_SHADER; ++kind) {
        uint32_t shader = gpu(backend, WEBGL_GPU_CREATE, kind, 0, 0, 0);
        if (!shader) goto failed;
        gpu(backend, WEBGL_GPU_SHADER_SOURCE, shader, kind - WEBGL_GPU_VERTEX_SHADER, 0, 0);
        gpu(backend, WEBGL_GPU_COMPILE, shader, 0, 0, 0);
        bool ok = gpu(backend, WEBGL_GPU_SHADER_OK, shader, 0, 0, 0) != 0;
        if (ok) gpu(backend, WEBGL_GPU_ATTACH, program, shader, 0, 0);
        gpu(backend, WEBGL_GPU_DELETE, kind, shader, 0, 0);
        if (!ok) goto failed;
    }
    gpu(backend, WEBGL_GPU_ATTRIBUTE, program, 0, 0, 0);
    gpu(backend, WEBGL_GPU_LINK, program, 0, 0, 0);
    if (!gpu(backend, WEBGL_GPU_PROGRAM_OK, program, 0, 0, 0)) goto failed;
    renderer->shade = gpu(backend, WEBGL_GPU_UNIFORM, program, 0, 0, 0);
    if (!renderer->shade || gpu(backend, WEBGL_GPU_ERROR, 0, 0, 0, 0)) goto failed;
    return renderer;
failed:
    webgl_mask_renderer_free(renderer, backend);
    return NULL;
}

static void restore(const webgl_backend_t *backend)
{
    gpu(backend, WEBGL_MASK_COLOUR_WRITE, 1, 0, 0, 0);
    gpu(backend, WEBGL_GPU_DISABLE, gpu_stencil_test, 0, 0, 0);
    gpu(backend, WEBGL_GPU_BIND_FRAMEBUFFER, 0, 0, 0, 0);
    gpu(backend, WEBGL_GPU_BIND_RENDERBUFFER, 0, 0, 0, 0);
}

/* Submit only a completely validated frame, then transfer texture ownership. */
uint32_t webgl_mask_renderer_draw(webgl_mask_renderer_t *renderer, const webgl_backend_t *backend,
                                 const webgl_mask_batch_t *batches, size_t count, size_t vertices,
                                 unsigned pixels, unsigned texture_unit, uint32_t quad, uint32_t previous)
{
    if (!renderer || !backend || !backend->call || !quad || !previous || pixels < 1 || pixels > 8192 ||
        texture_unit > UINT32_MAX - gpu_texture0 || count > 16384 ||
        previous == renderer->resources[WEBGL_GPU_TEXTURE]) return 0;
    size_t required = count * 13;
    if (required > renderer->command_capacity) {
        webgl_mask_command_t *next = realloc(renderer->commands, required * sizeof *next);
        if (!next) return 0;
        renderer->commands = next;
        renderer->command_capacity = required;
    }
    size_t commands = webgl_mask_commands(batches, count, vertices, renderer->commands, renderer->command_capacity);
    if (commands == SIZE_MAX) return 0;
    uint32_t texture = renderer->resources[WEBGL_GPU_TEXTURE];
    gpu(backend, WEBGL_GPU_ACTIVE_TEXTURE, gpu_texture0 + texture_unit, 0, 0, 0);
    gpu(backend, WEBGL_GPU_BIND_TEXTURE, texture, 0, 0, 0);
    if (renderer->texture_pixels != pixels) {
        gpu(backend, WEBGL_GPU_TEXTURE_PARAMETER, gpu_min_filter, gpu_linear, 0, 0);
        gpu(backend, WEBGL_GPU_TEXTURE_PARAMETER, gpu_mag_filter, gpu_linear, 0, 0);
        gpu(backend, WEBGL_GPU_TEXTURE_PARAMETER, gpu_wrap_s, gpu_clamp_to_edge, 0, 0);
        gpu(backend, WEBGL_GPU_TEXTURE_PARAMETER, gpu_wrap_t, gpu_clamp_to_edge, 0, 0);
        gpu(backend, WEBGL_GPU_TEXTURE_IMAGE, pixels, 0, 0, 0);
    }
    gpu(backend, WEBGL_GPU_BIND_FRAMEBUFFER, renderer->resources[WEBGL_GPU_FRAMEBUFFER], 0, 0, 0);
    gpu(backend, WEBGL_GPU_FRAMEBUFFER_TEXTURE, texture, 0, 0, 0);
    if (renderer->pixels != pixels) {
        gpu(backend, WEBGL_GPU_BIND_RENDERBUFFER, renderer->resources[WEBGL_GPU_RENDERBUFFER], 0, 0, 0);
        gpu(backend, WEBGL_GPU_RENDERBUFFER_STORAGE, pixels, 0, 0, 0);
        gpu(backend, WEBGL_GPU_FRAMEBUFFER_RENDERBUFFER, renderer->resources[WEBGL_GPU_RENDERBUFFER], 0, 0, 0);
    }
    if (!gpu(backend, WEBGL_GPU_FRAMEBUFFER_OK, 0, 0, 0, 0)) {
        renderer->pixels = renderer->texture_pixels = 0;
        restore(backend);
        return 0;
    }
    gpu(backend, WEBGL_GPU_VIEWPORT, pixels, 0, 0, 0);
    gpu(backend, WEBGL_GPU_USE_PROGRAM, renderer->resources[WEBGL_GPU_PROGRAM], 0, 0, 0);
    gpu(backend, WEBGL_GPU_DISABLE, gpu_blend, 0, 0, 0);
    gpu(backend, WEBGL_GPU_DISABLE, gpu_cull_face, 0, 0, 0);
    gpu(backend, WEBGL_GPU_DISABLE, gpu_depth_test, 0, 0, 0);
    gpu(backend, WEBGL_GPU_ENABLE, gpu_stencil_test, 0, 0, 0);
    gpu(backend, WEBGL_GPU_STENCIL_MASK, 255, 0, 0, 0);
    gpu(backend, WEBGL_GPU_CLEAR_STENCIL, 0, 0, 0, 0);
    gpu(backend, WEBGL_MASK_COLOUR_WRITE, 1, 0, 0, 0);
    gpu(backend, WEBGL_GPU_CLEAR_COLOUR, 0, 0, 0, 1);
    gpu(backend, WEBGL_MASK_CLEAR, gpu_colour_and_stencil, 0, 0, 0);
    gpu(backend, WEBGL_GPU_BIND_BUFFER, renderer->resources[WEBGL_GPU_BUFFER], 0, 0, 0);
    if (vertices * 2 * sizeof(float) > renderer->capacity)
        gpu(backend, WEBGL_GPU_BUFFER_DATA, (uint32_t)(vertices * 2 * sizeof(float)), 0, 0, 0);
    if (vertices) gpu(backend, WEBGL_GPU_BUFFER_UPLOAD, 0, 0, 0, 0);
    gpu(backend, WEBGL_GPU_ENABLE_ATTRIBUTE, 0, 0, 0, 0);
    for (size_t i = 0; i < commands; ++i) {
        webgl_mask_command_t command = renderer->commands[i];
        if (command.operation == WEBGL_MASK_BIND_XY)
            command.arguments[0] = command.arguments[0] ? quad : renderer->resources[WEBGL_GPU_BUFFER];
        if (command.operation == WEBGL_MASK_SHADE) command.arguments[1] = renderer->shade;
        backend->call(backend->context, command.operation, command.arguments);
    }
    bool ok = !gpu(backend, WEBGL_GPU_ERROR, 0, 0, 0, 0) && !gpu(backend, WEBGL_GPU_CONTEXT_LOST, 0, 0, 0, 0);
    restore(backend);
    if (!ok) {
        renderer->pixels = renderer->texture_pixels = 0;
        renderer->capacity = 0;
        return 0;
    }
    if (vertices * 2 * sizeof(float) > renderer->capacity) renderer->capacity = vertices * 2 * sizeof(float);
    renderer->pixels = pixels;
    renderer->texture_pixels = renderer->other_pixels;
    renderer->other_pixels = pixels;
    renderer->resources[WEBGL_GPU_TEXTURE] = previous;
    return texture;
}
