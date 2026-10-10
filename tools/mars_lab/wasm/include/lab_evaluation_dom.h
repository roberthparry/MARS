/**
 * @file lab_evaluation_dom.h
 * @brief Private interface: projection of native solver renderings into the WASM worksheet.
 *
 * Chooses supplied TeX/SVG representations and diagnostic fallbacks for equation,
 * differential-equation and integrator cards. No mathematical text is parsed or
 * rewritten. C owns dataset/visibility policy; the browser supplies opaque native
 * records, DOM operations and asynchronous rendering when a representation is absent.
 *
 * Used by the freestanding browser modules and the versioned JavaScript host ABI.
 * This is not an installed MARS library interface; calls run synchronously on the browser thread.
 */
#ifndef LAB_WASM_EVALUATION_DOM_H
#define LAB_WASM_EVALUATION_DOM_H

/** @brief Install native solver renderings and return the retained TeX. @param mode Equation 1, differential 2,
 * integral 4.
 * @param data Native response. @param expandable Whether native equation metadata permits digit expansion. */
void lab_evaluation_render(unsigned mode, int data, int expandable);

/** @brief Project expression notes and value title. @param data Native response. @return Differentiability flag. */
int lab_evaluation_notes(int data);

/** @brief Install a solver rendering after its asynchronous ownership check. @param data Native response.
 * @param source Solver TeX. @param details Plain-text fallback. @param svg Native SVG. @param token Parent request
 * token. */
void lab_evaluation_solver(int data, int source, int details, int svg, double token);

#endif
