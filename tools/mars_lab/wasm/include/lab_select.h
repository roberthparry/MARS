/**
 * @file lab_select.h
 * @brief Application API: accessible selection-menu construction and interaction in C/WebAssembly.
 *
 * Projects native select options into searchable menus, owns selection/filtering
 * policy, listener registration and keyboard navigation, and uses the browser for
 * Unicode normalisation, DOM access and deferred focus/change events. Subscriptions
 * retain real select objects in the browser; C handles never survive a call.
 * Rendering necessarily visits each option; temporary handles are released after
 * each row, so even large town catalogues use bounded bridge storage.
 *
 * Used by the freestanding browser modules and the versioned JavaScript host ABI.
 * This is not an installed MARS library interface; calls run synchronously on the browser thread.
 * Boolean parameters use the wasm32 i32 ABI; browser callers must supply exactly 0 or 1.
 */
#ifndef LAB_WASM_SELECT_H
#define LAB_WASM_SELECT_H

#include <stdbool.h>

#include "lab_handle.h"

/** @brief Selection interaction origin. */
typedef enum {
    LAB_SELECT_SOURCE_BUTTON  = 0, /**< Menu opener button. */
    LAB_SELECT_SOURCE_SEARCH  = 1, /**< Search text input. */
    LAB_SELECT_SOURCE_MENU    = 2, /**< Options menu. */
    LAB_SELECT_SOURCE_OUTSIDE = 3, /**< Outside click. */
    LAB_SELECT_SOURCE_OPTION  = 4, /**< Selected option. */
} lab_select_source_t;

/** @brief Normalised selection input; zero also denotes an unrecognised browser key. */
typedef enum {
    LAB_SELECT_KEY_CLICK  = 0, /**< Pointer click or unrecognised key. */
    LAB_SELECT_KEY_DOWN   = 1, /**< ArrowDown. */
    LAB_SELECT_KEY_UP     = 2, /**< ArrowUp. */
    LAB_SELECT_KEY_ENTER  = 3, /**< Enter. */
    LAB_SELECT_KEY_SPACE  = 4, /**< Space. */
    LAB_SELECT_KEY_ESCAPE = 5, /**< Escape. */
    LAB_SELECT_KEY_INPUT  = 6, /**< Search input edit. */
} lab_select_key_t;

/** @brief Subscribed menu events; numeric values are stable browser ABI selectors. */
typedef enum {
    LAB_SELECT_BUTTON_CLICK   = 0, /**< Button pointer click. */
    LAB_SELECT_BUTTON_KEY     = 1, /**< Button keyboard input. */
    LAB_SELECT_MENU_KEY       = 2, /**< Menu keyboard input. */
    LAB_SELECT_MENU_CLICK     = 3, /**< Menu pointer click. */
    LAB_SELECT_SEARCH_INPUT   = 4, /**< Search edit. */
    LAB_SELECT_SEARCH_KEY     = 5, /**< Search keyboard input. */
    LAB_SELECT_LABEL_CLICK    = 6, /**< Label click. */
    LAB_SELECT_DOCUMENT_CLICK = 7, /**< Document click. */
    LAB_SELECT_CHANGE         = 8, /**< Native select changed. */
    LAB_SELECT_ACTION_COUNT   = 9, /**< Number of event selectors. */
} lab_select_action_t;

/**
 * @brief Create an accessible menu around a native select.
 * @param select Scoped native select-node handle; zero or an already enhanced select is ignored.
 * @param label Scoped associated label-node handle; its id supplies aria-labelledby when present.
 * @param searchable True adds a search input; false creates an unfiltered menu.
 * @param placeholder Scoped text string shown when no option is selected.
 * @param empty Scoped text string shown when search has no matching options.
 * @param search_placeholder Scoped search-input placeholder and accessible-label string.
 * @return True when the menu was created; false for an absent or already enhanced select.
 */
bool lab_select_create(node_jsref select, browser_jsref label, bool searchable, browser_jsref placeholder,
                       browser_jsref empty, browser_jsref search_placeholder);

/**
 * @brief Rebuild menu options with bounded temporary handles.
 * @param select Scoped native select-node handle whose options supply values, labels and disabled state.
 * @param details True includes each option's dataset.detail in a second column; false omits details.
 */
void lab_select_rebuild(node_jsref select, bool details);

/**
 * @brief Project the native selection, disabled state and current search filter.
 * @param select Scoped enhanced select-node handle; no action when its surrounding menu is absent.
 */
void lab_select_sync(node_jsref select);

/**
 * @brief Apply selection-menu interaction policy.
 * @param select Scoped enhanced native select-node handle.
 * @param source Button 0, search input 1, menu 2, outside click 3 or option 4.
 * @param key Click 0, ArrowDown 1, ArrowUp 2, Enter 3, Space 4, Escape 5 or input 6.
 * @param target Scoped clicked option-node handle for source 4; zero for other sources.
 * @return True when the caller must suppress the key's browser default and propagation; false otherwise,
 * including invalid selectors and ordinary pointer selection.
 */
bool lab_select_event(node_jsref select, lab_select_source_t source, lab_select_key_t key, browser_jsref target);

/**
 * @brief Create, register and initially project an accessible selection menu.
 * @param select Scoped native select node, or zero.
 * @param options Scoped options record containing searchable, placeholder, emptyText,
 * searchPlaceholder and details; absent values receive the normal menu defaults.
 * @param document Scoped Document for outside-click subscriptions; zero uses the select's ownerDocument.
 * @return True when enhanced; false for an absent or already enhanced select.
 * @details Uses the browser's associated labels to identify the first label. The
 * JavaScript factory may retain sync/close closures and the existing __mars callback
 * properties, but performs no listener registration or event policy itself.
 */
bool lab_select_enhance(node_jsref select, browser_jsref options, browser_jsref document);

/**
 * @brief Rebuild a menu using its current opaque presentation options.
 * @param select Scoped native select-node handle with an existing surrounding menu.
 * @param options Scoped options record; truthy details enables second-column option text.
 * @post Menu options, selection, disabled state and filtering reflect the current native select and options.
 * @details The browser may capture the original options object in a rebuild closure; C reads its current
 * values on every call, so later option changes remain effective.
 */
void lab_select_refresh(node_jsref select, browser_jsref options);

/**
 * @brief Subscribe the controls of an existing native-built menu once.
 * @param select Scoped native select node whose surrounding menu already exists.
 * @return True when registered; false when already registered, absent or structurally incomplete.
 * @details Registers button/menu/search keyboard and pointer input, label focus,
 * native changes and document outside clicks. Every subscription captures the actual
 * select object as its browser-owned context; no opaque handle is retained. A marker
 * on the select prevents repeated registration from duplicating any listener.
 */
bool lab_select_register(node_jsref select);

/**
 * @brief Dispatch a browser event registered by the select module.
 * @param action Button click 0, button key 1, menu key 2, menu click 3, search input 4,
 * search key 5, label click 6, document click 7 or native select change 8.
 * @param event Scoped browser Event with target and keyboard fields.
 * @param select Scoped select supplied from the subscription's actual browser context.
 * @post The host receives a plan with an empty calls array and optional
 * prevent/stop flags through lab_dom_return; unknown actions produce an inert plan.
 * @details Native projection delegates to the same interaction policy as
 * lab_select_event. Focus and change effects execute only after the handle scope
 * closes, permitting event listeners to enter the bridge again safely.
 */
void lab_select_dispatch(lab_select_action_t action, event_jsref event, node_jsref select);

#endif
