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

/** @brief Advance startup through bootstrap, catalogue and application installation.
 * @param frame Borrowed mutable startup frame.
 * @param view Reserved borrowed host context.
 * @return Scoped continuation plan for the shared browser interpreter.
 */
int lab_bootstrap_step(int frame, int view);

/** @brief Return native search-control configuration as ordered browser effects.
 * The browser supplies the enhanced-select widget capability after definitions load.
 */
void lab_bootstrap_widgets(void);

/**
 * @brief Select workspace startup effects before the complete workspace exists.
 * @param phase Token URL cleanup plan 0, jurisdiction default plan 1, or GMT default value 2.
 * @param context Borrowed browser view. Phase 0 supplies token and lazy search,
 * prefix, pathname and hash strings. Phases 1 and 2 supply control and fallback.
 * @details Plans contain only replaceLocation or writeValue capabilities. The GMT
 * value is read separately after executing jurisdiction effects, preserving initialisation
 * order and avoiding reads of native-backed getters during a DOM handle scope.
 */
void lab_bootstrap_workspace(unsigned phase, int context);

#endif
