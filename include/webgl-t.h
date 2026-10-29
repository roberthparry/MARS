/**
 * @file webgl-t.h
 * @brief Native binary coverage plans for a thin WebGL browser backend.
 *
 * Builds bounded triangle fans and ordered stencil batches from simple polygon rings.
 * Coordinates are floating-point clip-space values, never formatted text. Concave
 * rings use non-zero stencil winding; all rings have normalised winding and batches
 * contain at most 127 rings, avoiding eight-bit stencil overflow even with overlaps.
 * Native command planning validates ranges and supplies stencil state and draw ordering.
 * The coverage renderer owns GPU resource lifetimes through opaque backend IDs, compiles native GLSL,
 * reuses buffer storage and publishes only completed textures. A synchronous platform callback retains
 * actual GPU objects and forwards the supplied API operations. Borrowed upload bytes must match the
 * declared vertex count, and resource IDs must remain stable until released by their owner.
 * This module does not provide a graphics context or bypass browser GPU validation.
 * Plans have one serial owner and must not be read while being modified.
 */
#ifndef MARS_WEBGL_H
#define MARS_WEBGL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** @brief Opaque bounded polygon coverage builder. */
typedef struct _webgl_mask_t webgl_mask_t;

/** @brief Binary draw command: vertex range and black (0) or white (1) coverage. */
typedef struct _webgl_mask_batch_t {
    uint32_t first; /**< First vertex, not byte offset. */
    uint32_t count; /**< Triangle vertex count. */
    uint32_t colour; /**< Coverage value. */
} webgl_mask_batch_t;

/** @brief Coverage submission operations; numeric values form the portable backend ABI. */
typedef enum {
    WEBGL_MASK_BIND_XY = 0, /**< Bind XY mesh slot: 0 coverage, 1 full-screen quad. */
    WEBGL_MASK_COLOUR_WRITE = 1, /**< Enable all colour channels when argument zero is non-zero. */
    WEBGL_MASK_STENCIL_FUNCTION = 2, /**< WebGL stencilFunc arguments. */
    WEBGL_MASK_STENCIL_SEPARATE = 3, /**< WebGL stencilOpSeparate arguments. */
    WEBGL_MASK_STENCIL_OPERATION = 4, /**< WebGL stencilOp arguments. */
    WEBGL_MASK_DRAW = 5, /**< WebGL drawArrays arguments. */
    WEBGL_MASK_SHADE = 6, /**< Set the coverage shader shade uniform. */
    WEBGL_MASK_CLEAR = 7 /**< WebGL clear bit mask. */
} webgl_mask_operation_t;

/** @brief Fixed five-word command with WebGL enum values, independent of browser object handles. */
typedef struct _webgl_mask_command_t {
    uint32_t operation; /**< webgl_mask_operation_t selector. */
    uint32_t arguments[4]; /**< Unused argument words are zero. */
} webgl_mask_command_t;

/**
 * @brief Validate coverage batches and build their ordered stencil submission commands.
 * @param batches Borrowed batch array, or NULL when count is zero.
 * @param count Batch count, bounded to 16384.
 * @param vertices Available XY vertex count, bounded to 1048576 and divisible by three.
 * @param commands Output storage, or NULL to validate and query the required command count.
 * @param capacity Output capacity in commands; insufficient capacity produces no output.
 * @return Required/written count, zero for empty coverage, or SIZE_MAX for invalid input or insufficient capacity.
 * @details Emits no commands until every range has been validated. The backend owns context setup,
 * buffers and error recovery; native code owns winding, paint order and stencil resets.
 */
size_t webgl_mask_commands(const webgl_mask_batch_t *batches, size_t count, size_t vertices,
                          webgl_mask_command_t *commands, size_t capacity);

/** @brief Allocate a builder with a vertex budget in [3,1048576]; release with webgl_mask_free. */
webgl_mask_t *webgl_mask_new(size_t vertex_limit);

/** @brief Release a builder and its binary buffers; accepts NULL. */
void webgl_mask_free(webgl_mask_t *mask);

