/**
 * @file lab_flow_request.h
 * @brief Request-specific browser continuations for the Lab's private WebAssembly runtime.
 *
 * Separates application sequencing and response policy from browser-owned network
 * resources. The central continuation dispatcher calls this module with a local
 * workflow index; all frame and view handles expire at the end of that call.
 */
#ifndef LAB_FLOW_REQUEST_H
#define LAB_FLOW_REQUEST_H

/**
 * @brief Advance a request workflow without retaining browser handles.
 * @param kind Local workflow: Function 0, calculus 1, integrator 2, land 3,
 * holidays 4, mobile 5, forms 6, presentation 7, solver resize 8, ordinary fetch 9.
 * @param frame Borrowed mutable continuation frame storing browser-owned values.
 * @param view Borrowed read-only application view.
 * @return Scoped continuation plan for the generic browser interpreter.
 */
int lab_flow_request(unsigned kind, int frame, int view);

#endif
