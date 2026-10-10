/**
 * @file lab_select.h
 * @brief Private interface: accessible selection-menu construction and interaction in C/WebAssembly.
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
 */
#ifndef LAB_WASM_SELECT_H
#define LAB_WASM_SELECT_H

/** @brief Create an accessible menu around a native select. @param select Select node. @param label Label node.
 * @param searchable Whether to include search. @param placeholder Empty selection label.
 * @param empty Empty-search message. @param search_placeholder Search label; all text parameters are handles.
 * @return One when created; zero for an absent or already enhanced select. */
int lab_select_create(int select, int label, int searchable, int placeholder, int empty, int search_placeholder);

/** @brief Rebuild menu options with bounded temporary handles. @param select Select node.
 * @param details Whether to show option dataset.detail as a second column. */
void lab_select_rebuild(int select, int details);

/** @brief Project current selection, disabled state and filter. @param select Scoped select node. */
void lab_select_sync(int select);

/** @brief Apply menu interaction policy. @param select Select node.
 * @param source Button 0, search 1, menu 2, outside 3, option 4.
 * @param key Click 0, down 1, up 2, enter 3, space 4, escape 5, input 6.
 * @param target Clicked option node, otherwise zero. @return Whether to suppress the browser key default/bubbling. */
int lab_select_event(int select, unsigned source, unsigned key, int target);

/**
 * @brief Create, register and initially project an accessible selection menu.
 * @param select Scoped native select node, or zero.
 * @param options Scoped options record containing searchable, placeholder, emptyText,
 * searchPlaceholder and details; absent values receive the normal menu defaults.
 * @param document Scoped Document for outside-click subscriptions; zero uses the select's ownerDocument.
 * @return One when enhanced; zero for an absent or already enhanced select.
 * @details Uses the browser's associated labels to identify the first label. The
 * JavaScript factory may retain sync/close closures and the existing __mars callback
 * properties, but performs no listener registration or event policy itself.
 */
int lab_select_enhance(int select, int options, int document);

/**
 * @brief Rebuild a menu using its current opaque presentation options.
 * @param select Scoped native select node with an existing surrounding menu.
 * @param options Scoped options record; truthy details enables second-column option text.
 * @return No value. Replaces menu options and synchronises selection, disabled state and filtering.
 * @details The browser may capture the original options object in a rebuild closure;
 * C reads its current values on every call, so later option changes remain effective.
 */
void lab_select_refresh(int select, int options);

/**
 * @brief Subscribe the controls of an existing native-built menu once.
 * @param select Scoped native select node whose surrounding menu already exists.
 * @return One when registered; zero when already registered, absent or structurally incomplete.
 * @details Registers button/menu/search keyboard and pointer input, label focus,
 * native changes and document outside clicks. Every subscription captures the actual
 * select object as its browser-owned context; no opaque handle is retained. A marker
 * on the select prevents repeated registration from duplicating any listener.
 */
int lab_select_register(int select);

/**
 * @brief Dispatch a browser event registered by the select module.
 * @param action Button click 0, button key 1, menu key 2, menu click 3, search input 4,
 * search key 5, label click 6, document click 7 or native select change 8.
 * @param event Scoped browser Event with target and keyboard fields.
 * @param select Scoped select supplied from the subscription's actual browser context.
 * @return No C value. The host receives a plan with an empty calls array and optional
 * prevent/stop flags through lab_dom_return; unknown actions produce an inert plan.
 * @details Native projection delegates to the same interaction policy as
 * lab_select_event. Focus and change effects execute only after the handle scope
 * closes, permitting event listeners to enter the bridge again safely.
 */
void lab_select_dispatch(unsigned action, int event, int select);

#endif
