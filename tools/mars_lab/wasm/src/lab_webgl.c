/**
 * @file lab_webgl.c
 * @brief Native coverage validation and WebGL command planning for the browser adapter.
 *
 * Worker-produced batch bytes are copied into bounded reusable storage. marslib owns
 * all stencil sequencing and rejects the complete frame before browser state changes.
 * Commands are borrowed only until the next synchronous call on this WASM instance.
 */
#include <stdlib.h>

#include "webgl-t.h"
#include "lab_dom.h"
#include "lab_webgl.h"

/* Build a validated command stream without retaining scoped browser handles. */
void lab_webgl_mask_commands(jsref bytes, unsigned vertices)
{
    static webgl_mask_batch_t batches[16384];
    static webgl_mask_command_t *commands;
    static size_t capacity;
    lab_dom_return(0);
    int length = lab_dom_bytes_copy(bytes, (unsigned char *)batches, sizeof batches);
    if (length < 0 || (size_t)length % sizeof *batches) return;
    size_t required = (size_t)length / sizeof *batches * 13;
    if (required > capacity) {
        webgl_mask_command_t *next = realloc(commands, required * sizeof *commands);
        if (!next) return;
        commands = next;
        capacity = required;
    }
    size_t count = webgl_mask_commands(batches, (size_t)length / sizeof *batches, vertices, commands, capacity);
    if (count == SIZE_MAX) return;
    jsref result = lab_dom_object(LAB_VALUE_OBJECT);
    lab_dom_set(result, "pointer", lab_dom_numeric((unsigned long)commands));
    lab_dom_set(result, "count", lab_dom_numeric(count));
    lab_dom_return(result);
}

/* The host forwards one API call and keeps only browser object slots. */
extern unsigned lab_webgl_host_call(jsref host, unsigned operation, unsigned a, unsigned b, unsigned c, unsigned d);

static uint32_t host_call(void *context, unsigned operation, const uint32_t arguments[4])
{
    return lab_webgl_host_call((jsref)(uintptr_t)context, operation, arguments[0], arguments[1], arguments[2], arguments[3]);
}

typedef struct _renderer_slot_t {
    webgl_mask_renderer_t *renderer;
    unsigned token;
} renderer_slot_t;

static renderer_slot_t renderer_slots[16];
static unsigned renderer_used, renderer_generation;

/* Allocate a direct-indexed owner token; stale tokens cannot release a newer renderer. */
unsigned lab_webgl_renderer_create(jsref host)
{
    unsigned available = ~renderer_used & 65535;
    if (!available || renderer_generation == 0x0fffffff) return 0;
    unsigned slot = (unsigned)__builtin_ctz(available);
    webgl_backend_t backend = {(void *)(uintptr_t)host, host_call};
    webgl_mask_renderer_t *renderer = webgl_mask_renderer_new(&backend);
    if (!renderer) return 0;
    renderer_slots[slot] = (renderer_slot_t){renderer, (++renderer_generation << 4) | slot};
    renderer_used |= 1u << slot;
    return renderer_slots[slot].token;
}

/* Delete only resources still owned by this native renderer. */
void lab_webgl_renderer_free(jsref host, unsigned token)
{
    renderer_slot_t *slot = renderer_slots + (token & 15);
    if (!token || slot->token != token) return;
    webgl_backend_t backend = {(void *)(uintptr_t)host, host_call};
    webgl_mask_renderer_free(slot->renderer, &backend);
    *slot = (renderer_slot_t){0};
    renderer_used &= ~(1u << (token & 15));
}

/* Borrow worker metadata and host upload bytes for one synchronous native draw. */
unsigned lab_webgl_renderer_draw(jsref host, unsigned token, jsref bytes, unsigned vertices,
                                 unsigned pixels, unsigned quad, unsigned previous)
{
    static webgl_mask_batch_t batches[16384];
    renderer_slot_t *slot = renderer_slots + (token & 15);
    if (!token || slot->token != token) return 0;
    int length = lab_dom_bytes_copy(bytes, (unsigned char *)batches, sizeof batches);
    if (length < 0 || (size_t)length % sizeof *batches) return 0;
    webgl_backend_t backend = {(void *)(uintptr_t)host, host_call};
    return webgl_mask_renderer_draw(slot->renderer, &backend, batches, (size_t)length / sizeof *batches,
                                   vertices, pixels, 4, quad, previous);
}
