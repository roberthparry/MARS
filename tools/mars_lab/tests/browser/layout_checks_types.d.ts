/**
 * @file layout_checks_types.d.ts
 * @brief Layout request, snapshot, DOM and native-export fixture contracts.
 *
 * Declares compile-time fixture shapes and browser harness entry points for these checks.
 * The declarations add no runtime behaviour and remain separate from production contracts.
 */

/** Entry points for these browser regressions. */
interface Window {
    checkLabLayout(): Promise<void>;
}

interface LayoutFixturePost {
    request: WorksheetRequests.MarsApiRequest | null;
    resolve(value: WorksheetRequests.MarsApiPostResult): void;
    reject(reason?: unknown): void;
}
interface LayoutFixtureFetch {
    resolve(value: MarsTransportResponse): void;
    signal: AbortSignal;
}
interface LayoutFixtureSnapshot {
    node: HTMLElement;
    compactSvg: string;
    wrappedTex: string;
    parentToken: string;
    restoredOwner: object | null;
    operation: string;
    mode: string;
    options: {parent: number; input: boolean};
    payload: {tex: string};
    endpoint: string;
}
interface LayoutFixtureResultElement extends HTMLElement {
    labSolverRestoreOwner?: unknown;
    labResultRenderRequest?: unknown;
}

/** Raw WebAssembly numeric arguments also accept JS booleans through native coercion. */
type LayoutFixtureNativeExports = {memory: WebAssembly.Memory} &
    Record<`lab_${string}`, (...args: (number | boolean)[]) => number>;
