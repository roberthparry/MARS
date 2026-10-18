/**
 * @file lab_bootstrap.h
 * @brief Native browser startup policy after WebAssembly instantiation.
 *
 * Keeps asset selection, response validation and startup order alongside the native
 * browser ABI. The initial script loader and WebAssembly instantiation remain host
 * responsibilities. No module handles survive a synchronous continuation.
 */
#ifndef LAB_BOOTSTRAP_H
#define LAB_BOOTSTRAP_H

#include "lab_handle.h"

/** @brief Workspace default-selection phase before full application initialisation. */
typedef enum {
    LAB_BOOTSTRAP_WORKSPACE_TOKEN = 0,        /**< Remove the authentication token from the displayed URL. */
    LAB_BOOTSTRAP_WORKSPACE_JURISDICTION = 1, /**< Fill an empty jurisdiction control from its default. */
    LAB_BOOTSTRAP_WORKSPACE_GMT = 2           /**< Publish the selected GMT value or its fallback. */
} lab_bootstrap_workspace_phase_t;

/**
 * @brief Advance startup through bootstrap, catalogue and application installation.
 * @param frame Scoped mutable browser frame; stage starts at zero, then response, data and valid store
 * results returned by the selected capabilities.
 * @param view Reserved scoped browser context, currently unused.
 * @return Scoped continuation plan for the shared browser interpreter.
 */
browser_handle lab_bootstrap_step(frame_handle frame, view_handle view);

/**
 * @brief Return native search-control configuration as ordered browser effects.
 * @post Publishes ordered enhance calls through lab_dom_return; the browser supplies the widget capability
 * after definitions load.
 */
void lab_bootstrap_widgets(void);

/**
 * @brief Select workspace startup effects before the complete workspace exists.
 * @param phase LAB_BOOTSTRAP_WORKSPACE_TOKEN (0), LAB_BOOTSTRAP_WORKSPACE_JURISDICTION (1),
 * or LAB_BOOTSTRAP_WORKSPACE_GMT (2).
 * @param context Scoped browser view. Phase 0 supplies token and lazy search,
 * prefix, pathname and hash strings. Phases 1 and 2 supply control and fallback.
 * @post Publishes a calls plan for phases 0 and 1, or the selected GMT string for phase 2, through lab_dom_return.
 * Unknown phases publish an empty calls plan.
 * @details Plans contain only replaceLocation or writeValue capabilities. The GMT
 * value is read separately after executing jurisdiction effects, preserving initialisation
 * order and avoiding reads of native-backed getters during a DOM handle scope.
 */
void lab_bootstrap_workspace(lab_bootstrap_workspace_phase_t phase, browser_handle context);

#endif
