/**
 * @file lab_binding_integrator_state.h
 * @brief Native integrator binding-response projection policy for the Lab browser.
 *
 * Selects exact server editor metadata and editable parameters, then returns ordered
 * browser services for editor projection, binding controls and saved integrator text.
 * Binding syntax is recognised only through the native editor metadata cache; this
 * module never parses, sorts or rewrites mathematics. Browser services retain their
 * asynchronous editor preparation and source ownership. Scoped handles are borrowed
 * for one call and returned arrays retain browser records, not handle numbers.
 */
#ifndef LAB_WASM_BINDING_INTEGRATOR_STATE_H
#define LAB_WASM_BINDING_INTEGRATOR_STATE_H

/**
 * @brief Select editor and binding projections for an integrator response.
 * @param data Scoped native response with binding_expression, expression and binding_values fields.
 * @param fallback Scoped fallback binding-expression value, used when the response field is falsey.
 * @param editors Scoped Map of exact trimmed source keys to native editor metadata.
 * @param bound_names Scoped Set of integration-bound names to exclude from editable parameters.
 * @details Publishes an ordered calls plan through lab_dom_return. A recognised wrapped
 * editor requests setExpressionEditor, optional clearVariableValues, then
 * setIntegratorBindingExpression. Otherwise the plan renders remaining editable
 * bindings or clears them. The current editor supplies the body fallback. No supplied
 * record or live state is mutated; browser services execute after the call completes.
 */
void lab_binding_integrator_state(int data, int fallback, int editors, int bound_names);

#endif
