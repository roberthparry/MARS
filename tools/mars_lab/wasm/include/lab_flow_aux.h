/**
 * @file lab_flow_aux.h
 * @brief Private goal, weather, solver-render and startup continuation interfaces.
 *
 * Used by the shared browser-workflow dispatcher. Each function advances a
 * browser-owned frame and returns a scoped capability plan; the host performs
 * effects and awaits outside the DOM scope. Request identities and opaque native
 * mathematical values stay in the frame, never in retained C handles.
 */
#ifndef LAB_FLOW_AUX_H
#define LAB_FLOW_AUX_H

/** @brief Advance goal preparation, reconstruction or ordered completion.
 * @param frame Borrowed mutable goal frame with sourceText, target, start, options and request.
 * @param view Borrowed browser view containing presentation caches.
 * @return Scoped continuation or completion plan; stale preparation returns explicit false.
 */
int lab_flow_goal(int frame, int view);

/** @brief Advance background weather retrieval and overview publication.
 * @param frame Borrowed mutable weather frame with state, overviewData and request.
 * @param view Borrowed browser view, currently unused.
 * @return Scoped plan preserving parent-request freshness and weather-specific recovery.
 */
int lab_flow_weather(int frame, int view);

/** @brief Advance native solver-source rendering without interpreting its mathematics.
 * @param frame Borrowed mutable solver frame with source and request.
 * @param view Borrowed browser view, currently unused.
 * @return Scoped plan yielding native response data, or a failure capability on rejected rendering.
 */
int lab_flow_solver(int frame, int view);

/** @brief Advance initial worksheet evaluation with guarded DateTime location refresh.
 * @param frame Borrowed mutable startup frame, initially empty.
 * @param view Borrowed browser view, currently unused.
 * @return Scoped plan retaining mode and request-context identity across the location await.
 */
int lab_flow_startup(int frame, int view);

#endif