/** @brief Reset logical contents and failure state, retaining allocations for the next frame. */
void webgl_mask_reset(webgl_mask_t *mask);

/** @brief Begin a simple ring in nesting level 1..4, in non-decreasing level order. */
bool webgl_mask_begin(webgl_mask_t *mask, unsigned level);

/** @brief Append a finite clip-space ring vertex; failure invalidates the current frame until reset. */
bool webgl_mask_point(webgl_mask_t *mask, double x, double y);

/** @brief Close and normalise the ring; fewer than three points produce no coverage. */
bool webgl_mask_end(webgl_mask_t *mask);

/** @brief Borrow interleaved binary32 XY vertices until the next mutation; NULL for incomplete/failed frames. */
const float *webgl_mask_vertices(const webgl_mask_t *mask, size_t *count);

/** @brief Borrow ordered draw batches until the next mutation; NULL for incomplete/failed frames. */
const webgl_mask_batch_t *webgl_mask_batches(const webgl_mask_t *mask, size_t *count);

/** @brief Borrow the immutable WebGL 1 coverage vertex shader; attribute location is backend-owned. */
const char *webgl_mask_vertex_shader(void);

/** @brief Borrow the immutable WebGL 1 coverage fragment shader; uniform shade is black or white. */
const char *webgl_mask_fragment_shader(void);

/** @brief Backend calls beyond the coverage-command ABI; each is one WebGL API operation. */
typedef enum {
    WEBGL_GPU_CREATE = 8, /**< Create resource kind (program, buffer, framebuffer, renderbuffer, texture, shader). */
    WEBGL_GPU_DELETE, /**< Delete resource kind and handle. */
    WEBGL_GPU_SHADER_SOURCE, /**< Shader handle and native shader selector. */
    WEBGL_GPU_COMPILE, /**< Shader handle. */
    WEBGL_GPU_SHADER_OK, /**< Query shader compile status. */
    WEBGL_GPU_ATTACH, /**< Program and shader handles. */
    WEBGL_GPU_ATTRIBUTE, /**< Bind the position attribute at location zero. */
    WEBGL_GPU_LINK, /**< Program handle. */
    WEBGL_GPU_PROGRAM_OK, /**< Query program link status. */
    WEBGL_GPU_UNIFORM, /**< Obtain the shade uniform handle. */
    WEBGL_GPU_ACTIVE_TEXTURE, /**< Texture unit enum. */
    WEBGL_GPU_BIND_TEXTURE, /**< Texture handle. */
    WEBGL_GPU_TEXTURE_PARAMETER, /**< Parameter enum and value. */
    WEBGL_GPU_TEXTURE_IMAGE, /**< Allocate square RGBA storage with the supplied side length. */
    WEBGL_GPU_BIND_FRAMEBUFFER, /**< Framebuffer handle or zero. */
    WEBGL_GPU_FRAMEBUFFER_TEXTURE, /**< Attach texture handle as colour attachment zero. */
    WEBGL_GPU_BIND_RENDERBUFFER, /**< Renderbuffer handle or zero. */
    WEBGL_GPU_RENDERBUFFER_STORAGE, /**< Allocate square depth/stencil storage. */
    WEBGL_GPU_FRAMEBUFFER_RENDERBUFFER, /**< Attach depth/stencil renderbuffer handle. */
    WEBGL_GPU_FRAMEBUFFER_OK, /**< Query framebuffer completeness. */
    WEBGL_GPU_VIEWPORT, /**< Square viewport side length, origin zero. */
    WEBGL_GPU_USE_PROGRAM, /**< Program handle. */
    WEBGL_GPU_DISABLE, /**< Capability enum. */
    WEBGL_GPU_ENABLE, /**< Capability enum. */
    WEBGL_GPU_STENCIL_MASK, /**< Stencil write mask. */
    WEBGL_GPU_CLEAR_STENCIL, /**< Stencil clear value. */
    WEBGL_GPU_CLEAR_COLOUR, /**< Four colour components. */
    WEBGL_GPU_BIND_BUFFER, /**< Array-buffer handle. */
    WEBGL_GPU_BUFFER_DATA, /**< Allocate dynamic array-buffer bytes. */
    WEBGL_GPU_BUFFER_UPLOAD, /**< Upload the host's borrowed frame vertices. */
    WEBGL_GPU_ENABLE_ATTRIBUTE, /**< Vertex attribute index. */
    WEBGL_GPU_ERROR, /**< Query and consume the WebGL error code. */
    WEBGL_GPU_CONTEXT_LOST /**< Query context loss. */
} webgl_gpu_operation_t;

