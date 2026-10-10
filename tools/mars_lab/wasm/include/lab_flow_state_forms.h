/**
 * @file lab_flow_state_forms.h
 * @brief Clear-button and integrator-form continuations for the Lab workspace.
 *
 * Extends the native state controller with request-bound clearing and guarded
 * forms preparation. Browser-owned frames hold callbacks and responses across
 * awaits; C retains no handles. Opaque mathematical text is forwarded unchanged
 * to the native forms endpoint, apart from established empty-input defaults.
 */
#ifndef LAB_FLOW_STATE_FORMS_H
#define LAB_FLOW_STATE_FORMS_H

/**
 * @brief Advance one clear or integrator-form workflow.
 * @param kind Clear 0, refresh forms 1, parse bounds 2, restore bounds 3.
 * @param frame Borrowed mutable continuation frame containing browser values.
 * @param view Borrowed browser view; expression reads use out-of-scope services.
 * @return Scoped continuation plan; global dispatcher kinds are 72 through 75.
 */
int lab_flow_state_forms(unsigned kind, int frame, int view);

#endif
