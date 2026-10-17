/**
 * @file evaluation_setup_checks_types.d.ts
 * @brief Evaluation setup-plan fixtures and their browser entry point.
 *
 * Declares compile-time fixture shapes and browser harness entry points for these checks.
 * The declarations add no runtime behaviour and remain separate from production contracts.
 */

/** Entry points for these browser regressions. */
interface Window {
    checkLabEvaluationSetup(): Promise<void>;
}

interface EvaluationSetupFixturePlan {
    calls: {service: string; args: unknown[]}[];
    wait: boolean;
    done: boolean;
}
