/**
 * @file evaluation_checks_types.d.ts
 * @brief Evaluation continuation plans, frames, installers and browser entry-point fixtures.
 *
 * Declares compile-time fixture shapes and browser harness entry points for these checks.
 * The declarations add no runtime behaviour and remain separate from production contracts.
 */

/** Entry points for these browser regressions. */
interface Window {
    /** Run lifecycle and request-ownership regressions. */
    checkLabEvaluation(): Promise<void>;
}

/** Service dispatch and policy fields returned by a native fixture call. */
interface EvaluationFixturePlan {
    calls: {service: string; args: unknown[]}[];
    operation: string;
    mode: string;
    options: {parent: number};
    ui: boolean;
    done: boolean;
    value: unknown;
    save: string;
    wait: boolean;
    install: boolean;
    awaitInstall: boolean;
    weather: boolean;
    status: string;
    label: string;
    [name: string]: unknown;
}

/** Mutable context retained while a native fixture advances. */
interface EvaluationFixtureFrame {
    mode: number;
    stage: number;
    phase?: number;
    context: {text?: string};
    options: {skipHistoryUpdate: boolean};
    [name: string]: unknown;
}

/** Callback replaced while observing installation boundaries. */
type EvaluationFixtureInstaller = (data: unknown, context: WorksheetRequests.MarsApiEvaluationContext, outcome: number,
    request?: WorksheetRequests.MarsApiRequest | null) => unknown;

/** Decoded reply with opaque fields retained for native policy checks. */
interface EvaluationFixtureReply {
    ok: boolean;
    binding_values?: WorksheetBinding[];
    [name: string]: unknown;
}

/** Controlled completion of a fixture operation with an opaque result. */
interface EvaluationFixtureGate {
    promise: Promise<unknown>;

    /** Resolve the operation without changing its result identity. */
    resolve(value: unknown): void;

    /** Reject the operation with its original failure value. */
    reject(reason: unknown): void;
}

/** DOM contents and attributes restored after lifecycle checks. */
interface EvaluationFixtureNodeSnapshot {
    children: ChildNode[];
    attributes: [string, string][];
}
