/**
 * @file worksheet_check_types.d.ts
 * @brief Shared native, DOM, calendar and asynchronous worksheet fixtures.
 *
 * Declares compile-time fixture shapes and browser harness entry points for these checks.
 * The declarations add no runtime behaviour and remain separate from production contracts.
 */

/** Detect loss of a controller's concrete type through circular initializer inference. */
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
type LabWorksheetTestNative = MarsTransportExports & {
    memset(destination: number, value: number, length: number): number;
    memcpy(destination: number, source: number, length: number): number;
    memmove(destination: number, source: number, length: number): number;
    memcmp(left: number, right: number, length: number): number;
};
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
interface Window { labWorkspaceChecks(): Promise<void>; }
interface Window { checkLabWorkspaceDom(): void; }
interface Window { checkLabStateFlow(): Promise<boolean>; }
interface Window { checkLabStorage(): Promise<boolean>; }
interface Window { checkLabPersistence(): Promise<void>; }
interface Window { checkLabLocationDom(): Promise<void>; }
interface Window { checkLabLocationFlow(): Promise<void>; }
interface Window { checkLabCalendarState(): Promise<void>; }
interface Window { checkLabPickerDom(): void; }
interface Window { checkLabAlmanacEvents(): void; }
interface Window { checkLabWeather(): Promise<void>; }
interface Window { checkLabSelects(): Promise<void>; }
interface Window { checkLabWidgets(): void; }


interface LabWorksheetTestDeferred {
    promise: Promise<unknown>;
    resolve(value?: unknown): void;
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
interface LabWorksheetDOMSnapshot {
    dataset: Record<string, string>;
    [field: string]: unknown;
}
interface LabWorksheetResultSnapshot extends WorksheetResultSnapshot {
    resultInputText: string;
}
interface LabWorksheetWeatherSection { title: string; html: string; }
type LabWorksheetWeatherEvent = ['status', string] |
    ['render', HTMLElement, null, LabWorksheetWeatherSection[], string];
