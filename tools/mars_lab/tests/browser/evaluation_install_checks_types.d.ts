/**
 * @file evaluation_install_checks_types.d.ts
 * @brief Native installation-plan fixtures and their browser entry point.
 *
 * Declares compile-time fixture shapes and browser harness entry points for these checks.
 * The declarations add no runtime behaviour and remain separate from production contracts.
 */

/** Entry points for these browser regressions. */
interface Window {
    checkLabEvaluationInstall(): Promise<void>;
}

interface EvaluationInstallFixturePlan {
    calls: {service: string; args: unknown[]}[];
}
