/**
 * @file lab_flow.h
 * @brief Scoped continuation plans for asynchronous Lab browser workflows.
 *
 * Native workflow modules select each operation, await boundary and error handler.
 * The browser stores request-local frames between calls; no C handle survives a
 * yield. Plans expose only named, allowlisted browser capabilities, never scripts.
 */
#ifndef LAB_FLOW_H
#define LAB_FLOW_H

/** @brief Build a single browser operation and advance its native continuation.
 * @param frame Borrowed request-local frame.
 * @param next Next native stage.
 * @param service Static allowlisted capability name.
 * @param save Optional frame property receiving the result.
 * @param wait Non-zero awaits the result before continuing.
 * @param count Number of following scoped argument handles.
 * @return Scoped operation plan.
 */
int lab_flow_call(int frame, unsigned next, const char *service, const char *save, int wait, unsigned count, ...);

/** @brief Execute an existing native service plan at its continuation boundary.
 * @param frame Borrowed frame.
 * @param next Next native stage.
 * @param plan Borrowed ordered service plan, or zero.
 * @return Scoped operation plan preserving its wait flag, but not its done flag.
 */
int lab_flow_effects(int frame, unsigned next, int plan);

/** @brief Finish a workflow. @param value Optional scoped return value; zero omits it.
 * @return Scoped completion plan.
 */
int lab_flow_done(int value);

/** @brief Select request ownership for a native workflow.
 * @param kind Evaluation 0, goal 1, weather 2, solver render 3, startup 4, installation 5.
 * @param frame Borrowed request-local input frame.
 */
void lab_flow_begin(unsigned kind, int frame);

/** @brief Advance one native continuation without retaining scoped handles.
 * @param kind Core workflow 0--5, request 6--15, state 16--25, location 26--39,
 * binding 40--55, result action 56--63, bootstrap 64, transport 65--69, or state/forms 72--75.
 * Only core request owners use lab_flow_begin.
 * @param frame Borrowed mutable request-local frame.
 * @param view Borrowed read-only browser capabilities.
 */
void lab_flow_step(unsigned kind, int frame, int view);

/** @brief Advance evaluation preparation, transport and completion.
 * @param frame Borrowed evaluation frame. @param view Borrowed read-only browser view.
 * @return Scoped continuation plan.
 */
int lab_flow_evaluation(int frame, int view);

/** @brief Advance two-phase result installation and optional solver rendering.
 * @param frame Borrowed installation frame. @param view Borrowed read-only browser view.
 * @return Scoped continuation plan.
 */
int lab_flow_install(int frame, int view);

#endif
