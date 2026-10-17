/**
 * @file mobile_layout_checks_types.d.ts
 * @brief Browser entry-point declaration for responsive layout regressions.
 *
 * Declares compile-time fixture shapes and browser harness entry points for these checks.
 * The declarations add no runtime behaviour and remain separate from production contracts.
 */

/** Entry points for these browser regressions. */
interface Window {
    checkLabMobileLayout(state: string): Promise<void>;
}
