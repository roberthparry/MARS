/**
 * @file layout_checks_types.d.ts
 * @brief Layout request, snapshot, DOM and native-export fixture contracts.
 *
 * Declares compile-time fixture shapes and browser harness entry points for these checks.
 * The declarations add no runtime behaviour and remain separate from production contracts.
 */

/** Entry points for these browser regressions. */
interface Window {
    /** @brief Check result geometry, deferred rendering and restored solver ownership. */
    checkLabLayout(): Promise<void>;
}

/** @brief One solver transport held for controlled completion or rejection. */
interface LayoutFixturePost {
    request: WorksheetRequests.MarsApiRequest | null;

    /** @brief Complete the pending solver transport. */
    resolve(value: WorksheetRequests.MarsApiPostResult): void;

    /** @brief Reject the pending solver transport with the supplied failure. */
    reject(reason?: unknown): void;
}

/** @brief One deferred fetch with the real request's cancellation signal. */
interface LayoutFixtureFetch {
    /** @brief Complete the fetch while retaining native response decoding. */
    resolve(value: MarsTransportResponse): void;

    signal: AbortSignal;
}

/** @brief Solver request identity supplemented by the isolated fixture's epoch. */
interface LayoutFixtureRequest extends WorksheetRequests.MarsApiRequest {
    epoch: number;
}

/** @brief Pending solver work and the transport it owns. */
interface LayoutFixturePending {
    task: Promise<unknown>;
    post: LayoutFixturePost;
}

/** @brief Restored solver work with its real native ownership record. */
interface LayoutFixtureRestoredPending {
    task: Promise<unknown>;
    post: LayoutFixtureFetch;
    pending: WorksheetRequests.MarsApiSolverPending;
}

/** @brief Ordinary values returned by native solver preparation. */
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

/** @brief DOM properties temporarily owned by the solver renderer and layout fixtures. */
interface LayoutFixtureResultElement extends HTMLElement {
    labSolverRestoreOwner?: unknown;
    labResultRenderRequest?: unknown;
    disabled?: boolean;
}

/** @brief Exact DOM state retained while a layout fixture temporarily replaces content. */
interface LayoutFixtureElementSnapshot {
    element: LayoutFixtureResultElement;
    attributes: [string, string][];
    children: ChildNode[];
    disabled: boolean | undefined;
    clientWidth: PropertyDescriptor | undefined;
}

/** @brief Native layout calls used here; geometry arrays also exercise invalid numeric inputs. */
interface LayoutFixtureNativeExports {
    memory: WebAssembly.Memory;

    /** @brief Return the currently expanded card index. */
    lab_view_card_expanded(): number;

    /** @brief Set and clamp one registered card's zoom index. */
    lab_view_card_set_zoom(card: number, zoom: number): number;

    /** @brief Step one card's zoom within the native limits. */
    lab_view_card_step_zoom(card: number, step: number): number;

    /** @brief Toggle exclusive expansion for a registered card. */
    lab_view_card_toggle(card: number): number;

    /** @brief Read one registered card's zoom index. */
    lab_view_card_zoom(card: number): number;

    /** @brief Collapse every registered card. */
    lab_view_cards_collapse(): number;

    /** @brief Register the requested number of cards. */
    lab_view_cards_reset(count: number): number;

    /** @brief Determine control availability, accepting booleans through native numeric coercion. */
    lab_view_controls(mode: number, busy: boolean, ready: boolean, back: number, forward: number,
                      canGoal: boolean, minimum: boolean, maximum: boolean): number;

    /** @brief Return borrowed resize geometry for the supplied editor measurements. */
    lab_view_editor_resize(...measurements: (number | boolean)[]): number;

    /** @brief Fit a matrix before applying its selected zoom. */
    lab_view_matrix_scale(base: number, zoom: number, width: number, available: number): number;

    /** @brief Select visible cards for a mode and value availability. */
    lab_view_mode_flags(mode: number, hasValue: boolean): number;

    /** @brief Return borrowed picker geometry for the supplied measurements. */
    lab_view_picker_rect(...measurements: number[]): number;

    /** @brief Read the length of a native mode label. */
    lab_view_text_length(mode: number, label: number): number;

    /** @brief Return borrowed tooltip geometry for the supplied measurements. */
    lab_view_tooltip_rect(...measurements: number[]): number;

    /** @brief Decide whether an available wrapped variant should be selected. */
    lab_view_wrapped(width: number, scale: number, available: number, hasWrapped: boolean): number;

    /** @brief Read a native zoom scale. */
    lab_view_zoom(index: number): number;

    /** @brief Read the native zoom catalogue size. */
    lab_view_zoom_count(): number;

    /** @brief Normalise a requested zoom index. */
    lab_view_zoom_index(index: number): number;

    /** @brief Step and clamp a zoom index. */
    lab_view_zoom_step(index: number, step: number): number;
}
