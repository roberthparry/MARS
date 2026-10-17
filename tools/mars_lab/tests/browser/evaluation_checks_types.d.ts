/**
 * @file evaluation_checks_types.d.ts
 * @brief Evaluation continuation plans, frames, installers and browser entry-point fixtures.
 *
 * Declares compile-time fixture shapes and browser harness entry points for these checks.
 * The declarations add no runtime behaviour and remain separate from production contracts.
 */

/** Entry points for these browser regressions. */
interface Window {
    checkLabEvaluation(): Promise<void>;
}

interface EvaluationFixturePlan {
    calls: {service: string; args: unknown[]}[];
    operation: string; mode: string; options: {parent: number};
    ui: boolean; done: boolean; value: unknown; save: string; wait: boolean;
    install: boolean; awaitInstall: boolean; weather: boolean; status: string; label: string;
    [name: string]: unknown;
}
interface EvaluationFixtureFrame {
    mode: number; stage: number; phase?: number;
    context: {text?: string}; options: {skipHistoryUpdate: boolean};
    [name: string]: unknown;
}
type EvaluationFixtureInstaller = (data: unknown, context: WorksheetRequests.MarsApiEvaluationContext, outcome: number,
    request?: WorksheetRequests.MarsApiRequest | null) => unknown;
interface EvaluationFixtureReply {
    ok: boolean; binding_values?: WorksheetBinding[];
    [name: string]: unknown;
}
