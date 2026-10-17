/**
 * @file result_dom_checks_types.d.ts
 * @brief Browser entry points and mutable capabilities for result DOM regressions.
 *
 * Declares compile-time fixture shapes and browser harness entry points for these checks.
 * The declarations add no runtime behaviour and remain separate from production contracts.
 */

/** Entry points for these browser regressions. */
interface Window {
    checkLabResultDom(): Promise<void>;
}