/** @brief Portable opaque GPU resource kinds used by create and delete operations. */
typedef enum {
    WEBGL_GPU_PROGRAM = 0, /**< Shader program. */
    WEBGL_GPU_BUFFER, /**< Vertex buffer. */
    WEBGL_GPU_FRAMEBUFFER, /**< Offscreen framebuffer. */
    WEBGL_GPU_RENDERBUFFER, /**< Depth/stencil renderbuffer. */
    WEBGL_GPU_TEXTURE, /**< Two-dimensional RGBA texture. */
    WEBGL_GPU_VERTEX_SHADER, /**< Vertex shader. */
    WEBGL_GPU_FRAGMENT_SHADER, /**< Fragment shader. */
    WEBGL_GPU_UNIFORM_LOCATION /**< Borrowed uniform location; deleting releases only its host handle. */
} webgl_gpu_resource_t;

/**
 * @brief Synchronous platform adapter, borrowed for each renderer call.
 * @details Return zero for unavailable resources or false queries. The callback must not throw or
 * re-enter the renderer. Resource IDs remain stable until deleted; zero denotes no resource.
 * Coverage commands use resolved buffer IDs for BIND_XY and argument one as the SHADE uniform ID.
 * The host supplies borrowed vertex bytes for BUFFER_UPLOAD and native GLSL for SHADER_SOURCE.
 */
typedef struct _webgl_backend_t {
    void *context; /**< Borrowed adapter state. */
    uint32_t (*call)(void *context, unsigned operation, const uint32_t arguments[4]); /**< API forwarding callback. */
} webgl_backend_t;

/** @brief Native owner of coverage GPU resources, allocation sizes and submission state. */
typedef struct _webgl_mask_renderer_t webgl_mask_renderer_t;

/** @brief Allocate and compile coverage GPU resources; NULL on failure, with partial allocations released. */
webgl_mask_renderer_t *webgl_mask_renderer_new(const webgl_backend_t *backend);

/** @brief Release native state and owned GPU resources; never deletes the last published texture or borrowed quad. */
void webgl_mask_renderer_free(webgl_mask_renderer_t *renderer, const webgl_backend_t *backend);

/**
 * @brief Validate and draw coverage, publishing a texture only after successful GPU submission.
 * @param renderer Native resource owner, used serially with the same underlying GPU context.
 * @param backend Synchronous adapter with this frame's vertex bytes available for upload.
 * @param batches Coverage batches; NULL allowed for an empty frame.
 * @param count Number of batches.
 * @param vertices Number of XY vertices in the borrowed host upload buffer.
 * @param pixels Square texture side length, 1..8192; hardware limits are checked by the backend.
 * @param texture_unit Caller-selected zero-based texture unit; hardware availability is checked by the backend.
 * @param quad Borrowed full-screen XY triangle buffer handle.
 * @param previous Previously published texture handle; its ownership transfers only on success.
 * @return Newly published texture handle, or zero on failure; previous remains visible on failure.
 * @details Leaves the selected texture unit, program, viewport and array buffer bound. Colour writes
 * are enabled, stencil testing is disabled and framebuffer/renderbuffer bindings are cleared on exit
 * after submission. Callers restore any surrounding render state they require.
 */
uint32_t webgl_mask_renderer_draw(webgl_mask_renderer_t *renderer, const webgl_backend_t *backend,
                                 const webgl_mask_batch_t *batches, size_t count, size_t vertices,
                                 unsigned pixels, unsigned texture_unit, uint32_t quad, uint32_t previous);

#endif
