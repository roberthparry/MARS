/**
 * @file evaluation_setup_checks_types.d.ts
 * @brief Evaluation setup-plan fixtures and their browser entry point.
 *
 * Declares compile-time fixture shapes and browser harness entry points for these checks.
 * The declarations add no runtime behaviour and remain separate from production contracts.
 */

/** Entry points for these browser regressions. */
interface Window {
    /** Run setup ordering and lazy-read regressions. */
    checkLabEvaluationSetup(): Promise<void>;
}

/** Ordered native service calls and completion flags for one setup phase. */
interface EvaluationSetupFixturePlan {
    calls: {service: string; args: unknown[]}[];
    wait: boolean;
    done: boolean;
}

/** Values supplied by a watched fixture; omitted fields must stay unread. */
interface EvaluationSetupFixtureValues {
    expressionText?: unknown;
    bodyText?: unknown;
    lastInput?: unknown;
}

/** Mutable fields populated by native setup while preserving unrelated data. */
interface EvaluationSetupFixtureContext {
    enteredBindings?: unknown;
    unrelated?: unknown;
    editorText?: unknown;
    editorBodyText?: unknown;
    text?: unknown;
    entered?: unknown;
}
