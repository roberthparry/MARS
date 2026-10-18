/**
 * @file mobile_layout_checks_types.d.ts
 * @brief Browser entry-point declaration for responsive layout regressions.
 *
 * Declares compile-time fixture shapes and browser harness entry points for these checks.
 * The declarations add no runtime behaviour and remain separate from production contracts.
 */

/** Entry points for these browser regressions. */
interface Window {
    /** @brief Check a live worksheet snapshot at the fixture's phone and tablet sizes. */
    checkLabMobileLayout(state: string): Promise<void>;
}

/** @brief One responsive viewport exercised by the isolated CSS fixture. */
interface MobileLayoutViewport {
    width: number;
    height: number;
}
