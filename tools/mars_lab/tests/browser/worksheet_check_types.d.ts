/**
 * @file worksheet_check_types.d.ts
 * @brief Shared native, DOM, calendar and asynchronous worksheet fixtures.
 *
 * Declares compile-time fixture shapes and browser harness entry points for these checks.
 * The declarations add no runtime behaviour and remain separate from production contracts.
 */

/**
 * Detect loss of a controller's concrete type through circular initialiser inference.
 * Ordinary interface assignments accept any, so this check guards the editor typing regression.
 */
type LabHasConcreteType<T> = 0 extends (1 & T) ? false : true;

/** Require a compile-time invariant without emitting browser code. */
type LabRequireConcreteType<T extends true> = T;

/** Mutually dependent owners must retain their types, including in the editor compiler. */
type LabControllerTypeChecks = [
    LabRequireConcreteType<LabHasConcreteType<typeof labDOM>>,
    LabRequireConcreteType<LabHasConcreteType<typeof labWire>>,
    LabRequireConcreteType<LabHasConcreteType<typeof WorksheetRequests.labRequests>>,
    LabRequireConcreteType<LabHasConcreteType<typeof WorksheetRequests.functionCard>>,
    LabRequireConcreteType<LabHasConcreteType<typeof WorksheetResults.reset>>,
    LabRequireConcreteType<LabHasConcreteType<typeof labSurfacePreview>>,
    LabRequireConcreteType<LabHasConcreteType<typeof labSurfaceVolume>>
];

/** Structural fixture contracts for worksheet, calendar and persistence regressions. */
interface LabWorksheetTestPlan extends MarsTransportPlan {
    calls: {service: string; args: unknown[]}[];
}

/** Native exports used by the isolated fixture, including memory operations. */
interface LabWorksheetTestNative extends MarsTransportExports {
    /** Fill a native memory range and return its start address. */
    memset(destination: number, value: number, length: number): number;

    /** Copy a disjoint native memory range and return its start address. */
    memcpy(destination: number, source: number, length: number): number;

    /** Copy a possibly overlapping native memory range. */
    memmove(destination: number, source: number, length: number): number;

    /** Compare native memory ranges using unsigned bytes. */
    memcmp(left: number, right: number, length: number): number;
}

/** Original callback and its owner, retained until fixture cleanup. */
interface LabWorksheetTestReplacement {
    owner: object;
    key: string;
    callback: unknown;
}

/** DOM state restored after projection checks, retaining node identity. */
interface LabWorksheetTestNodeSnapshot {
    node: HTMLElement;
    attributes: {namespace: string | null; name: string; value: string}[];
    children: ChildNode[];
    value: string | undefined;
    selected: boolean | undefined;
}

interface LabWorksheetTestSnapshotElement extends HTMLElement {
    value?: string;
    selected?: boolean;
}
interface LabWorksheetTestField {
    field: number;
    key: string;
    local: string;
    server: string;
    element: string;
}
interface LabWorksheetTestEffect {
    service: string;
    args: unknown[];
    visibility?: string;
}

/** Browser entry points invoked by the sequential harness. */
interface Window {
    /** Check the isolated native state owner. */
    labWorkspaceChecks(): Promise<void>;

    /** Check native DOM projection and restoration. */
    checkLabWorkspaceDom(): void;

    /** Check state continuations and their cleanup. */
    checkLabStateFlow(): Promise<boolean>;

    /** Check native storage behaviour. */
    checkLabStorage(): Promise<boolean>;

    /** Check persistence adapters. */
    checkLabPersistence(): Promise<void>;

    /** Check selection projection and restoration. */
    checkLabLocationDom(): Promise<void>;

    /** Check selection continuations and ownership. */
    checkLabLocationFlow(): Promise<void>;

    /** Check control-state capture and restoration. */
    checkLabCalendarState(): Promise<void>;

    /** Check picker projection. */
    checkLabPickerDom(): void;

    /** Check delegated control events. */
    checkLabAlmanacEvents(): void;

    /** Check deferred section rendering. */
    checkLabWeather(): Promise<void>;

    /** Check select enhancement and cleanup. */
    checkLabSelects(): Promise<void>;

    /** Check isolated widget behaviour. */
    checkLabWidgets(): void;
}

/** Deferred fixture value whose settlement is controlled by the check. */
interface LabWorksheetTestDeferred {
    promise: Promise<unknown>;

    /** Fulfil the pending operation with an opaque fixture value. */
    resolve(value?: unknown): void;

    /** Reject the pending operation without changing the reason. */
    reject(reason?: unknown): void;
}
interface LabWorksheetTestEditorFrame {
    read: (key: string) => unknown;
    local: boolean;
    raw?: unknown;
    text?: string;
    stage?: number;
}


interface LabWorksheetLocationSnapshot {
    generation: object;
    options: HTMLOptionElement[];
    value: string;
    candidates: string[];
}
interface LabWorksheetLocationRestore {
    restored: boolean;
    controls: WorksheetTownControls;
}

/** Deferred match response used by selection ownership fixtures. */
interface LabWorksheetTestMatchResponse {
    match_index: number;
}

/** Original notice content retained for fixture cleanup. */
interface LabWorksheetTestNoticeSnapshot {
    text: string;
    classes: string;
}
interface LabWorksheetDOMSnapshot {
    dataset: LabTestTextFields;
    [field: string]: unknown;
}
interface LabWorksheetResultSnapshot extends WorksheetResultSnapshot {
    resultInputText: string;
}

/** @brief The seven independent result slots returned by the native worksheet owner. */
interface LabWorksheetResultTable {
    expression: LabWorksheetResultSnapshot | null;
    equation: LabWorksheetResultSnapshot | null;
    diffequation: LabWorksheetResultSnapshot | null;
    matrix: LabWorksheetResultSnapshot | null;
    integrator: LabWorksheetResultSnapshot | null;
    datetime: LabWorksheetResultSnapshot | null;
    almanac: LabWorksheetResultSnapshot | null;
}

interface LabWorksheetWeatherSection { title: string; html: string; }
type LabWorksheetWeatherEvent = ['status', string] |
    ['render', HTMLElement, null, LabWorksheetWeatherSection[], string];
