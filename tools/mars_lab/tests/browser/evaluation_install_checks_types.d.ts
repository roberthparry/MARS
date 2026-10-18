/**
 * @file evaluation_install_checks_types.d.ts
 * @brief Native installation-plan fixtures and their browser entry point.
 *
 * Declares compile-time fixture shapes and browser harness entry points for these checks.
 * The declarations add no runtime behaviour and remain separate from production contracts.
 */

/** Entry points for these browser regressions. */
interface Window {
    /** Run installation order and asynchronous completion regressions. */
    checkLabEvaluationInstall(): Promise<void>;
}

/** One ordered service call returned by a native fixture. */
interface EvaluationInstallFixtureCall {
    service: string;
    args: unknown[];
}

/** Ordered service calls for one installation phase. */
interface EvaluationInstallFixturePlan {
    calls: EvaluationInstallFixtureCall[];
}
