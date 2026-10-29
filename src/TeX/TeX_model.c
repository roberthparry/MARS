/**
 * @file TeX_model.c
 * @brief Own immutable semantic snapshots exposed through provider callbacks.
 *
 * Nodes describe mathematical structure, never TeX source. Native providers can
 * prepare these snapshots without keeping their original objects alive. Each
 * child is indexed directly; model destruction visits allocated nodes once.
 */
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "TeX-t.h"

#define MARS_SHARED_TeX_INTERNAL_ACCESS
#include "internal/TeX_internal.h"
#define MARS_TeX_INTERNAL_ACCESS
#include "TeX_internal.h"

struct _TeX_model_node_t {
    struct _TeX_model_node_t *next;
    const TeX_model_t *owner;
    TeX_node_view_t view;
    string_t *text;
    const TeX_model_node_t *children[];
};

struct _TeX_model_t {
    TeX_model_node_t *nodes;
};

static bool model_describe(const void *context, const void *object, TeX_node_view_t *view)
{
    const TeX_model_node_t *node = object;
    if (node->owner != context)
        return false;
    *view = node->view;
    return true;
}

static bool model_child(const void *context, const void *object, size_t index, TeX_node_ref_t *child);

static const TeX_provider_t model_provider = {
    .describe = model_describe,
    .child = model_child
};

static bool model_child(const void *context, const void *object, size_t index, TeX_node_ref_t *child)
{
    const TeX_model_node_t *node = object;
    if (node->owner != context || index >= node->view.child_count)
        return false;
    *child = (TeX_node_ref_t){.provider = &model_provider, .context = context, .node = node->children[index]};
    return true;
}

/* Allocate semantic storage without any native-module dependency. */
TeX_model_t *TeX_model_new(void)
{
    return calloc(1u, sizeof(TeX_model_t));
}

/* Copy each descriptor and child list; the model owns only its own node allocations. */
const TeX_model_node_t *TeX_model_add(TeX_model_t *model, TeX_node_view_t view,
                                      const TeX_model_node_t *const *children)
{
    if (!model || (view.child_count && !children) ||
        view.child_count > (SIZE_MAX - sizeof(TeX_model_node_t)) / sizeof *children) {
        errno = EINVAL;
        return NULL;
    }
    for (size_t i = 0; i < view.child_count; ++i)
        if (!children[i] || children[i]->owner != model) {
            errno = EINVAL;
            return NULL;
        }
    TeX_model_node_t *node = calloc(1u, sizeof *node + view.child_count * sizeof *children);
    if (!node)
        return NULL;
    node->text = view.text ? string_clone(view.text) : NULL;
    if (view.text && !node->text) {
        free(node);
        return NULL;
    }
    node->owner = model;
    node->view = view;
    node->view.text = node->text;
    if (view.child_count)
        memcpy(node->children, children, view.child_count * sizeof *children);
    node->next = model->nodes;
    model->nodes = node;
    return node;
}

/* Release independent node allocations without recursively following shared children. */
void TeX_model_free(TeX_model_t *model)
{
    if (!model)
        return;
    TeX_model_node_t *node = model->nodes;
    while (node) {
        TeX_model_node_t *next = node->next;
        string_free(node->text);
        free(node);
        node = next;
    }
    free(model);
}

static void model_release(const void *context)
{
    TeX_model_free((TeX_model_t *)context);
}

/* Hand a complete snapshot to an opaque context, retaining ownership on failure. */
TeX_context_t *TeX_model_context(TeX_model_t *model, const TeX_model_node_t *root)
{
    if (!model || !root || root->owner != model) {
        errno = EINVAL;
        return NULL;
    }
    return TeX_context_adopt((TeX_node_ref_t){&model_provider, model, root}, model_release);
}
